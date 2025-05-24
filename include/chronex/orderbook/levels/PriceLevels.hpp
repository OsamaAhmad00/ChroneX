#pragma once

#include <chronex/orderbook/levels/Levels.hpp>

namespace chronex {

template <concepts::Order OrderType>
struct PriceLevels {
public:

    // Or DescendingLevels. It doesn't matter.
    using OrderIterator = typename AscendingLevels<OrderType>::OrderIterator;

    constexpr auto& best_bid_level() const noexcept { return bids().best(); }
    constexpr auto& best_ask_level() const noexcept { return asks().best(); }
    constexpr auto bids() const noexcept { return _bids; }
    constexpr auto asks() const noexcept { return _asks; }

private:

    DescendingLevels<OrderType> _bids;
    AscendingLevels <OrderType> _asks;
};

};