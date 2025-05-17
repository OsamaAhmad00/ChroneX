#pragma once

namespace chronex {

struct StopOrder {
    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

template <concepts::OrderBook OrderBook>
bool StopOrder::execute(OrderBook& orderbook) const noexcept {
    return true;
}

}
