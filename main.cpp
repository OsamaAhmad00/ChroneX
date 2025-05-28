#include <chronex/matching/MatchingEngine.hpp>

int main() {
    chronex::MatchingEngine<> matching_engine;
    auto order = chronex::Order { };
    auto order_id = chronex::OrderId { 3 };
    auto quantity = chronex::Quantity { 100 };
    auto price = chronex::Price { 100 };
    matching_engine.execute_order(order_id, quantity, price);
    matching_engine.reduce_order(order_id, quantity);
    matching_engine.remove_order(order_id);
    matching_engine.replace_order(order_id, order);
    matching_engine.add_order(std::move(order));
}
