#pragma once

#include <cassert>
#include <cstdint>
#include <compare>
#include <functional>
#include <limits>

namespace chronex {

struct Price {
    uint64_t value;
    explicit constexpr Price(const uint64_t _value) noexcept : value(_value) { }
    constexpr auto operator<=>(const Price &) const noexcept = default;
    constexpr Price& operator+=(const Price &other) noexcept { value += other.value; return *this; }
    constexpr Price& operator-=(const Price &other) noexcept { value -= other.value; return *this; }
    constexpr Price operator+(const Price &other) const noexcept { return Price{ value + other.value }; }
    constexpr Price operator-(const Price &other) const noexcept { return Price{ value - other.value }; }

    constexpr static Price invalid() noexcept { return Price { std::numeric_limits<decltype(value)>::max() }; }
};

struct Slippage : public Price {
    template <typename Op>
    [[nodiscard]] constexpr Price slippage_price(const Price price) const noexcept {
        return Op { } (price, Price { value });
    }

    [[nodiscard]] constexpr Price buy_slippage_price(const Price price) const noexcept {
        return slippage_price<std::plus<>>(price);
    }

    [[nodiscard]] constexpr Price sell_slippage_price(const Price price) const noexcept {
        return slippage_price<std::minus<>>(price);
    }
};

struct Quantity {
    uint64_t value;
    explicit constexpr Quantity(const uint64_t _value) noexcept : value(_value) { }
    constexpr auto operator<=>(const Quantity &) const noexcept = default;
    constexpr Quantity& operator+=(const Quantity &other) noexcept { value += other.value; return *this; }
    constexpr Quantity& operator-=(const Quantity &other) noexcept { value -= other.value; return *this; }
    constexpr Quantity operator+(const Quantity &other) const noexcept { return Quantity{ value + other.value }; }
    constexpr Quantity operator-(const Quantity &other) const noexcept { return Quantity{ value - other.value }; }

    constexpr static Quantity max() noexcept { return Quantity { std::numeric_limits<decltype(value)>::max() - 1 }; }
    constexpr static Quantity invalid() noexcept { return Quantity { std::numeric_limits<decltype(value)>::max() }; }
};

struct TrailingOffset {

    using ValueType = int64_t;

    /*
        When positive: Absolute distance from the market
        When negative: percentage distance from the market in 0.01% units
        Examples:
         *  100    = 100 price units away from market price
         * -1      = 0.01% away from market price
         * -10,000 = 100% away from market price
    */

    constexpr static TrailingOffset from_price(const Price price) noexcept {
        return TrailingOffset { static_cast<decltype(value)>(price.value) };
    }

