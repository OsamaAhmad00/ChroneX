<div align="center"> <img src="ChroneX.png" alt="Logo" width=400px> </div>

# ChroneX: High-Performance Trading Engine

Welcome to **ChroneX**, an ambitious trading engine, combining **Chrono** (Greek god of time) and **eX** (exchange and execution), designed for speed and scalability.

## Early Development Stage
ChroneX is in its initial phase. We're currently building the foundation, refining the architecture, and shaping its future direction.

## Features
ChroneX currently supports an orderbook and a matching engine, including the following order types:
- Market
- Limit
- Stop Loss
- Stop Limit
- Trailing Stop Loss
- Trailing Stop Limit

## Performance Optimization
Our design prioritizes performance by:
- Leveraging **template metaprogramming** to shift computations to compile-time.
- Identifying and eliminating runtime bottlenecks.
- Maximizing hardware utilization for optimal execution speed.


## Example Usage
Below is a sample demonstrating the matching engine with limit orders, using `StdOutEventHandler` to output events to `stdout`:

```c++
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
```
### Output
```text
add_new_orderbook	GOOG
add_level			(Limit, Buy)		OrderBook { Symbol = GOOG }	Price = 100
add_order			(Limit, Buy)		OrderBook { Symbol = GOOG }	Order { ID = 3 }
execute_order		(Limit, Buy)		OrderBook { Symbol = GOOG }	Order { ID = 3 }	Quantity = 20	Price = 100
execute_order		(Limit, Sell)		OrderBook { Symbol = GOOG }	Order { ID = 4 }	Quantity = 20	Price = 100
remove_order		(Limit, Sell)		OrderBook { Symbol = GOOG }	Order { ID = 4 }
execute_order		(Limit, Buy)		OrderBook { Symbol = GOOG }	Order { ID = 3 }	Quantity = 80	Price = 100
remove_order		(Limit, Buy)		OrderBook { Symbol = GOOG }	Order { ID = 3 }
remove_level		(Limit, Buy)		OrderBook { Symbol = GOOG }	Price = 100
execute_order		(Limit, Sell)		OrderBook { Symbol = GOOG }	Order { ID = 5 }	Quantity = 80	Price = 100
add_level			(Limit, Sell)		OrderBook { Symbol = GOOG }	Price = 100
add_order			(Limit, Sell)		OrderBook { Symbol = GOOG }	Order { ID = 5 }
add_level			(Limit, Buy)		OrderBook { Symbol = GOOG }	Price = 42
add_order			(Limit, Buy)		OrderBook { Symbol = GOOG }	Order { ID = 2 }
execute_order		(Limit, Sell)		OrderBook { Symbol = GOOG }	Order { ID = 5 }	Quantity = 10	Price = 100
remove_order		(Limit, Buy)		OrderBook { Symbol = GOOG }	Order { ID = 2 }
remove_level		(Limit, Buy)		OrderBook { Symbol = GOOG }	Price = 42
add_level			(Limit, Sell)		OrderBook { Symbol = GOOG }	Price = 42
add_order			(Limit, Sell)		OrderBook { Symbol = GOOG }	Order { ID = 6 }
remove_order		(Limit, Sell)		OrderBook { Symbol = GOOG }	Order { ID = 6 }
remove_level		(Limit, Sell)		OrderBook { Symbol = GOOG }	Price = 42
remove_orderbook	OrderBook { Symbol = GOOG }
```

## Event Handlers
ChroneX currently offers two event handler types:
- **NullEventHandler**: Ignores events, ideal for minimal overhead.
- **StreamEventHandler** (including **StdOutEventHandler**): Streams events for processing.

Since event handlers are template parameters, template metaprogramming removes event-handling logic when using NullEventHandler, reducing runtime costs. Additional event handlers will be implemented for network communication.

## Project Goal
The goal of this project is to create a complete trading platform that is extremely fast, and have its components (orderbook, matching engine, etc) be reusable in other projects as well.

To achieve this goal, we need to create a swift, scalable, and resilient backend, and an intuitive frontend for users.

Since order matching must be serialized, there is no point in making the matching engine multithreaded. The initial architecture design includes three main entities:
 - **Matching Engine**: A single-threaded program that is isolated from the outside world, and reports changes in different orderbooks to connected servers via network
 - **Orderbooks Store**: A scalable, replicated, and possibly sharded set of running servers, each of which will be listening to events from the matching engine, and update their orderbooks accordingly. This is the entity responsible for giving information to users.
 - **Authenticator**: A set of possibly sharded servers that are responsible for sending new orders to the matching engine. They handle storing of user information, authorizing their requests, and proxying the requests to the matching engine.

Since the current implementation allows hooking different kinds of event handlers, the communication will happen by an event handler that reports accurately the changes happening in the orderbooks of the matching engine, and a listener will be implemented on the other side (orderbooks store). The communication will probably be in gRPC.

To allow this to happen, the orderbook should be extracted into its own library, and use the exact same orderbook in both the matching engine and the orderbooks store.

## Next Steps
ChroneX is actively evolving and requires additional testing, new features, and resolution of existing TODOs.

## References
- Largely inspired by [CppTrader](https://github.com/chronoxor/CppTrader)
- [When Nanoseconds Matter: Ultrafast Trading Systems in C++ - David Gross - CppCon 2024](https://www.youtube.com/watch?v=sX2nF1fW7kI)