#pragma once

#include <chronex/concepts/OrderBook.hpp>
#include <chronex/matching/orders/TrailingStopOrderBase.hpp>
#include <chronex/matching/orders/MarketOrder.hpp>

namespace chronex {

using TrailingStopOrder = TrailingStopOrderBase<MarketOrder>;

template <>
template <concepts::OrderBook OrderBook>
bool TrailingStopOrder::execute(OrderBook& orderbook) const noexcept {
    return true;
}

}
