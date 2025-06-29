#include <ranges>
#include <gtest/gtest.h>

#include <chronex/matching/MatchingEngine.hpp>

#include <chronex/handlers/StreamEventHandler.hpp>

// Test cases are taken from https://github.com/chronoxor/CppTrader/blob/master/tests/test_matching_engine.cpp

namespace chronex {

namespace googletest = ::testing;

namespace {

using OrderBook = OrderBook<>;
// using OrderBook = OrderBook<Order, handlers::StdOutEventHandler>;

template <OrderType type, typename Func>
constexpr auto accumulate_orderbook(const OrderBook& orderbook, Func func) {
    int bid = 0;
    for (const auto &bids: orderbook.bids<type>() | std::views::values)
        bid += static_cast<int>(func(bids));

    int ask = 0;
    for (const auto &asks: orderbook.asks<type>() | std::views::values)
        ask += static_cast<int>(func(asks));

    return std::make_pair(bid, ask);
}

template <OrderType type>
constexpr auto accumulate_orders_count(const OrderBook& orderbook) {
    return accumulate_orderbook<type>(orderbook, [](const auto& level) { return level.size(); });
}

template <OrderType type>
constexpr auto accumulate_total_volume(const OrderBook& orderbook) {
    return accumulate_orderbook<type>(orderbook, [](const auto& level) { return level.total_volume().value; });
}

template <OrderType type>
constexpr auto accumulate_visible_volume(const OrderBook& orderbook) {
    return accumulate_orderbook<type>(orderbook, [](const auto& level) { return level.visible_volume().value; });
}

constexpr auto orders_count(const OrderBook& orderbook) {
    return accumulate_orders_count<OrderType::LIMIT>(orderbook);
}

constexpr auto orders_volume(const OrderBook& orderbook) {
    return accumulate_total_volume<OrderType::LIMIT>(orderbook);
}

constexpr auto visible_volume(const OrderBook& orderbook) {
    return accumulate_visible_volume<OrderType::LIMIT>(orderbook);
}

constexpr auto stop_orders_count(const OrderBook& orderbook) {
    auto [a, b] = accumulate_orders_count<OrderType::STOP>(orderbook);
    auto [c, d] = accumulate_orders_count<OrderType::TRAILING_STOP>(orderbook);
    return std::make_pair(a + c, b + d);
}

constexpr auto stop_orders_volume(const OrderBook& orderbook) {
    auto [a, b] = accumulate_total_volume<OrderType::STOP>(orderbook);
    auto [c, d] = accumulate_total_volume<OrderType::TRAILING_STOP>(orderbook);
    return std::make_pair(a + c, b + d);
}

}

// Test fixture for common setup
class MatchingEngineTest : public testing::Test {
protected:
    void SetUp() override {
        matching_engine.add_new_orderbook(symbol);
        matching_engine.enable_matching();
    }

    MatchingEngine<> matching_engine;
    // MatchingEngine<Order, handlers::StdOutEventHandler> matching_engine;
    Symbol symbol{ 0, "test" };
};

// Test case: Automatic matching - market order
TEST_F(MatchingEngineTest, AutomaticMatchingMarketOrder) {
    // Add buy limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 10, 10));
    matching_engine.add_order(Order::buy_limit(2, 0, 10, 20));
    matching_engine.add_order(Order::buy_limit(3, 0, 10, 30));
    matching_engine.add_order(Order::buy_limit(4, 0, 20, 10));
    matching_engine.add_order(Order::buy_limit(5, 0, 20, 20));
    matching_engine.add_order(Order::buy_limit(6, 0, 20, 30));
    matching_engine.add_order(Order::buy_limit(7, 0, 30, 10));
    matching_engine.add_order(Order::buy_limit(8, 0, 30, 20));
    matching_engine.add_order(Order::buy_limit(9, 0, 30, 30));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(9, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(180, 0));

    // Add sell limit orders
    matching_engine.add_order(Order::sell_limit(10, 0, 40, 30));
    matching_engine.add_order(Order::sell_limit(11, 0, 40, 20));
    matching_engine.add_order(Order::sell_limit(12, 0, 40, 10));
    matching_engine.add_order(Order::sell_limit(13, 0, 50, 30));
    matching_engine.add_order(Order::sell_limit(14, 0, 50, 20));
    matching_engine.add_order(Order::sell_limit(15, 0, 50, 10));
    matching_engine.add_order(Order::sell_limit(16, 0, 60, 30));
    matching_engine.add_order(Order::sell_limit(17, 0, 60, 20));
    matching_engine.add_order(Order::sell_limit(18, 0, 60, 10));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(9, 9));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(180, 180));

    // Automatic matching on add market order
    matching_engine.add_order(Order::sell_market(19, 0, 15));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(8, 9));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(165, 180));

    // Automatic matching on add market order with slippage
    matching_engine.add_order(Order::sell_market(20, 0, 100, 0));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(6, 9));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(120, 180));
    matching_engine.add_order(Order::buy_market(21, 0, 160, 20));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(6, 2));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(120, 20));

    // Automatic matching on add market order with reaching end of the book
    matching_engine.add_order(Order::sell_market(22, 0, 1000));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 2));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 20));
    matching_engine.add_order(Order::buy_market(23, 0, 1000));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
}