    constexpr static TrailingOffset from_percentage_units(const int64_t units) noexcept {
        return TrailingOffset { -units / 10'000 };
    }

    constexpr static TrailingOffset from_percentage(const double percentage) noexcept {
        return from_percentage_units(static_cast<int64_t>(percentage * 10'000));
    }

    [[nodiscard]] constexpr auto raw() const noexcept { return value; }

    [[nodiscard]] constexpr bool is_absolute()   const noexcept { return value >  0; }
    [[nodiscard]] constexpr bool is_percentage() const noexcept { return value <  0; }
    [[nodiscard]] constexpr bool is_valid()      const noexcept { return value != 0; }

    template <typename Op>
    [[nodiscard]] constexpr Price trailing_limit(const Price price) const noexcept {
        int64_t diff = value;
        if (is_percentage()) {
            diff = -value * static_cast<int64_t>(price.value) / 10'000;
        }
        return Op { } (price, Price { static_cast<uint64_t>(diff) });
    }

    [[nodiscard]] constexpr Price buy_trailing_limit(const Price price) const noexcept {
        return trailing_limit<std::plus<>>(price);
    }

    [[nodiscard]] constexpr Price sell_trailing_limit(const Price price) const noexcept {
        return trailing_limit<std::minus<>>(price);
    }

    constexpr auto operator<=>(const TrailingOffset& other) const noexcept {
        // When comparing different types (one absolute, one percentage),
        // we need a reference price to make a meaningful comparison.
        // Without a reference price, we can't directly compare them.

        // If both are the same type (both absolute or both percentage), direct comparison works
        assert((is_absolute() && other.is_absolute()) ||
            (is_percentage() && other.is_percentage()) && "Can't compare different types of trailing offsets");

        return value <=> other.value;
    }

    constexpr bool operator==(const TrailingOffset& invalid) const = default;

    constexpr static TrailingOffset invalid() noexcept { return TrailingOffset { 0 }; }

    ValueType value;

private:

    constexpr explicit TrailingOffset(const int64_t raw) noexcept : value(raw) { }
};

enum class OrderType : uint8_t;

struct OrderTypeBits {
    constexpr static uint8_t Market    = 0b00000001;
    constexpr static uint8_t Limit     = 0b00000010;
    constexpr static uint8_t Stop      = 0b00000100;
    constexpr static uint8_t StopLimit = 0b00001000;
    constexpr static uint8_t Trailing  = 0b00010000;
    constexpr static uint8_t Triggered = 0b00100000;
};

enum class OrderType : uint8_t
{
    MARKET = OrderTypeBits::Market,
    LIMIT = OrderTypeBits::Limit,

    STOP = OrderTypeBits::Stop,
    STOP_LIMIT = OrderTypeBits::StopLimit,
    TRAILING_STOP = OrderTypeBits::Trailing | OrderTypeBits::Stop,
    TRAILING_STOP_LIMIT = OrderTypeBits::Trailing | OrderTypeBits::StopLimit,

    TRIGGERED_STOP = MARKET | STOP | OrderTypeBits::Triggered,
    TRIGGERED_STOP_LIMIT = LIMIT | STOP_LIMIT | OrderTypeBits::Triggered,
    TRIGGERED_TRAILING_STOP = MARKET | TRAILING_STOP | OrderTypeBits::Triggered,
    TRIGGERED_TRAILING_STOP_LIMIT = LIMIT | TRAILING_STOP_LIMIT | OrderTypeBits::Triggered,
};

constexpr bool is_market(const OrderType type) noexcept {
    return static_cast<uint8_t>(type) & OrderTypeBits::Market;
}

constexpr bool is_limit(const OrderType type) noexcept {
    return static_cast<uint8_t>(type) & OrderTypeBits::Limit;
}

constexpr bool is_triggered(const OrderType type) noexcept {
    return static_cast<uint8_t>(type) & OrderTypeBits::Triggered;
}

constexpr bool is_trailing(const OrderType type) noexcept {
    return static_cast<uint8_t>(type) & OrderTypeBits::Trailing && !is_triggered(type);
}

constexpr bool is_stop(const OrderType type) noexcept {
    return ((static_cast<uint8_t>(type) & OrderTypeBits::Stop) ||
            (static_cast<uint8_t>(type) & OrderTypeBits::StopLimit)) &&
            !is_triggered(type);
}

template <OrderType type>
[[nodiscard]] constexpr OrderType get_triggered() noexcept {
    OrderType result = type;
    if constexpr (type == OrderType::STOP || type == OrderType::TRAILING_STOP) {
        result = static_cast<OrderType>(static_cast<uint8_t>(type) | OrderTypeBits::Market);
    } else {
        result = static_cast<OrderType>(static_cast<uint8_t>(type) | OrderTypeBits::Limit);
    }
    return static_cast<OrderType>(static_cast<uint8_t>(result) | OrderTypeBits::Triggered);
}

enum class OrderSide : uint8_t {
    BUY,
    SELL,
};

/*
 * Market orders can only be one of these two. Have a separate enum for them
 *  to constrain users from using an undesired TIF when creating market orders.
 */
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

    static constexpr OrderId invalid() noexcept { return OrderId { std::numeric_limits<decltype(value)>::max() }; }
};

}

namespace std {
    template <>
    struct hash<chronex::OrderId> {
        size_t operator()(const chronex::OrderId &id) const noexcept {
            return std::hash<decltype(id.value)>{}(id.value);
        }
    };

    template <>
    struct hash<chronex::Price> {
        size_t operator()(const chronex::Price &price) const noexcept {
            return std::hash<decltype(price.value)>{}(price.value);
        }
    };
}