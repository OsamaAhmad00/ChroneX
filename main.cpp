#include <chronex/matching/MatchingEngine.hpp>

auto create_order(uint64_t id) {
    return chronex::Order {
        chronex::OrderId { id },
        chronex::SymbolId { 1 },
        chronex::OrderType::LIMIT,
        chronex::OrderSide::BUY,
        chronex::TimeInForce::AON,
        chronex::Quantity { 100 },
        chronex::Quantity { 100 },
        chronex::Quantity { 100 },
        chronex::Price { 100 },
        chronex::Price { 100 },
        chronex::Price { 100 },
        chronex::Price { 100 },
        chronex::TrailingOffset::from_price(chronex::Price { 100 }),
        chronex::TrailingOffset::from_price(chronex::Price { 100 })
    };
}

int main() {
    chronex::MatchingEngine<> matching_engine;
    auto order_id = chronex::OrderId { 3 };
    auto quantity = chronex::Quantity { 100 };
    auto price = chronex::Price { 100 };
    matching_engine.add_new_orderbook(chronex::Symbol { { 1 }, "BTC" });
    matching_engine.add_order(create_order(3));
    matching_engine.add_order(create_order(4));
    matching_engine.add_order(create_order(5));
    matching_engine.add_order(create_order(2));
    matching_engine.execute_order(order_id, quantity, price);
    matching_engine.reduce_order(order_id, quantity);
    matching_engine.remove_order(order_id);
    matching_engine.replace_order(order_id, create_order(5));
}