// Test case: Automatic matching - limit order
TEST_F(MatchingEngineTest, AutomaticMatchingLimitOrder) {
    // Add buy limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 10, 10));
    matching_engine.add_order(Order::buy_limit(2, 0, 10, 20));
    matching_engine.add_order(Order::buy_limit(3, 0, 10, 30));
    matching_engine.add_order(Order::buy_limit(4, 0, 20, 10));
    matching_engine.add_order(Order::buy_limit(5, 0, 20, 20));
    matching_engine.add_order(Order::buy_limit(6, 0, 20, 30));
    matching_engine.add_order(Order::buy_limit(7, 0, 30, 10));
    matching_engine.add_order(Order::buy_limit(8, 0, 30, 20));
    matching_engine.add_order(Order::buy_limit(9, 0, 30, 30));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(9, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(180, 0));

    // Add sell limit orders
    matching_engine.add_order(Order::sell_limit(10, 0, 40, 30));
    matching_engine.add_order(Order::sell_limit(11, 0, 40, 20));
    matching_engine.add_order(Order::sell_limit(12, 0, 40, 10));
    matching_engine.add_order(Order::sell_limit(13, 0, 50, 30));
    matching_engine.add_order(Order::sell_limit(14, 0, 50, 20));
    matching_engine.add_order(Order::sell_limit(15, 0, 50, 10));
    matching_engine.add_order(Order::sell_limit(16, 0, 60, 30));
    matching_engine.add_order(Order::sell_limit(17, 0, 60, 20));
    matching_engine.add_order(Order::sell_limit(18, 0, 60, 10));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(9, 9));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(180, 180));

    // Automatic matching on add limit orders
    matching_engine.add_order(Order::sell_limit(19, 0, 30, 5));
    matching_engine.add_order(Order::sell_limit(20, 0, 30, 25));
    matching_engine.add_order(Order::sell_limit(21, 0, 30, 15));
    matching_engine.add_order(Order::sell_limit(22, 0, 30, 20));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(6, 10));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(120, 185));

    // Automatic matching on several levels
    matching_engine.add_order(Order::buy_limit(23, 0, 60, 105));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(6, 5));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(120, 80));

    // Automatic matching on modify order
    matching_engine.modify_order(OrderId{ 15 }, Price{ 20 }, Quantity{ 20 });
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(5, 4));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(100, 70));

    // Automatic matching on replace order
    matching_engine.replace_order(OrderId{ 2 }, OrderId{ 24 }, Price{ 70 }, Quantity{ 100 });
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(5, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(110, 0));
    matching_engine.replace_order(OrderId{ 1 }, Order::sell_limit(25, 0, 0, 100));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
}

// Test case: Automatic matching - Immediate-Or-Cancel limit order
TEST_F(MatchingEngineTest, AutomaticMatchingIOCLimitOrder) {
    // Add limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 10, 10));
    matching_engine.add_order(Order::buy_limit(2, 0, 20, 20));
    matching_engine.add_order(Order::buy_limit(3, 0, 30, 30));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(3, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(60, 0));

    // Automatic matching 'Immediate-Or-Cancel' order
    matching_engine.add_order(Order::sell_limit(4, 0, 10, 100, TimeInForce::IOC));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
}

// Test case: Automatic matching - Fill-Or-Kill limit order (filled)
TEST_F(MatchingEngineTest, AutomaticMatchingFOKLimitOrderFilled) {
    // Add limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 10, 10));
    matching_engine.add_order(Order::buy_limit(2, 0, 20, 20));
    matching_engine.add_order(Order::buy_limit(3, 0, 30, 30));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(3, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(60, 0));

    // Automatic matching 'Fill-Or-Kill' order
    matching_engine.add_order(Order::sell_limit(4, 0, 10, 40, TimeInForce::FOK));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(2, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(20, 0));
}

// Test case: Automatic matching - Fill-Or-Kill limit order (killed)
TEST_F(MatchingEngineTest, AutomaticMatchingFOKLimitOrderKilled) {
    // Add limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 10, 10));
    matching_engine.add_order(Order::buy_limit(2, 0, 20, 20));
    matching_engine.add_order(Order::buy_limit(3, 0, 30, 30));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(3, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(60, 0));

    // Automatic matching 'Fill-Or-Kill' order
    matching_engine.add_order(Order::sell_limit(4, 0, 10, 100, TimeInForce::FOK));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(3, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(60, 0));
}

