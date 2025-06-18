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
// TEST_F(MatchingEngineTest, AutomaticMatchingLimitOrder) {
//     // Add buy limit orders
//     matching_engine.add_order(Order::buy_limit(1, 0, 10, 10));
//     matching_engine.add_order(Order::buy_limit(2, 0, 10, 20));
//     matching_engine.add_order(Order::buy_limit(3, 0, 10, 30));
//     matching_engine.add_order(Order::buy_limit(4, 0, 20, 10));
//     matching_engine.add_order(Order::buy_limit(5, 0, 20, 20));
//     matching_engine.add_order(Order::buy_limit(6, 0, 20, 30));
//     matching_engine.add_order(Order::buy_limit(7, 0, 30, 10));
//     matching_engine.add_order(Order::buy_limit(8, 0, 30, 20));
//     matching_engine.add_order(Order::buy_limit(9, 0, 30, 30));
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(9, 0));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(180, 0));
//
//     // Add sell limit orders
//     matching_engine.add_order(Order::sell_limit(10, 0, 40, 30));
//     matching_engine.add_order(Order::sell_limit(11, 0, 40, 20));
//     matching_engine.add_order(Order::sell_limit(12, 0, 40, 10));
//     matching_engine.add_order(Order::sell_limit(13, 0, 50, 30));
//     matching_engine.add_order(Order::sell_limit(14, 0, 50, 20));
//     matching_engine.add_order(Order::sell_limit(15, 0, 50, 10));
//     matching_engine.add_order(Order::sell_limit(16, 0, 60, 30));
//     matching_engine.add_order(Order::sell_limit(17, 0, 60, 20));
//     matching_engine.add_order(Order::sell_limit(18, 0, 60, 10));
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(9, 9));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(180, 180));
//
//     // Automatic matching on add limit orders
//     matching_engine.add_order(Order::sell_limit(19, 0, 30, 5));
//     matching_engine.add_order(Order::sell_limit(20, 0, 30, 25));
//     matching_engine.add_order(Order::sell_limit(21, 0, 30, 15));
//     matching_engine.add_order(Order::sell_limit(22, 0, 30, 20));
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(6, 10));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(120, 185));
//
//     // Automatic matching on several levels
//     matching_engine.add_order(Order::buy_limit(23, 0, 60, 105));
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(6, 5));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(120, 80));
//
//     // Automatic matching on modify order
//     matching_engine.modify_order(OrderId{ 15 }, 20, 20);
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(5, 4));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(100, 70));
//
//     // Automatic matching on replace order
//     matching_engine.replace_order(OrderId{ 2 }, 24, 70, 100);
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(5, 0));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(110, 0));
//     matching_engine.replace_order(1, Order::sell_limit(25, 0, 0, 100));
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
// }

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
// TEST_F(MatchingEngineTest, AutomaticMatchingTrailingStopOrder) {
//     // Create the market with last prices
//     matching_engine.add_order(Order::buy_limit(1, 0, 100, 20));
//     matching_engine.add_order(Order::sell_limit(2, 0, 200, 20));
//     matching_engine.add_order(Order::sell_market(3, 0, 10));
//     matching_engine.add_order(Order::buy_market(4, 0, 10));
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(10, 10));
//     EXPECT_EQ(BookStopOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
//     EXPECT_EQ(BookStopVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
//
//     // Add some trailing stop orders
//     matching_engine.add_order(Order::trailing_buy_stop(5, 0, 1000, 10, 10, 5));
//     matching_engine.add_order(Order::trailing_sell_stop_limit(6, 0, 0, 10, 10, -1000, -500));
//     EXPECT_EQ(matching_engine.order_at(OrderId{ 5 })->StopPrice, 210);
//     EXPECT_EQ(matching_engine.order_at(OrderId{ 6 })->StopPrice, 90);
//     EXPECT_EQ(matching_engine.order_at(OrderId{ 6 })->Price, 100);
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(10, 10));
//     EXPECT_EQ(BookStopOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
//     EXPECT_EQ(BookStopVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(10, 10));
//
//     // Move the market best bid price level
//     matching_engine.modify_order(1, 103, 20);
//     EXPECT_EQ(matching_engine.order_at(OrderId{ 6 })->StopPrice, 90);
//     EXPECT_EQ(matching_engine.order_at(OrderId{ 6 })->Price, 100);
//     matching_engine.modify_order(1, 120, 20);
//     EXPECT_EQ(matching_engine.order_at(OrderId{ 6 })->StopPrice, 108);
//     EXPECT_EQ(matching_engine.order_at(OrderId{ 6 })->Price, 118);
//
//     // Move the market best ask price level. Trailing stop price will not move
//     // because the last bid price = 200
//     matching_engine.modify_order(2, 197, 20);
//     EXPECT_EQ(matching_engine.order_at(OrderId{ 5 })->StopPrice, 210);
//     matching_engine.modify_order(2, 180, 20);
//     EXPECT_EQ(matching_engine.order_at(OrderId{ 5 })->StopPrice, 210);
//
//     // Move the market best ask price level
//     matching_engine.modify_order(2, 197, 20);
//     matching_engine.add_order(Order::buy_market(7, 0, 10));
//     EXPECT_EQ(matching_engine.order_at(OrderId{ 5 })->StopPrice, 210);
//     matching_engine.modify_order(2, 180, 20);
//     matching_engine.add_order(Order::buy_market(7, 0, 10));
//     EXPECT_EQ(matching_engine.order_at(OrderId{ 5 })->StopPrice, 190);
// }

// // Test case: In-Flight Mitigation
// TEST_F(MatchingEngineTest, InFlightMitigation) {
//     // Add buy limit order
//     matching_engine.add_order(Order::buy_limit(1, 0, 10, 100));
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 0));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(100, 0));
//
//     // Add sell limit order
//     matching_engine.add_order(Order::sell_limit(2, 0, 20, 100));
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(100, 100));
//
//     // Automatic matching on add limit orders
//     matching_engine.add_order(Order::sell_limit(3, 0, 10, 20));
//     matching_engine.add_order(Order::buy_limit(4, 0, 20, 20));
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(80, 80));
//
//     // Mitigate orders
//     matching_engine.MitigateOrder(1, 10, 150);
//     matching_engine.MitigateOrder(2, 20, 50);
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(1, 1));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(130, 30));
//
//     // Mitigate orders
//     matching_engine.MitigateOrder(1, 10, 20);
//     matching_engine.MitigateOrder(2, 20, 10);
//     EXPECT_EQ(BookOrders(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
//     EXPECT_EQ(BookVolume(matching_engine.orderbook_at(SymbolId{ 0 })), std::make_pair(0, 0));
// }

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

}
