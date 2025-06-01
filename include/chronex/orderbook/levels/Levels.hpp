#pragma once

#include <map>

#include <chronex/concepts/Common.hpp>
#include <chronex/concepts/Order.hpp>

#include <chronex/orderbook/Order.hpp>
#include <chronex/orderbook/levels/Level.hpp>

namespace chronex {

template <
    concepts::Order OrderType,
    concepts::UniTypeComparator<Price> Comp,
    concepts::EventHandler<OrderType> EventHandler = handlers::NullEventHandler
>
class Levels {

    using LevelType = Level<OrderType, EventHandler>;
    using ContainerType = std::map<Price, LevelType, Comp>;

public:

    using OrderIterator = typename LevelType::iterator;

    using iterator = typename ContainerType::iterator;
    using const_iterator = typename ContainerType::const_iterator;
    using reverse_iterator = typename ContainerType::reverse_iterator;
    using const_reverse_iterator = typename ContainerType::const_reverse_iterator;

    // TODO cache the best iterator and when it gets deleted, use std::next.
    //  This is to maintain an O(1) lookup for the best value instead of O(log(N))
    template <typename Self>
    constexpr auto best(this Self&& self) noexcept { return self.map().begin(); }

    constexpr auto add_level(const Price price) {
        return map().emplace(price, &event_handler());
    }

    template <typename T, typename Iter>
    constexpr auto add_order(T&& order, Iter level_it) {
        assert(level_it != this->end() && "Trying to add an order to a non-existing level");

        ++_orders_count;

        return level_it->second.add_order(std::forward<T>(order));
    }

    template <typename T>
    constexpr auto add_order(T&& order) {
        return add_order(std::forward<T>(order), this->find(order.price()));
    }

    template <typename T, typename Iter>
    constexpr auto reduce_order(T&& order, Iter level_it) {
        assert(level_it != this->end() && "Trying to reduce an order from a non-existing level");

        ++_orders_count;

        return level_it->second.reduce_order(std::forward<T>(order));
    }

    template <typename T>
    constexpr auto reduce_order(T&& order) {
        return reduce_order(std::forward<T>(order), this->find(order.price()));
    }
    template <typename Iter>
    constexpr auto remove_order(OrderIterator order_it, Iter level_it) {
        // assert(orders_count > 0 && "Trying to remove order from empty level");
        assert(level_it != this->end() && "Trying to remove order from a non-existing level");

        --_orders_count;

        // Is removing levels with total_quantity == 0 an optimization or pessimization?
        // TODO benchmark removing levels with total_quantity == 0
        return level_it.remove_order(order_it);
    }

    constexpr auto remove_order(OrderIterator order_it) {
        return remove_order(order_it, this->find(order_it->price()));
    }

    [[nodiscard]] constexpr size_t orders_count() const noexcept { return _orders_count; }

    [[nodiscard]] constexpr bool is_empty() const noexcept { return orders_count() == 0; }

    template <typename Iter>
    [[nodiscard]] constexpr Iter next_level(Iter it) const noexcept { return std::next(it); }

    template <typename Iter>
    [[nodiscard]] constexpr Iter prev_level(Iter it) const noexcept { return std::prev(it); }

    template <typename Self>
    [[nodiscard]] constexpr auto find(this Self&& self, Price price) noexcept { return self.map().find(price); }

    template <typename Self>
    [[nodiscard]] constexpr auto begin(this Self&& self) noexcept { return self.map().begin();  }

    template <typename Self>
    [[nodiscard]] constexpr auto end(this Self&& self) noexcept { return self.map().end();  }

    template <typename Self>
    [[nodiscard]] constexpr auto rbegin(this Self&& self) noexcept { return self.map().rbegin();  }

    template <typename Self>
    [[nodiscard]] constexpr auto rend(this Self&& self) noexcept { return self.map().rend();  }

    constexpr void clear() noexcept { map().clear(); }

    template <typename Self>
    [[nodiscard]] constexpr EventHandler& event_handler(this Self&& self) noexcept { return *self._event_handler; }

    explicit Levels(EventHandler* event_handler) : _event_handler(event_handler) { }

    Levels(const Levels&) = delete;

    Levels& operator=(const Levels&) = delete;

    Levels(Levels&&) = default;

    Levels& operator=(Levels&&) = default;

    ~Levels() = default;

private:

    template <typename Self>
    [[nodiscard]] constexpr auto& map(this Self&& self) noexcept { return self._map; }

    // TODO use AVL tree instead
    // TODO try using std::vector with linear searching and benchmark (https://www.youtube.com/watch?v=sX2nF1fW7kI)
    ContainerType _map;

    size_t _orders_count { 0 };

    EventHandler* _event_handler = nullptr;
};

template <concepts::Order OrderType> using AscendingLevels  = Levels<OrderType, std::less<>>;
template <concepts::Order OrderType> using DescendingLevels = Levels<OrderType, std::greater<>>;

}