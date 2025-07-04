#pragma once

#include <chronex/orderbook/Order.hpp>

#include <chronex/data-structures/LinkedList.hpp>

namespace chronex {

template <
    concepts::Order Order = Order,
    typename ListType = ds::LinkedList<Order>
>
class Level {
public:

    // TODO remove this
    using LevelQueueDataType = ListType;

    using value_type = Order;
    using iterator = typename ListType::iterator;
    using const_iterator = typename ListType::const_iterator;
    using reverse_iterator = typename ListType::reverse_iterator;
    using const_reverse_iterator = typename ListType::const_reverse_iterator;

    template <typename Iter>
    constexpr std::remove_reference_t<Iter> prev(Iter&& it) {
        return std::prev(it);
    }

    template <typename Iter>
    constexpr std::remove_reference_t<Iter> next(Iter&& it) {
        return std::next(it);
    }

    [[nodiscard]] constexpr size_t size() const noexcept { return orders.size(); }
    [[nodiscard]] constexpr bool is_empty() const noexcept { return size() == 0; }

    [[nodiscard]] constexpr Quantity visible_volume() const noexcept { return _visible_volume; }
    [[nodiscard]] constexpr Quantity hidden_volume() const noexcept { return _hidden_volume; }
    [[nodiscard]] constexpr Quantity total_volume() const noexcept { return visible_volume() + hidden_volume(); }

    [[nodiscard]] constexpr iterator begin() noexcept { return orders.begin(); }
    [[nodiscard]] constexpr iterator end() noexcept { return orders.end(); }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return orders.begin(); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return orders.end(); }
    [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return orders.rbegin(); }
    [[nodiscard]] constexpr reverse_iterator rend() noexcept { return orders.rend(); }
    [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return orders.rbegin(); }
    [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return orders.rend(); }

    constexpr auto free(iterator it) noexcept {
        orders.free(it);
    }

    Level() = default;

    Level(const Level&) = delete;

    Level& operator=(const Level&) = delete;

    Level(Level&&) = default;

    Level& operator=(Level&&) = default;

    ~Level() = default;

private:

    constexpr auto add_order(value_type&& order) {
        _visible_volume += order.visible_quantity();
        _hidden_volume += order.hidden_quantity();
        return orders.emplace_back(std::move(order));
    }

    [[nodiscard]] constexpr auto modify_order(iterator it, Quantity quantity) {
        if (quantity == Quantity{ 0 }) {
            return remove_order(it);
        }

        auto old_visible = it->visible_quantity();
        auto old_hidden = it->hidden_quantity();

        it->set_leaves_quantity(quantity);

        _visible_volume -= (old_visible - it->visible_quantity());
        _hidden_volume -= (old_hidden - it->hidden_quantity());

        return it;
    }

    [[nodiscard]] constexpr auto remove_order(iterator it) noexcept {
        _visible_volume -= it->visible_quantity();
        _hidden_volume -= it->hidden_quantity();
        auto n = next(it);
        orders.erase(it);
        return n;
    }

    constexpr auto unlink_order(iterator it) noexcept {
        _visible_volume -= it->visible_quantity();
        _hidden_volume -= it->hidden_quantity();
        orders.unlink_node(it);
    }

    constexpr auto link_order_back(iterator it) noexcept {
        _visible_volume += it->visible_quantity();
        _hidden_volume += it->hidden_quantity();
        orders.link_node_back(it);
    }

    template <concepts::Order, concepts::UniTypeComparator<Price>>
    friend class Levels;

    // TODO: experiment with other types including different lists and arrays as well
    ListType orders { };

    Quantity _visible_volume = Quantity { 0 };
    Quantity _hidden_volume = Quantity { 0 };
};

}