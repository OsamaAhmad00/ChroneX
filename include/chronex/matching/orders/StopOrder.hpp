#pragma once

#include <chronex/concepts/OrderBook.hpp>
#include <chronex/matching/orders/StopOrderBase.hpp>
#include <chronex/matching/orders/MarketOrder.hpp>

namespace chronex {

using StopOrder = StopOrderBase<MarketOrder>;

template <>
template <concepts::OrderBook OrderBook>
bool StopOrder::execute(OrderBook& orderbook) const noexcept {
    return true;
}

}
