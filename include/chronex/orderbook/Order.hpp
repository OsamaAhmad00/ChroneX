#pragma once

#include <chronex/orderbook/OrderUtils.hpp>
#include <chronex/Symbol.hpp>

namespace chronex {

struct Order {
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
    [[nodiscard]] constexpr Quantity hidden_quantity() const noexcept {
        if (max_visible_quantity() > leaves_quantity()) return Quantity { 0 };
        return leaves_quantity() - max_visible_quantity();
    }

    [[nodiscard]] constexpr bool is_hidden() const noexcept { return max_visible_quantity() == Quantity { 0 }; }
    // TODO make sure of this
    [[nodiscard]] constexpr bool is_iceberg() const noexcept { return max_visible_quantity() < leaves_quantity(); }

    [[nodiscard]] constexpr Price price() const noexcept { return _price; }
    [[nodiscard]] constexpr Price stop_price() const noexcept { return _stop_price; }
    [[nodiscard]] constexpr Price initial_stop_price() const noexcept { return _initial_stop_price; }

    [[nodiscard]] constexpr Price slippage() const noexcept { return _slippage; }
    [[nodiscard]] constexpr bool has_slippage() const noexcept { return slippage() != Price::invalid(); }

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
        increase_filled_quantity(quantity);
        reduce_quantity(quantity);
    }

    constexpr void increase_filled_quantity(const Quantity quantity) noexcept {
        _filled_quantity += quantity;
    }

    template <OrderSide side>
    constexpr void add_slippage() noexcept {
        // If there is no slippage, the slippage value will be very big,
        //  and the price will be min or max
        if constexpr (side == OrderSide::BUY) {
            _price.value = clipping_add(_price.value, slippage().value);
        } else {
            _price.value = clipping_sub(_price.value, slippage().value);
        }
    }

    Order(const Order&) noexcept = delete;
    Order& operator=(const Order&) noexcept = delete;

    // An order can only be moved so that it's not present in multiple locations simultaneously
    Order(Order&&) noexcept = default;
    Order& operator=(Order&&) noexcept = default;

    ~Order() noexcept = default;

    // TODO extract common parts, and arrange the parameters and args in a nice way
    constexpr static Order market(uint64_t id, uint32_t symbol_id, OrderSide side, uint64_t quantity, uint64_t slippage = Price::invalid().value) noexcept {
        // TODO are the values with invalid correct?
        return Order{ id, symbol_id, OrderType::MARKET, side, TimeInForce::IOC, quantity, Quantity::max().value, Price::invalid().value, Price::invalid().value, slippage, TrailingDistance::invalid() };
    }
    constexpr static Order buy_market(uint64_t id, uint32_t symbol_id, uint64_t quantity, uint64_t slippage = Price::invalid().value) noexcept {
        return market(id, symbol_id, OrderSide::BUY, quantity, slippage);
    }
    constexpr static Order sell_market(uint64_t id, uint32_t symbol_id, uint64_t quantity, uint64_t slippage = Price::invalid().value) noexcept {
        return market(id, symbol_id, OrderSide::SELL, quantity, slippage);
    }

    constexpr static Order limit(uint64_t id, uint32_t symbol_id, OrderSide side, uint64_t price, uint64_t quantity, TimeInForce tif = TimeInForce::GTC, uint64_t max_visible_quantity = Quantity::max().value) noexcept {
        // TODO are the values with invalid correct?
        return Order{ id, symbol_id, OrderType::LIMIT, side, tif, quantity, max_visible_quantity, price, Price::invalid().value, Quantity::invalid().value, TrailingDistance::invalid() };
    }
    constexpr static Order buy_limit(uint64_t id, uint32_t symbol_id, uint64_t price, uint64_t quantity, TimeInForce tif = TimeInForce::GTC, uint64_t max_visible_quantity = Quantity::max().value) noexcept {
        return limit(id, symbol_id, OrderSide::BUY, price, quantity, tif, max_visible_quantity);
    }
    constexpr static Order sell_limit(uint64_t id, uint32_t symbol_id, uint64_t price, uint64_t quantity, TimeInForce tif = TimeInForce::GTC, uint64_t max_visible_quantity = Quantity::max().value) noexcept {
        return limit(id, symbol_id, OrderSide::SELL, price, quantity, tif, max_visible_quantity);
    }

    constexpr static Order stop(uint64_t id, uint32_t symbol_id, OrderSide side, uint64_t stop_price, uint64_t quantity, TimeInForce tif = TimeInForce::GTC, uint64_t slippage = Price::invalid().value) noexcept {
        // TODO are the values with invalid correct?
        return Order{ id, symbol_id, OrderType::STOP, side, tif, quantity, Quantity::max().value, Price::invalid().value, stop_price, slippage, TrailingDistance::invalid() };
    }
    constexpr static Order buy_stop(uint64_t id, uint32_t symbol_id, uint64_t stop_price, uint64_t quantity, TimeInForce tif = TimeInForce::GTC, uint64_t slippage = Price::invalid().value) noexcept {
        return stop(id, symbol_id, OrderSide::BUY, stop_price, quantity, tif, slippage);
    }
    constexpr static Order sell_stop(uint64_t id, uint32_t symbol_id, uint64_t stop_price, uint64_t quantity, TimeInForce tif = TimeInForce::GTC, uint64_t slippage = Price::invalid().value) noexcept {
        return stop(id, symbol_id, OrderSide::SELL, stop_price, quantity, tif, slippage);
    }

