#pragma once

#include <unordered_map>

#include <chronex/Symbol.hpp>

#include <chronex/concepts/OrderBook.hpp>
#include <chronex/concepts/EventHandler.hpp>

#include <chronex/handlers/NullEventHandler.hpp>

#include <chronex/orderbook/Order.hpp>
#include <chronex/orderbook/OrderBook.hpp>
#include <chronex/orderbook/OrderUtils.hpp>

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

    using OrderIterator = typename OrderBook::OrderIterator;

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
        add_existing_orderbook(OrderBook { &orders(), symbol, &event_handler() }, false);
    }

    template <typename T>
    constexpr void add_existing_orderbook(T&& orderbook, const bool report = true) {
        assert(!is_symbol_taken(orderbook.symbol()) &&
            "Symbol with the same ID already exists in the matching engine");

        auto symbol = orderbook.symbol();
        auto id = symbol.id;

        if (orderbooks().size() <= id.value) {
            orderbooks().resize(id.value + 1);
        }

        // TODO remove this conditional
        if (report) {
            event_handler().on_add_orderbook(symbol);
        }

        orderbook_at(id) = std::forward<T>(orderbook);
    }

    constexpr void remove_orderbook(Symbol symbol) noexcept {
        assert(is_symbol_taken(symbol) && "No symbol with the given ID exists in the matching engine");

        auto& orderbook = orderbook_at(symbol.id);

        event_handler().on_remove_orderbook(orderbook);

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
        // Triggered types shouldn't be passed to this function.
        //  This function resolves newly added orders. Being
        //  triggered means it was just being processed,
        //  and the type information should be present by now.
        switch (order.type()) {
            case OrderType::MARKET:
                return add_order<side, OrderType::MARKET>(std::forward<T>(order));
            case OrderType::LIMIT:
                return add_order<side, OrderType::LIMIT>(std::forward<T>(order));
            case OrderType::STOP:
                return add_order<side, OrderType::STOP>(std::forward<T>(order));
            case OrderType::TRAILING_STOP:
                return add_order<side, OrderType::TRAILING_STOP>(std::forward<T>(order));
            case OrderType::STOP_LIMIT:
                return add_order<side, OrderType::STOP_LIMIT>(std::forward<T>(order));
            case OrderType::TRAILING_STOP_LIMIT:
                return add_order<side, OrderType::TRAILING_STOP_LIMIT>(std::forward<T>(order));
            default:
                assert(false && "Wrong order type to use here");
        }
    }

    template <OrderSide side, OrderType type, concepts::Order T>
    constexpr void add_order(T&& order) {
        assert(order.is_valid() && "Order is invalid");

        if constexpr (is_market(type)) {
            return add_market_order<side>(std::forward<T>(order));
        } else if constexpr (is_limit(type)) {
            return add_limit_order<side>(std::forward<T>(order));
        } else if constexpr (type == OrderType::STOP) {
            return add_stop_order<OrderType::STOP, side>(std::forward<T>(order));
        } else if constexpr (type == OrderType::TRAILING_STOP) {
            return add_stop_order<OrderType::TRAILING_STOP, side>(std::forward<T>(order));
        } else if constexpr (type == OrderType::STOP_LIMIT) {
            return add_stop_limit_order<OrderType::STOP_LIMIT, side>(std::forward<T>(order));
        } else if constexpr (type == OrderType::TRAILING_STOP_LIMIT) {
            return add_stop_limit_order<OrderType::TRAILING_STOP_LIMIT, side>(std::forward<T>(order));
        } else {
            assert(false && "Wrong order type to use here");
        }
    }

    template <OrderSide side, concepts::Order T>
    constexpr void add_market_order(T&& order) {
        auto& orderbook = orderbook_at(order.symbol_id());

        event_handler().template on_add_order<OrderType::MARKET, side>(orderbook, order);

        if (is_matching_enabled())
            match_market_order<side>(orderbook, order);

        event_handler().template on_remove_order<OrderType::MARKET, side>(orderbook, order);

        perform_post_order_processing(orderbook);
    }

    template <OrderSide side, concepts::Order T>
    constexpr void add_limit_order(T&& order) {
        auto& orderbook = orderbook_at(order.symbol_id());

        event_handler().template on_add_order<OrderType::LIMIT, side>(orderbook, order);

        // Since this is a single-threaded engine, order additions will take place only
        //  after no current possible matches. This means that we don't need to respect
        //  the time priority here. If the order is to be filled (possibly partially),
        //  then its price level is better than any order sitting in the orderbook.
        //  Otherwise, it'll not execute and will be put at the end of the queue
        if (is_matching_enabled()) {
            match_limit_order<side>(orderbook, order);
        }

        try_add_limit_order<OrderType::LIMIT, side>(orderbook, std::forward<T>(order));

        // TODO perform this only if the order is added?
        perform_post_order_processing(orderbook);
    }

    template <OrderType type, OrderSide side, concepts::Order T>
    constexpr void add_stop_order(T&& order) {
        auto& orderbook = orderbook_at(order.symbol_id());

        if constexpr (type == OrderType::TRAILING_STOP) {
            order.set_stop_price(orderbook.template calculate_trailing_stop_price<side>(order));
        }

        event_handler().template on_add_order<type, side>(orderbook, order);

        if (int(is_matching_enabled()) & int(should_trigger<side>(orderbook, order))) {
            return trigger_new_stop_order<type, side>(orderbook, std::forward<T>(order));
        }

        insert_stop_order<type, side>(orderbook, std::forward<T>(order));

        // TODO perform this only if the order is added?
        perform_post_order_processing(orderbook);
    }

    template <OrderType type, OrderSide side, concepts::Order T>
    constexpr void add_stop_limit_order(T&& order) {
        // TODO refactor similarities with add_stop_order
        auto& orderbook = orderbook_at(order.symbol_id());

        if constexpr (type == OrderType::TRAILING_STOP_LIMIT) {
            auto trailing_stop_price = orderbook.template calculate_trailing_stop_price<side>(order);
            order.set_stop_and_trailing_stop_prices(trailing_stop_price);
        }

        event_handler().template on_add_order<type, side>(orderbook, order);

        if (int(is_matching_enabled()) & int(should_trigger<side>(orderbook, order))) {
            return trigger_new_stop_order<type, side>(orderbook, std::forward<T>(order));
        }

        insert_stop_order<type, side>(orderbook, std::forward<T>(order));

        // TODO should this be here or after adding the order to the orderbook only?
        perform_post_order_processing(orderbook);
    }

