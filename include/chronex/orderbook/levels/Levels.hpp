#pragma once

#include <map>

#include <chronex/concepts/Common.hpp>
#include <chronex/concepts/Order.hpp>

#include <chronex/orderbook/Order.hpp>
#include <chronex/orderbook/levels/Level.hpp>

namespace chronex {

// TODO remove OrderType template param from here. At this point,
//  we don't need this information. Reporting happens from the
//  orderbook, and Levels and Level don't report anything as of now.

template <
    concepts::Order Order,
    concepts::UniTypeComparator<Price> Comp,
    concepts::EventHandler<Order> EventHandler = handlers::NullEventHandler
>
class Levels {

    using LevelType = Level<Order, EventHandler>;
    using ContainerType = std::map<Price, LevelType, Comp>;

public:

    // TODO remove this
    using LevelQueueDataType = typename LevelType::LevelQueueDataType;

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

    template <typename Iter>
    constexpr auto remove_level(Iter level_it) {
        return map().erase(level_it);
    }

    template <typename Iter>
    constexpr auto add_order(Order&& order, Iter level_it) {
        assert(level_it != this->end() && "Trying to add an order to a non-existing level");

        ++_orders_count;

        return level_it->second.add_order(std::move(order));
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr auto add_order(T&& order) {
        return add_order<type, side>(std::forward<T>(order), this->find(order.price()));
    }

    template <OrderType type, OrderSide side, typename Iter>
    constexpr auto reduce_order(OrderIterator order_it, Iter level_it) {
        assert(level_it != this->end() && "Trying to reduce an order from a non-existing level");
        return level_it->second.template reduce_order<type, side>(order_it);
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr auto reduce_order(T&& order) {
        return reduce_order<type, side>(std::forward<T>(order), this->find(order.price()));
    }

    template <typename Iter>
    constexpr auto remove_order(OrderIterator order_it, Iter level_it) {
        // assert(orders_count > 0 && "Trying to remove order from empty level");
        assert(level_it != this->end() && "Trying to remove order from a non-existing level");

        --_orders_count;

        return level_it->second.remove_order(order_it);
    }

    template <OrderType type, OrderSide side>
    constexpr auto remove_order(OrderIterator order_it) {
        return remove_order<type, side>(order_it, this->find(order_it->price()));
    }

    constexpr auto link_order_back(OrderIterator order_it, iterator level_it) {
        level_it->second.link_order_back(order_it);
        ++_orders_count;
    }

    constexpr auto unlink_order(OrderIterator order_it, iterator level_it) {
        level_it->second.unlink_order(order_it);
        --_orders_count;
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

template <
    concepts::Order Order,
    concepts::EventHandler<Order> EventHandler
> using AscendingLevels  = Levels<Order, std::less<>>;

template <
    concepts::Order Order,
    concepts::EventHandler<Order> EventHandler
> using DescendingLevels = Levels<Order, std::greater<>>;

}