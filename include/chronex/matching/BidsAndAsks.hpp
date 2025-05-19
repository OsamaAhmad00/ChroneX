#pragma once

#include <map>

#include <chronex/matching/Levels.hpp>

namespace chronex {

template <typename OrderType>
class LimitBidsAndAsks {
public:

    using AscendingLevels = Levels<OrderType, std::less<>>;
    using DescendingLevels = Levels<OrderType, std::greater<>>;

protected:

    AscendingLevels asks;
    DescendingLevels bids;
};

template <typename OrderType>
class StopBidsAndAsks : public LimitBidsAndAsks<OrderType> {

};

template <typename OrderType>
class TrailingStopBidsAndAsks : public StopBidsAndAsks<OrderType> {

};

}