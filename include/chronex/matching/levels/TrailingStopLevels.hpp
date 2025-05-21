#pragma once

#include <chronex/matching/levels/StopLevels.hpp>

namespace chronex {

template <concepts::Order OrderType>
struct TrailingStopLevels : public StopLevels<OrderType> { };

};