// Test case: Automatic matching - All-Or-None limit order several levels full matching
TEST_F(MatchingEngineTest, AutomaticMatchingAONLimitOrderFullMatching) {
    // Add limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 20, 30, TimeInForce::AON));
    matching_engine.add_order(Order::buy_limit(2, 0, 20, 10));
    matching_engine.add_order(Order::buy_limit(3, 0, 30, 30, TimeInForce::AON));
    matching_engine.add_order(Order::buy_limit(4, 0, 30, 10));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(4, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(80, 0));

    // Automatic matching 'All-Or-None' order
    matching_engine.add_order(Order::sell_limit(5, 0, 20, 80, TimeInForce::AON));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
}

// Test case: Automatic matching - All-Or-None limit order several levels partial matching
TEST_F(MatchingEngineTest, AutomaticMatchingAONLimitOrderPartialMatching) {
    // Add limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 20, 30, TimeInForce::AON));
    matching_engine.add_order(Order::buy_limit(2, 0, 20, 10));
    matching_engine.add_order(Order::buy_limit(3, 0, 30, 30, TimeInForce::AON));
    matching_engine.add_order(Order::buy_limit(4, 0, 30, 10));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(4, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(80, 0));

    // Place huge 'All-Or-None' order in the order book with arbitrage price
    matching_engine.add_order(Order::sell_limit(5, 0, 20, 100, TimeInForce::AON));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(4, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(80, 100));

    // Automatic matching 'All-Or-None' order
    matching_engine.add_order(Order::buy_limit(6, 0, 20, 20, TimeInForce::AON));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
}

// Test case: Automatic matching - All-Or-None limit order complex matching
TEST_F(MatchingEngineTest, AutomaticMatchingAONLimitOrderComplexMatching) {
    // Add limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 10, 20, TimeInForce::AON));
    matching_engine.add_order(Order::sell_limit(2, 0, 10, 10, TimeInForce::AON));
    matching_engine.add_order(Order::sell_limit(3, 0, 10, 5));
    matching_engine.add_order(Order::sell_limit(4, 0, 10, 15, TimeInForce::AON));
    matching_engine.add_order(Order::buy_limit(5, 0, 10, 5));
    matching_engine.add_order(Order::buy_limit(6, 0, 10, 20, TimeInForce::AON));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(3, 3));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(45, 30));

    // Automatic matching 'All-Or-None' order
    matching_engine.add_order(Order::sell_limit(7, 0, 10, 15));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
}

// Test case: Automatic matching - Hidden limit order
TEST_F(MatchingEngineTest, AutomaticMatchingHiddenLimitOrder) {
    // Add limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 10, 10, TimeInForce::GTC, 5));
    matching_engine.add_order(Order::buy_limit(2, 0, 20, 20, TimeInForce::GTC, 10));
    matching_engine.add_order(Order::buy_limit(3, 0, 30, 30, TimeInForce::GTC, 15));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(3, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(60, 0));
    EXPECT_EQ(visible_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(30, 0));

    // Automatic matching with market order
    matching_engine.add_order(Order::sell_market(4, 0, 55));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(5, 0));
    EXPECT_EQ(visible_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(5, 0));
}

// Test case: Automatic matching - stop order
TEST_F(MatchingEngineTest, AutomaticMatchingStopOrder) {
    // Add limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 10, 10));
    matching_engine.add_order(Order::buy_limit(2, 0, 20, 20));
    matching_engine.add_order(Order::buy_limit(3, 0, 30, 30));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(3, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(60, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));

    // Automatic matching with stop order
    matching_engine.add_order(Order::sell_stop(4, 0, 40, 60));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));

    // Add stop order
    matching_engine.add_order(Order::sell_limit(5, 0, 30, 30));
    matching_engine.add_order(Order::buy_stop(6, 0, 40, 40));
    matching_engine.add_order(Order::sell_limit(7, 0, 60, 60));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 2));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 90));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(40, 0));

    // Automatic matching with limit order
    matching_engine.add_order(Order::buy_limit(8, 0, 40, 40));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(10, 20));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
}

// Test case: Automatic matching - stop order with an empty market
TEST_F(MatchingEngineTest, AutomaticMatchingStopOrderEmptyMarket) {
    matching_engine.add_order(Order::sell_stop(1, 0, 10, 10));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));

    matching_engine.add_order(Order::buy_stop(2, 0, 20, 20));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
}

