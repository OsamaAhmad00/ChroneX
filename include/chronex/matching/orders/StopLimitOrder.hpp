#pragma once

#include <chronex/concepts/OrderBook.hpp>
#include <chronex/matching/orders/StopOrderBase.hpp>
#include <chronex/matching/orders/LimitOrder.hpp>

namespace chronex {

using StopLimitOrder = StopOrderBase<LimitOrder>;

template <>
template <concepts::OrderBook OrderBook>
bool StopLimitOrder::execute(OrderBook& orderbook) const noexcept {
    return true;
}

}
