#pragma once

#include <chronex/orderbook/levels/PriceLevels.hpp>

namespace chronex {

template <
    concepts::Order OrderType,
    concepts::EventHandler<OrderType> EventHandler = handlers::NullEventHandler
>
struct StopLevels : public PriceLevels<OrderType, EventHandler> {
    using PriceLevels<OrderType, EventHandler>::PriceLevels;
};
}