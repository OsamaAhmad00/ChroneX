#pragma once

#include <chronex/concepts/OrderBook.hpp>
#include <chronex/matching/orders/TrailingStopOrderBase.hpp>
#include <chronex/matching/orders/LimitOrder.hpp>

namespace chronex {

using TrailingStopLimitOrder = TrailingStopOrderBase<LimitOrder>;

template <>
template <concepts::OrderBook OrderBook>
bool TrailingStopLimitOrder::execute(OrderBook& orderbook) const noexcept {
    return true;
}

}
