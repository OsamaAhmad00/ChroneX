#pragma once

#include <unordered_map>
#include <bits/locale_facets_nonio.h>

#include <chronex/Symbol.hpp>

#include <chronex/concepts/OrderBook.hpp>
#include <chronex/concepts/EventHandler.hpp>

#include <chronex/handlers/NullEventHandler.hpp>

#include <chronex/orderbook/Order.hpp>
#include <chronex/orderbook/OrderBook.hpp>
#include <chronex/orderbook/OrderUtils.hpp>

// Only works with stateless functors
#define RESOLVE_TYPE_AND_SIDE_THEN_CALL(order, func) resolve_type_and_side_then_call<decltype(func)>(order)

namespace chronex {

template <
    concepts::Order Order = Order,
    concepts::EventHandler<OrderType> EventHandler = handlers::NullEventHandler,
    concepts::OrderBook OrderBook = OrderBook<Order, EventHandler>
    typename HashMap = std::unordered_map
>
class MatchingEngine {
public:

    // This shouldn't affect performance since it's very predictable because
    //  the matching is typically enabled or disabled for the entire session
    constexpr bool is_matching_enabled() const noexcept { return _is_matching_enabled; }
    constexpr void enable_matching() noexcept { _is_matching_enabled = true; }
    constexpr void disable_matching() noexcept { _is_matching_enabled = false; }

    constexpr void add_new_orderbook(Symbol symbol) {
        event_handler().on_add_new_orderbook(symbol);
        add_existing_orderbook(OrderBook { symbol, &event_handler(), &orders() }, false);
    }

    template <typename T>
    constexpr void add_existing_orderbook(T&& orderbook, bool report = true) {
        assert(!is_symbol_taken(symbol) &&
            "Symbol with the same ID already exists in the matching engine");

        auto symbol = orderbook.symbol();
        auto id = symbol.id;

        if (orderbooks().size() <= id.value) {
            orderbooks().resize(id.value + 1);
        }

        if (report) {
            event_handler().on_add_orderbook(symbol);
        }

        orderbook_at(id) = std::forward<T>(orderbook);
    }

    constexpr void remove_orderbook(Symbol symbol) noexcept {
        assert(is_symbol_taken(symbol) && "No symbol with the given ID exists in the matching engine");

        auto& orderbook = orderbook_at(symbol.id);

        event_handler().on_remove_symbol(symbol);

        orderbook.clear();
        orderbook.invalidate();
    }

    template <concepts::Order T>
    constexpr void add_order(T&& order) {
        if (order.is_buy_order()) {
            add_order<OrderSide::BUY> (std::forward<T>(order));
        } else {
            add_order<OrderSide::SELL>(std::forward<T>(order));
        }
    }

    template <OrderSide side, concepts::Order T>
    constexpr void add_order(T&& order) {
        assert(order.is_valid() && "Order is invalid");

        switch (order.type()) {
            case OrderType::MARKET:
                return add_market_order(std::forward<T>(order));
            case OrderType::LIMIT:
                return add_limit_order<side>(std::forward<T>(order));
            case OrderType::STOP:
                return add_stop_order<OrderType::STOP, side>(std::forward<T>(order));
            case OrderType::TRAILING_STOP:
                return add_stop_order<OrderType::TRAILING_STOP, side>(std::forward<T>(order));
            case OrderType::STOP_LIMIT:
                return add_stop_limit_order<OrderType::STOP_LIMIT, side>(std::forward<T>(order));
            case OrderType::TRAILING_STOP_LIMIT:
                return add_stop_limit_order<OrderType::TRAILING_STOP_LIMIT, side>(std::forward<T>(order));
            default:
                assert(false && "Wrong order type to use here");
        }
    }

    template <concepts::Order T>
    constexpr void add_market_order(T&& order) {
        auto& orderbook = orderbook_at(order.symbol_id());

        event_handler().on_market_order_add(order);

        if (is_matching_enabled())
            match_market_order(orderbook, order);

        event_handler().on_market_order_remove(order);

        perform_post_order_processing(orderbook);
    }

    template <OrderSide side, concepts::Order T>
    constexpr void add_limit_order(T&& order) {
        auto& orderbook = orderbook_at(order.symbol_id());

        event_handler().template on_limit_order_add<side>(order);

        if (is_matching_enabled())
            match_limit_order(orderbook, order);

        try_add_limit_order<OrderType::LIMIT, side>(orderbook, std::forward<T>(order));

        perform_post_order_processing(orderbook);
    }

