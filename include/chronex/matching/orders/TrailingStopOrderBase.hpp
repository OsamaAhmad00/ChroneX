#pragma once

#include <chronex/matching/orders/StopOrderBase.hpp>

namespace chronex {

template <typename TriggerOrderType>
struct TrailingStopOrderBase : public StopOrderBase<TriggerOrderType> {
    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

}
