#include <chronex/matching/MatchingEngine.hpp>
#include <chronex/handlers/StreamEventHandler.hpp>

auto create_order(uint64_t id) {
    return chronex::Order::buy_limit(
        id,
        1,
        100,
        100
    );
}

int main() {
    chronex::MatchingEngine<
        chronex::Order,
        chronex::handlers::StdOutEventHandler
    > matching_engine;
    auto order_id = chronex::OrderId { 3 };
    auto price = chronex::Price { 100 };
    auto symbol = chronex::Symbol { { 1 }, "BTC" };
    matching_engine.add_new_orderbook(symbol);
    matching_engine.add_order(create_order(3));
    matching_engine.add_order(create_order(4));
    matching_engine.add_order(create_order(5));
    matching_engine.add_order(create_order(2));
    matching_engine.execute_order(order_id, chronex::Quantity{ 10 }, price);
    matching_engine.reduce_order(order_id, chronex::Quantity{ 10 });
    matching_engine.replace_order(order_id, create_order(6));
    matching_engine.remove_order(chronex::OrderId{ 4 });
    matching_engine.remove_orderbook(symbol);
}
