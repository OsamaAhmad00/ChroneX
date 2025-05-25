#pragma once

#include <map>
#include <map>

#include <chronex/concepts/Common.hpp>
#include <chronex/concepts/Order.hpp>

#include <chronex/orderbook/Order.hpp>
#include <chronex/orderbook/levels/Level.hpp>

namespace chronex {

template <concepts::Order OrderType, concepts::UniTypeComparator<Price> Comp>
class Levels {

    using LevelType = Level<OrderType>;
    using ContainerType = std::map<Price, LevelType, Comp>;

public:

    using OrderIterator = typename LevelType::iterator;

    using iterator = typename ContainerType::iterator;
    using const_iterator = typename ContainerType::const_iterator;
    using reverse_iterator = typename ContainerType::reverse_iterator;
    using const_reverse_iterator = typename ContainerType::const_reverse_iterator;

    constexpr auto& best() const noexcept { return map.begin()->second; }

    template <typename T, typename Iter>
    constexpr auto add_order(T&& order, Iter level_it) {
        assert(level_it != this->end() && "Trying to remove order from non-existing level");

        ++_orders_count;

        return *level_it.add_order(std::forward<T>(order));
    }

    template <typename T>
    constexpr auto add_order(T&& order) {
        return add_order(std::forward<T>(order), this->find(order.price()));
    }

    template <typename Iter>
    constexpr auto remove_order(OrderIterator order_it, Iter level_it) {
        assert(orders_count > 0 && "Trying to remove order from empty level");
        assert(level_it != this->end() && "Trying to remove order from non-existing level");

        --_orders_count;

        // Is removing levels with total_quantity == 0 an optimization or pessimization?
        // TODO benchmark removing levels with total_quantity == 0
        return *level_it.remove_order(order_it);
    }

    constexpr auto remove_order(OrderIterator order_it) {
        return remove_order(order_it, this->find(order_it->price()));
    }

    template <typename Iter>
    [[nodiscard]] constexpr Iter next_level(Iter it) const noexcept { return std::next(it); }

    template <typename Iter>
    [[nodiscard]] constexpr Iter prev_level(Iter it) const noexcept { return std::prev(it); }

    template <typename Self>
    [[nodiscard]] constexpr auto find(this Self&& self, Price price) noexcept { return self.map().find(price); }

    template <typename Self>
    [[nodiscard]] constexpr auto& begin(this Self&& self) noexcept { return self.map().begin();  }

    template <typename Self>
    [[nodiscard]] constexpr auto& end(this Self&& self) noexcept { return self.map().end();  }

    template <typename Self>
    [[nodiscard]] constexpr auto& rbegin(this Self&& self) noexcept { return self.map().rbegin();  }

    template <typename Self>
    [[nodiscard]] constexpr auto& rend(this Self&& self) noexcept { return self.map().rend();  }

private:

    [[nodiscard]] constexpr auto& map() noexcept { return _map; }

    // TODO use AVL tree instead
    // TODO try using std::vector with linear searching and benchmark (https://www.youtube.com/watch?v=sX2nF1fW7kI)
    ContainerType _map;

    size_t _orders_count { 0 };
};

template <concepts::Order OrderType> using AscendingLevels  = Levels<OrderType, std::less<>>;
template <concepts::Order OrderType> using DescendingLevels = Levels<OrderType, std::greater<>>;

}