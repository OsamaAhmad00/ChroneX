#include <chronex/matching/MatchingEngine.hpp>
#include <chronex/handlers/StreamEventHandler.hpp>

int main() {
    using namespace chronex;

    MatchingEngine<Order, handlers::StdOutEventHandler> matching_engine;

    constexpr uint32_t symbol_id { 1 };
    constexpr Symbol symbol { symbol_id, "GOOG" };
    auto replacement = Order::limit(6, symbol_id, OrderSide::SELL, 42, 100);

    matching_engine.add_new_orderbook(symbol);
    matching_engine.add_order(Order::limit(3, symbol_id, OrderSide::BUY, 100, 100));
    matching_engine.add_order(Order::limit(4, symbol_id, OrderSide::SELL, 100, 20));
    matching_engine.add_order(Order::limit(5, symbol_id, OrderSide::SELL, 100, 100));
    matching_engine.add_order(Order::limit(2, symbol_id, OrderSide::BUY, 42, 24));
    matching_engine.execute_order(OrderId{ 5 }, Quantity{ 10 }, Price{ 100 });
    matching_engine.reduce_order(OrderId{ 2 }, Quantity{ 10 });
    matching_engine.replace_order(OrderId{ 2 }, std::move(replacement));
    matching_engine.remove_order(OrderId{ 6 });
    matching_engine.remove_orderbook(symbol);
}
