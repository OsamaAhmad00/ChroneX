#pragma once

#include <chronex/orderbook/OrderUtils.hpp>
#include <chronex/Symbol.hpp>

namespace chronex {

struct Order {

    // TODO make it private after testing
    Order(
        const OrderId id,
        const SymbolId symbol_id,
        const OrderType type,
        const OrderSide side,
        const TimeInForce time_in_force,
        const Quantity leaves_quantity,
        const Quantity filled_quantity,
        const Quantity max_visible_quantity,
        const Price price,
        const Price stop_price,
        const Price initial_stop_price,
        const Price slippage,
        const TrailingDistance trailing_distance
    ) noexcept :
        _id(id),
        _symbol_id(symbol_id),
        _type(type),
        _side(side),
        _time_in_force(time_in_force),
        _leaves_quantity(leaves_quantity),
        _filled_quantity(filled_quantity),
        _max_visible_quantity(max_visible_quantity),
        _price(price),
        _stop_price(stop_price),
        _initial_stop_price(initial_stop_price),
        _slippage(slippage),
        _trailing_distance(trailing_distance)
    {

    }

    [[nodiscard]] constexpr OrderId id() const noexcept { return _id; }

    [[nodiscard]] SymbolId symbol_id() const noexcept { return _symbol_id; }

    [[nodiscard]] constexpr OrderType type() const noexcept { return _type; }
    [[nodiscard]] constexpr bool is_market_order() const noexcept { return is_market(type()); }
    [[nodiscard]] constexpr bool is_limit_order() const noexcept { return is_limit(type()); }
    [[nodiscard]] constexpr bool is_stop_order() const noexcept { return type() == OrderType::STOP; }
    [[nodiscard]] constexpr bool is_stop_limit_order() const noexcept { return type() == OrderType::STOP_LIMIT; }
    [[nodiscard]] constexpr bool is_trailing_stop_order() const noexcept { return type() == OrderType::TRAILING_STOP; }
    [[nodiscard]] constexpr bool is_trailing_stop_limit_order() const noexcept { return type() == OrderType::TRAILING_STOP_LIMIT; }

    [[nodiscard]] constexpr OrderSide side() const noexcept { return _side; }
    [[nodiscard]] constexpr bool is_buy_order() const noexcept { return side() == OrderSide::BUY; }
    [[nodiscard]] constexpr bool is_sell_order() const noexcept { return side() == OrderSide::SELL; }

    [[nodiscard]] constexpr TimeInForce time_in_force() const noexcept { return _time_in_force; }
    [[nodiscard]] constexpr bool is_ioc() const noexcept { return time_in_force() == TimeInForce::IOC; }
    [[nodiscard]] constexpr bool is_fok() const noexcept { return time_in_force() == TimeInForce::FOK; }
    [[nodiscard]] constexpr bool is_gtc() const noexcept { return time_in_force() == TimeInForce::GTC; }
    [[nodiscard]] constexpr bool is_aon() const noexcept { return time_in_force() == TimeInForce::AON; }

    [[nodiscard]] constexpr Quantity leaves_quantity() const noexcept { return _leaves_quantity; }
    [[nodiscard]] constexpr Quantity filled_quantity() const noexcept { return _filled_quantity; }
    [[nodiscard]] constexpr Quantity initial_quantity() const noexcept { return leaves_quantity() + filled_quantity(); }
    [[nodiscard]] constexpr bool is_fully_filled() const noexcept { return leaves_quantity() == Quantity { 0 }; }

    [[nodiscard]] constexpr Quantity max_visible_quantity() const noexcept { return _max_visible_quantity; }
    [[nodiscard]] constexpr Quantity visible_quantity() const noexcept { return std::min(leaves_quantity(), max_visible_quantity()); }
    [[nodiscard]] constexpr Quantity hidden_quantity() const noexcept { return std::max(Quantity { 0 }, leaves_quantity() - max_visible_quantity()); }

    [[nodiscard]] constexpr bool is_hidden() const noexcept { return max_visible_quantity() == Quantity { 0 }; }
    // TODO make sure of this
    [[nodiscard]] constexpr bool is_iceberg() const noexcept { return max_visible_quantity() < leaves_quantity(); }

    [[nodiscard]] constexpr Price price() const noexcept { return _price; }
    [[nodiscard]] constexpr Price stop_price() const noexcept { return _stop_price; }
    [[nodiscard]] constexpr Price initial_stop_price() const noexcept { return _initial_stop_price; }

    [[nodiscard]] constexpr Price slippage() const noexcept { return _slippage; }
    [[nodiscard]] constexpr bool has_slippage() const noexcept { return slippage() != Price { 0 }; }

    [[nodiscard]] constexpr TrailingDistance trailing_distance() const noexcept { return _trailing_distance; }

    [[nodiscard]] bool is_valid() const noexcept;

    constexpr void set_price(const Price& price) noexcept { _price = price; }
    constexpr void set_stop_price(const Price& price) noexcept { _stop_price = price; }

    // When triggering, we know the exact type. No need to check our own type again.
    template <OrderType type>
    constexpr void mark_triggered() noexcept { _type = get_triggered<type>(); }

    constexpr void set_stop_and_trailing_stop_prices(const Price trailing_stop_price) noexcept {
        const auto diff = price() - trailing_stop_price;
        _stop_price = trailing_stop_price;
        _price = trailing_stop_price + diff;
    }

    constexpr void set_time_in_force(TimeInForce time_in_force) noexcept { _time_in_force = time_in_force; }

    constexpr void reduce_quantity(const Quantity quantity) noexcept {
        assert(quantity <= _leaves_quantity && "Trying to reduce more quantity than the order has left");
        _leaves_quantity -= quantity;
    }

    constexpr void execute_quantity(const Quantity quantity) noexcept {
        reduce_quantity(quantity);
        increase_filled_quantity(quantity);
    }

    constexpr void increase_filled_quantity(const Quantity quantity) noexcept {
        _filled_quantity += quantity;
    }

    template <OrderSide side>
    constexpr void add_slippage() noexcept {
        if constexpr (side == OrderSide::BUY) {
            _price.value = safe_add(_price.value, slippage().value);
        } else {
            _price.value = safe_sub(_price.value, slippage().value);
        }
    }

    Order(const Order&) noexcept = delete;
    Order& operator=(const Order&) noexcept = delete;

    // An order can only be moved so that it's not present in multiple locations simultaneously
    Order(Order&&) noexcept = default;
    Order& operator=(Order&&) noexcept = default;

    ~Order() noexcept = default;

private:

    OrderId _id = OrderId::invalid();

    SymbolId _symbol_id = SymbolId::invalid();

    OrderType _type { OrderType::MARKET };
    OrderSide _side { OrderSide::BUY };
    TimeInForce _time_in_force { TimeInForce::GTC };
    // 1-byte padding

    Quantity _leaves_quantity = Quantity::invalid();
    Quantity _filled_quantity = Quantity::invalid();
    Quantity _max_visible_quantity = Quantity::max();

    Price _price = Price::invalid();
    Price _stop_price = Price::invalid();
    // For being able to construct initial order information for trailing stop (limit) orders
    Price _initial_stop_price = Price::invalid();

    // The default is to not have slippage. Even though limit orders can't have slippage, having
    //  as 0 by default allows us to always add/subtract the slippage without checking the order type.
    Price _slippage = Price { 0 };

    TrailingDistance _trailing_distance = TrailingDistance::invalid();
};

}