    template <OrderType type, OrderSide side, concepts::Order T>
    constexpr void add_stop_order(T&& order) {
        auto& orderbook = orderbook_at(order.symbol_id());

        if constexpr (type == OrderType::TRAILING_STOP) {
            order.set_stop_price(orderbook.template calculate_trailing_stop_price<side>(order));
        }

        event_handler().template on_stop_order_add<type, side>(order);

        if (is_matching_enabled()) {
            const bool triggered = try_trigger_stop_order<type, side>(orderbook, order);
            if (triggered) {
                return;
            }
        }

        if (!order.is_fully_filled()) {
            if constexpr (type == OrderType::STOP) {
                orderbook.template add_order<LevelsType::STOP, side>(std::forward<T>(order));
            } else {
                orderbook.template add_order<LevelsType::TRAILING_STOP, side>(std::forward<T>(order));
            }
        } else {
            event_handler().template on_stop_order_remove<type, side>(order);
        }

        perform_post_order_processing(orderbook);
    }

    template <OrderType type, OrderSide side, concepts::Order T>
    constexpr void add_stop_limit_order(T&& order) {
        auto& orderbook = orderbook_at(order.symbol_id());

        if constexpr (type == OrderType::TRAILING_STOP_LIMIT) {
            auto trailing_stop_price = orderbook.template calculate_trailing_stop_price<side>(order);
            order.set_stop_and_trailing_stop_prices(trailing_stop_price);
        }

        event_handler().template on_stop_limit_order_add<type, side>(order);

        if (is_matching_enabled()) {
            auto stop_trigger_price = orderbook.template get_market_price<side>();

            constexpr auto should_trigger = [&] {
                if constexpr (side == OrderSide::BUY) return stop_trigger_price >= order.stop_price();
                return stop_trigger_price <= order.stop_price();
            }();

            if (should_trigger) {
                order.mark_triggered();
                event_handler().template on_stop_limit_order_trigger<type, side>(order);

                match_limit(orderbook, order);
                try_add_limit_order<type, side>(orderbook, std::forward<T>(order));

                perform_post_order_processing(orderbook);

                return;
            }
        }

        if (!order.is_fully_filled()) {
            if constexpr (type == OrderType::STOP_LIMIT) {
                orderbook.template add_order<LevelsType::STOP, side>(std::forward<T>(order));
            } else {
                orderbook.template add_order<LevelsType::TRAILING_STOP, side>(std::forward<T>(order));
            }
        } else {
            event_handler().template on_stop_limit_order_remove<type, side>(order);
        }

        // TODO should this be here or after adding the order to the orderbook only?
        perform_post_order_processing(orderbook);
    }

    constexpr void reduce_order(const OrderId id, const Quantity quantity) noexcept {
        // TODO extract the duplication between this and reduce_order, modify_order, remove_order, and replace_order
        assert(quantity.value > 0 && "Quantity must be greater than zero");
        auto [order, orderbook] = get_order_and_orderbook(id);
        // This will do the reporting and the removal of the orders from the hash map if needed
        auto reduce_func = [&] <OrderType type, OrderSide side> {
            orderbook.template reduce_order<type, side>(order, quantity);
        };
        RESOLVE_TYPE_AND_SIDE_THEN_CALL(order, reduce_func);
    }

    constexpr void reduce_order_recursive(const OrderId id, const Quantity quantity) noexcept {
        reduce_order(id, quantity);
        perform_post_order_processing();
    }

    constexpr void modify_order(const OrderId id, const Quantity quantity, const Price price, const bool mitigate) noexcept {
        // TODO extract the duplication between this and reduce_order, modify_order, remove_order, and replace_order
        assert(quantity.value > 0 && "Quantity must be greater than zero");
        auto [order, orderbook] = get_order_and_orderbook(id);
        // This will do the reporting and the removal of the orders from the hash map if needed
        auto modify_func = [&] <OrderType type, OrderSide side> {
            orderbook.template modify_order<type, side>(order, quantity, price, mitigate);
        };
        RESOLVE_TYPE_AND_SIDE_THEN_CALL(order, modify_func);

        perform_post_order_processing(orderbook);
    }

    constexpr void remove_order(const OrderId id) noexcept {
        // TODO extract the duplication between this and reduce_order, modify_order, remove_order, and replace_order
        // This will do the reporting and the removal of the orders from the hash map
        auto [order, orderbook] = get_order_and_orderbook(id);
        auto remove_func = [&] <OrderType type, OrderSide side> {
            orderbook.template remove_order<type, side>(order);
        };
        RESOLVE_TYPE_AND_SIDE_THEN_CALL(order, remove_func);
    }

