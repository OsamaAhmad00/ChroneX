#pragma once

#include <cstdint>
#include <compare>

namespace chronex {

struct Price {
    uint64_t value;
    explicit constexpr Price(const uint64_t _value) noexcept : value(_value) { }
    constexpr auto operator<=>(const Price &) const noexcept = default;
};

struct Quantity {
    uint64_t value;
    explicit constexpr Quantity(const uint64_t _value) noexcept : value(_value) { }
    constexpr auto operator<=>(const Quantity &) const noexcept = default;
    Quantity& operator+=(const Quantity &other) noexcept { value += other.value; return *this; }
    Quantity& operator-=(const Quantity &other) noexcept { value -= other.value; return *this; }
    Quantity operator+(const Quantity &other) const noexcept { return Quantity{value + other.value}; }
    Quantity operator-(const Quantity &other) const noexcept { return Quantity{value - other.value}; }
};

enum class OrderType : uint8_t;

struct OrderTypeBits {
    constexpr static uint8_t Market    = 0b00000001;
    constexpr static uint8_t Limit     = 0b00000010;
    constexpr static uint8_t Stop      = 0b00000100;
    constexpr static uint8_t Trailing  = 0b00001000;
    constexpr static uint8_t Triggered = 0b00010000;
};

enum class OrderType : uint8_t
{
    MARKET = OrderTypeBits::Market,
    LIMIT = OrderTypeBits::Limit,

    STOP = OrderTypeBits::Stop,
    STOP_LIMIT = OrderTypeBits::Stop | OrderTypeBits::Limit,
    TRAILING_STOP = OrderTypeBits::Trailing | OrderTypeBits::Stop,
    TRAILING_STOP_LIMIT = OrderTypeBits::Trailing | OrderTypeBits::Stop | OrderTypeBits::Limit,

    TRIGGERED_STOP = STOP | OrderTypeBits::Triggered,
    TRIGGERED_STOP_LIMIT = STOP_LIMIT | OrderTypeBits::Triggered,
    TRIGGERED_TRAILING_STOP = TRAILING_STOP | OrderTypeBits::Triggered,
    TRIGGERED_TRAILING_STOP_LIMIT = TRAILING_STOP_LIMIT | OrderTypeBits::Triggered,
};

constexpr bool is_market(const OrderType type) noexcept {
    return type == OrderType::MARKET ||
           type == OrderType::TRIGGERED_STOP ||
           type == OrderType::TRIGGERED_TRAILING_STOP;
}

constexpr bool is_limit(const OrderType type) noexcept {
    return type == OrderType::LIMIT ||
           type == OrderType::TRIGGERED_STOP_LIMIT ||
           type == OrderType::TRIGGERED_TRAILING_STOP_LIMIT;
}

constexpr bool is_trailing(const OrderType type) noexcept {
    return type == OrderType::TRAILING_STOP ||
           type == OrderType::TRAILING_STOP_LIMIT;
}

constexpr bool is_stop(const OrderType type) noexcept {
    return type == OrderType::STOP ||
           type == OrderType::STOP_LIMIT ||
           is_trailing(type);
}

enum class OrderSide : uint8_t {
    BUY,
    SELL,
};

enum class MarketTimeInForce : uint8_t {
    IOC,  // Immediate-Or-Cancel
    FOK,  // Fill-Or-Kill
};

enum class TimeInForce : uint8_t {
    IOC,  // Immediate-Or-Cancel
    FOK,  // Fill-Or-Kill
    GTC,  // Good-Till-Cancelled
    AON,  // All-Or-None
};

struct OrderId {
    uint64_t value;
    explicit constexpr OrderId(const uint64_t _value) noexcept : value(_value) { }
    constexpr bool operator==(const OrderId &) const noexcept = default;
};


}
