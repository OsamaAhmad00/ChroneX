#pragma once

#include <chronex/concepts/Order.hpp>
#include <chronex/concepts/EventHandler.hpp>

#include <chronex/Symbol.hpp>
#include <chronex/orderbook/Order.hpp>

#include <chronex/orderbook/levels/TrailingStopLevels.hpp>

#include <chronex/handlers/NullEventHandler.hpp>

namespace chronex {

enum class LevelsType {
    PRICE,
    STOP,
    TRAILING_STOP,
};

template <OrderType type>
constexpr LevelsType order_type_to_levels_type() noexcept {
    if constexpr (is_limit(type)) return LevelsType::PRICE;
    else if constexpr (is_trailing(type)) return LevelsType::TRAILING_STOP;
    return LevelsType::STOP;
}

template <typename Key, typename Value>
using unordered_map = std::unordered_map<Key, Value>;

template <
    concepts::Order Order = Order,
    concepts::EventHandler<Order> EventHandler = handlers::NullEventHandler,
    template <typename, typename> typename HashMap = unordered_map
>
class OrderBook {
public:

    // We can use StopLevels or TrailingStopLevels as
    //  well, all have the same OrderIterator type
    using OrderIterator = typename PriceLevels<Order>::OrderIterator;

    // TODO remove this
    using LevelQueueDataType = typename PriceLevels<Order>::LevelQueueDataType;

    constexpr OrderBook(HashMap<OrderId, OrderIterator>* orders, const Symbol symbol, EventHandler* event_handler) noexcept
        : _price_levels(), _stop_levels(), _trailing_stop_levels(),
          _orders(orders), _symbol(symbol), _event_handler(event_handler) { }

    constexpr OrderBook() noexcept : OrderBook(nullptr, Symbol::invalid() , nullptr) { };

    [[nodiscard]] constexpr bool is_valid() const noexcept { return symbol_id() != SymbolId::invalid(); }

    constexpr void invalidate() noexcept { _symbol.id = SymbolId::invalid(); }

    template <OrderType type, typename Self>
    [[nodiscard]] constexpr auto& levels(this Self&& self) noexcept {
        // TODO are we returning the right stop and trailing stop side?
        constexpr auto level_type = order_type_to_levels_type<type>();
        if constexpr (level_type == LevelsType::PRICE) {
            return self._price_levels;
        } else if constexpr (level_type == LevelsType::STOP) {
            return self._stop_levels;
        } else if constexpr (level_type == LevelsType::TRAILING_STOP) {
            return self._trailing_stop_levels;
        } else {
            assert(false && "Unknown level type");
        }
    }

    template <OrderType type, OrderSide side, typename Self>
    constexpr auto& levels(this Self&& self) noexcept {
        return self.template levels<type>().template levels<side>();
    }

    template <OrderType type, typename Self>
    [[nodiscard]] constexpr auto& bids(this Self&& self) noexcept {
        return self.template levels<type>().bids();
    }

    template <OrderType type, typename Self>
    [[nodiscard]] constexpr auto& asks(this Self&& self) noexcept {
        return self.template levels<type>().asks();
    }

    template <typename Self>
    [[nodiscard]] constexpr auto& bids(this Self&& self) noexcept {
        return self.template levels<OrderType::LIMIT>().bids();
    }

    template <typename Self>
    [[nodiscard]] constexpr auto& asks(this Self&& self) noexcept {
        return self.template levels<OrderType::LIMIT>().asks();
    }

