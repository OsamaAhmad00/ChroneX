#pragma once

#include <map>
#include <map>

#include <chronex/concepts/Common.hpp>
#include <chronex/concepts/Order.hpp>

#include <chronex/matching/Order.hpp>
#include <chronex/matching/levels/Level.hpp>

namespace chronex {

template <concepts::Order OrderType, concepts::UniTypeComparator<Price> Comp>
class Levels {

    using LevelType = Level<OrderType>;
    using ContainerType = std::map<Price, LevelType, Comp>;

public:

    using OrderIterator = typename LevelType::iterator;

    using const_iterator = typename ContainerType::const_iterator;
    using const_reverse_iterator = typename ContainerType::const_reverse_iterator;

    constexpr auto& best() const noexcept { return map.begin()->second; }

    template <typename T>
    auto add_order(T&& order) {
        orders_count++;
        return map[order.price].add_order(std::forward<T>(order));
    }

    template <typename T>
    void remove_order(T&& order) {
        assert(orders_count > 0 && "Trying to remove order from empty level");
        map[order.price].remove_order(std::forward<T>(order));
        // Is removing levels with total_quantity == 0 an optimization or pessimization?
        // TODO benchmark removing levels with total_quantity == 0
        orders_count--;
    }

    template <typename Iter>
    [[nodiscard]] constexpr Iter next_level(Iter it) const noexcept {
        return std::next(it);
    }

    template <typename Iter>
    [[nodiscard]] constexpr Iter prev_level(Iter it) const noexcept {
        return std::prev(it);
    }

    // Expose only const iterators so that it can't be modified by iterators
    [[nodiscard]] constexpr const_iterator         begin()  const noexcept { return map.begin();  }
    [[nodiscard]] constexpr const_iterator         end()    const noexcept { return map.end();    }
    [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return map.rbegin(); }
    [[nodiscard]] constexpr const_reverse_iterator rend()   const noexcept { return map.rend();   }

private:
    // TODO use AVL tree instead
    // TODO try using std::vector with linear searching and benchmark (https://www.youtube.com/watch?v=sX2nF1fW7kI)
    ContainerType map;

    size_t orders_count { 0 };
};

template <concepts::Order OrderType> using AscendingLevels  = Levels<OrderType, std::less<>>;
template <concepts::Order OrderType> using DescendingLevels = Levels<OrderType, std::greater<>>;

}