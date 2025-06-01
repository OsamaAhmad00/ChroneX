#pragma once

namespace chronex {
    enum class OrderType : uint8_t;
    enum class OrderSide : uint8_t;
}

namespace chronex::handlers {

class NullEventHandler {
public:
    // OrderBooks
    template <typename T>
    void on_add_new_orderbook(T&) noexcept { };
    template <typename T>
    void on_add_orderbook(T&) noexcept { };
    template <typename T>
    void on_remove_orderbook(T&) noexcept { };

    // Orders
    template <OrderType, OrderSide, typename T>
    void on_create_order(T&) const noexcept { }
    template <OrderType, OrderSide, typename T>
    void on_add_order(T&) const noexcept { }
    template <OrderType, OrderSide, typename T, typename V>
    void on_execute_order(T&, V) const noexcept { }
    template <OrderSide, typename T, typename U, typename V>
    void on_execute_order(T&, U, V) const noexcept { }
    template <OrderSide, typename T, typename U>
    void on_match_order(T&, U&) const noexcept { }
    template <OrderType, OrderSide, typename T>
    void on_remove_order(T&) const noexcept { }
    template <OrderType type, OrderSide side, typename T>
    void on_stop_order_trigger(T&) noexcept { };
};

}
