#pragma once

#include <cassert>
#include <cstdint>
#include <compare>
#include <functional>
#include <limits>

namespace chronex {

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

// TODO add mechanisms to report errors in release builds as well. Not just here, but in-place of all asserts.

template <typename T, typename U>
constexpr auto clipping_add(T t, U u) noexcept {
    auto max = std::numeric_limits<decltype(t + u)>::max();
    if (t <= (max - u)) return t + u;
    return max;
}

template <typename T, typename U>
constexpr auto clipping_sub(T t, U u) noexcept {
    auto min = std::numeric_limits<decltype(t - u)>::min();
    if (t >= (min + u)) return t - u;
    return min;
}

template <typename T, typename U>
constexpr auto safe_add(T t, U u) noexcept {
    auto max = std::numeric_limits<decltype(t + u)>::max();
    (void)max;
    assert((t <= (max - u)) && "Overflow");
    return t + u;
}

template <typename T, typename U>
constexpr auto safe_sub(T t, U u) noexcept {
    auto min = std::numeric_limits<decltype(t - u)>::min();
    (void)min;
    assert((t >= (min + u)) && "Underflow");
    return t - u;
}

struct Price {
    uint64_t value;
    explicit constexpr Price(const uint64_t _value) noexcept : value(_value) { }
    constexpr auto operator<=>(const Price &) const noexcept = default;
    constexpr Price& operator+=(const Price &other) noexcept { value += other.value; return *this; }
    constexpr Price& operator-=(const Price &other) noexcept { value -= other.value; return *this; }
    constexpr Price operator+(const Price &other) const noexcept { return Price{ value + other.value }; }
    constexpr Price operator-(const Price &other) const noexcept { return Price{ value - other.value }; }

    constexpr static Price invalid() noexcept { return Price { std::numeric_limits<decltype(value)>::max() }; }
    constexpr static Price max() noexcept { return Price { std::numeric_limits<decltype(value)>::max() - 1 }; }
    constexpr static Price min() noexcept { return Price { std::numeric_limits<decltype(value)>::min() }; }
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

struct TrailingDistance {

    using ValueType = int64_t;

    /*
        When positive: Absolute distance from the market
        When negative: percentage distance from the market in 0.01% units
        Examples:
         *  100    = 100 price units away from market price
         * -1      = 0.01% away from market price
         * -10,000 = 100% away from market price
    */

    constexpr static TrailingDistance from_price(const Price distance, const Price step) noexcept {
        return TrailingDistance { cast(distance.value), cast(step.value) };
    }

    constexpr static TrailingDistance from_percentage_units(const ValueType distance, const ValueType step) noexcept {
        return TrailingDistance { distance, step };
    }

    constexpr static TrailingDistance from_percentage(const double distance, const double step) noexcept {
        assert(distance >= 0 && distance <= 100 && step >= 0 && step <= 100);
        int64_t a = cast(distance * 100);
        int64_t b = cast(step * 100);
        return from_percentage_units(a, b);
    }

    [[nodiscard]] constexpr auto raw_distance() const noexcept { return distance; }
    [[nodiscard]] constexpr auto raw_step()     const noexcept { return step; }

    [[nodiscard]] constexpr bool is_absolute()   const noexcept { return distance >  0; }
    [[nodiscard]] constexpr bool is_percentage() const noexcept { return distance <  0; }
    [[nodiscard]] constexpr bool is_valid()      const noexcept { return distance != 0; }

    template <OrderSide side>
    [[nodiscard]] constexpr Price trailing_limit(const Price old_price, const Price market_price) const noexcept {
        ValueType diff = distance;
        ValueType trailing_step = step;
        if (is_percentage()) {
            diff = -distance * cast(market_price.value) / 10'000;
            trailing_step = -step * cast(market_price.value) / 10'000;
        }

        // TODO can signs or casts here cause any problem?
        if constexpr (side == OrderSide::BUY) {
            auto new_price = safe_add(market_price.value, static_cast<uint64_t>(diff));
            return (cast(old_price.value) - cast(new_price) >= trailing_step) ? Price{ new_price } : old_price;
        } else {
            auto new_price = safe_sub(market_price.value, static_cast<uint64_t>(diff));
            return (cast(new_price) - cast(old_price.value) >= trailing_step) ? Price{ new_price } : old_price;
        }
    }

    constexpr auto operator<=>(const TrailingDistance& other) const noexcept {
        // When comparing different types (one absolute, one percentage),
        // we need a reference price to make a meaningful comparison.
        // Without a reference price, we can't directly compare them.

        // If both are the same type (both absolute or both percentage), direct comparison works
        assert((is_absolute() && other.is_absolute()) ||
            (is_percentage() && other.is_percentage()) && "Can't compare different types of trailing offsets");

        return distance <=> other.distance;
    }

    constexpr bool operator==(const TrailingDistance& invalid) const = default;

    constexpr static TrailingDistance invalid() noexcept { return TrailingDistance { 0, 0 }; }

    ValueType distance;
    ValueType step;

private:

    static constexpr ValueType cast(auto num) noexcept { return static_cast<ValueType>(num); }

    constexpr explicit TrailingDistance(const ValueType distance, const ValueType step) noexcept
        : distance(distance), step(step) { }
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

struct OrderId {
    uint64_t value;
    explicit constexpr OrderId(const uint64_t _value) noexcept : value(_value) { }
    constexpr bool operator==(const OrderId &) const noexcept = default;

    static constexpr OrderId invalid() noexcept { return OrderId { std::numeric_limits<decltype(value)>::max() }; }
};
template <OrderSide side>
static constexpr auto opposite_side() noexcept {
    if constexpr (side == OrderSide::BUY) {
        return OrderSide::SELL;
    } else {
        return OrderSide::BUY;
    }
}

template <OrderSide side, typename OrderBook>
static constexpr auto& get_side(OrderBook& orderbook) noexcept {
    return orderbook.template levels<OrderType::LIMIT, side>();
}

template <OrderSide side, typename OrderBook>
static constexpr auto& get_opposite_side(OrderBook& orderbook) noexcept {
    return get_side<opposite_side<side>()>(orderbook);
}

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