# define ADD_ORDERBOOK_OP_PROXY(OP_NAME) \
    template <typename... Args>                                                                                 \
    constexpr void OP_NAME(const OrderId id, Args&&... args) noexcept {                                         \
        auto [order_it, orderbook] = get_order_and_orderbook(id);                                               \
                                                                                                                \
        auto func = [&] <OrderType type, OrderSide side, typename... Args2> (Args2&&... args2) {                \
            OP_NAME<type, side>(orderbook.get(), order_it, std::forward<Args2>(args2)...);                      \
        };                                                                                                      \
                                                                                                                \
        resolve_type_and_side_then_call(*order_it, func, std::forward<Args>(args)...);                          \
                                                                                                                \
        perform_post_order_processing(orderbook);                                                               \
    }                                                                                                           \
                                                                                                                \
    template <OrderType type, OrderSide side, typename... Args>                                                 \
    constexpr void OP_NAME(OrderBook& orderbook, OrderIterator order_it, Args&&... args) noexcept {             \
        /* This will do the reporting and the removal of the orders from the hash map if needed */              \
        orderbook.template OP_NAME<type, side>(order_it, std::forward<Args>(args)...);                          \
    }                                                                                                           \

    ADD_ORDERBOOK_OP_PROXY(reduce_order)

    ADD_ORDERBOOK_OP_PROXY(modify_order)

    ADD_ORDERBOOK_OP_PROXY(remove_order)

    ADD_ORDERBOOK_OP_PROXY(replace_order)

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
    constexpr void execute_order(const OrderId id, Quantity quantity, Price price) noexcept {
        auto [order_it, orderbook_ref] = get_order_and_orderbook(id);
        auto& orderbook = orderbook_ref.get();

        if constexpr (use_order_price) {
            price = order_it->price();
        }

        auto func = [&] <OrderType type, OrderSide side> {
            quantity = std::min(quantity, order_it->leaves_quantity());
            orderbook.template execute_quantity<type, side>(order_it, quantity, price);
        };

        resolve_type_and_side_then_call(*order_it, func);

        perform_post_order_processing(orderbook);
    }

    constexpr std::tuple<OrderIterator, std::reference_wrapper<OrderBook>> get_order_and_orderbook(OrderId id) {
        assert(orders().contains(id) && "Order with the given ID doesn't exists in the matching engine");
        auto order_it = order_by_id(id);
        auto& orderbook = orderbook_at(order_it->symbol_id());
        return std::make_pair(order_it, std::ref(orderbook));
    }

    template <typename Func, typename... Args>
    constexpr void resolve_type_and_side_then_call(Order& order, Func&& func, Args&&... args) noexcept {
        #define CASE(TYPE) case OrderType::TYPE: \
            return func.template operator()<OrderType::TYPE, side>(std::forward<Args2>(args2)...)

        auto f = [&] <OrderSide side, typename... Args2> (Args2&&... args2) {
            switch (order.type()) {
                CASE(MARKET);
                CASE(LIMIT);
                CASE(STOP);
                CASE(TRAILING_STOP);
                CASE(STOP_LIMIT);
                CASE(TRAILING_STOP_LIMIT);
                CASE(TRIGGERED_STOP);
                CASE(TRIGGERED_STOP_LIMIT);
                CASE(TRIGGERED_TRAILING_STOP);
                CASE(TRIGGERED_TRAILING_STOP_LIMIT);
                default: assert(false && "Invalid order type");
            }
        };

        if (order.side() == OrderSide::BUY)
            return f.template operator()<OrderSide::BUY>(std::forward<Args>(args)...);

        return f.template operator()<OrderSide::SELL>(std::forward<Args>(args)...);
    }

    constexpr void match(OrderBook& orderbook) {
        while (true) {
            while (int(!orderbook.bids().is_empty()) & int(!orderbook.asks().is_empty())) {
                // Maintain price-time priority
                auto& [bid_price, bid_level] = *orderbook.bids().best();
                auto& [ask_price, ask_level] = *orderbook.asks().best();

                // No orders to match
                if (bid_price < ask_price) break;

                auto bid_it = bid_level.begin();
                auto ask_it = ask_level.begin();

                // If both of them are AONs, then the first branch will be taken.
                // TODO In case both of them are AONs, should we favor (execute
                //  with the price of) the bid order or the ask order?
                // TODO pass the already fetched begin() instead of letting
                //  other methods fetch it again?
                if (bid_it->is_aon()) {
                    match_aon(orderbook, bid_it->price());
                } else if (ask_it->is_aon()) {
                    match_aon(orderbook, ask_it->price());
                } else {
                    // If the quantity is the same, it doesn't matter which branch is executed.
                    // The point here is that if the quantities are not equal, then there will
                    //  be an order which will execute (get fully filled and deleted), and the
                    //  other will be partially filled (reduced). Determine which side is
                    //  executed and which side is reduced.
                    if (bid_it->leaves_quantity() > ask_it->leaves_quantity()) {
                        match_orders<OrderSide::BUY, OrderSide::SELL>(orderbook, bid_it, ask_it);
                    } else {
                        match_orders<OrderSide::SELL, OrderSide::BUY>(orderbook, ask_it, bid_it);
                    }
                }

                // At this point, we must've had executed at least one order. Check for stop orders
                try_trigger_stop_orders<OrderSide::BUY>(orderbook, bid_level, bid_price);
                try_trigger_stop_orders<OrderSide::SELL>(orderbook, ask_level, ask_price);
            }

            // Trailing stop orders modify the stop price, and this is not for free.
            //  Only after all limit and stop orders have settled down, try triggering
            //  trailing stop limit orders. If none is triggered, then we're done.
            if (try_trigger_stop_orders(orderbook) == StopOrdersAction::NOT_TRIGGERED)
                break;
        }
    }

    constexpr void match_aon(OrderBook& orderbook, Price price) noexcept {
        Quantity chain_volume = calculate_matching_chain(orderbook);
        if (chain_volume.value == 0) return;
        execute_matching_chain<OrderSide::BUY>(orderbook, price, chain_volume);
        execute_matching_chain<OrderSide::SELL>(orderbook, price, chain_volume);
    }

    template <OrderSide executing_side, OrderSide reducing_side>
    constexpr void match_orders(OrderBook& orderbook, OrderIterator executing_order, OrderIterator& reducing_order) noexcept {
        Quantity quantity = executing_order->leaves_quantity();
        // TODO should the price be of an arbitrary side?
        Price price = executing_order->price();

        event_handler().template on_match_order<executing_side, reducing_side>(orderbook, *executing_order, *reducing_order);

        orderbook.template execute_quantity<OrderType::LIMIT, executing_side>(executing_order, quantity, price);
        orderbook.template execute_quantity<OrderType::LIMIT, reducing_side >(reducing_order , quantity, price);
    }

    template <OrderSide side>
    constexpr void match_order(OrderBook& orderbook, Order& order) noexcept {
        // TODO remove this nasty hack
        typename OrderBook::LevelQueueDataType temp_queue;
        temp_queue.push_back(std::move(order));
        auto order_it = temp_queue.begin();

        auto& opposite = get_opposite_side<side>(orderbook);

        while (!opposite.is_empty()) {
            auto& [price, level] = *opposite.best();

            // Make sure there are crossed orders first
            if (!prices_cross<side>(order_it->price(), price)) break;

            // No short-circuit behavior
            if (int(order_it->is_fok()) | int(order_it->is_aon())) {
                return try_match_aon<side>(orderbook, order_it, level);
            }

            match_order_with_level<side>(orderbook, order_it, level);
        }

        // TODO remove this
        order = std::move(*order_it);
    }

    template <OrderSide order_side, typename T>
    constexpr void match_order_with_level(OrderBook& orderbook, OrderIterator order_it, T& level) noexcept {
        // TODO refactor this method

        constexpr auto opposite = opposite_side<order_side>();

        // TODO should we traverse through the next pointer instead of always getting the best?
        while (!level.is_empty()) {
            // Either this order is fully "executing" or the target order `order` is executing.
            // It can happen that both of them are fully executed if the leaves_quantity of them
            //  are equal. Otherwise, the non-executing order will just be reduced (execute partially)
            auto executing_it = level.begin();

            // Special case for All-Or-None orders. The case of `order.is_aon()` is handled before
            //  calling this function, but we must check the other end (executing_it) as well.
            // No short-circuit behavior

            auto order_has_less_quantity = order_it->leaves_quantity() < executing_it->leaves_quantity();

            if (order_has_less_quantity) {
                if (executing_it->is_aon()) {
                    // No more orders can be matched for now, respecting the time priority
                    return;
                } else {
                    std::swap(order_it, executing_it);
                }
            }

            // auto price = executing_it->price();  // TODO or order_it->price()?

            Quantity quantity = [&] {
                if (order_has_less_quantity) {
                    // `order` is the actual executing order
                    event_handler().template on_match_order<order_side, opposite>(orderbook, *order_it, *executing_it);
                    return order_it->leaves_quantity();
                }

                event_handler().template on_match_order<opposite, order_side>(orderbook, *executing_it, *order_it);
                return executing_it->leaves_quantity();
            }();

            orderbook.template execute_quantity<OrderType::LIMIT, order_side>(order_it    , quantity);
            orderbook.template execute_quantity<OrderType::LIMIT, opposite>(executing_it, quantity);

            if (order_has_less_quantity) {
                // The order has executed successfully
                return;
            }
        }
    }

    template <OrderSide side>
    static constexpr auto& get_side(OrderBook& orderbook) noexcept {
        return orderbook.template levels<OrderType::LIMIT, side>();
    }

    template <OrderSide side>
    static constexpr auto& get_opposite_side(OrderBook& orderbook) noexcept {
        return get_side<opposite_side<side>()>(orderbook);
    }

    template <OrderSide side>
    static constexpr auto opposite_side() noexcept {
        if constexpr (side == OrderSide::BUY) {
            return OrderSide::SELL;
        } else {
            return OrderSide::BUY;
        }
    }

    template <OrderSide side>
    constexpr void match_market_order(OrderBook& orderbook, Order& order) noexcept {
        // TODO Overwriting the price takes away the
        //  ability to have historical data about the
        //  original price. keep track of the original
        //  price
        auto& opposite = get_opposite_side<side>(orderbook);

        if (opposite.is_empty()) return;

        auto& [price, _] = *opposite.best();
        order.set_price(price);
        order.template add_slippage<side>();

        match_order<side>(orderbook, order);
    }

    template <OrderType type, OrderSide side>
    constexpr bool try_add_limit_order(OrderBook& orderbook, Order&& order) {
        // No need for short-circuit behavior here because the checks are simple, and
        //  having short-circuit behavior would be compiled to multiple branches, making
        //  it harder for the branch predictor to do its job
        if (int(!order.is_fully_filled()) & int(!order.is_ioc()) & int(!order.is_fok())) {
            orderbook.template add_order<type, side>(std::move(order));
            return true;
        } else {
            event_handler().template on_remove_order<OrderType::LIMIT, side>(orderbook, order);
            return false;
        }
    }

    template <OrderType type, OrderSide side>
    constexpr bool try_link_limit_order(OrderBook& orderbook, OrderIterator order_it) {
        // TODO refactor the duplication between this and try_add_limit_order
        // No need for short-circuit behavior here because the checks are simple, and
        //  having short-circuit behavior would be compiled to multiple branches, making
        //  it harder for the branch predictor to do its job
        auto& order = *order_it;
        if (int(!order.is_fully_filled()) & int(!order.is_ioc()) & int(!order.is_fok())) {
            orderbook.template link_order<type, side>(order_it);
            return true;
        } else {
            event_handler().template on_remove_order<OrderType::LIMIT, side>(orderbook, order);
            return false;
        }
    }

    template <typename First, typename... Rest>
    constexpr bool is_any_triggered(First&& first, Rest&&... rest) const noexcept {
        const bool res = first == StopOrdersAction::TRIGGERED;
        if constexpr (sizeof...(Rest) == 0) return res;
        else {
            return bool(
                int(res) |
                int(is_any_triggered(std::forward<Rest>(rest)...))
            );
        }
    }

    template <OrderType type, OrderSide side>
    constexpr auto try_trigger_stop_order_level(OrderBook& orderbook, const Price stop_price) noexcept {
        // TODO rename this function
        auto& levels = orderbook.template bids<type>();
        if (levels.is_empty()) return StopOrdersAction::NOT_TRIGGERED;
        auto& [_, best] = *levels.begin();
        return try_trigger_stop_orders<side>(orderbook, best, stop_price);
    }

    template <OrderSide side>
    constexpr auto try_trigger_stop_orders_side(OrderBook& orderbook, const Price stop_price) noexcept -> std::pair<StopOrdersAction, StopOrdersAction> {
        // TODO rename this function
        auto a = try_trigger_stop_order_level<OrderType::STOP, side>(orderbook, stop_price);
        auto b = try_trigger_stop_order_level<OrderType::TRAILING_STOP, side>(orderbook, stop_price);
        recalculate_trailing_stop_price(orderbook);
        return { a, b };
    }

    constexpr StopOrdersAction try_trigger_stop_orders(OrderBook& orderbook) noexcept {
        auto result = StopOrdersAction::NOT_TRIGGERED;

        while (true) {
            // TODO make sure of sides
            auto ask_price = orderbook.template get_market_price<OrderSide::SELL>();
            auto [a, b] = try_trigger_stop_orders_side<OrderSide::BUY>(orderbook, ask_price);
            auto bid_price = orderbook.template get_market_price<OrderSide::BUY>();
            auto [c, d] = try_trigger_stop_orders_side<OrderSide::SELL>(orderbook, bid_price);

            if (is_any_triggered(a, b, c, d)) {
                result = StopOrdersAction::TRIGGERED;
            } else {
                break;
            }
        }

        return result;
    }

    template <OrderSide level_side, typename T>
    constexpr StopOrdersAction try_trigger_stop_orders(OrderBook& orderbook, T& level, const Price level_price) noexcept {
        constexpr auto opposite = opposite_side<level_side>();
        auto stop_price = orderbook.template get_market_price<opposite>();
        bool should_trigger = prices_cross<level_side>(level_price, stop_price);

        if (int(!should_trigger) | int(level.is_empty())) return StopOrdersAction::NOT_TRIGGERED;

        while (!level.is_empty()) {
            auto order_it = level.begin();
            switch (order_it->type()) {
                case OrderType::STOP:
                    trigger_stop_order<OrderType::STOP, level_side>(orderbook, order_it);
                    break;
                case OrderType::TRAILING_STOP:
                    trigger_stop_order<OrderType::TRAILING_STOP, level_side>(orderbook, order_it);
                    break;
                case OrderType::STOP_LIMIT:
                    trigger_stop_limit_order<OrderType::STOP_LIMIT, level_side>(orderbook, order_it);
                    break;
                case OrderType::TRAILING_STOP_LIMIT:
                    trigger_stop_limit_order<OrderType::TRAILING_STOP_LIMIT, level_side>(orderbook, order_it);
                    break;
                default:
                    assert(false && "Unsupported order type");
            }
        }

        return StopOrdersAction::TRIGGERED;
    }

    template <OrderType type, OrderSide side>
    constexpr void trigger_stop_order(OrderBook& orderbook, OrderIterator order_it) noexcept {
        order_it->template mark_triggered<type>();
        // TODO remove this and have a way to handle triggered stop orders accordingly
        if (order_it->time_in_force() != TimeInForce::FOK) {
            order_it->set_time_in_force(TimeInForce::IOC);
        }

        event_handler().template on_trigger_stop_order<type, side>(orderbook, *order_it);

        // TODO call add_market_order?

        match_market_order<side>(orderbook, *order_it);

        // Remove only after we're done using it
        // Remove the order from its stop order level and create a market
        event_handler().template on_remove_order<type, side>(orderbook, *order_it);
        orders().erase(order_it->id());
        orderbook.template remove_order<type, side>(order_it);
    }

    template <OrderType type, OrderSide side>
    constexpr void trigger_stop_limit_order(OrderBook& orderbook, OrderIterator order_it) noexcept {
        // Unlink the order from its stop order level and link it into a limit order level
        orderbook.template unlink_order<type, side>(order_it);

        order_it->template mark_triggered<type>();

        event_handler().template on_trigger_stop_order<type, side>(orderbook, *order_it);

        match_limit_order<side>(orderbook, *order_it);

        try_link_limit_order<type, side>(orderbook, order_it);
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
        (void)orderbook;
        // auto& [bid_price, bid_level] = *orderbook.bids().begin();
        // auto& [ask_price, ask_level] = *orderbook.asks().begin();
        // // TODO remove this nasty hack
        // auto ask_end = &orderbook.asks().end()->second;
        // auto bid_end = &orderbook.bids().end()->second;
        //
        // // There is a "longer" and a "shorter" order. In case both are
        // //  AON or both are not AON, then the longer is the one that
        // //  has more quantity. Otherwise, the longer is the AON order
        // auto [shorter_level, shorter_level_end, longer_level, longer_level_end] = [&] {
        //     auto bid_it = bid_level.begin();
        //     auto ask_it = ask_level.begin();
        //     bool bid_is_longer = bid_it->leaves_quantity() > ask_it->leaves_quantity();
        //     if (int(bid_it->is_aon()) & (int(bid_is_longer) | int(!ask_it->is_aon()))) {
        //         return std::make_tuple(&ask_level, ask_end, &bid_level, bid_end);
        //     }
        //     return std::make_tuple(&bid_level, bid_end, &ask_level, ask_end);
        // }();
        //
        // auto longer_order = longer_level->begin();
        //
        // // TODO get rid of the end() iterator (by comparing to nullptr or something)
        // auto shorter_end = shorter_level->end();
        // auto longer_end = longer_level->end();
        //
        // auto available = Quantity { 0 };
        // auto required = longer_order->leaves_quantity();
        //
        // // No need to check for longer_level as it's not
        // //  going to reach the end before shorter_level
        // while (shorter_level != shorter_end) {
        //     // Same here
        //     auto shorter_order = shorter_level.begin();
        //     while (shorter_order != shorter_end) {
        //         auto needed = required - available;
        //         auto quantity = calculate_matching_chain_quantity(shorter_order, needed);
        //         available += quantity;
        //
        //         if (required == available) {
        //             return required;
        //         }
        //
        //         // Longer has become shorter
        //         if (required < available) {
        //             std::swap(shorter_order, longer_order);
        //             std::swap(shorter_level, longer_level);
        //             std::swap(shorter_end, longer_end);
        //
        //             shorter_order = shorter_level->next(shorter_level);
        //             std::swap(required, available);
        //             continue;
        //         }
        //
        //         shorter_order = shorter_level->next(shorter_level);
        //     }
        //
        //     ++shorter_level;
        // }

        return Quantity { 0 };
    }

    template <OrderSide side>
    constexpr void execute_matching_chain(OrderBook& orderbook, Price price, Quantity volume) noexcept {
        auto& levels = get_side<side>(orderbook);

        // TODO Should we say while (!levels.is_empty())?
        auto level_it = levels.begin();
        while (int(level_it != levels.end()) & int(volume > Quantity { 0 })) {
            auto& [level_price, level] = *level_it;
            auto order_it = level.begin();

            while (order_it != level.end()) {
                auto quantity = volume;
                if (int(order_it->is_aon()) | int(order_it->leaves_quantity() < volume)) {
                    quantity = order_it->leaves_quantity();
                }

                event_handler().template on_execute_order<side>(*order_it, price, quantity);

                // TODO remove this and include it in the reduce order
                orderbook.template update_last_matching_price<side>(price);

                reduce_order<OrderType::LIMIT, side>(orderbook, order_it, quantity);

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

        auto& levels = orderbook.template levels<LevelsType::TRAILING_STOP, side>();
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
        // TODO check where this is added redundently
        if (is_matching_enabled()) {
            match(orderbook);
        }

        orderbook.reset_matching_prices();
    }

    template <OrderSide side>
    [[nodiscard]] constexpr bool should_trigger(OrderBook& orderbook, Order& order) const noexcept {
        constexpr auto opposite = opposite_side<side>();
        auto stop_trigger_price = orderbook.template get_market_price<opposite>();

        if constexpr (side == OrderSide::BUY)
            return stop_trigger_price >= order.stop_price();
        return stop_trigger_price <= order.stop_price();
    }

    [[nodiscard]] constexpr auto& orderbook_at(SymbolId id) noexcept {
        assert(id.value < orderbooks().size() && "No orderbook with the given ID exists in the matching engine");
        return orderbooks()[id.value];
    }

    [[nodiscard]] constexpr bool is_symbol_taken(uint64_t id) const noexcept {
        return id < orderbooks().size() && orderbooks()[id].is_valid();
    }

    [[nodiscard]] constexpr bool is_symbol_taken(const Symbol symbol) const noexcept {
        return is_symbol_taken(symbol.id.value);
    }

    template <OrderSide side1>
    constexpr bool prices_cross(Price p1, Price p2) {
        if constexpr (side1 == OrderSide::BUY) {
            return p1 >= p2;
        } else {
            return p1 <= p2;
        }
    }

    template <typename Self>
    constexpr auto& orders(this Self&& self) noexcept { return self._orders; }

    template <typename Self>
    constexpr auto& order_by_id(this Self&& self, OrderId id) noexcept { return self.orders()[id]; }

    template <typename Self>
    auto& event_handler(this Self&& self) noexcept { return self._event_handler; }

    template <typename Self>
    auto& orderbooks(this Self&& self) noexcept { return self._orderbooks; }

    bool _is_matching_enabled = true;

    EventHandler _event_handler;
    std::vector<OrderBook> _orderbooks;

    HashMap<OrderId, OrderIterator> _orders { };


public:

    template <OrderType type, OrderSide side, typename T>
    constexpr bool try_trigger_new_stop_order(OrderBook& orderbook, T&& order) noexcept {
        if (should_trigger()) {
            trigger_new_stop_order<type, side>(orderbook, std::forward<T>(order));
            return true;
        }
        return false;
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr void trigger_new_stop_order(OrderBook& orderbook, T&& order) noexcept {
        order.template mark_triggered<type>();

        if constexpr (is_market(type)) {
            if (order.time_in_force() != TimeInForce::FOK)
                order.set_time_in_force(TimeInForce::IOC);
        }

        event_handler().template on_trigger_stop_order<type, side>(orderbook, order);

        if constexpr (is_market(type)) {
            match_market_order<side>(orderbook, order);
            event_handler().on_delete_order(order);
        } else if constexpr (is_limit(type)) {
            match_limit_order<side>(orderbook, order);
            try_add_limit_order<type, side>(orderbook, order);
        } else {
            assert(false && "Unsupported order type");
        }

        perform_post_order_processing(orderbook);
    }

    template <OrderType type, OrderSide side, concepts::Order T>
    constexpr void insert_stop_order(OrderBook& orderbook, T&& order) noexcept {
        // TODO this conditional is useless (always true), remove it?
        if (!order.is_fully_filled()) {
            orderbook.template add_order<type, side>(std::forward<T>(order));
        } else {
            event_handler().template on_remove_order<type, side>(orderbook, order);
        }
    }

    template <OrderSide side, typename T>
    constexpr void try_match_aon(OrderBook& orderbook, OrderIterator order_it, T& level) noexcept {
        (void)orderbook;
        (void)order_it;
        (void)level;
    }

    template <OrderSide side>
    constexpr void match_limit_order(OrderBook& orderbook, Order& order) noexcept {
        match_order<side>(orderbook, order);
    }

    constexpr auto recalculate_trailing_stop_price(OrderBook& orderbook) noexcept {
        (void)orderbook;
    }
};

}