    constexpr static Order stop_limit(uint64_t id, uint32_t symbol_id, OrderSide side, uint64_t stop_price, uint64_t price, uint64_t quantity, TimeInForce tif = TimeInForce::GTC, uint64_t max_visible_quantity = Quantity::max().value) noexcept {
        return Order{ id, symbol_id, OrderType::STOP_LIMIT, side, tif, quantity, max_visible_quantity, price, stop_price, Price::invalid().value, TrailingDistance::invalid() };
    }
    constexpr static Order buy_stop_limit(uint64_t id, uint32_t symbol_id, uint64_t stop_price, uint64_t price, uint64_t quantity, TimeInForce tif = TimeInForce::GTC, uint64_t max_visible_quantity = Quantity::max().value) noexcept {
        return stop_limit(id, symbol_id, OrderSide::BUY, stop_price, price, quantity, tif, max_visible_quantity);
    }
    constexpr static Order sell_stop_limit(uint64_t id, uint32_t symbol_id, uint64_t stop_price, uint64_t price, uint64_t quantity, TimeInForce tif = TimeInForce::GTC, uint64_t max_visible_quantity = Quantity::max().value) noexcept {
        return stop_limit(id, symbol_id, OrderSide::SELL, stop_price, price, quantity, tif, max_visible_quantity);
    }

    constexpr static Order trailing_stop(uint64_t id, uint32_t symbol_id, OrderSide side, uint64_t stop_price, uint64_t quantity, TrailingDistance trailing_distance, TimeInForce tif = TimeInForce::GTC, uint64_t slippage = Price::invalid().value) noexcept {
        return Order{ id, symbol_id, OrderType::TRAILING_STOP, side, tif, quantity, Quantity::max().value, Price::invalid().value, stop_price, slippage, trailing_distance };
    }
    constexpr static Order trailing_buy_stop(uint64_t id, uint32_t symbol_id, uint64_t stop_price, uint64_t quantity, TrailingDistance trailing_distance, TimeInForce tif = TimeInForce::GTC, uint64_t slippage = Price::invalid().value) noexcept {
        return trailing_stop(id, symbol_id, OrderSide::BUY, stop_price, quantity, trailing_distance, tif, slippage);
    }
    constexpr static Order trailing_sell_stop(uint64_t id, uint32_t symbol_id, uint64_t stop_price, uint64_t quantity, TrailingDistance trailing_distance, TimeInForce tif = TimeInForce::GTC, uint64_t slippage = Price::invalid().value) noexcept {
        return trailing_stop(id, symbol_id, OrderSide::SELL, stop_price, quantity, trailing_distance, tif, slippage);
    }

    constexpr static Order trailing_stop_limit(uint64_t id, uint32_t symbol_id, OrderSide side, uint64_t stop_price, uint64_t price, uint64_t quantity, TrailingDistance trailing_distance, TimeInForce tif = TimeInForce::GTC, uint64_t max_visible_quantity = Quantity::max().value) noexcept {
        return Order{ id, symbol_id, OrderType::TRIGGERED_TRAILING_STOP_LIMIT, side, tif, quantity, max_visible_quantity, price, stop_price, Price::invalid().value, trailing_distance };
    }
    constexpr static Order trailing_buy_stop_limit(uint64_t id, uint32_t symbol_id, uint64_t stop_price, uint64_t price, uint64_t quantity, TrailingDistance trailing_distance, TimeInForce tif = TimeInForce::GTC, uint64_t max_visible_quantity = Quantity::max().value) noexcept {
        return trailing_stop_limit(id, symbol_id, OrderSide::BUY, stop_price, price, quantity, trailing_distance, tif, max_visible_quantity);
    }
    constexpr static Order trailing_sell_stop_limit(uint64_t id, uint32_t symbol_id, uint64_t stop_price, uint64_t price, uint64_t quantity, TrailingDistance trailing_distance, TimeInForce tif = TimeInForce::GTC, uint64_t max_visible_quantity = Quantity::max().value) noexcept {
        return trailing_stop_limit(id, symbol_id, OrderSide::SELL, stop_price, price, quantity, trailing_distance, tif, max_visible_quantity);
    }

private:
    
    constexpr Order(
        const uint64_t id,
        const uint32_t symbol_id,
        const OrderType type,
        const OrderSide side,
        const TimeInForce time_in_force,
        const uint64_t quantity,
        const uint64_t max_visible_quantity,
        const uint64_t price,
        const uint64_t stop_price,
        const uint64_t slippage,
        const TrailingDistance trailing_distance
    ) noexcept :
        _id(id),
        _symbol_id(symbol_id),
        _type(type),
        _side(side),
        _time_in_force(time_in_force),
        _leaves_quantity(quantity),
        _filled_quantity(0),
        _max_visible_quantity(max_visible_quantity),
        _price(price),
        _stop_price(stop_price),
        _initial_stop_price(stop_price),
        _slippage(slippage),
        _trailing_distance(trailing_distance)
    {

    }

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

    Price _slippage = Price::invalid();

    TrailingDistance _trailing_distance = TrailingDistance::invalid();
};

}
