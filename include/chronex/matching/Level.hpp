#pragma once

#include <chronex/matching/Order.hpp>

#include <chronex/data-structures/LinkedList.hpp>

namespace chronex {

template <typename OrderType, typename ListType = ds::LinkedList<OrderType>>
class Level {
public:

    using iterator = typename ListType::iterator;
    using const_iterator = typename ListType::const_iterator;
    using reverse_iterator = typename ListType::reverse_iterator;
    using const_reverse_iterator = typename ListType::const_reverse_iterator;

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
    ListType orders { };
    Quantity _total_volume { 0 };
};

}