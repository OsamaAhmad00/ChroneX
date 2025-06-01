#pragma once

#include <chronex/orderbook/levels/Levels.hpp>

namespace chronex {

template <
    concepts::Order OrderType,
    concepts::EventHandler<OrderType> EventHandler = handlers::NullEventHandler
>
struct PriceLevels {
public:

    // TODO remove this
    using LevelQueueDataType = typename DescendingLevels<OrderType, EventHandler>::LevelQueueDataType;

    explicit PriceLevels(EventHandler* event_handler) : _bids(event_handler), _asks(event_handler) { }

    // Or DescendingLevels. It doesn't matter.
    using OrderIterator = typename AscendingLevels<OrderType, EventHandler>::OrderIterator;

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

    DescendingLevels<OrderType, EventHandler> _bids;
    AscendingLevels <OrderType, EventHandler> _asks;
};

};