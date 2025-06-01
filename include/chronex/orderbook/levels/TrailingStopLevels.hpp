#pragma once

#include <chronex/orderbook/levels/StopLevels.hpp>

namespace chronex {

template <
    concepts::Order OrderType,
    concepts::EventHandler<OrderType> EventHandler = handlers::NullEventHandler
>
struct TrailingStopLevels : public StopLevels<OrderType, EventHandler> {
    using StopLevels<OrderType, EventHandler>::StopLevels;
};

}

