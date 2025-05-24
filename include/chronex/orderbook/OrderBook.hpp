#pragma once

#include <chronex/concepts/Order.hpp>
#include <chronex/concepts/EventHandler.hpp>

#include <chronex/handlers/NullEventHandler.hpp>

#include <../Symbol.hpp>
#include <chronex/orderbook/Order.hpp>

#include <chronex/orderbook/levels/TrailingStopLevels.hpp>

namespace chronex {

template <
    concepts::Order OrderType = Order,
    concepts::EventHandler<OrderType> EventHandler = handlers::NullEventHandler
>
class OrderBook {
public:

    // Price Levels ----------
    template <typename Self>
    [[nodiscard]] constexpr auto& price_levels(this Self&& self) noexcept { return self._price_levels; }

    template <typename Self>
    [[nodiscard]] constexpr auto& bids(this Self&& self) noexcept { return self.price_levels().bids(); }

    template <typename Self>
    [[nodiscard]] constexpr auto& asks(this Self&& self) noexcept { return self.price_levels().asks(); }

    template <concepts::Order T>
    constexpr auto add_ask(T&& order) noexcept { return add_order(std::forward<T>(order), asks()); }

    template <concepts::Order T>
    constexpr auto add_bid(T&& order) noexcept { return add_order(std::forward<T>(order), bids()); }
    // ----------

    // Stop Levels ----------
    template <typename Self>
    [[nodiscard]] constexpr auto& stop_levels(this Self&& self) noexcept { return self._stop_levels; }

    template <typename Self>
    [[nodiscard]] constexpr auto& stop_bids(this Self&& self) noexcept { return self.stop_levels().bids(); }

    template <typename Self>
    [[nodiscard]] constexpr auto& stop_asks(this Self&& self) noexcept { return self.stop_levels().asks(); }

    template <concepts::Order T>
    constexpr auto add_stop_ask(T&& order) noexcept { return add_order(std::forward<T>(order), stop_asks()); }

    template <concepts::Order T>
    constexpr auto add_stop_bid(T&& order) noexcept { return add_order(std::forward<T>(order), stop_bids()); }
    // ----------

    // Trailing Stop Levels ----------
    template <typename Self>
    [[nodiscard]] constexpr auto& trailing_stop_levels(this Self&& self) noexcept { return self._trailing_stop_levels; }

    template <typename Self>
    [[nodiscard]] constexpr auto& trailing_stop_bids(this Self&& self) noexcept { return self.trailing_stop_levels().bids(); }

    template <typename Self>
    [[nodiscard]] constexpr auto& trailing_stop_asks(this Self&& self) noexcept { return self.trailing_stop_levels().asks(); }

    template <concepts::Order T>
    constexpr auto add_trailing_stop_ask(T&& order) noexcept { return add_order(std::forward<T>(order), trailing_stop_asks()); }

    template <concepts::Order T>
    constexpr auto add_trailing_stop_bid(T&& order) noexcept { return add_order(std::forward<T>(order), trailing_stop_bids()); }
    // ----------

    [[nodiscard]] constexpr auto& symbol() const noexcept { return _symbol; }



private:

    constexpr auto& orders() noexcept { return _orders; }

    template <concepts::Order T, typename Level>
    constexpr void add_order(const T&& order, Level& level) noexcept {
        auto id = order.id();
        auto iterator = level.add_order(std::forward<T>(order));
        assert(orders().find(id) == orders().end() && "Order with the same ID already exists in the order book");
        orders()[id] = iterator;
    }

    PriceLevels       <OrderType> _price_levels         { };
    StopLevels        <OrderType> _stop_levels          { };
    TrailingStopLevels<OrderType> _trailing_stop_levels { };

    // We can use StopLevels or TrailingStopLevels as
    //  well, all have the same OrderIterator type
    using OrderIterator = typename PriceLevels<OrderType>::OrderIterator;
    std::unordered_map<OrderId, OrderIterator> _orders { };

    Price _last_bid_price     = Price::invalid();
    Price _last_ask_price     = Price::invalid();

    Price _matching_bid_price = Price::invalid();
    Price _matching_ask_price = Price::invalid();

    Price _trailing_bid_price = Price::invalid();
    Price _trailing_ask_price = Price::invalid();

    EventHandler* event_handler { nullptr };

    Symbol _symbol { };
};

}
