#pragma once

#include <chronex/matching/Order.hpp>

#include <chronex/data-structures/LinkedList.hpp>

namespace chronex {

template <typename OrderType, typename ListType = ds::LinkedList<OrderType>>
class Level {
public:

    using value_type = OrderType;
    using iterator = typename ListType::iterator;
    using const_iterator = typename ListType::const_iterator;
    using reverse_iterator = typename ListType::reverse_iterator;
    using const_reverse_iterator = typename ListType::const_reverse_iterator;

    template <typename T>
    constexpr void add_order(T&& order) {
        _total_volume += order.quantity;
        orders.emplace_back(std::forward<T>(order));
    }

    constexpr void remove_order(iterator it) noexcept {
        _total_volume -= it->quantity;
        orders.erase(it);
    }

    [[nodiscard]] constexpr Quantity total_volume() const noexcept { return _total_volume; }

    constexpr iterator begin() noexcept { return orders.begin(); }
    constexpr iterator end() noexcept { return orders.end(); }
    constexpr const_iterator begin() const noexcept { return orders.begin(); }
    constexpr const_iterator end() const noexcept { return orders.end(); }
    constexpr reverse_iterator rbegin() noexcept { return orders.rbegin(); }
    constexpr reverse_iterator rend() noexcept { return orders.rend(); }
    constexpr const_reverse_iterator rbegin() const noexcept { return orders.rbegin(); }
    constexpr const_reverse_iterator rend() const noexcept { return orders.rend(); }

private:

    // TODO: experiment with other types including different lists and arrays as well
    ListType orders { };
    Quantity _total_volume { 0 };
};

}