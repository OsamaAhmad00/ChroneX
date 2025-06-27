#pragma once

#include <iostream>
#include <cstdint>

namespace chronex {
    enum class OrderType : uint8_t;
    enum class OrderSide : uint8_t;
}

namespace chronex::handlers {

// TODO Add all of these structs, functions, and operators inside the StreamEventHandler class
// TODO Remove these structs and functions. This is needed to
//  distinguish orders and orderbooks from other template args

template <typename Order>
struct OrderWrapper {
    Order& order;
};

template <typename Order>
OrderWrapper<Order> o(Order& order) {
    return { order };
}

template <typename OrderBook>
struct OrderBookWrapper {
    OrderBook& orderbook;
};

template <typename OrderBook>
OrderBookWrapper<OrderBook> ob(OrderBook& orderbook) {
    return { orderbook };
}


auto& operator<<(auto& s, Symbol symbol) {
    s << symbol.name;
    return s;
}

auto& operator<<(auto& s, OrderId id) {
    s << id.value;
    return s;
}

auto& operator<<(auto& s, Price price) {
    s << price.value;
    return s;
}

auto& operator<<(auto& s, Quantity quantity) {
    s << quantity.value;
    return s;
}

auto& operator<<(auto& s, OrderSide side) {
    s << ((side == OrderSide::BUY) ? "Buy" : "Sell");
    return s;
}

auto& operator<<(auto& s, OrderType type) {
    switch (type) {
        case OrderType::MARKET:
            s << "Market";
            break;
        case OrderType::LIMIT:
            s << "Limit";
            break;
        case OrderType::STOP:
            s << "Stop";
            break;
        case OrderType::STOP_LIMIT:
            s << "Stop Limit";
            break;
        case OrderType::TRAILING_STOP:
            s << "Trailing Stop";
            break;
        case OrderType::TRAILING_STOP_LIMIT:
            s << "Trailing Stop Limit";
            break;
        default:
            s << "Unknown";
    }
    return s;
}

template <typename T>
auto& operator<<(auto& s, OrderWrapper<T> wrapper) {
    auto& order = wrapper.order;
    s << "Order { ID = " << order.id() << " }";
    return s;
}

template <typename T>
auto& operator<<(auto& s, OrderBookWrapper<T> wrapper) {
    auto& orderbook = wrapper.orderbook;
    s << "OrderBook { Symbol = " << orderbook.symbol() << " }";
    return s;
}

template <typename StreamFunc>
class StreamEventHandler {

#define EVENT_NAME &__FUNCTION__[3]

    // TODO extract common logic
    // TODO fix the problem that we can't accept the orderbook as a template arg because the
    //  orderbook takes the event handler as an arg

    // This will be a reference type for functors that return references
    using Stream = decltype(StreamFunc{ }());

    Stream stream = StreamFunc{ }();

public:

    // OrderBooks

    void on_add_new_orderbook(auto& symbol) const noexcept {
        stream << EVENT_NAME << '\t' << symbol << '\n';
    }

    void on_add_orderbook(auto& symbol) const noexcept {
        stream << EVENT_NAME << '\t' << symbol << '\n';
    }

    void on_remove_orderbook(auto& orderbook) const noexcept {
        stream << EVENT_NAME << '\t' << ob(orderbook) << '\n';
    }

    // Levels

    template <OrderType type, OrderSide side>
    void on_add_level(auto& orderbook, const Price price) const noexcept {
        stream << EVENT_NAME << "\t\t\t(" << type << ", " << side << ")\t" << '\t' << ob(orderbook) << "\tPrice = " << price << '\n';
    }

    template <OrderType type, OrderSide side>
    void on_remove_level(auto& orderbook, const Price price) const noexcept {
        stream << EVENT_NAME << "\t\t(" << type << ", " << side << ")\t" << '\t' << ob(orderbook) << "\tPrice = " << price << '\n';
    }

    // Orders

    template <OrderType type, OrderSide side>
    void on_add_order(auto& orderbook, auto& order) const noexcept {
        stream << EVENT_NAME << "\t\t\t(" << type << ", " << side << ")\t" << '\t' << ob(orderbook) << '\t' << o(order) << '\n';
    }

    template <OrderType type, OrderSide side>
    void on_remove_order(auto& orderbook, auto& order) const noexcept {
        stream << EVENT_NAME << "\t\t(" << type << ", " << side << ")\t" << '\t' << ob(orderbook) << '\t' << o(order) << '\n';
    }

    template <OrderSide side>
    void on_execute_order(auto& orderbook, auto& order, const Quantity quantity, const Price price) const noexcept {
        stream << EVENT_NAME << "\t\t(" << order.type() << ", " << side << ")\t" << '\t' << ob(orderbook) << '\t' << o(order) << "\tQuantity = " << quantity << "\tPrice = " << price << '\n';
    }

    template <OrderType type, OrderSide side>
    void on_reduce_order(auto& orderbook, auto& order, Quantity quantity) const noexcept {
        stream << EVENT_NAME << "\t\t(" << type << ", " << side << ")\t" << '\t' << ob(orderbook) << '\t' << o(order) << "\tQuantity = " << quantity << '\n';
    }

    template <OrderSide side1, OrderSide side2>
    void on_match_order(auto& orderbook, auto& executing_order, auto& reducing_order) const noexcept {
        stream << EVENT_NAME << "\t\t\t(" << side1 << ", " << side2 << ")\t" << '\t' << ob(orderbook) << '\t' << o(executing_order) << '\t' << o(reducing_order) << '\n';
    }

    template <OrderSide side>
    void on_update_stop_price(auto& orderbook, auto& order) const noexcept {
        stream << EVENT_NAME << "\t\t\t(" << order.type() << ", " << side << ")\t" << '\t' << ob(orderbook) << '\t' << o(order) << '\n';
    }

    template <OrderType type, OrderSide side>
    void on_trigger_stop_order(auto& orderbook, auto& order) const noexcept {
        stream << EVENT_NAME << "\t\t\t(" << type << ", " << side << ")\t" << '\t' << ob(orderbook) << '\t' << o(order) << '\n';
    }
};

constinit static auto StdOutFunc = [] -> std::ostream& {
    return std::cout;
};

using StdOutEventHandler = StreamEventHandler<decltype(StdOutFunc)>;

}
