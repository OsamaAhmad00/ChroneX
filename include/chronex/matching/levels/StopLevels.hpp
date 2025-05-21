#pragma once

#include <chronex/matching/levels/PriceLevels.hpp>

namespace chronex {

template <concepts::Order OrderType>
struct StopLevels : public PriceLevels<OrderType> { };

};