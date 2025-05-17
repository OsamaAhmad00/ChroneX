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
        orders.emplace_back(std::forward<T>(order));
        total_quantity += order.quantity;
        ++order_count;
    }

    void remove_order(iterator it) {
        orders.erase(it);
        total_quantity -= it->quantity;
        --order_count;
    }

private:
    // TODO: experiment with other types including different lists and arrays as well
    std::list<OrderType> orders { };
    Quantity total_quantity { };
    size_t order_count { };
};

}