    template <OrderType type, OrderSide side>
    constexpr auto add_order(Order&& order) noexcept {
        auto id = order.id();

        assert(!orders().contains(id) && "Order with the same ID already exists in the order book");

        auto level_it = [&] {
            if constexpr (is_stop(type)) {
                return get_or_add_level<type, side>(order.stop_price());
            } else {
                return get_or_add_level<type, side>(order.price());
            }
        }();

        if constexpr (should_report()) {
            event_handler().template on_add_order<type, side>(*this, order);
        }

        OrderIterator order_it = levels<type, side>().add_order(std::move(order), level_it);

        add_order_to_map(id, order_it);
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr auto reduce_order(OrderIterator order_it, T level_it, Quantity quantity) noexcept {
        return levels<type, side>().template reduce_order<type, side>(order_it, level_it, quantity);
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr auto remove_order(OrderIterator order_it, T level_it) noexcept {
        auto id = order_it->id();

        assert(orders().contains(id) && "Order with the same ID doesn't exists in the order book");

        auto& levels = this->template levels<type, side>();
        assert(level_it != levels.end());

        if constexpr (should_report()) {
            event_handler().template on_remove_order<type, side>(*this, *order_it);

            if (level_it->second.is_empty()) {
                event_handler().template on_remove_level<type, side>(*this, level_it->first);
            }
        }

        levels.remove_order(order_it, level_it);

        if (level_it->second.is_empty()) {
            event_handler().template on_remove_level<type, side>(*this, level_it->first);
            // Is removing levels with total_quantity == 0 an optimization or pessimization?
            // If you're not going to remove it, remember to consider levels with size == 0
            //  non-present, and not count a Levels struct with multiple 0-size levels not empty
            levels.remove_level(level_it);
        }

        remove_order_from_map(id);
    }

    template <OrderType type, OrderSide side, typename T>
    [[nodiscard]] constexpr auto execute_quantity(OrderIterator order_it, T level_it, const Quantity quantity, const Price price) noexcept {

        update_last_and_matching_price<side>(price);

        auto& [level_price, level] = *level_it;

        event_handler().template on_execute_order<side>(*this, *order_it, quantity, price);
        // TODO find a better way for knowing if the order is to be deleted
        if (quantity == order_it->leaves_quantity()) {
            // TODO report both execute, then remove if it's fully executed?
            event_handler().template on_remove_order<type, side>(*this, *order_it);
            // The order is fully executed and removed from the level
            orders().erase(order_it->id());
        }

        // Execution might remove the order. Don't use it after execution
        auto valid_order_it = levels<type, side>().execute_quantity(order_it, level_it, quantity);

        auto valid_level_it = level_it;
        if (level.is_empty()) {
            event_handler().template on_remove_level<type, side>(*this, level_price);
            auto& price_levels = levels<type, side>();
            ++valid_level_it;
            valid_order_it = valid_level_it->second.begin();
            price_levels.remove_level(level_it);
        }

        return std::make_pair(valid_order_it, valid_level_it);
    }

    template <OrderType type, OrderSide side>
    [[nodiscard]] constexpr auto execute_quantity(OrderIterator order_it, const Quantity quantity) noexcept {
        return execute_quantity<type, side>(order_it, quantity, order_it->price());
    }

    template <OrderSide side>
    constexpr Price calculate_trailing_stop_price(Order& order) noexcept {
        auto market_price = get_market_price<opposite_side<side>()>();
        auto old_price = order.stop_price();
        return order.trailing_distance().template trailing_limit<side>(old_price, market_price);
    }

    template <OrderType type, OrderSide side>
    constexpr auto get_or_add_level(const Price price) noexcept {
        auto& levels = this->template levels<type, side>();
        auto level_it = levels.find(price);
        if (level_it == levels.end()) {
            // Price level doesn't exist and we need to create a new one
            event_handler().template on_add_level<type, side>(*this, price);
            auto [new_it, success] = levels.add_level(price);
            level_it = new_it;
            assert(success && "Price level already exists, but you think it doesn't!");
        }
        return level_it;
    }

    template <OrderSide side>
    [[nodiscard]] constexpr Price get_market_price() const noexcept {
        if constexpr (side == OrderSide::BUY) {
            auto best_price = bids().is_empty() ? Price::min() : bids().begin()->first;
            return std::max(_matching_bid_price, best_price);
        } else {
            auto best_price = asks().is_empty() ? Price::max() : asks().begin()->first;
            return std::min(_matching_ask_price, best_price);
        }
    }

    template <OrderSide side>
    [[nodiscard]] constexpr Price get_market_trailing_stop_price() const noexcept {
        // TODO remove the duplication
        // Take care, here is min then max, where as for market, it's max then min
        if constexpr (side == OrderSide::BUY) {
            auto best_price = bids().is_empty() ? Price::min() : bids().begin()->first;
            return std::min(_last_bid_price, best_price);
        } else {
            auto best_price = asks().is_empty() ? Price::max() : asks().begin()->first;
            return std::max(_last_ask_price, best_price);
        }
    }

    template <OrderSide side>
    [[nodiscard]] constexpr Price get_trailing_stop_price() const noexcept {
        if constexpr (side == OrderSide::BUY) {
            return _trailing_bid_price;
        } else {
            return _trailing_ask_price;
        }
    }

    template <OrderSide side>
    constexpr void update_last_and_matching_price(const Price price) noexcept {
        update_last_price<side>(price);
        update_matching_price<side>(price);
    }

    template <OrderSide side>
    constexpr void update_last_price(const Price price) noexcept {
        if constexpr (side == OrderSide::BUY) {
            _last_bid_price = price;
        } else {
            _last_ask_price = price;
        }
    }

    template <OrderSide side>
    constexpr void update_matching_price(Price price) noexcept {
        if constexpr (side == OrderSide::BUY) {
            _matching_bid_price = price;
        } else {
            _matching_ask_price = price;
        }
    }

    template <OrderSide side>
    constexpr void update_trailing_stop_price(const Price price) noexcept {
        if constexpr (side == OrderSide::BUY) {
            _trailing_bid_price = price;
        } else {
            _trailing_ask_price = price;
        }
    }

    constexpr void reset_matching_prices() noexcept {
        _matching_bid_price = Price::min();
        _matching_ask_price = Price::max();
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr void unlink_order(OrderIterator order_it, T level_it) noexcept {
        levels<type, side>().unlink_order(order_it, level_it);
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr void link_order(OrderIterator order_it, T level_it) noexcept {
        levels<type, side>().link_order_back(order_it, level_it);
    }

    template <OrderType type, OrderSide side>
    constexpr void unlink_order(OrderIterator order_it) noexcept {
        auto level = levels<type, side>().find(order_it->price());
        return unlink_order<type, side>(order_it, level);
    }

    template <OrderType type, OrderSide side>
    constexpr void link_order(OrderIterator order_it) noexcept {
        auto level = get_or_add_level<type, side>(order_it->price());
        return link_order<type, side>(order_it, level);
    }

    template <OrderType type, OrderSide side>
    constexpr void clear_levels() noexcept {
        auto& l = levels<type, side>();
        for (auto& [_, level] : l) {
            for (auto& order : level) {
                orders().erase(order.id());
            }
        }
        l.clear();
    }

    template <OrderType type>
    constexpr void clear_levels() noexcept {
        clear_levels<type, OrderSide::BUY>();
        clear_levels<type, OrderSide::SELL>();
    }

    constexpr void clear() noexcept {
        clear_levels<OrderType::LIMIT>();
        clear_levels<OrderType::STOP>();
        clear_levels<OrderType::TRAILING_STOP>();
    }

    [[nodiscard]] constexpr auto& symbol() const noexcept { return _symbol; }

    [[nodiscard]] constexpr auto& symbol_id() const noexcept { return symbol().id; }

    OrderBook(const OrderBook&) = default;
    OrderBook(OrderBook&&) = default;

    OrderBook& operator=(const OrderBook&) = default;
    OrderBook& operator=(OrderBook&&) = default;

    ~OrderBook() { clear(); }

private:

    constexpr void add_order_to_map(OrderId id, OrderIterator order_it) noexcept {
        assert(!orders().contains(id) && "Order with the same ID already exists in the order book");
        orders()[id] = order_it;
    }

    constexpr void remove_order_from_map(OrderId id) noexcept {
        // TODO later, you might want to append the order somewhere
        //  else to keep track of history instead of getting rid of it
        assert(orders().contains(id) && "Order with the given ID doesn't exists in the order book");
        orders().erase(id);
    }

    constexpr auto& orders() noexcept { return *_orders; }

    [[nodiscard]] constexpr static bool should_report() noexcept {
        // Use this to help the compiler determine at compile-time that
        //  there is no need to call the event handler. Even though it
        //  will probably figure it out, since we're only storing a pointer
        //  to the handler, there is a possibility that it won't.
        // TODO figure out if this is actually useful
        return !std::is_same_v<EventHandler, handlers::NullEventHandler>;
    }

    constexpr auto& event_handler() const noexcept {
        assert(_event_handler != nullptr && "Event handler is not set!");
        return *_event_handler;
    }

    PriceLevels       <Order> _price_levels;
    StopLevels        <Order> _stop_levels;
    TrailingStopLevels<Order> _trailing_stop_levels;

    HashMap<OrderId, OrderIterator>* _orders;

    Symbol _symbol;

    EventHandler* _event_handler;

    Price _last_bid_price = Price::min();
    Price _last_ask_price = Price::max();

    Price _matching_bid_price = Price::min();
    Price _matching_ask_price = Price::max();

    Price _trailing_bid_price = Price::min();
    Price _trailing_ask_price = Price::max();
};

}
