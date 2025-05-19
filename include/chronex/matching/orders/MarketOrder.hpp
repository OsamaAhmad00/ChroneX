#pragma once

#include <chronex/concepts/OrderBook.hpp>

namespace chronex {

struct MarketOrder : public OrderBase {
    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

template <concepts::OrderBook OrderBook>
bool MarketOrder::execute(OrderBook& orderbook) const noexcept{
    return true;
}

}
