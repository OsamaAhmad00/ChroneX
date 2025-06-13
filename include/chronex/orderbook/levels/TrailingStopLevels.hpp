#pragma once

#include <chronex/orderbook/levels/StopLevels.hpp>

namespace chronex {

template <
    concepts::Order OrderType
>
struct TrailingStopLevels : public StopLevels<OrderType> {
    using StopLevels<OrderType>::StopLevels;
};

}

