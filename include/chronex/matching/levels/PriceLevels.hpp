#pragma once

#include <chronex/matching/Levels.hpp>

namespace chronex {

template <concepts::Order OrderType>
struct PriceLevels {
public:

    constexpr auto& best_bid_level() const noexcept { return bids().best(); }
    constexpr auto& best_ask_level() const noexcept { return asks().best(); }
    constexpr auto bids() const noexcept { return _bids; }
    constexpr auto asks() const noexcept { return _asks; }

private:

    DescendingLevels<OrderType> _bids;
    AscendingLevels <OrderType> _asks;
};

};