// Test case: Automatic matching - stop-limit order
TEST_F(MatchingEngineTest, AutomaticMatchingStopLimitOrder) {
    // Add limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 10, 10));
    matching_engine.add_order(Order::buy_limit(2, 0, 20, 20));
    matching_engine.add_order(Order::buy_limit(3, 0, 30, 30));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(3, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(60, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));

    // Automatic matching with stop-limit orders
    matching_engine.add_order(Order::sell_stop_limit(4, 0, 40, 20, 40));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(2, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(20, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    matching_engine.add_order(Order::sell_stop_limit(5, 0, 30, 10, 30));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 10));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));

    // Add stop-limit order
    matching_engine.add_order(Order::buy_stop_limit(6, 0, 20, 10, 10));
    matching_engine.add_order(Order::sell_limit(7, 0, 20, 20));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 2));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 30));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(10, 0));

    // Automatic matching with limit order
    matching_engine.add_order(Order::buy_limit(7, 0, 20, 30));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(10, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
}

// Test case: Automatic matching - stop-limit order with an empty market
TEST_F(MatchingEngineTest, AutomaticMatchingStopLimitOrderEmptyMarket) {
    matching_engine.add_order(Order::sell_stop_limit(1, 0, 10, 30, 30));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 30));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    matching_engine.remove_order(OrderId{ 1 });

    matching_engine.add_order(Order::buy_stop_limit(2, 0, 30, 10, 10));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(10, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    matching_engine.remove_order(OrderId{ 2 });
}

// Test case: Automatic matching - trailing stop order
TEST_F(MatchingEngineTest, AutomaticMatchingTrailingStopOrder) {
    // Create the market with last prices
    matching_engine.add_order(Order::buy_limit(1, 0, 100, 20));
    matching_engine.add_order(Order::sell_limit(2, 0, 200, 20));
    matching_engine.add_order(Order::sell_market(3, 0, 10));
    matching_engine.add_order(Order::buy_market(4, 0, 10));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(10, 10));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));

    // Add some trailing stop orders
    matching_engine.add_order(Order::trailing_buy_stop(5, 0, 1000, 10, TrailingDistance::from_percentage_units(10, 5)));
    matching_engine.add_order(Order::trailing_sell_stop_limit(6, 0, 0, 10, 10, TrailingDistance::from_percentage_units(-1000, -500)));
    EXPECT_EQ(matching_engine.order_at(OrderId{ 5 })->stop_price().value, 210);
    EXPECT_EQ(matching_engine.order_at(OrderId{ 6 })->stop_price().value, 90);
    EXPECT_EQ(matching_engine.order_at(OrderId{ 6 })->price().value, 100);
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(10, 10));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(10, 10));

    // Move the market best bid price level
    matching_engine.modify_order(OrderId{ 1 }, Price{ 103 }, Quantity{ 20 });
    EXPECT_EQ(matching_engine.order_at(OrderId{ 6 })->stop_price().value, 90);
    EXPECT_EQ(matching_engine.order_at(OrderId{ 6 })->price().value, 100);
    matching_engine.modify_order(OrderId{ 1 }, Price{ 120 }, Quantity{ 20 });
    EXPECT_EQ(matching_engine.order_at(OrderId{ 6 })->stop_price().value, 108);
    EXPECT_EQ(matching_engine.order_at(OrderId{ 6 })->price().value, 118);

    // Move the market best ask price level. Trailing stop price will not move
    // because the last bid price = 200
    matching_engine.modify_order(OrderId{ 2 }, Price{ 197 }, Quantity{ 20 });
    EXPECT_EQ(matching_engine.order_at(OrderId{ 5 })->stop_price().value, 210);
    matching_engine.modify_order(OrderId{ 2 }, Price{ 180 }, Quantity{ 20 });
    EXPECT_EQ(matching_engine.order_at(OrderId{ 5 })->stop_price().value, 210);

    // Move the market best ask price level
    matching_engine.modify_order(OrderId{ 2 }, Price{ 197 }, Quantity{ 20 });
    matching_engine.add_order(Order::buy_market(7, 0, 10));
    EXPECT_EQ(matching_engine.order_at(OrderId{ 5 })->stop_price().value, 210);
    matching_engine.modify_order(OrderId{ 2 }, Price{ 180 }, Quantity{ 20 });
    matching_engine.add_order(Order::buy_market(7, 0, 10));
    EXPECT_EQ(matching_engine.order_at(OrderId{ 5 })->stop_price().value, 190);
}

// Test case: In-Flight Mitigation
TEST_F(MatchingEngineTest, InFlightMitigation) {
    // Add buy limit order
    matching_engine.add_order(Order::buy_limit(1, 0, 10, 100));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(100, 0));

    // Add sell limit order
    matching_engine.add_order(Order::sell_limit(2, 0, 20, 100));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(100, 100));

    // Automatic matching on add limit orders
    matching_engine.add_order(Order::sell_limit(3, 0, 10, 20));
    matching_engine.add_order(Order::buy_limit(4, 0, 20, 20));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(80, 80));

    // Mitigate orders
    matching_engine.mitigate_order(OrderId{ 1 }, Price{ 10 }, Quantity{ 150 });
    matching_engine.mitigate_order(OrderId{ 2 }, Price{ 20 }, Quantity{ 50 });
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(130, 30));

    // Mitigate orders
    matching_engine.mitigate_order(OrderId{ 1 }, Price{ 10 }, Quantity{ 20 });
    matching_engine.mitigate_order(OrderId{ 2 }, Price{ 20 }, Quantity{ 10 });
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
}

