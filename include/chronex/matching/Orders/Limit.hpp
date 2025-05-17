#pragma once

namespace chronex {

struct LimitOrder {
    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

template <concepts::OrderBook OrderBook>
bool LimitOrder::execute(OrderBook& orderbook) const noexcept{
    return true;
}

}
