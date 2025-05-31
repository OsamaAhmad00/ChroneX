#pragma once

namespace chronex {
    enum class OrderType : uint8_t;
    enum class OrderSide : uint8_t;
}

namespace chronex::handlers {

class NullEventHandler {
public:
    template <typename T>
    void on_order_creation(T&) const noexcept { }
    template <typename T>
    void on_market_order_add(T&) const noexcept { }
    template <typename T, typename U>
    void on_order_execute(T&, U) const noexcept { }
    template <typename T, typename U, typename V>
    void on_execute_order(T&, U, V) const noexcept { }
    template <typename T, typename U>
    void on_order_match(T&, U&) const noexcept { }
    template <OrderSide side, typename T, typename U>
    void on_order_match(T&, U&) const noexcept { }
    template <typename T>
    void on_market_order_remove(T&) const noexcept { }
    template <OrderSide side, typename T>
    void on_limit_order_add(T&) const noexcept { }
    template <OrderSide side, typename T>
    void on_limit_order_remove(T&) const noexcept { }
    template <OrderType type, OrderSide side, typename T>
    void on_stop_order_add(T&) const noexcept { }
    template <OrderType type, OrderSide side, typename T>
    void on_stop_order_remove(T&) const noexcept { }
    template <OrderType type, OrderSide side, typename T>
    void on_stop_limit_order_add(T&) noexcept { };
    template <OrderType type, OrderSide side, typename T>
    void on_stop_order_trigger(T&) noexcept { };
    template <OrderType type, OrderSide side, typename T>
    void on_stop_limit_order_remove(T&) noexcept { };
    template <typename T>
    void on_add_new_orderbook(T&) noexcept { };
    template <typename T>
    void on_add_orderbook(T&) noexcept { };
    template <typename T>
    void on_remove_orderbook(T&) noexcept { };
};

}
