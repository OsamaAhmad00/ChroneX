#include <chronex/matching/MatchingEngine.hpp>
#include <chronex/handlers/StreamEventHandler.hpp>

auto create_order(uint64_t id, chronex::OrderSide side, uint64_t price, uint64_t quantity) {
    return chronex::Order::limit(
        id,
        1,
        side,
        price,
        quantity
    );
}

int main() {
    using namespace chronex;

    MatchingEngine<
        Order,
        handlers::StdOutEventHandler
    > matching_engine;
    constexpr auto symbol = Symbol { { 1 }, "BTC" };
    matching_engine.add_new_orderbook(symbol);
    matching_engine.add_order(create_order(3, OrderSide::BUY, 100, 100));
    matching_engine.add_order(create_order(4, OrderSide::SELL, 100, 20));
    matching_engine.add_order(create_order(5, OrderSide::SELL, 100, 100));
    matching_engine.add_order(create_order(2, OrderSide::BUY, 42, 24));
    matching_engine.execute_order(OrderId{ 5 }, Quantity{ 10 }, Price{ 100 });
    matching_engine.reduce_order(OrderId{ 2 }, Quantity{ 10 });
    matching_engine.replace_order(OrderId{ 2 }, create_order(6, OrderSide::SELL, 42, 100));
    matching_engine.remove_order(OrderId{ 6 });
    matching_engine.remove_orderbook(symbol);
}
