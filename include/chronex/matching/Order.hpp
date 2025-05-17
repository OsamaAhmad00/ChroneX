#pragma once

#include <cstdint>
#include <compare>

#include <chronex/concepts/OrderBook.hpp>
#include <chronex/matching/Symbol.hpp>

#include "Orders/Market.hpp"
#include "Orders/Limit.hpp"
#include "Orders/Stop.hpp"
#include "Orders/StopLimit.hpp"
#include "Orders/TrailingStop.hpp"
#include "Orders/TrailingStopLimit.hpp"

namespace chronex {

struct Price {
    uint64_t value;
    explicit constexpr Price(const uint64_t value) noexcept : value(value) { }
    constexpr auto operator<=>(const Price &) const noexcept = default;
};

struct Quantity {
    uint64_t value;
    explicit constexpr Quantity(const uint64_t value) noexcept : value(value) { }
    constexpr auto operator<=>(const Quantity &) const noexcept = default;
    Quantity& operator+=(const Quantity &other) noexcept { value += other.value; return *this; }
    Quantity& operator-=(const Quantity &other) noexcept { value -= other.value; return *this; }
    Quantity operator+(const Quantity &other) const noexcept { return Quantity{value + other.value}; }
    Quantity operator-(const Quantity &other) const noexcept { return Quantity{value - other.value}; }
};

enum class OrderType : uint8_t
{
    MARKET,
    LIMIT,
    STOP,
    STOP_LIMIT,
    TRAILING_STOP,
    TRAILING_STOP_LIMIT
};

enum class OrderSide : uint8_t {
    BUY,
    SELL,
};

enum class TimeInForce : uint8_t {
    GTC,  // Good-Till-Cancelled
    IOC,  // Immediate-Or-Cancel
    FOK,  // Fill-Or-Kill
    AON   // All-Or-None
};

struct OrderId {
    uint64_t value;
    explicit constexpr OrderId(const uint64_t value) noexcept : value(value) { }
    constexpr bool operator==(const OrderId &) const noexcept = default;
};

struct OrderBase {
    OrderId id;

    SymbolId symbol_id;

    OrderType type;
    OrderSide side;
    TimeInForce time_in_force;
    // 1-byte padding

    Quantity quantity;
    Quantity filled;
    [[nodiscard]] constexpr Quantity remaining() const noexcept { return quantity - filled; }
};

struct Order : public OrderBase {
    union {
        MarketOrder as_market;
        LimitOrder as_limit;
        StopOrder as_stop;
        StopLimitOrder as_stop_limit;
        TrailingStopOrder as_trailing_stop;
        TrailingStopLimitOrder as_trailing_stop_limit;
    };

    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

template <concepts::OrderBook OrderBook>
bool Order::execute(OrderBook& orderbook) const noexcept {
    switch (type) {
        case OrderType::MARKET:
            return as_market.execute(orderbook);
        case OrderType::LIMIT:
            return as_limit.execute(orderbook);
        case OrderType::STOP:
            return as_stop.execute(orderbook);
        case OrderType::STOP_LIMIT:
            return as_stop_limit.execute(orderbook);
        case OrderType::TRAILING_STOP:
            return as_trailing_stop.execute(orderbook);
        case OrderType::TRAILING_STOP_LIMIT:
            return as_trailing_stop_limit.execute(orderbook);
        default:
            return false;
    }
}

}
