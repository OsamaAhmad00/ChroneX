#pragma once

#include <cstdint>
#include <map>

#include <chronex/concepts/Order.hpp>
#include <chronex/concepts/Logger.hpp>

#include <chronex/Loggers/NullLogger.hpp>

#include <chronex/matching/Symbol.hpp>
#include <chronex/matching/Order.hpp>
#include <chronex/matching/Levels.hpp>

namespace chronex {

// Make use of the empty base class optimization and inherit from the logger
template <concepts::Order OrderType = Order, concepts::Logger Logger = loggers::NullLogger>
class OrderBook : private Logger {
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
