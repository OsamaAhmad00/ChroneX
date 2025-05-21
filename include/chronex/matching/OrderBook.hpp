#pragma once

#include <chronex/concepts/Order.hpp>
#include <chronex/concepts/EventHandler.hpp>

#include <chronex/handlers/NullEventHandler.hpp>

#include <chronex/matching/Symbol.hpp>
#include <chronex/matching/Order.hpp>
#include <chronex/matching/Levels.hpp>

namespace chronex {

template <
    concepts::Order OrderType = Order,
    concepts::EventHandler<OrderType> EventHandler = handlers::NullEventHandler
>
// Make use of the empty base class optimization and inherit from the logger
class OrderBook : protected EventHandler {
public:

    [[nodiscard]] size_t size() const noexcept { return 0; }

private:

    using AscendingLevels = Levels<OrderType, std::less<>>;
    using DescendingLevels = Levels<OrderType, std::greater<>>;

    AscendingLevels asks;
    DescendingLevels bids;

    AscendingLevels stop_asks;
    DescendingLevels stop_bids;

    AscendingLevels trailing_asks;
    DescendingLevels trailing_bids;

    Symbol Symbol { };
};

}
