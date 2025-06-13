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
    void on_add_new_orderbook(T&) const noexcept { }
    template <typename T>
    void on_add_orderbook(T&) const noexcept { }
    template <typename T>
    void on_remove_orderbook(T&) const noexcept { }

    // Levels
    template <OrderType, OrderSide, typename T, typename U>
    void on_add_level(T&, U&) const noexcept { }
    template <OrderType, OrderSide, typename T, typename U>
    void on_remove_level(T&, U&) const noexcept { }

    // Orders
    template <OrderType, OrderSide, typename T, typename U>
    void on_add_order(T&, U&) const noexcept { }
    template <OrderSide, typename T, typename U, typename V, typename P>
    void on_execute_order(T&, U&, V, P) const noexcept { }
    template <OrderSide, OrderSide, typename T, typename U, typename V>
    void on_match_order(T&, U&, V&) const noexcept { }
    template <OrderType, OrderSide, typename T, typename U>
    void on_remove_order(T&, U&) const noexcept { }
    template <OrderSide, typename T, typename U>
    void on_update_stop_price(T&, U&) const noexcept { }
    template <OrderType, OrderSide, typename T, typename U>
    void on_trigger_stop_order(T&, U&) const noexcept { };
};

}
