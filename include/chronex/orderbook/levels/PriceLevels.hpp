#pragma once

#include <chronex/orderbook/levels/Levels.hpp>

namespace chronex {

template <
    concepts::Order OrderType
>
struct PriceLevels {
public:

    // TODO remove this
    using LevelQueueDataType = typename DescendingLevels<OrderType>::LevelQueueDataType;

    PriceLevels() = default;

    // Or DescendingLevels. It doesn't matter.
    using OrderIterator = typename AscendingLevels<OrderType>::OrderIterator;

    template <typename Self>
    constexpr auto& bids(this Self&& self) noexcept { return self._bids; }

    template <typename Self>
    constexpr auto& asks(this Self&& self) noexcept { return self._asks; }

    template <OrderSide side, typename Self>
    constexpr auto& levels(this Self&& self) noexcept {
        if constexpr (side == OrderSide::BUY) {
            return self.bids();
        } else if constexpr (side == OrderSide::SELL) {
            return self.asks();
        } else {
            assert(false && "Unknown side");
        }
    }

    template <typename Self, OrderSide side>
    constexpr auto best(this Self&& self) noexcept { return self.template levels<side>.best(); }

private:

    DescendingLevels<OrderType> _bids;
    AscendingLevels <OrderType> _asks;
};

};