#pragma once

namespace chronex {

struct MarketOrder {
    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

template <concepts::OrderBook OrderBook>
bool MarketOrder::execute(OrderBook& orderbook) const noexcept{
    return true;
}

}
