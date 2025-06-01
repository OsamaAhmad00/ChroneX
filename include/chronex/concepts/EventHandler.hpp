#pragma once

#include <chronex/orderbook/OrderUtils.hpp>

namespace chronex::concepts {

template <typename HandlerType, typename Order>
concept EventHandler = requires(HandlerType handler, Order order) {
    handler.template on_create_order<OrderType::MARKET, OrderSide::BUY>(order);
};

}
