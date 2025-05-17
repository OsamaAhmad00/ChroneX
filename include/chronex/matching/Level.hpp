#pragma once

#include <list>

#include <chronex/matching/Order.hpp>

namespace chronex {

template <typename OrderType>
class Level {
public:

    using iterator = typename std::list<OrderType>::iterator;
    using const_iterator = typename std::list<OrderType>::const_iterator;
    using reverse_iterator = typename std::list<OrderType>::reverse_iterator;
    using const_reverse_iterator = typename std::list<OrderType>::const_reverse_iterator;

    template <typename T>
    void add_order(T&& order) {
        _total_volume += order.quantity;
        orders.emplace_back(std::forward<T>(order));
    }

    void remove_order(iterator it) {
        _total_volume -= it->quantity;
        orders.erase(it);
    }

    [[nodiscard]] Quantity total_volume() const noexcept { return _total_volume; }

private:
    // TODO: experiment with other types including different lists and arrays as well
    std::list<OrderType> orders { };
    Quantity _total_volume { 0 };
};

}