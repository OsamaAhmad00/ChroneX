#pragma once

#include <chronex/orderbook/levels/PriceLevels.hpp>

namespace chronex {

template <concepts::Order OrderType>
struct StopLevels : public PriceLevels<OrderType> { };

};