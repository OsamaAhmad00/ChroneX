#include <cassert>

#include <chronex/orderbook/Order.hpp>

namespace chronex {

bool Order::is_valid() const noexcept {

    assert(id() != OrderId::invalid() && "Order id must be valid!");
    assert(symbol_id() != SymbolId::invalid() && "Order symbol id must be valid!");
    assert(leaves_quantity() != Quantity::invalid() && "Order leaves quantity must be valid!");
    assert(filled_quantity() != Quantity::invalid() && "Order filled quantity must be valid!");
    assert(max_visible_quantity() != Quantity::invalid() && "Order max visible quantity must be valid!");
    assert(price() != Price::invalid() && "Order price must be valid!");
    assert(stop_price() != Price::invalid() && "Order stop price must be valid!");
    assert(initial_stop_price() != Price::invalid() && "Order initial stop price must be valid!");
    assert(slippage() != Price::invalid() && "Order slippage must be valid!");
    assert(trailing_distance() != TrailingOffset::invalid() && "Order trailing distance must be valid!");
    assert(trailing_step() != TrailingOffset::invalid() && "Order trailing step must be valid!");
    assert(initial_quantity() == leaves_quantity() + filled_quantity() && "Order total quantity must be equal to leaves quantity plus filled quantity!");
    assert(leaves_quantity() > Quantity { 0 } && "Order leaves quantity must be greater than zero!");

    if (is_market_order()) {
        assert(is_ioc() || is_fok() && "Market order must have 'Immediate-Or-Cancel' or 'Fill-Or-Kill' parameter!");
        assert(!is_iceberg() && "Market order cannot be 'Iceberg'!");
    }

    if (is_limit_order()) {
        assert(!has_slippage() && "Limit order cannot have slippage parameter!");
    }

    if (is_stop_order() || is_trailing_stop_order()) {
        assert(!is_aon() && "Stop order cannot have 'All-Or-None' parameter!");
        assert(!is_iceberg() && "Stop order cannot be 'Iceberg'!");
    }

    if (is_stop_limit_order() || is_trailing_stop_limit_order()) {
        assert(!has_slippage() && "Stop-limit order cannot have slippage!");
    }

    if (is_trailing_stop_order() || is_trailing_stop_limit_order())
    {
        assert((trailing_distance().value != 0) &&
            "Trailing stop order must have non zero distance to the market!");

        if (trailing_distance().value > 0) {
            assert(trailing_step().value >= 0 && trailing_step() < trailing_distance() &&
                "Trailing step must be less than trailing distance!");
        } else {
            assert(trailing_distance().value <= -1 && trailing_distance().value >= -1000 &&
                "Trailing percentage distance must be in the range [0.01, 100%] (from -1 down to -10000)!");
            assert(trailing_step().value <= 0 && trailing_step() > trailing_distance() &&
                "Trailing step must be less than trailing distance!");
        }
    }

    return _id != OrderId::invalid();
}

}