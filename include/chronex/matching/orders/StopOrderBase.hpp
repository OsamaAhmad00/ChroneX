#pragma once

#include <chronex/concepts/OrderBook.hpp>

namespace chronex {

template <typename TriggerOrderType>
struct StopOrderBase {
    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

}