// Test case: Manual matching
TEST_F(MatchingEngineTest, ManualMatching) {
    matching_engine.disable_matching();

    // Add buy limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 10, 10));
    matching_engine.add_order(Order::buy_limit(2, 0, 10, 20));
    matching_engine.add_order(Order::buy_limit(3, 0, 10, 30));
    matching_engine.add_order(Order::buy_limit(4, 0, 20, 10));
    matching_engine.add_order(Order::buy_limit(5, 0, 20, 20));
    matching_engine.add_order(Order::buy_limit(6, 0, 20, 30));
    matching_engine.add_order(Order::buy_limit(7, 0, 30, 10));
    matching_engine.add_order(Order::buy_limit(8, 0, 30, 20));
    matching_engine.add_order(Order::buy_limit(9, 0, 30, 30));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(9, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(180, 0));

    // Add sell limit orders
    matching_engine.add_order(Order::sell_limit(10, 0, 10, 30));
    matching_engine.add_order(Order::sell_limit(11, 0, 10, 20));
    matching_engine.add_order(Order::sell_limit(12, 0, 10, 10));
    matching_engine.add_order(Order::sell_limit(13, 0, 20, 30));
    matching_engine.add_order(Order::sell_limit(14, 0, 20, 25));
    matching_engine.add_order(Order::sell_limit(15, 0, 20, 10));
    matching_engine.add_order(Order::sell_limit(16, 0, 30, 30));
    matching_engine.add_order(Order::sell_limit(17, 0, 30, 20));
    matching_engine.add_order(Order::sell_limit(18, 0, 30, 10));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(9, 9));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(180, 185));

    // Perform manual matching
    matching_engine.match();
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(3, 4));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(60, 65));
}

TEST_F(MatchingEngineTest, ComplexMatchingMultipleOrderTypesEdgeCases) {
    // Step 1: Populate orderbook with a large number of limit orders across multiple price levels
    matching_engine.add_order(Order::buy_limit(1, 0, 10, 100));
    matching_engine.add_order(Order::buy_limit(2, 0, 10, 50));
    matching_engine.add_order(Order::buy_limit(3, 0, 15, 75));
    matching_engine.add_order(Order::buy_limit(4, 0, 15, 25, TimeInForce::AON));
    matching_engine.add_order(Order::buy_limit(5, 0, 20, 200, TimeInForce::GTC, 50));
    matching_engine.add_order(Order::buy_limit(6, 0, 20, 100));
    matching_engine.add_order(Order::buy_limit(7, 0, 25, 150));
    matching_engine.add_order(Order::buy_limit(8, 0, 25, 50, TimeInForce::FOK));
    matching_engine.add_order(Order::buy_limit(9, 0, 30, 300));
    matching_engine.add_order(Order::buy_limit(10, 0, 30, 100, TimeInForce::IOC));
    matching_engine.add_order(Order::sell_limit(11, 0, 35, 100));
    matching_engine.add_order(Order::sell_limit(12, 0, 35, 50));
    matching_engine.add_order(Order::sell_limit(13, 0, 40, 75));
    matching_engine.add_order(Order::sell_limit(14, 0, 40, 25, TimeInForce::AON));
    matching_engine.add_order(Order::sell_limit(15, 0, 45, 200, TimeInForce::GTC, 50));
    matching_engine.add_order(Order::sell_limit(16, 0, 45, 100));
    matching_engine.add_order(Order::sell_limit(17, 0, 50, 150));
    matching_engine.add_order(Order::sell_limit(18, 0, 50, 50, TimeInForce::FOK));
    matching_engine.add_order(Order::sell_limit(19, 0, 55, 300));
    matching_engine.add_order(Order::sell_limit(20, 0, 55, 100, TimeInForce::IOC));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(8, 8));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1000, 1000));
    EXPECT_EQ(visible_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(850, 850));

    // Step 2: Add stop orders and trailing stop orders
    matching_engine.add_order(Order::buy_stop(21, 0, 50, 100));
    matching_engine.add_order(Order::sell_stop(22, 0, 15, 100));
    matching_engine.add_order(Order::trailing_buy_stop(23, 0, 1000, 200, TrailingDistance::from_percentage_units(200, 10)));
    matching_engine.add_order(Order::trailing_sell_stop_limit(24, 0, 0, 150, 150, TrailingDistance::from_percentage_units(-1000, -500)));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 2));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(300, 250));
    EXPECT_EQ(matching_engine.order_at(OrderId{23})->stop_price().value, 1000);
    EXPECT_EQ(matching_engine.order_at(OrderId{24})->stop_price().value, 0);
    EXPECT_EQ(matching_engine.order_at(OrderId{24})->price().value, 150);

    // Step 3: Execute market orders to trigger matches and stop orders
    matching_engine.add_order(Order::sell_market(25, 0, 400));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(7, 8));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(600, 1000));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 2));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(300, 250));

    // Step 4: Add aggressive limit orders to trigger further matches
    matching_engine.add_order(Order::sell_limit(26, 0, 15, 500, TimeInForce::IOC));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 9));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(50, 1150));

    // Step 5: Modify trailing stop orders by moving market prices
    matching_engine.add_order(Order::buy_limit(27, 0, 60, 100));
    matching_engine.add_order(Order::sell_market(28, 0, 50));
    EXPECT_EQ(matching_engine.order_at(OrderId{23})->stop_price().value, 235);
    EXPECT_EQ(matching_engine.order_at(OrderId{24})->stop_price().value, 0);
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 8));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 1050));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(300, 0));

    // Step 6: Test order modification and replacement
    matching_engine.modify_order(OrderId{15}, Price{35}, Quantity{150});
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 8));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 1000));
    matching_engine.replace_order(OrderId{17}, Order::buy_limit(30, 0, 35, 100, TimeInForce::AON));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 6));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 750));

    // Step 7: Test mitigation with a large volume
    matching_engine.mitigate_order(OrderId{19}, Price{55}, Quantity{400});
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 6));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 850));

    // Step 8: Test manual matching
    matching_engine.disable_matching();
    matching_engine.add_order(Order::buy_limit(31, 0, 55, 500));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 6));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(500, 850));
    matching_engine.match();
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 2));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 250));

    // Step 9: Test edge case - Empty market with stop orders
    matching_engine.remove_order(OrderId{19});
    matching_engine.remove_order(OrderId{24});
    matching_engine.remove_order(OrderId{23});
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
    matching_engine.add_order(Order::buy_stop(32, 0, 60, 100));
    matching_engine.add_order(Order::sell_stop_limit(33, 0, 10, 100, 100));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));
    matching_engine.add_order(Order::buy_market(34, 0, 50));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));

    // Step 10: Test edge case - Large volume FOK order
    matching_engine.enable_matching();
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 1));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));

    matching_engine.add_order(Order::buy_limit(35, 0, 10, 1000, TimeInForce::FOK));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 1));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));

    // Step 11: Final cleanup and verification
    matching_engine.remove_order(OrderId{33});
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
}

