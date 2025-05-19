#pragma once

#include <chronex/concepts/OrderBook.hpp>

namespace chronex {

struct LimitOrder : public OrderBase {
    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

template <concepts::OrderBook OrderBook>
bool LimitOrder::execute(OrderBook& orderbook) const noexcept{
    return true;
}

}
