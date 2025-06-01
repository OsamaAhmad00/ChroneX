#pragma once

#include <chronex/orderbook/Order.hpp>

#include <chronex/data-structures/LinkedList.hpp>

namespace chronex {

template <
    concepts::Order OrderType,
    concepts::EventHandler<OrderType> EventHandler = handlers::NullEventHandler,
    typename ListType = ds::LinkedList<OrderType>
>
class Level {
public:

    using value_type = OrderType;
    using iterator = typename ListType::iterator;
    using const_iterator = typename ListType::const_iterator;
    using reverse_iterator = typename ListType::reverse_iterator;
    using const_reverse_iterator = typename ListType::const_reverse_iterator;

    template <concepts::Order T>
    constexpr auto add_order(T&& order) {
        _leaves_quantity += order.leaves_quantity();
        _max_visible_quantity += order.visible_quantity();
        return orders.emplace_back(std::forward<T>(order));
    }

    constexpr auto reduce_order(iterator it, Quantity quantity) {
        assert(_leaves_quantity <= it->leaves_quantity() && "Trying to reduce more quantity than the level has left");
        if (quantity == it->leaves_quantity()) {
            return remove_order(it);
        }
        _leaves_quantity -= quantity;
        it->reduce_quantity(quantity);
    }

    constexpr auto remove_order(iterator it) noexcept {
        _leaves_quantity -= it->leaves_quantity();
        _max_visible_quantity -= it->visible_quantity();
        orders.erase(it);
    }

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

    [[nodiscard]] constexpr Quantity leaves_quantity() const noexcept { return _leaves_quantity; }
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

    explicit Level(EventHandler* event_handler) : _event_handler(event_handler) { }

    Level(const Level&) = delete;

    Level& operator=(const Level&) = delete;

    Level(Level&&) = default;

    Level& operator=(Level&&) = default;

    ~Level() = default;

private:

    // TODO: experiment with other types including different lists and arrays as well
    ListType orders { };

    Quantity _leaves_quantity = Quantity { 0 };
    Quantity _filled_quantity = Quantity { 0 };
    Quantity _max_visible_quantity = Quantity { 0 };

    EventHandler* _event_handler = nullptr;
};

}