TEST_F(MatchingEngineTest, ChainReactionOfStopOrders) {
    // Step 1: Populate order book with limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 100, 200));
    matching_engine.add_order(Order::buy_limit(2, 0, 95, 150));
    matching_engine.add_order(Order::sell_limit(3, 0, 110, 200));
    matching_engine.add_order(Order::sell_limit(4, 0, 115, 150));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 2));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(350, 350));

    // Step 2: Add stop orders to create a chain reaction
    matching_engine.add_order(Order::sell_stop(5, 0, 100, 100));
    matching_engine.add_order(Order::buy_stop(6, 0, 115, 100));
    matching_engine.add_order(Order::sell_stop(7, 0, 95, 50));
    matching_engine.add_order(Order::buy_stop(8, 0, 120, 50));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 1));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(150, 50));

    // Step 3: Trigger chain with a large market sell order
    matching_engine.add_order(Order::sell_market(9, 0, 300));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 2));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 350));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(150, 0));
}

TEST_F(MatchingEngineTest, HiddenOrdersWithPartialMatching) {
    // Step 1: Add hidden and visible limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 100, 200, TimeInForce::GTC, 50));
    matching_engine.add_order(Order::buy_limit(2, 0, 95, 100));
    matching_engine.add_order(Order::sell_limit(3, 0, 110, 200, TimeInForce::GTC, 50));
    matching_engine.add_order(Order::sell_limit(4, 0, 115, 100));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 2));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(300, 300));
    EXPECT_EQ(visible_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(150, 150));

    // Step 2: Add market order to match against hidden orders
    matching_engine.add_order(Order::sell_market(5, 0, 250));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 2));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(50, 300));
    EXPECT_EQ(visible_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(50, 150));

    // Step 3: Add aggressive limit order to match remaining hidden volume
    matching_engine.add_order(Order::sell_limit(6, 0, 95, 100));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 3));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 350));
    EXPECT_EQ(visible_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 200));
}

TEST_F(MatchingEngineTest, AONAndFOKInVolatileMarket) {
    // Step 1: Create volatile market with limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 100, 100));
    matching_engine.add_order(Order::sell_limit(2, 0, 110, 100));
    matching_engine.add_order(Order::buy_market(3, 0, 50));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(100, 50));

    // Step 2: Add AON and FOK orders
    matching_engine.add_order(Order::buy_limit(4, 0, 105, 75, TimeInForce::AON));
    matching_engine.add_order(Order::sell_limit(5, 0, 105, 50, TimeInForce::FOK));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(175, 50));

    // Step 3: Move market price to test AON/FOK behavior
    matching_engine.add_order(Order::sell_market(6, 0, 100));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(75, 50));
}

