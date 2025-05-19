#pragma once

#include <cstdint>

#include <chronex/concepts/Order.hpp>
#include <chronex/concepts/Logger.hpp>

#include <chronex/Loggers/NullLogger.hpp>

#include <chronex/matching/Symbol.hpp>
#include <chronex/matching/Order.hpp>
#include <chronex/matching/BidsAndAsks.hpp>

namespace chronex {

// Make use of the empty base class optimization and inherit from the logger
template <concepts::Order OrderType = Order, concepts::Logger Logger = loggers::NullLogger>
class OrderBook : private Logger {
public:

    [[nodiscard]] size_t size() const noexcept { return 0; }

private:

    LimitBidsAndAsks<OrderType> limit;
    StopBidsAndAsks<OrderType> stop;
    TrailingStopBidsAndAsks<OrderType> trailing_stop;
    Symbol Symbol { };
};

}
