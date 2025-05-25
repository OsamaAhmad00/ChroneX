#pragma once

#include <chronex/orderbook/Order.hpp>

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

    template <concepts::Order T>
    constexpr auto add_order(T&& order) {
        _leaves_quantity -= order.leaves_quantity();
        _filled_quantity -= order.filled_quantity();
        _max_visible_quantity -= order.visible_quantity();
        return orders.emplace_back(std::forward<T>(order));
    }

    constexpr void remove_order(iterator it) noexcept {
        _leaves_quantity -= it->leaves_quantity();
        _filled_quantity -= it->filled_quantity();
        _max_visible_quantity -= it->visible_quantity();
        orders.erase(it);
    }

    [[nodiscard]] constexpr size_t size() const noexcept { return orders.size(); }
    [[nodiscard]] constexpr bool is_empty() const noexcept { return size() == 0; }

    [[nodiscard]] constexpr Quantity leaves_quantity() const noexcept { return _leaves_quantity; }
    [[nodiscard]] constexpr Quantity filled_quantity() const noexcept { return _filled_quantity; }
    [[nodiscard]] constexpr Quantity max_visible_quantity() const noexcept { return _max_visible_quantity; }
    [[nodiscard]] constexpr Quantity visible_quantity() const noexcept { return std::min(leaves_quantity(), max_visible_quantity()); }
    [[nodiscard]] constexpr Quantity hidden_quantity() const noexcept { return std::max(Quantity { 0 }, leaves_quantity() - max_visible_quantity()); }

    [[nodiscard]] constexpr iterator begin() noexcept { return orders.begin(); }
    [[nodiscard]] constexpr iterator end() noexcept { return orders.end(); }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return orders.begin(); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return orders.end(); }
    [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return orders.rbegin(); }
    [[nodiscard]] constexpr reverse_iterator rend() noexcept { return orders.rend(); }
    [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return orders.rbegin(); }
    [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return orders.rend(); }

private:

    // TODO: experiment with other types including different lists and arrays as well
    ListType orders { };

    Quantity _leaves_quantity = Quantity { 0 };
    Quantity _filled_quantity = Quantity { 0 };
    Quantity _max_visible_quantity = Quantity { 0 };
};

}