TEST_F(MatchingEngineTest, TrailingStopOrdersInTrendingMarket) {
    // Step 1: Set up initial market
    matching_engine.add_order(Order::buy_limit(1, 0, 100, 100));
    matching_engine.add_order(Order::sell_limit(2, 0, 110, 100));
    matching_engine.add_order(Order::buy_market(3, 0, 50));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(100, 50));

    // Step 2: Add trailing stop orders
    matching_engine.add_order(Order::trailing_buy_stop(4, 0, 1000, 100, TrailingDistance::from_percentage_units(100, 10)));
    matching_engine.add_order(Order::trailing_sell_stop_limit(5, 0, 0, 100, 100, TrailingDistance::from_percentage_units(-1000, -500)));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(100, 100));
    EXPECT_EQ(matching_engine.order_at(OrderId{4})->stop_price().value, 210);
    EXPECT_EQ(matching_engine.order_at(OrderId{5})->stop_price().value, 90);

    // Step 3: Simulate upward trend
    matching_engine.add_order(Order::buy_limit(6, 0, 120, 100));
    matching_engine.add_order(Order::sell_market(7, 0, 50));
    EXPECT_EQ(matching_engine.order_at(OrderId{5})->stop_price().value, 99);
}

TEST_F(MatchingEngineTest, OrderModificationDuringMatching) {
    // Step 1: Populate order book
    matching_engine.add_order(Order::buy_limit(1, 0, 100, 200));
    matching_engine.add_order(Order::sell_limit(2, 0, 110, 200));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(200, 200));

    // Step 2: Add aggressive order and modify existing order
    matching_engine.add_order(Order::sell_limit(3, 0, 100, 150));
    matching_engine.modify_order(OrderId{1}, Price{100}, Quantity{100});
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(100, 200));
}

TEST_F(MatchingEngineTest, OrderReplacementWithTypeChange) {
    // Step 1: Populate order book
    matching_engine.add_order(Order::buy_limit(1, 0, 100, 200));
    matching_engine.add_order(Order::sell_limit(2, 0, 110, 200));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(200, 200));

    // Step 2: Replace limit order with stop order
    matching_engine.replace_order(OrderId{1}, Order::buy_stop(3, 0, 110, 100));
    matching_engine.add_order(Order::sell_limit(4, 0, 110, 150));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 2));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 250));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
}

TEST_F(MatchingEngineTest, MitigationWithLargeOrders) {
    // Step 1: Add large orders
    matching_engine.add_order(Order::buy_limit(1, 0, 100, 1000));
    matching_engine.add_order(Order::sell_limit(2, 0, 110, 1000));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1000, 1000));

    // Step 2: Mitigate orders
    matching_engine.mitigate_order(OrderId{1}, Price{100}, Quantity{500});
    matching_engine.mitigate_order(OrderId{2}, Price{110}, Quantity{500});
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(500, 500));
}

TEST_F(MatchingEngineTest, IOCAndFOKInThinMarket) {
    // Step 1: Create thin market
    matching_engine.add_order(Order::buy_limit(1, 0, 100, 50));
    matching_engine.add_order(Order::sell_limit(2, 0, 110, 50));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(50, 50));

    // Step 2: Add IOC and FOK orders
    matching_engine.add_order(Order::sell_limit(3, 0, 100, 100, TimeInForce::IOC));
    matching_engine.add_order(Order::buy_limit(4, 0, 110, 100, TimeInForce::FOK));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 50));
}

TEST_F(MatchingEngineTest, StopLimitOrdersWithPriceGaps) {
    // Step 1: Create market with price gap
    matching_engine.add_order(Order::buy_limit(1, 0, 100, 100));
    matching_engine.add_order(Order::sell_limit(2, 0, 120, 100));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(100, 100));

    // Step 2: Add stop-limit orders
    matching_engine.add_order(Order::sell_stop_limit(3, 0, 110, 100, 100));
    matching_engine.add_order(Order::buy_stop_limit(4, 0, 110, 100, 110));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));

    // Step 3: Trigger stop-limit with market order
    matching_engine.add_order(Order::sell_market(5, 0, 150));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 100));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
}

TEST_F(MatchingEngineTest, TimeInForceExpirationSimulation) {
    // Step 1: Add orders with different time-in-force
    matching_engine.add_order(Order::buy_limit(1, 0, 100, 100, TimeInForce::IOC));
    matching_engine.add_order(Order::buy_limit(2, 0, 100, 100, TimeInForce::FOK));
    matching_engine.add_order(Order::buy_limit(3, 0, 100, 100, TimeInForce::AON));
    matching_engine.add_order(Order::buy_limit(4, 0, 100, 100, TimeInForce::GTC));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(200, 0));

    // Step 2: Add sell order that partially matches
    matching_engine.add_order(Order::sell_limit(5, 0, 100, 50));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(200, 50));
}

