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

    constexpr OrderBook(HashMap<OrderId, OrderIterator>* orders, const Symbol symbol, EventHandler* event_handler) noexcept
        : _orders(orders), _symbol(symbol), _event_handler(event_handler) { }

    constexpr OrderBook() : OrderBook(nullptr, Symbol::invalid() , nullptr) { };

    [[nodiscard]] constexpr bool is_valid() const noexcept { return symbol_id() != SymbolId::invalid(); }

    constexpr void invalidate() noexcept { _symbol.id = SymbolId::invalid(); }

    template <LevelsType type, typename Self>
    [[nodiscard]] constexpr auto& levels(this Self&& self) noexcept {
        if constexpr (type == LevelsType::PRICE) {
            return self._price_levels;
        } else if constexpr (type == LevelsType::STOP) {
            return self._stop_levels;
        } else if constexpr (type == LevelsType::TRAILING_STOP) {
            return self._trailing_stop_levels;
        } else {
            assert(false && "Unknown level type");
        }
    }

    template <LevelsType type, OrderSide side, typename Self>
    constexpr auto& levels(this Self&& self) noexcept {
        return self.template levels<type>().template levels<side>();
    }

    template <LevelsType type, typename Self>
    [[nodiscard]] constexpr auto& bids(this Self&& self) noexcept {
        return self.template levels<type>().bids();
    }

    template <LevelsType type, typename Self>
    [[nodiscard]] constexpr auto& asks(this Self&& self) noexcept {
        return self.template levels<type>().asks();
    }

    template <typename Self>
    [[nodiscard]] constexpr auto& bids(this Self&& self) noexcept {
        return self.template levels<LevelsType::PRICE>().bids();
    }

    template <typename Self>
    [[nodiscard]] constexpr auto& asks(this Self&& self) noexcept {
        return self.template levels<LevelsType::PRICE>().asks();
    }

    template <LevelsType type, OrderSide side, concepts::Order T>
    constexpr auto add_order(T&& order) noexcept {
        auto id = order.id();

        assert(!orders().contains(id) && "Order with the same ID already exists in the order book");

        auto& levels = this->template levels<type, side>();
        auto level_it = levels.find(order.price());
        if (level_it == levels.end()) {
            // Price level doesn't exist and we need to create a new one
            if constexpr (should_report()) {
                event_handler().template on_price_level_add<type, side>(*this, order);
            }
            auto [new_it, success] = levels.add_level(order.price());
            level_it = new_it;
            assert(success && "Price level already exists, but you think it doesn't!");
        }

        if constexpr (should_report()) {
            event_handler().template on_order_add<type, side>(*this, order);
        }

        OrderIterator order_it = levels.add_order(std::forward<T>(order), level_it);

        add_order_to_map(id, order_it);
    }

    template <LevelsType type, OrderSide side>
    constexpr auto remove_order(OrderIterator order_it) {
        auto id = order_it.id();

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

        levels.remove_order(order_it, level_it);

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

    PriceLevels       <Order> _price_levels         { };
    StopLevels        <Order> _stop_levels          { };
    TrailingStopLevels<Order> _trailing_stop_levels { };

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
    constexpr void execute_quantity(Order& order, Quantity quantity, Price price) noexcept {
        (void)order;
        (void)quantity;
        (void)price;
    }

    constexpr void execute_quantity(Order& order, Quantity quantity) noexcept {
        (void)order;
        (void)quantity;
    }

    constexpr void reset_matching_prices() noexcept { }

    template <OrderSide side>
    constexpr Price calculate_trailing_stop_price(Order& order) noexcept {
        (void)order;
        return Price::invalid();
    }

    template <LevelsType type, OrderSide side>
    constexpr void reduce_order(Order& order, const Quantity quantity) noexcept {
        (void)order;
        (void)quantity;
    }

    template <LevelsType type, OrderSide side>
    constexpr void modify_order(Order& order, const Quantity quantity, const Price price, const bool mitigate) noexcept {
        (void)order;
        (void)quantity;
        (void)price;
        (void)mitigate;
    }

    template <LevelsType type, OrderSide side>
    constexpr void remove_order(Order& order) noexcept {
        (void)order;
    }

    template <LevelsType type, OrderSide side, concepts::Order T>
    constexpr void replace_order(Order& order, T&& new_order) noexcept {
        // Replace atomically. Since the matching engine is single-threaded,
        //  it can do it by performing remove then add, without worrying
        //  about other operations happening between remove and add
        // TODO have the event handler report replacement instead of removal and addition
        // TODO extract the duplication between this and reduce_order, modify_order, remove_order, and replace_order
        (void)order;
        (void)new_order;
    }

    template <OrderSide side>
    [[nodiscard]] constexpr Price get_market_price() const noexcept { return Price::invalid(); }

    template <OrderType type, OrderSide side>
    constexpr void unlink_order(Order& order) noexcept {
        (void)order;
    }

    template <LevelsType type, OrderSide side>
    constexpr void clear_levels() noexcept {
        auto& l = levels<type, side>();
        for (auto& [_, level] : l) {
            for (auto& order : level) {
                orders().erase(order.id());
            }
        }
        l.clear();
    }

    template <LevelsType type>
    constexpr void clear_levels() noexcept {
        clear_levels<type, OrderSide::BUY>();
        clear_levels<type, OrderSide::SELL>();
    }

    constexpr void clear() noexcept {
        clear_levels<LevelsType::PRICE>();
        clear_levels<LevelsType::STOP>();
        clear_levels<LevelsType::TRAILING_STOP>();
    }

};

}