    template <typename T>
    constexpr void replace_order(const OrderId id, T&& new_order) {
        // Replace atomically. Since the matching engine is single-threaded,
        //  it can do it by performing remove then add, without worrying
        //  about other operations happening between remove and add
        // TODO have the event handler report replacement instead of removal and addition
        // TODO extract the duplication between this and reduce_order, modify_order, remove_order, and replace_order
        auto [order, orderbook] = get_order_and_orderbook(id);
        // This will do the reporting and the removal and the addition of the orders from the hash map
        auto replace_func = [&] <OrderType type, OrderSide side> {
            orderbook.template replace_order<type, side>(order, std::forward<T>(new_order));
        };
        RESOLVE_TYPE_AND_SIDE_THEN_CALL(order, replace_func);
    }

    constexpr void execute_order(const OrderId id, const Quantity quantity) noexcept {
        auto [order, orderbook] = get_order_and_orderbook(id);

        auto func = [&] <OrderType type, OrderSide side> {
            quantity = std::min(quantity, order.leaves_quantity());

            event_handler().on_order_execute(order, quantity);

            orderbook.template update_last_price<side>(order.price());
            orderbook.template update_last_matching_price<side>(order.price());
        };

        RESOLVE_TYPE_AND_SIDE_THEN_CALL(order, func);
    }

private:

    constexpr std::tuple<std::reference_wrapper<Order>, std::reference_wrapper<OrderBook>> get_order_and_orderbook(OrderId id) {
        assert(orders().contains(id) && "Order with the given ID doesn't exists in the matching engine");
        auto order = &order_by_id(id);
        auto orderbook = &orderbook_at(order.symbol_id());
        return std::make_tuple(std::ref(order), std::ref(orderbook));
    }

    template <template <OrderType, OrderSide> typename Func>
    constexpr auto&& resolve_type_and_side_then_call(Order& order) noexcept {
        constexpr auto type = [&] {
            switch (order.type()) {
                case OrderType::MARKET: return OrderType::MARKET;
                case OrderType::LIMIT: return OrderType::LIMIT;
                case OrderType::STOP: return OrderType::STOP;
                case OrderType::TRAILING_STOP: return OrderType::TRAILING_STOP;
                case OrderType::STOP_LIMIT: return OrderType::STOP_LIMIT;
                case OrderType::TRAILING_STOP_LIMIT: return OrderType::TRAILING_STOP_LIMIT;
                case OrderType::TRIGGERED_STOP: return OrderType::TRIGGERED_STOP;
                case OrderType::TRIGGERED_STOP_LIMIT: return OrderType::TRIGGERED_STOP_LIMIT;
                case OrderType::TRIGGERED_TRAILING_STOP: return OrderType::TRIGGERED_TRAILING_STOP;
                default: ;
            }
            return OrderType::TRIGGERED_TRAILING_STOP_LIMIT;
        }();

        constexpr auto side = [&] {
            if (order.side() == OrderSide::BUY) return OrderSide::BUY;
            return OrderSide::SELL;
        }();

        return Func<type, side> { } ();
    }

    using OrderIterator = typename OrderBook::OrderIterator;

    constexpr void match(OrderBook& orderbook) {

    }

    constexpr void match_limit(OrderBook& orderbook, Order& order) {

    }

    template <OrderType type, OrderSide side, concepts::Order T>
    constexpr void try_add_limit_order(OrderBook& orderbook, T&& order) {
        if (!order.is_fully_filled() && !order.is_ioc() && !order.is_fok()) {
            constexpr auto level = [] {
                // The order is important here because a trailing stop is a stop as well
                if constexpr (type == OrderType::LIMIT) return LevelsType::PRICE;
                else if constexpr (is_trailing(type)) return LevelsType::TRAILING_STOP;
                return LevelsType::STOP;
            }();
            orderbook.template add_order<level, side>(std::forward<T>(order));
        } else {
            event_handler().template on_limit_order_remove<side>(order);
        }
    }

    constexpr void perform_post_order_processing(OrderBook& orderbook) {
        if (is_matching_enabled()) {
            match(orderbook);
        }

        orderbook.reset_matching_prices();
    }

    constexpr auto& orderbook_at(SymbolId id) noexcept {
        assert(id.value < orderbooks().size() && "No orderbook with the given ID exists in the matching engine");
        return orderbooks()[id];
    }

    constexpr bool is_symbol_taken(Symbol symbol) const noexcept {
        auto id = symbol.id.value;
        return id < orderbooks().size() && orderbooks()[id].symbol_id() != SymbolId::invalid();
    }

    constexpr auto& orders() noexcept { return _orders; }

    constexpr auto& order_by_id(OrderId id) noexcept { return orders()[id]; }

    EventHandler& event_handler() noexcept { return _event_handler; }
    std::vector<OrderBook>& orderbooks() noexcept { return _orderbooks; }

    bool _is_matching_enabled = true;

    EventHandler _event_handler;
    std::vector<OrderBook> _orderbooks;

    HashMap<OrderId, OrderIterator> _orders { };
};

}