TEST_F(MatchingEngineTest, ComplexOrderbookInteractions) {
    // Step 1: Populate order book with a large number of limit orders
    matching_engine.add_order(Order::buy_limit(1, 0, 90, 100));
    matching_engine.add_order(Order::buy_limit(2, 0, 95, 150));
    matching_engine.add_order(Order::buy_limit(3, 0, 100, 200, TimeInForce::GTC, 50)); // Hidden order
    matching_engine.add_order(Order::buy_limit(4, 0, 105, 250));
    matching_engine.add_order(Order::buy_limit(5, 0, 110, 300, TimeInForce::AON));
    matching_engine.add_order(Order::buy_limit(6, 0, 115, 350, TimeInForce::IOC));
    matching_engine.add_order(Order::buy_limit(7, 0, 120, 400, TimeInForce::FOK));
    matching_engine.add_order(Order::sell_limit(8, 0, 125, 400));
    matching_engine.add_order(Order::sell_limit(9, 0, 130, 350, TimeInForce::GTC, 50)); // Hidden order
    matching_engine.add_order(Order::sell_limit(10, 0, 135, 300, TimeInForce::AON));
    matching_engine.add_order(Order::sell_limit(11, 0, 140, 250, TimeInForce::IOC));
    matching_engine.add_order(Order::sell_limit(12, 0, 145, 200, TimeInForce::FOK));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(5, 3));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1000, 1050));
    EXPECT_EQ(visible_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(850, 750));

    // Step 2: Add stop and stop-limit orders
    matching_engine.add_order(Order::sell_stop(13, 0, 100, 100));
    matching_engine.add_order(Order::buy_stop(14, 0, 120, 100));
    matching_engine.add_order(Order::sell_stop_limit(15, 0, 110, 50, 110));
    matching_engine.add_order(Order::buy_stop_limit(16, 0, 115, 50, 115));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 1));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 100));

    // Step 3: Add trailing stop orders
    matching_engine.add_order(Order::trailing_buy_stop(17, 0, 1000, 100, TrailingDistance::from_percentage_units(100, 10)));
    matching_engine.add_order(Order::trailing_sell_stop_limit(18, 0, 0, 100, 100, TrailingDistance::from_percentage_units(-1000, -500)));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 2));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(100, 200));
    EXPECT_EQ(matching_engine.order_at(OrderId{17})->stop_price().value, 225);
    EXPECT_EQ(matching_engine.order_at(OrderId{18})->stop_price().value, 95);
    EXPECT_EQ(matching_engine.order_at(OrderId{18})->price().value, 195);

    // Step 4: Trigger matches with market orders
    matching_engine.add_order(Order::sell_market(19, 0, 500));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 4));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(215, 860));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(1, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(100, 0));

    // Step 5: Modify and replace orders
    matching_engine.modify_order(OrderId{10}, Price{102}, Quantity{180});
    matching_engine.replace_order(OrderId{18}, Order::buy_limit(20, 0, 108, 280, TimeInForce::GTC));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(3, 2));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(315, 460));

    // Step 6: Mitigate orders
    matching_engine.mitigate_order(OrderId{9}, Price{132}, Quantity{180});
    matching_engine.mitigate_order(OrderId{8}, Price{125}, Quantity{200});
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(3, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(315, 180));

    // Step 7: Add aggressive limit orders
    matching_engine.add_order(Order::sell_limit(21, 0, 98, 300, TimeInForce::IOC));
    matching_engine.add_order(Order::buy_limit(22, 0, 132, 300, TimeInForce::FOK));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 1));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(215, 180));

    // Step 8: Simulate price gap and thin market
    matching_engine.add_order(Order::sell_limit(23, 0, 150, 50));
    matching_engine.add_order(Order::buy_stop(24, 0, 150, 100));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 2));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(215, 230));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 0));

    // Step 9: Trigger stop orders with market order
    matching_engine.add_order(Order::buy_market(25, 0, 400));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(2, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(215, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));

    // Step 10: Test manual matching
    matching_engine.disable_matching();
    matching_engine.add_order(Order::buy_limit(26, 0, 130, 500));
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(3, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(715, 0));
    matching_engine.match();
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(3, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(715, 0));

    // Step 11: Final cleanup and verification
    matching_engine.remove_order(OrderId{16});
    matching_engine.remove_order(OrderId{26});
    matching_engine.remove_order(OrderId{1});
    EXPECT_EQ(orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
    EXPECT_EQ(orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_count(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
    EXPECT_EQ(stop_orders_volume(matching_engine.orderbook_at(SymbolId{0})), std::make_pair(0, 0));
}

}
