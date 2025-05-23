#pragma once

#include <map>

#include <chronex/concepts/Common.hpp>
#include <chronex/concepts/Order.hpp>

#include <chronex/matching/Order.hpp>
#include <chronex/matching/levels/Level.hpp>

namespace chronex {

template <concepts::Order OrderType, concepts::UniTypeComparator<Price> Comp>
class Levels {
public:

    constexpr auto& best() const noexcept { return map.begin()->second; }

    template <typename T>
    void add_order(T&& order) {
        map[order.price].add_order(std::forward<T>(order));
    }

    template <typename T>
    void remove_order(T&& order) {
        map[order.price].remove_order(std::forward<T>(order));
        // Is removing levels with total_quantity == 0 an optimization or pessimization?
        // TODO benchmark removing levels with total_quantity == 0
    }

private:
    // TODO use AVL tree instead
    // TODO try using std::vector with linear searching and benchmark (https://www.youtube.com/watch?v=sX2nF1fW7kI)
    std::map<Price, Level<OrderType>, Comp> map;
};

template <concepts::Order OrderType> using AscendingLevels  = Levels<OrderType, std::less<>>;
template <concepts::Order OrderType> using DescendingLevels = Levels<OrderType, std::greater<>>;

}