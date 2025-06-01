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
    if constexpr (type == OrderType::LIMIT) return LevelsType::PRICE;
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
        : _price_levels(event_handler), _stop_levels(event_handler), _trailing_stop_levels(event_handler),
          _orders(orders), _symbol(symbol), _event_handler(event_handler) { }

    constexpr OrderBook() : OrderBook(nullptr, Symbol::invalid() , nullptr) { };

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

    template <OrderType type, OrderSide side, concepts::Order T>
    constexpr auto add_order(T&& order) noexcept {
        auto id = order.id();

        assert(!orders().contains(id) && "Order with the same ID already exists in the order book");

        auto& levels = this->template levels<type, side>();
        auto level_it = levels.find(order.price());
        if (level_it == levels.end()) {
            // Price level doesn't exist and we need to create a new one
            event_handler().template on_add_level<type, side>(*this, order);
            auto [new_it, success] = levels.add_level(order.price());
            level_it = new_it;
            assert(success && "Price level already exists, but you think it doesn't!");
        }

        if constexpr (should_report()) {
            event_handler().template on_add_order<type, side>(*this, order);
        }

        OrderIterator order_it = levels.add_order(std::forward<T>(order), level_it);

        add_order_to_map(id, order_it);
    }

    template <OrderType type, OrderSide side>
    constexpr auto remove_order(OrderIterator order_it) {
        auto id = order_it->id();

        assert(orders().contains(id) && "Order with the same ID doesn't exists in the order book");

        auto& levels = this->template levels<type, side>();
        auto level_it = levels.find(order_it->price());
        assert(level_it != levels.end());

        if constexpr (should_report()) {
            event_handler().template on_order_delete<type, side>(*this, *order_it);

            if (level_it->is_empty()) {
                event_handler().template on_price_level_delete<type, side>(*this, *order_it);
            }
        }

        levels.template remove_order<type, side>(order_it, level_it);

        if (level_it->second.is_empty()) {
            event_handler().template on_remove_level<type, side>(*this, level_it->second);
            // Is removing levels with total_quantity == 0 an optimization or pessimization?
            // If you're not going to remove it, remember to consider levels with size == 0
            //  non-present, and not count a Levels struct with multiple 0-size levels not empty
            levels.remove_level(level_it);
        }

        remove_order_from_map(id);
    }

    template <LevelsType type>
    constexpr auto remove_order(OrderId id) noexcept {
        auto order_it = orders().find(id);
        if (order_it->side() == OrderSide::BUY) return this->remove_order<type, OrderSide::BUY>(order_it);
        return this->remove_order<type, OrderSide::SELL>(order_it);
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

    HashMap<OrderId, OrderIterator>* _orders { };

    Price _matching_bid_price = Price::invalid();
    Price _matching_ask_price = Price::invalid();

    Price _trailing_bid_price = Price::invalid();
    Price _trailing_ask_price = Price::invalid();

    Symbol _symbol { };

    EventHandler* _event_handler = nullptr;


public:

    template <OrderSide side>
    constexpr void update_last_matching_price(Price price) noexcept {
        (void)price;
    }

    template <OrderType type, OrderSide side>
    constexpr void execute_quantity(OrderIterator order_it, const Quantity quantity, const Price price) noexcept {

        event_handler().template on_execute_order<type, side>(*order_it, quantity);

        update_last_matching_price<side>(price);

        auto& price_levels = levels<type, side>();
        auto level_it = price_levels.find(order_it->price());
        auto& [level_price, level] = *level_it;

        level.template execute_quantity<type, side>(order_it, quantity);

        if (level.is_empty()) {
            event_handler().template on_remove_level<type, side>(*this, level_price);
            price_levels.remove_level(level_it);
        }
    }

    template <OrderType type, OrderSide side>
    constexpr void execute_quantity(OrderIterator order_it, const Quantity quantity) noexcept {
        execute_quantity<type, side>(order_it, quantity, order_it->price());
    }

    constexpr void reset_matching_prices() noexcept { }

    template <OrderSide side>
    constexpr Price calculate_trailing_stop_price(Order& order) noexcept {
        (void)order;
        return Price::invalid();
    }

    template <OrderType type, OrderSide side>
    constexpr void reduce_order(OrderIterator order_it, const Quantity quantity) noexcept {
        (void)order_it;
        (void)quantity;
    }

    template <OrderType type, OrderSide side>
    constexpr void modify_order(Order& order, const Quantity quantity, const Price price, const bool mitigate) noexcept {
        (void)order;
        (void)quantity;
        (void)price;
        (void)mitigate;
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr void replace_order(OrderIterator order_it, T&& new_order) noexcept {
        // Replace atomically. Since the matching engine is single-threaded,
        //  it can do it by performing remove then add, without worrying
        //  about other operations happening between remove and add
        // TODO have the event handler report replacement instead of removal and addition
        // TODO extract the duplication between this and reduce_order, modify_order, remove_order, and replace_order
        (void)order_it;
        (void)new_order;
    }

    template <OrderSide side>
    [[nodiscard]] constexpr Price get_market_price() const noexcept { return Price::invalid(); }

    template <OrderType type, OrderSide side>
    constexpr void unlink_order(Order& order) noexcept {
        (void)order;
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

};

}
