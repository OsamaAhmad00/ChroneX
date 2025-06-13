#pragma once

#include <chronex/orderbook/OrderUtils.hpp>

namespace chronex::concepts {

// TODO add the orderbook to the template params. Remember that the OrderBook takes the EventHandler as a template param.
template <typename HandlerType, typename Order>
concept EventHandler = requires(HandlerType handler, int orderbook, Order order) {
    handler.template on_add_order<OrderType::MARKET, OrderSide::BUY>(orderbook, order);
};

}
