#pragma once

namespace chronex {

struct TrailingStopOrder {
    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

template <concepts::OrderBook OrderBook>
bool TrailingStopOrder::execute(OrderBook& orderbook) const noexcept {
    return true;
}

}
