#pragma once

#include <chronex/concepts/OrderBook.hpp>
#include <chronex/matching/Symbol.hpp>

#include <chronex/matching/orders/OrderBase.hpp>
#include <chronex/matching/orders/MarketOrder.hpp>
#include <chronex/matching/orders/LimitOrder.hpp>
#include <chronex/matching/orders/StopOrder.hpp>
#include <chronex/matching/orders/StopLimitOrder.hpp>
#include <chronex/matching/orders/TrailingStopOrder.hpp>
#include <chronex/matching/orders/TrailingStopLimit.hpp>

namespace chronex {

struct Order : public OrderBase {

    OrderType type;

    union {
        MarketOrder as_market;
        LimitOrder as_limit;
        StopOrder as_stop;
        StopLimitOrder as_stop_limit;
        TrailingStopOrder as_trailing_stop;
        TrailingStopLimitOrder as_trailing_stop_limit;
    };

    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

template <concepts::OrderBook OrderBook>
bool Order::execute(OrderBook& orderbook) const noexcept {
    switch (type) {
        case OrderType::MARKET:
            return as_market.execute(orderbook);
        case OrderType::LIMIT:
            return as_limit.execute(orderbook);
        case OrderType::STOP:
            return as_stop.execute(orderbook);
        case OrderType::STOP_LIMIT:
            return as_stop_limit.execute(orderbook);
        case OrderType::TRAILING_STOP:
            return as_trailing_stop.execute(orderbook);
        case OrderType::TRAILING_STOP_LIMIT:
            return as_trailing_stop_limit.execute(orderbook);
        default:
            return false;
    }
}

}
