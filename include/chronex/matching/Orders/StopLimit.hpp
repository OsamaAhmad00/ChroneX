#pragma once

namespace chronex {

struct StopLimitOrder {
    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

template <concepts::OrderBook OrderBook>
bool StopLimitOrder::execute(OrderBook& orderbook) const noexcept {
    return true;
}

}
