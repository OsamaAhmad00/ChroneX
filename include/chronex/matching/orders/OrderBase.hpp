#pragma once

#include <chronex/matching/Common.hpp>
#include <chronex/matching/Symbol.hpp>

#include <chronex/concepts/OrderBook.hpp>

namespace chronex {

struct OrderBase {
    OrderId id;

    SymbolId symbol_id;

    OrderType type;
    OrderSide side;
    TimeInForce time_in_force;
    // 1-byte padding

    Quantity quantity;
    Quantity filled;
    [[nodiscard]] constexpr Quantity remaining() const noexcept { return quantity - filled; }

    template <concepts::OrderBook OrderBook>
    bool execute(OrderBook& orderbook) const noexcept;
};

}
