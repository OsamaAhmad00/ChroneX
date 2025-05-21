#pragma once

namespace chronex::concepts {

template <typename HandlerType, typename OrderType>
concept EventHandler = requires(HandlerType handler, OrderType order) {
    handler.on_order_creation(order);
};

}
