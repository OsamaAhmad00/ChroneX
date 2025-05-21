#pragma once

#include <chronex/concepts/Order.hpp>
#include <chronex/concepts/EventHandler.hpp>

#include <chronex/handlers/NullEventHandler.hpp>

#include <chronex/matching/Symbol.hpp>
#include <chronex/matching/Order.hpp>

#include <chronex/matching/levels/TrailingStopLevels.hpp>

namespace chronex {

template <
    concepts::Order OrderType = Order,
    concepts::EventHandler<OrderType> EventHandler = handlers::NullEventHandler
>
// Make use of the empty base class optimization and inherit from the logger
class OrderBook : protected EventHandler {
public:

    [[nodiscard]] size_t size() const noexcept { return 0; }

    [[nodiscard]] constexpr auto& price_levels() const noexcept { return _price_levels; }
    [[nodiscard]] constexpr auto& bids() const noexcept { return price_levels().bids(); }
    [[nodiscard]] constexpr auto& asks() const noexcept { return price_levels().asks(); }

    [[nodiscard]] constexpr auto& stop_levels() const noexcept { return _stop_levels; }
    [[nodiscard]] constexpr auto& stop_bids() const noexcept { return stop_levels().bids(); }
    [[nodiscard]] constexpr auto& stop_asks() const noexcept { return stop_levels().asks(); }

    [[nodiscard]] constexpr auto& trailing_stop_levels() const noexcept { return _trailing_stop_levels; }
    [[nodiscard]] constexpr auto& trailing_stop_bids() const noexcept { return trailing_stop_levels().bids(); }
    [[nodiscard]] constexpr auto& trailing_stop_asks() const noexcept { return trailing_stop_levels().asks(); }

    [[nodiscard]] constexpr auto& symbol() const noexcept { return Symbol; }

private:

    PriceLevels       <OrderType> _price_levels         { };
    StopLevels        <OrderType> _stop_levels          { };
    TrailingStopLevels<OrderType> _trailing_stop_levels { };

    Symbol Symbol { };
};

}
