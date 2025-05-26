#pragma once

#include <unordered_map>

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

template <typename Key, typename Value>
using unordered_map = std::unordered_map<Key, Value>;

template <
    concepts::Order Order = Order,
    concepts::EventHandler<OrderType> EventHandler = handlers::NullEventHandler,
    concepts::OrderBook OrderBook = OrderBook<Order, EventHandler>,
    template <typename, typename> typename HashMap = unordered_map
>
class MatchingEngine {

    // TODO mark methods that allocate noexcept conditionally, conditioned that the allocation does throw or not

    enum class StopOrdersAction {
        TRIGGERED,
        NOT_TRIGGERED
    };

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

        // Since this is a single-threaded engine, order additions will take place only
        //  after no current possible matches. This means that we don't need to respect
        //  the time priority here. If the order is to be filled (possibly partially),
        //  then its price level is better than any order sitting in the orderbook.
        //  Otherwise, it'll not execute and will be put at the end of the queue
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
                if constexpr (side == OrderSide::BUY)
                    return stop_trigger_price >= order.stop_price();
                return stop_trigger_price <= order.stop_price();
            }();

            if (should_trigger) {
                order.mark_triggered();
                event_handler().template on_stop_limit_order_trigger<type, side>(order);

                match_order<side>(orderbook, order);
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

    // TODO make versions that take the orderbook and the order directly, and refactor

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

    constexpr void execute_order(const OrderId id, const Quantity quantity, Price price) noexcept {
        return execute_order<false>(id, quantity, price);
    }

    constexpr void execute_order(const OrderId id, const Quantity quantity) noexcept {
        return execute_order<true>(id, quantity, Price::invalid());
    }

    constexpr void match() noexcept {
        // TODO store valid orderbook symbol IDs in a set instead of trying all IDs?
        for (auto& orderbook : orderbooks()) {
            if (is_symbol_taken(orderbook.symbol_id())) {
                // A valid orderbook
                match(orderbook);
            }
        }
    }

private:

    template <bool use_order_price>
    constexpr void execute_order(const OrderId id, const Quantity quantity, Price price) noexcept {
        auto [order, orderbook] = get_order_and_orderbook(id);

        if constexpr (use_order_price) {
            price = order.price();
        }

        auto func = [&] <OrderType type, OrderSide side> {
            quantity = std::min(quantity, order.leaves_quantity());

            event_handler().on_order_execute(order, quantity);

            // TODO add it to orderbook.execute_quantity()
            orderbook.template update_last_matching_price<side>(price);

            orderbook.template execute_quantity<type, side>(order, quantity, price);
        };

        RESOLVE_TYPE_AND_SIDE_THEN_CALL(order, func);

        perform_post_order_processing(orderbook);
    }

    constexpr std::tuple<std::reference_wrapper<Order>, std::reference_wrapper<OrderBook>> get_order_and_orderbook(OrderId id) {
        assert(orders().contains(id) && "Order with the given ID doesn't exists in the matching engine");
        auto order = &order_by_id(id);
        auto orderbook = &orderbook_at(order.symbol_id());
        return std::make_pair(std::ref(order), std::ref(orderbook));
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
            if (order.side() == OrderSide::BUY)
                return OrderSide::BUY;
            return OrderSide::SELL;
        }();

        return Func<type, side> { } ();
    }

    using OrderIterator = typename OrderBook::OrderIterator;

    constexpr void match(OrderBook& orderbook) {
        while (true) {
            while (!orderbook.bids().is_empty() & !orderbook.asks().is_empty()) {
                // Maintain price-time priority
                auto& [bid_price, bid_level] = orderbook.bids().best();
                auto& [ask_price, ask_level] = orderbook.asks().best();

                // No orders to match
                if (bid_price < ask_price) break;

                auto bid_it = bid_level.begin();
                auto ask_it = ask_level.begin();

                // If both of them are AONs, then the first branch will be taken.
                // TODO In case both of them are AONs, should we favor (execute
                //  with the price of) the bid order or the ask order?
                if (bid_it->is_aon()) {
                    match_aon(bid_it, ask_level, bid_level, orderbook);
                } else if (ask_it->is_aon()) {
                    match_aon(ask_it, ask_level, bid_level, orderbook);
                } else {
                    // If the quantity is the same, it doesn't matter which branch is executed.
                    // The point here is that if the quantities are not equal, then there will
                    //  be an order which will execute (get fully filled and deleted), and the
                    //  other will be partially filled (reduced). Determine which side is
                    //  executed and which side is reduced.
                    if (bid_it->leaves_quantity() > ask_it->leaves_quantity()) {
                        match_orders<OrderSide::BUY>(bid_it, ask_it, orderbook);
                    } else {
                        match<OrderSide::SELL>(ask_it, bid_it, orderbook);
                    }
                }

                // At this point, we must've had executed at least one order. Check for stop orders
                try_trigger_stop_orders<OrderSide::BUY>(orderbook, bid_level);
                try_trigger_stop_orders<OrderSide::SELL>(orderbook, ask_level);
            }

            // Trailing stop orders modify the stop price, and this is not for free.
            //  Only after all limit and stop orders have settled down, try triggering
            //  trailing stop limit orders. If none is triggered, then we're done.
            if (!try_trigger_stop_orders(orderbook))
                break;
        }
    }

    template <typename T, typename U>
    constexpr void match_aon(OrderIterator& aon_order, T& bid_level, U& ask_level, OrderBook& orderbook) noexcept {
        Quantity chain_volume = calculate_matching_chain(orderbook, bid_level, ask_level);
        if (chain_volume.value == 0) return;
        Price price = aon_order->price();
        execute_matching_chain(orderbook, bid_level, price, chain_volume);
        execute_matching_chain(orderbook, ask_level, price, chain_volume);
    }

    template <OrderSide executing_side>
    constexpr void match_orders(OrderIterator executing_order, OrderIterator& reducing_order, OrderBook& orderbook) noexcept {
        constexpr auto reducing_side = [&] {
            if constexpr (executing_side == OrderSide::BUY)
                return OrderSide::SELL;
            return OrderSide::BUY;
        }();

        Quantity quantity = executing_order->leaves_quantity();
        // TODO should the price be of an arbitrary side?
        Price price = executing_order->price();

        event_handler().on_order_match(*executing_order, *reducing_order);

        // TODO include them in orderbook
        orderbook.template update_last_matching_price<executing_side>(price);
        orderbook.template update_last_matching_price<reducing_side>(price);

        orderbook.execute_quantity(*executing_order, quantity);
        orderbook.execute_quantity(*reducing_order , quantity);
    }

    template <OrderSide side>
    constexpr void match_order(OrderBook& orderbook, Order& order) noexcept {
        auto& opposite = [&] {
            if constexpr (side == OrderSide::BUY)
                return orderbook.asks();
            return orderbook.bids();
        }();
        while (!opposite.is_empty()) {
            auto& [price, level] = opposite.best();

            // Make sure there are crossed orders first
            if (!prices_cross<side>(order.price(), price)) break;

            // No short-circuit behavior
            if (order.is_fok() | order.is_aon()) {
                return try_match_aon<side>(orderbook, order, level);
            }

            match_order_with_level<side>(orderbook, order, level);
        }
    }

    template <OrderSide order_side, typename T>
    constexpr void match_order_with_level(OrderBook& orderbook, Order& _order, T& level) noexcept {
        // TODO refactor this method

        constexpr auto opposite_side = [] {
            if constexpr (order_side == OrderSide::BUY)
                return OrderSide::SELL;
            return OrderSide::BUY;
        }();

        auto* order = &_order;

        // TODO should we traverse through the next pointer instead of always getting the best?
        while (!level.is_empty()) {
            // Either this order is fully "executing" or the target order `order` is executing.
            // It can happen that both of them are fully executed if the leaves_quantity of them
            //  are equal. Otherwise, the non-executing order will just be reduced (execute partially)
            auto executing_it = level.best();
            auto* executing = &(*executing_it);

            // Special case for All-Or-None orders. The case of `order.is_aon()` is handled before
            //  calling this function, but we must check the other end (executing_it) as well.
            // No short-circuit behavior

            auto order_has_less_quantity = order->leaves_quantity() < executing->leaves_quantity();

            if (order_has_less_quantity) {
                if (executing->is_aon()) {
                    // No more orders can be matched for now, respecting the time priority
                    return;
                } else {
                    std::swap(order, executing);
                }
            }

            auto price = executing->price();  // TODO or order->price()?

            Quantity quantity = Quantity::invalid();
            if (order_has_less_quantity) {
                // `order` is the actual executing order
                event_handler().on_order_match<order_side>(*order, *executing);
                quantity = order->leaves_quantity();
            } else {
                event_handler().on_order_match<opposite_side>(*executing, *order);
                quantity = executing->leaves_quantity();
            }

            orderbook.execute_quantity(*order       , quantity);
            orderbook.execute_quantity(*executing_it, quantity);

            if (order_has_less_quantity) {
                // The order has executed successfully
                return;
            }
        }
    }

    template <OrderSide side>
    constexpr void match_market_order(OrderBook& orderbook, Order& order) noexcept {
        // TODO Overwriting the price takes away the
        //  ability to have historical data about the
        //  original price. keep track of the original
        //  price
        auto& opposite = [&] {
            if constexpr (side == OrderSide::BUY)
                return orderbook.asks();
            return orderbook.bids();
        }();

        if (opposite.is_empty()) return;

        auto& [price, _] = opposite.best();
        order.set_price(price);
        order.template add_slippage<side>();

        match_order<side>(orderbook, order);
    }

    template <OrderType type, OrderSide side, concepts::Order T>
    constexpr void try_add_limit_order(OrderBook& orderbook, T&& order) {
        // No need for short-circuit behavior here because the checks are simple, and
        //  having short-circuit behavior would be compiled to multiple branches, making
        //  it harder for the branch predictor to do its job
        if (!order.is_fully_filled() & !order.is_ioc() & !order.is_fok()) {
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

    template <typename First, typename... Rest>
    constexpr bool is_any_triggered(First&& first, Rest&&... rest) const noexcept {
        if constexpr (sizeof...(rest) == 0) return first == StopOrdersAction::TRIGGERED;
        return (first == StopOrdersAction::TRIGGERED) | is_any_triggered<rest...>();
    }

    constexpr StopOrdersAction activate_stop_orders(OrderBook& orderbook) noexcept {
        // TODO refactor this method
        auto result = StopOrdersAction::NOT_TRIGGERED;

        while (true) {
            auto ask_price = orderbook.template get_market_price<OrderSide::SELL>();
            auto a = activate_stop_orders(orderbook, orderbook.template levels<LevelsType::STOP>().bids(), ask_price);
            auto b = activate_stop_orders(orderbook, orderbook.template levels<LevelsType::TRAILING_STOP>().bids(), ask_price);
            recalculate_trailing_stop_price(orderbook);

            auto bid_price = orderbook.template get_market_price<OrderSide::BUY>();
            auto c = activate_stop_orders(orderbook, orderbook.template levels<LevelsType::STOP>().asks(), bid_price);
            auto d = activate_stop_orders(orderbook, orderbook.template levels<LevelsType::TRAILING_STOP>().asks(), bid_price);
            recalculate_trailing_stop_price(orderbook);

            constexpr auto triggered = is_any_triggered(a, b, c, d);
            if (triggered) {
                result = StopOrdersAction::TRIGGERED;
            } else {
                break;
            }
        }

        return result;
    }

    template <OrderSide level_side, typename T>
    constexpr StopOrdersAction activate_stop_orders(OrderBook& orderbook, T& level, Price level_price, Price stop_price) noexcept {
        bool should_trigger = [&] {
            if constexpr (level_side == OrderSide::BUY) {
                return stop_price <= level_price;
            }
            return stop_price >= level_price;
        };

        if (!should_trigger | level.is_empty()) return StopOrdersAction::NOT_TRIGGERED;

        while (!level.is_empty()) {
            auto order_it = level.best();
            switch (order_it->type()) {
                case OrderType::STOP:
                    activate_stop_order<OrderType::STOP, level_side>(orderbook, *order_it, level_price);
                    break;
                case OrderType::TRAILING_STOP:
                    activate_stop_order<OrderType::TRAILING_STOP, level_side>(orderbook, *order_it, level_price);
                    break;
                case OrderType::STOP_LIMIT:
                    activate_stop_limit_order<OrderType::STOP_LIMIT, level_side>(orderbook, *order_it, level_price);
                    break;
                case OrderType::TRAILING_STOP_LIMIT:
                    activate_stop_limit_order<OrderType::TRAILING_STOP_LIMIT, level_side>(orderbook, *order_it, level_price);
                    break;
                default:
                    assert(false && "Unsupported order type");
            }
        }

        return StopOrdersAction::TRIGGERED;
    }

    template <OrderType type, OrderSide side>
    constexpr void activate_stop_order(OrderBook& orderbook, Order& order) noexcept {
        order.mark_triggered();
        // TODO remove this and have a way to handle triggered stop orders accordingly
        if (order.time_in_force() != TimeInForce::FOK) {
            order.set_time_in_force(TimeInForce::IOC);
        }

        event_handler().on_stop_order_trigger(order);

        match_market_order<side>(orderbook, order);

        event_handler().on_delete_order(order);

        orders().erase(order.id());

        // Remove only after we're done using it
        // Remove the order from its stop order level and create a market
        orderbook.template remove_order<type, side>(order);
    }

    template <OrderType type, OrderSide side>
    constexpr void activate_stop_limit_order(OrderBook& orderbook, Order& order) noexcept {
        // Unlink the order from its stop order level and link it into a limit order level
        orderbook.template unlink_order<type, side>(order);

        order.mark_triggered();

        event_handler().on_stop_limit_order_trigger(order);

        add_limit_order<type, side>(orderbook, order);
    }

    constexpr Quantity calculate_matching_chain_quantity(OrderIterator order_it, Quantity needed) noexcept {
        auto quantity = order_it->leaves_quantity();
        if (order_it->is_aon() | (needed < quantity)) {
            // quantity = min(order_it->leaves_quantity(), needed);
            // BUT, if the order is All-Or-None, then we must either
            //  match it as a whole or not proceed at all
            quantity = needed;
        }
        return quantity;
    }

    template <OrderSide level_side, typename T>
    constexpr Quantity calculate_matching_chain(OrderBook& orderbook, Price price, Quantity required) noexcept {
        Quantity available = Quantity { 0 };

        auto& levels = orderbook.template levels<level_side>();

        for (auto it = levels.begin(); it != levels.end(); ++it) {

            auto& [level_price, level] = *it;

            if (!prices_cross<level_side>(level_price, price)) return Quantity { 0 };

            while (!level.is_empty()) {
                auto order_it = level.best();

                auto needed = required - available;
                auto quantity = calculate_matching_chain_quantity(order_it, needed);
                available += quantity;

                if (available == required) {
                    return available;
                }

                if (available > required) {
                    // If it happens that we must match more than the
                    // intended volume, then it's not possible to match
                    // exactly the intended volume. This can happen in
                    // the presence of AON orders
                    return Quantity { 0 };
                }
            }
        }
    }

    constexpr Quantity calculate_matching_chain(OrderBook& orderbook) noexcept {
        auto& [bid_price, bid_level] = *orderbook.bids().begin();
        auto& [ask_price, ask_level] = *orderbook.asks().begin();

        // There is a "longer" and a "shorter" order. In case both are
        //  AON or both are not AON, then the longer is the one that
        //  has more quantity. Otherwise, the longer is the AON order
        auto [shorter_level, longer_level] = [&] {
            auto bid_it = bid_level.begin();
            auto ask_it = ask_level.begin();
            bool bid_is_longer = bid_it->leaves_quantity() > ask_it->leaves_quantity();
            if (bid_it->is_aon() & (bid_is_longer | !ask_it->is_aon())) {
                return std::make_pair(ask_level, bid_level);
            }
            return std::make_pair(bid_level, ask_level);
        }();

        auto longer_order = longer_level.begin();

        // TODO get rid of the end() iterator (by comparing to nullptr or something)
        auto shorter_end = shorter_level.end();
        auto longer_end = longer_level.end();

        auto available = Quantity { 0 };
        auto required = longer_order->leaves_quantity();

        // No need to check for longer_level as it's not
        //  going to reach the end before shorter_level
        while (shorter_level != shorter_end) {
            // Same here
            auto shorter_order = shorter_level.begin();
            while (shorter_order != shorter_end) {
                auto needed = required - available;
                auto quantity = calculate_matching_chain_quantity(shorter_order, needed);
                available += quantity;

                if (required == available) {
                    return required;
                }

                // Longer has become shorter
                if (required < available) {
                    std::swap(shorter_order, longer_order);
                    std::swap(shorter_level, longer_level);
                    std::swap(shorter_end, longer_end);

                    shorter_order = shorter_level->next(shorter_level);
                    std::swap(required, available);
                    continue;
                }

                shorter_order = shorter_level->next(shorter_level);
            }

            ++shorter_level;
        }

        return Quantity { 0 };
    }

    template <OrderSide side>
    constexpr void execute_matching_chain(OrderBook& orderbook, Price price, Quantity volume) noexcept {
        auto levels = [&] {
            if constexpr (side == OrderSide::BUY)
                return orderbook.bids();
            return orderbook.asks();
        }();

        // TODO Should we say while (!levels.is_empty())?
        auto level_it = levels.begin();
        while ((level_it != levels.end()) & (volume > Quantity { 0 })) {
            auto& [level_price, level] = *level_it;
            auto order_it = level.begin();

            while (order_it != level.end()) {
                auto quantity = volume;
                if (order_it->is_aon() | (order_it->leaves_quantity() < volume))
                    quantity = order_it->leaves_quantity();

                event_handler().on_execute_order(*order_it, price, quantity);

                // TODO remove this and include it in the reduce order
                orderbook.update_last_matching_price<side>(price);

                reduce_order(*order_it, quantity);

                volume -= quantity;

                order_it = level.next(order_it);
            }

            ++level_it;
        }
    }

    template <OrderType type, OrderSide side>
    constexpr void update_trailing_stop_price(OrderBook& orderbook) noexcept {
        auto old_trailing_price = orderbook.template get_trailing_stop_price<side>();
        auto new_trailing_price = orderbook.template get_market_price<side>();
        orderbook.template set_trailing_stop_price<side>(new_trailing_price);
        if constexpr (side == OrderSide::BUY) {
            if (new_trailing_price <= old_trailing_price) return;
        } else {
            if (new_trailing_price >= old_trailing_price) return;
        }

        auto levels = orderbook.template levels<LevelsType::TRAILING_STOP, side>();
        auto prev_level_it = levels.begin();
        for (auto level_it = levels.begin(); level_it != levels.end(); ) {
            bool updated = false;
            auto& [price, level] = *level_it;
            for (auto order_it = level.begin(); order_it != level.end(); ++order_it) {
                auto old_stop_price = order_it->stop_price();
                auto new_stop_price = orderbook.calculate_trailing_stop_price(*order_it);

                if (new_stop_price != old_stop_price) {
                    orderbook.template unlink_order<OrderType::TRAILING_STOP, side>(order_it);

                    event_handler().on_update_stop_price(*order_it);

                    if constexpr (type == OrderType::TRAILING_STOP) {
                        order_it->set_stop_price(new_stop_price);
                    } else if constexpr (type == OrderType::TRAILING_STOP_LIMIT) {
                        auto diff = order_it->price() - order_it->stop_price();
                        order_it->set_stop_price(new_stop_price);
                        order_it->set_price(new_stop_price + diff);
                    } else {
                        assert(false && "Unsupported order type");
                    }

                    orderbook.template add_order<LevelsType::TRAILING_STOP, side>(*order_it);

                    updated = true;
                }
            }
            if (updated) {
                level_it = prev_level_it;
            } else {
                // TODO should we really do it this way?
                prev_level_it = level_it;
                ++level_it;
            }
        }
    }

    constexpr void perform_post_order_processing(OrderBook& orderbook) noexcept {
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

    template <OrderSide side1>
    constexpr bool prices_cross(Price p1, Price p2) {
        if constexpr (side1 == OrderSide::BUY) {
            return p1 >= p2;
        } else {
            return p1 <= p2;
        }
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
