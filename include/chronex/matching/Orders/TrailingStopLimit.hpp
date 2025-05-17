#pragma once

namespace chronex {

struct TrailingStopLimitOrder {
    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

template <concepts::OrderBook OrderBook>
bool TrailingStopLimitOrder::execute(OrderBook& orderbook) const noexcept {
    return true;
}

}
