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
    using ConstOrderIterator = typename OrderBook::ConstOrderIterator;

    // TODO mark methods that allocate noexcept conditionally, conditioned that the allocation does throw or not

    // TODO don't take forwarding reference for orders, in which the function should own the order

    enum class StopOrdersAction {
        TRIGGERED,
        NOT_TRIGGERED
    };

public:

    // This shouldn't affect performance since it's very predictable because
    //  the matching is typically enabled or disabled for the entire session
    constexpr bool is_matching_enabled() const noexcept { return _is_matching_enabled; }
    constexpr void enable_matching() noexcept { _is_matching_enabled = true; match(); }
    constexpr void disable_matching() noexcept { _is_matching_enabled = false; }

    // TODO should these two methods be public? They're used in testing, but should they be exposed to users?
    [[nodiscard]] constexpr auto& orderbook_at(SymbolId id) noexcept {
        assert(id.value < orderbooks().size() && "No orderbook with the given ID exists in the matching engine");
        return orderbooks()[id.value];
    }

    [[nodiscard]] constexpr ConstOrderIterator order_at(OrderId id) const noexcept {
        assert(orders().contains(id) && "Order with the given ID doesn't exists in the matching engine");
        return orders().find(id)->second;
    }

    constexpr void add_new_orderbook(Symbol symbol) {
        event_handler().on_add_new_orderbook(symbol);
        add_existing_orderbook(OrderBook { &orders(), symbol, &event_handler() }, false);
    }

    template <typename T>
    constexpr void add_existing_orderbook(T&& orderbook, const bool report = true) {
        assert(!is_symbol_taken(orderbook.symbol_id()) &&
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
        assert(is_symbol_taken(symbol.id) && "No symbol with the given ID exists in the matching engine");

        auto& orderbook = orderbook_at(symbol.id);

        event_handler().on_remove_orderbook(orderbook);

        orderbook.clear();
        orderbook.invalidate();
    }

    template <concepts::Order T>
    constexpr void add_order(T&& order) {
        return resolve_type_and_side_then_call(order, [&]<OrderType type, OrderSide side> {
            return add_order<type, side>(std::forward<T>(order));
        });
    }

    template <OrderType type, OrderSide side, concepts::Order T>
    constexpr void add_order(T&& order) {
        assert(order.is_valid() && "Order is invalid");

        if constexpr (is_market(type)) {
            return add_market_order<side>(std::forward<T>(order));
        } else if constexpr (is_limit(type)) {
            return add_limit_order<side>(std::forward<T>(order));
        } else if constexpr (type == OrderType::STOP) {
            return add_stop_order<type, side>(std::forward<T>(order));
        } else if constexpr (type == OrderType::TRAILING_STOP) {
            return add_stop_order<type, side>(std::forward<T>(order));
        } else if constexpr (type == OrderType::STOP_LIMIT) {
            return add_stop_limit_order<type, side>(std::forward<T>(order));
        } else if constexpr (type == OrderType::TRAILING_STOP_LIMIT) {
            return add_stop_limit_order<type, side>(std::forward<T>(order));
        } else {
            assert(false && "Wrong order type to use here");
        }
    }

    template <OrderSide side, concepts::Order T>
    constexpr void add_market_order(T&& order) {
        auto& orderbook = orderbook_at(order.symbol_id());

        if (is_matching_enabled())
            match_market_order<side>(orderbook, order);

        event_handler().template on_remove_order<OrderType::MARKET, side>(orderbook, order);

        perform_post_order_processing(orderbook);
    }

    template <OrderSide side>
    constexpr void add_limit_order(Order order) {
        auto& orderbook = orderbook_at(order.symbol_id());

        // Since this is a single-threaded engine, order additions will take place only
        //  after no current possible matches. This means that we don't need to respect
        //  the time priority here. If the order is to be filled (possibly partially),
        //  then its price level is better than any order sitting in the orderbook.
        //  Otherwise, it'll not execute and will be put at the end of the queue
        if (is_matching_enabled()) {
            match_limit_order<side>(orderbook, order);
        }

        try_add_limit_order<OrderType::LIMIT, side>(orderbook, std::move(order));

        // TODO perform this only if the order is added?
        perform_post_order_processing(orderbook);
    }

    template <OrderType type, OrderSide side, concepts::Order T>
    constexpr void add_stop_order(T&& order) {
        auto& orderbook = orderbook_at(order.symbol_id());

        if constexpr (type == OrderType::TRAILING_STOP) {
            order.set_stop_price(orderbook.template calculate_trailing_stop_price<side>(order));
        }

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

        if (int(is_matching_enabled()) & int(should_trigger<side>(orderbook, order))) {
            return trigger_new_stop_order<type, side>(orderbook, std::forward<T>(order));
        }

        insert_stop_order<type, side>(orderbook, std::forward<T>(order));

        // TODO should this be here or after adding the order to the orderbook only?
        perform_post_order_processing(orderbook);
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr void remove_order(OrderBook& orderbook, OrderIterator order_it, T level_it) noexcept {
        // This will do the reporting and the removal of the orders from the hash map if needed
        orderbook.template remove_order<type, side>(order_it, level_it);
        perform_post_order_processing(orderbook);
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr auto reduce_order(OrderBook& orderbook, OrderIterator order_it, T level_it, const Quantity quantity) noexcept {
        auto res = orderbook.template reduce_order<type, side>(order_it, level_it, quantity);
        perform_post_order_processing(orderbook);
        return res;
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr void modify_order(OrderBook& orderbook, OrderIterator order_it, T level_it, const Quantity leaves_quantity, const Price price, const bool mitigate) noexcept {
        orderbook.template unlink_order<type, side>(order_it, level_it);
        order_it->set_leaves_quantity(leaves_quantity);
        order_it->set_price(price);

        // In-Flight Mitigation (IFM)
        if (mitigate) {
            if (leaves_quantity > order_it->filled_quantity()) {
                order_it->set_leaves_quantity(leaves_quantity - order_it->filled_quantity());
            } else {
                order_it->set_leaves_quantity(Quantity{ 0 });
            }
        }

        if (!order_it->is_fully_filled()) {
            // TODO don't remove it then add it again!
            orders().erase(order_it->id());
            add_order<type, side>(std::move(*order_it));
        }

        // TODO Reuse the node
        // TODO don't expose memory management here
        // Free the node
        level_it->second.free(order_it);

        // TODO do we need this here or can we perform it only if the order is not removed?
        perform_post_order_processing(orderbook);
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr void replace_order(OrderBook& orderbook, OrderIterator order_it, T level_it, OrderId new_id, Price new_price, Quantity new_quantity) noexcept {
        return replace_order<type, side>(orderbook, order_it, level_it, order_it->clone(new_id.value, new_price.value, new_quantity.value));
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr void replace_order(OrderBook& orderbook, OrderIterator order_it, T level_it, Order new_order) noexcept {
        // Replace atomically. Since the matching engine is single-threaded,
        //  it can do it by performing remove then add, without worrying
        //  about other operations happening between remove and add
        // TODO have the event handler report replacement instead of removal and addition
        remove_order<type, side>(orderbook, order_it, level_it);
        // TODO pass the orderbook and possibly the level

        // We don't know the type and side of the new order
        add_order(std::move(new_order));
    }

# define ADD_BY_ID_METHOD(OP_NAME) \
    template <typename... Args>                                                                                 \
    constexpr void OP_NAME(const OrderId id, Args&&... args) noexcept {                                         \
        auto [order_it, orderbook] = get_order_and_orderbook(id);                                               \
                                                                                                                \
        auto func = [&] <OrderType type, OrderSide side, typename... Args2> (Args2&&... args2) {                \
            auto level_it = orderbook.get().template get_or_add_level<type, side>(                              \
                order_it->template key_price<type>()                                                            \
            );                                                                                                  \
            OP_NAME<type, side>(orderbook.get(), order_it, level_it, std::forward<Args2>(args2)...);            \
        };                                                                                                      \
                                                                                                                \
        resolve_type_and_side_then_call(*order_it, func, std::forward<Args>(args)...);                          \
                                                                                                                \
        perform_post_order_processing(orderbook);                                                               \
    }                                                                                                           \

    ADD_BY_ID_METHOD(remove_order);

    ADD_BY_ID_METHOD(reduce_order);

    ADD_BY_ID_METHOD(modify_order);

    ADD_BY_ID_METHOD(replace_order);

    void modify_order(OrderId id, Price new_price, Quantity new_quantity) {
        return modify_order(id, new_quantity, new_price, false);
    }

    void mitigate_order(OrderId id, Price new_price, Quantity new_quantity) {
        return modify_order(id, new_quantity, new_price, true);
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
    constexpr void execute_order(const OrderId id, Quantity quantity, Price price) noexcept {
        auto [order_it, orderbook_ref] = get_order_and_orderbook(id);
        auto& orderbook = orderbook_ref.get();

        if constexpr (use_order_price) {
            price = order_it->price();
        }

        auto func = [&] <OrderType type, OrderSide side> {
            quantity = std::min(quantity, order_it->leaves_quantity());
            auto level_it = orderbook.template levels<type, side>().find(order_it->template key_price<type>());
            (void)orderbook.template execute_quantity<type, side>(order_it, level_it, quantity, price);
            orderbook.reset_matching_prices();
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
                default: assert(false && "Invalid order type");
            }
        };

        if (order.side() == OrderSide::BUY)
            return f.template operator()<OrderSide::BUY>(std::forward<Args>(args)...);

        return f.template operator()<OrderSide::SELL>(std::forward<Args>(args)...);
    }

    constexpr void match(OrderBook& orderbook) {
        // TODO Don't call .begin() a lot if possible. Remember that different method
        //  can alter the `levels`, which makes using the same begin problematic. Can
        //  you return the next valid level from the invoked methods?

        #define MATCH_AON(PRICE) \
            Quantity chain_volume = calculate_matching_chain(orderbook);\
            if (chain_volume.value == 0) return;\
            auto price = PRICE;\
            execute_matching_chain<OrderSide::BUY>(orderbook, bid_it, bid_level_it, price, chain_volume);\
            execute_matching_chain<OrderSide::SELL>(orderbook, ask_it, ask_level_it, price, chain_volume);\

        while (true) {
            while (int(!orderbook.bids().is_empty()) & int(!orderbook.asks().is_empty())) {
                // Maintain price-time priority
                auto bid_level_it = orderbook.bids().begin();
                auto ask_level_it = orderbook.asks().begin();
                auto& [bid_price, bid_level] = *bid_level_it;
                auto& [ask_price, ask_level] = *ask_level_it;

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
                    MATCH_AON(bid_it->price());
                } else if (ask_it->is_aon()) {
                    MATCH_AON(ask_it->price());
                } else {
                    // If the quantity is the same, it doesn't matter which branch is executed.
                    // The point here is that if the quantities are not equal, then there will
                    //  be an order which will execute (get fully filled and deleted), and the
                    //  other will be partially filled (reduced). Determine which side is
                    //  executed and which side is reduced.
                    if (bid_it->leaves_quantity() < ask_it->leaves_quantity()) {
                        match_orders<OrderSide::BUY, OrderSide::SELL>(orderbook, bid_it, orderbook.bids().begin(), ask_it, orderbook.asks().begin());
                    } else {
                        match_orders<OrderSide::SELL, OrderSide::BUY>(orderbook, ask_it, orderbook.asks().begin(), bid_it, orderbook.bids().begin());
                    }
                }

                // TODO make sure of the OrderType template param
                // At this point, we must've had executed at least one order. Check for stop orders
                try_trigger_stop_orders<OrderType::STOP, OrderSide::BUY> (orderbook);
                try_trigger_stop_orders<OrderType::STOP, OrderSide::SELL>(orderbook);
            }

            // Trailing stop orders modify the stop price, and this is not for free.
            //  Only after all limit and stop orders have settled down, try triggering
            //  trailing stop limit orders. If none is triggered, then we're done.
            if (try_trigger_stop_orders(orderbook) == StopOrdersAction::NOT_TRIGGERED)
                break;
        }
    }

    template <OrderSide executing_side, OrderSide reducing_side, typename T>
    constexpr void match_orders(OrderBook& orderbook, OrderIterator executing_order, T executing_level_it, OrderIterator reducing_order, T reducing_level_it) noexcept {
        Quantity quantity = executing_order->leaves_quantity();
        // TODO should the price be of an arbitrary side?
        Price price = executing_order->price();

        event_handler().template on_match_order<executing_side, reducing_side>(orderbook, *executing_order, *reducing_order);

        // TODO you can pass a template argument stating whether this is an executing
        //  side or not, and if so, delete it without checking for full execution
        // TODO ignore the new order_it and level_it?
        (void)orderbook.template execute_quantity<OrderType::LIMIT, executing_side>(executing_order, executing_level_it, quantity, price);
        orderbook.reset_matching_prices();

        (void)orderbook.template execute_quantity<OrderType::LIMIT, reducing_side >(reducing_order , reducing_level_it , quantity, price);
        perform_post_order_processing(orderbook);
    }

    template <OrderSide side>
    constexpr void match_order(OrderBook& orderbook, Order& order) noexcept {

        // This function doesn't remove an order from its container. The
        //  passed order is standalone, preparing to be added into a level,
        //  but being executed if there is a match first.
        // This keeps matching as long as it can with the opposite side,
        //  and the calling function will handle the rest accordingly.
        // This removes orders being matched from the opposite side.

        constexpr auto opposite_side_value = opposite_side<side>();
        auto& opposite = get_side<opposite_side_value>(orderbook);

        while (!opposite.is_empty()) {
            auto level_it = opposite.begin();
            auto& [level_price, level] = *level_it;

            // Make sure there are crossed orders first
            if (!prices_cross<side>(order.price(), level_price)) break;

            // No short-circuit behavior
            if (int(order.is_fok()) | int(order.is_aon())) {
                return try_match_aon<opposite_side_value>(orderbook, order);
            }

            // match_order_with_level<side>(orderbook, order_it, level);

            // TODO refactor here
            int level_size = static_cast<int>(level.size());
            while (level_size--) {
                auto other_it = level.begin();
                auto& other = *other_it;

                // Either this order is fully "executing" or the target order `order` is executing.
                // It can happen that both of them are fully executed if the leaves_quantity of them
                //  are equal. Otherwise, the non-executing order will just be reduced (execute partially)

                Quantity quantity = Quantity::invalid();
                if (order.leaves_quantity() < other.leaves_quantity()) {
                    if (other.is_aon()) {
                        // We can't match the entire opposite-side order
                        return;
                    }
                    quantity = order.leaves_quantity();
                } else {
                    quantity = other.leaves_quantity();
                }

                // TODO why not order.price()?
                auto execution_price = other.price();

                // TODO make sure of the order type
                // This will report the execution of other
                (void)orderbook.template execute_quantity<OrderType::LIMIT, opposite_side_value>(other_it, level_it, quantity, execution_price);
                orderbook.reset_matching_prices();

                order.execute_quantity(quantity);
                event_handler().template on_execute_order<side>(orderbook, order, quantity, execution_price);
                orderbook.template update_last_and_matching_price<side>(execution_price);

                if (order.is_fully_filled()) {
                    return;
                }
            }
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

    template <OrderSide side>
    constexpr bool try_link_limit_order(OrderBook& orderbook, OrderIterator order_it) {
        // TODO refactor the duplication between this and try_add_limit_order
        // No need for short-circuit behavior here because the checks are simple, and
        //  having short-circuit behavior would be compiled to multiple branches, making
        //  it harder for the branch predictor to do its job
        auto& order = *order_it;
        if (int(!order.is_fully_filled()) & int(!order.is_ioc()) & int(!order.is_fok())) {
            auto level_it = orderbook.template get_or_add_level<OrderType::LIMIT, side>(order_it->template key_price<OrderType::LIMIT>());
            orderbook.template link_order<OrderType::LIMIT, side>(order_it, level_it);
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
        auto& levels = orderbook.template levels<type, side>();
        if (levels.is_empty()) return StopOrdersAction::NOT_TRIGGERED;
        return try_trigger_stop_orders<type, side>(orderbook, levels.begin(), stop_price);
    }

    template <OrderSide side>
    constexpr auto try_trigger_stop_orders_side(OrderBook& orderbook, const Price stop_price) noexcept -> std::pair<StopOrdersAction, StopOrdersAction> {
        // TODO rename this function
        auto a = try_trigger_stop_order_level<OrderType::STOP, side>(orderbook, stop_price);
        auto b = try_trigger_stop_order_level<OrderType::TRAILING_STOP, side>(orderbook, stop_price);
        // TODO recalculate only if a something is triggered?
        update_trailing_stop_price<side>(orderbook);
        return { a, b };
    }

    constexpr StopOrdersAction try_trigger_stop_orders(OrderBook& orderbook) noexcept {
        auto result = StopOrdersAction::NOT_TRIGGERED;

        while (true) {
            // TODO why switching order of a b and c d changes behavior?
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

    template <OrderType type, OrderSide level_side>
    constexpr StopOrdersAction try_trigger_stop_orders(OrderBook& orderbook) noexcept {
        constexpr auto opposite = opposite_side<level_side>();
        auto stop_price = orderbook.template get_market_price<opposite>();
        auto& levels = orderbook.template levels<type, level_side>();
        if (levels.is_empty()) return StopOrdersAction::NOT_TRIGGERED;
        return try_trigger_stop_orders<type, level_side>(orderbook, levels.begin(), stop_price);
    }

    template <OrderType type>
    constexpr static bool is_equal(OrderType other) noexcept {
        const auto a = static_cast<uint8_t>(type);
        const auto b = static_cast<uint8_t>(other);
        return (a & b) == b;
    }

    template <OrderType type, OrderSide level_side, typename T>
    constexpr StopOrdersAction try_trigger_stop_orders(OrderBook& orderbook, T level_it, const Price stop_price) noexcept {

        if constexpr (!is_stop(type)) {
            assert(false && "Unsupported order type");
        }

        auto level_price = level_it->first;
        bool should_trigger = prices_cross<level_side>(stop_price, level_price);

        auto& level = level_it->second;

        if (int(!should_trigger) | int(level.is_empty())) return StopOrdersAction::NOT_TRIGGERED;

        auto size = level.size();
        while (size--) {
            // TODO you can reduce this to two checks only, using `type`, and whether it's a limit order or not
            auto order_it = level.begin();
            auto t = order_it->type();
            if (is_equal<OrderType::STOP>(t)) {
                trigger_stop_order<OrderType::STOP, level_side>(orderbook, order_it, level_it);
            } else if (is_equal<OrderType::TRAILING_STOP>(t)) {
                trigger_stop_order<OrderType::TRAILING_STOP, level_side>(orderbook, order_it, level_it);
            } else if (is_equal<OrderType::STOP_LIMIT>(t)) {
                trigger_stop_limit_order<OrderType::STOP_LIMIT, level_side>(orderbook, order_it, level_it);
            } else if (is_equal<OrderType::TRAILING_STOP_LIMIT>(t)) {
                trigger_stop_limit_order<OrderType::TRAILING_STOP_LIMIT, level_side>(orderbook, order_it, level_it);
            } else {
                assert(false && "Unsupported order type");
            }
        }

        return StopOrdersAction::TRIGGERED;
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr void trigger_stop_order(OrderBook& orderbook, OrderIterator order_it, T level_it) noexcept {
        event_handler().template on_trigger_stop_order<type, side>(orderbook, *order_it);

        order_it->template mark_triggered<type>();
        // TODO remove this and have a way to handle triggered stop orders accordingly
        if (order_it->time_in_force() != TimeInForce::FOK) {
            order_it->set_time_in_force(TimeInForce::IOC);
        }

        // TODO call add_market_order?
        match_market_order<side>(orderbook, *order_it);

        // Remove only after we're done using it
        // Remove the order from its stop order level and create a market
        event_handler().template on_remove_order<type, side>(orderbook, *order_it);
        orderbook.template remove_order<type, side>(order_it, level_it);
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr void trigger_stop_limit_order(OrderBook& orderbook, OrderIterator order_it, T level_it) noexcept {
        // Unlink the order from its stop order level and link it into a limit order level
        orderbook.template unlink_order<type, side>(order_it, level_it);

        order_it->template mark_triggered<type>();
        order_it->set_stop_price(Price{ 0 });  // TODO remove this?

        event_handler().template on_trigger_stop_order<type, side>(orderbook, *order_it);

        match_limit_order<side>(orderbook, *order_it);

        try_link_limit_order<side>(orderbook, order_it);
    }

    static constexpr Quantity calculate_matching_chain_quantity(Order& order, const Quantity needed) noexcept {
        auto order_quantity = order.leaves_quantity();
        if (int(order.is_aon()) | int(order_quantity < needed)) {
            // quantity = min(order_it->leaves_quantity(), needed);
            // BUT, if the order is All-Or-None, then we must either
            //  match it as a whole or not proceed at all
            return order_quantity;
        }
        return needed;
    }

    template <OrderSide level_side>
    constexpr Quantity calculate_matching_chain(OrderBook& orderbook, const Price price, const Quantity required) noexcept {
        Quantity available = Quantity { 0 };

        auto& levels = get_side<level_side>(orderbook);

        for (auto& [level_price, level] : levels) {
            if (!prices_cross<level_side>(level_price, price)) {
                return Quantity { 0 };
            }

            // TODO change these names
            for (auto& order : level) {
                auto needed = required - available;
                auto quantity = calculate_matching_chain_quantity(order, needed);
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

        // Matching is not possible
        return Quantity { 0 };
    }

    constexpr Quantity calculate_matching_chain(OrderBook& orderbook) noexcept {
        // TODO try to reduce the number of iterators used here

        // TODO merge this check with stuff below
        if (int(orderbook.bids().is_empty()) | int(orderbook.asks().is_empty())) {
            return Quantity { 0 };
        }

        // There is a "longer" and a "shorter" order. In case both are
        //  AON or both are not AON, then the longer is the one that
        //  has more quantity. Otherwise, the longer is the AON order
        auto [shorter_level_it, shorter_end_it, longer_level_it, longer_end_it] = [&] {
            // TODO is there a way to not hold reference to end() as well?
            auto shorter_level = orderbook.bids().begin();
            auto longer_level = orderbook.asks().begin();
            auto shorter_end = orderbook.bids().end();
            auto longer_end = orderbook.asks().end();

            auto& shorter_order = *shorter_level->second.begin();
            auto& longer_order = *longer_level->second.begin();

            bool shorter_is_longer = shorter_order.leaves_quantity() > longer_order.leaves_quantity();

            if (int(shorter_order.is_aon()) & (int(shorter_is_longer) | int(!longer_order.is_aon()))) {
                return std::make_tuple(longer_level, longer_end, shorter_level, shorter_end);
            } else {
                return std::make_tuple(shorter_level, shorter_end, longer_level, longer_end);
            }
        }();

        auto longer_order_it = longer_level_it->second.begin();

        auto available = Quantity { 0 };
        auto required = longer_order_it->leaves_quantity();

        // No need to check for longer_level as it's not
        //  going to reach the end before shorter_level
        while (shorter_level_it != shorter_end_it) {

            // Same here
            auto shorter_order_it = shorter_level_it->second.begin();

            auto shorter_level_end = shorter_level_it->second.end();
            auto longer_level_end = longer_level_it->second.end();

            while (shorter_order_it != shorter_level_end) {
                auto needed = required - available;
                auto quantity = calculate_matching_chain_quantity(*shorter_order_it, needed);
                available += quantity;

                if (required == available) {
                    return required;
                }

                // Longer has become shorter
                if (required < available) {
                    std::swap(shorter_order_it, longer_order_it);
                    std::swap(shorter_level_it, longer_level_it);
                    std::swap(shorter_level_end, longer_level_end);
                    std::swap(shorter_end_it, longer_end_it);
                    std::swap(required, available);
                }

                shorter_order_it = shorter_level_it->second.next(shorter_order_it);
            }

            ++shorter_level_it;
        }

        return Quantity{0};
    }

    // TODO reorder the price and volume params so that they're consistent
    template <OrderSide side, typename T>
    constexpr auto execute_matching_chain(OrderBook& orderbook, OrderIterator order_it, T level_it, Price price, Quantity volume) noexcept {
        // The matching chain is already calculated.
        // We don't need to keep checking for boundaries
        while (volume > Quantity { 0 }) {
            auto quantity = calculate_matching_chain_quantity(*order_it, volume);

            // execute_quantity is likely to delete the order and possibly the level
            std::tie(order_it, level_it) = orderbook.template execute_quantity<OrderType::LIMIT, side>(order_it, level_it, quantity, price);
            orderbook.reset_matching_prices();

            volume -= quantity;
        }

        return std::make_pair(order_it, level_it);
    }

    template <OrderSide side>
    constexpr auto execute_matching_chain(OrderBook& orderbook, Price price, Quantity volume) noexcept {
        auto& levels = get_side<side>(orderbook);
        auto level_it = levels.begin();
        auto order_it = level_it->second.begin();

        return execute_matching_chain<side>(orderbook, order_it, level_it, price, volume);
    }

    template <OrderSide side>
    constexpr void update_trailing_stop_price(OrderBook& orderbook) noexcept {
        auto old_trailing_price = orderbook.template get_trailing_stop_price<side>();
        auto new_trailing_price = orderbook.template get_market_trailing_stop_price<side>();
        orderbook.template update_trailing_stop_price<side>(new_trailing_price);
        if constexpr (side == OrderSide::BUY) {
            if (new_trailing_price <= old_trailing_price) return;
        } else {
            if (new_trailing_price >= old_trailing_price) return;
        }

        constexpr auto opposite = opposite_side<side>();
        auto& levels = orderbook.template levels<OrderType::TRAILING_STOP, opposite>();
        for (auto level_it = levels.begin(); level_it != levels.end(); ) {
            bool updated = false;
            auto& [price, level] = *level_it;
            auto order_it = level.begin();
            auto size = level.size();
            while (size--) {
                auto next_it = std::next(order_it);

                auto old_stop_price = order_it->stop_price();
                auto new_stop_price = orderbook.template calculate_trailing_stop_price<opposite>(*order_it);

                if (new_stop_price != old_stop_price) {
                    orderbook.template unlink_order<OrderType::TRAILING_STOP, opposite>(order_it);

                    // TODO can we make this check a compile time check, or at least get rid of the conditional?
                    if (order_it->type() == OrderType::TRAILING_STOP) {
                        order_it->set_stop_price(new_stop_price);
                    } else if (order_it->type() == OrderType::TRAILING_STOP_LIMIT) {
                        auto diff = order_it->price() - order_it->stop_price();
                        order_it->set_stop_price(new_stop_price);
                        order_it->set_price(new_stop_price + diff);
                    } else {
                        assert(false && "Unsupported order type");
                    }

                    // The unlinking can remove the current level, invalidating the iterator. Here,
                    //  we get the newly inserted level, or the same level if it's not removed
                    level_it = orderbook.template link_order<OrderType::TRAILING_STOP, opposite>(order_it);

                    event_handler().template on_update_stop_price<opposite>(orderbook, *order_it);

                    updated = true;
                }

                order_it = next_it;
            }

            if (updated) {
                // TODO get rid of this conditional
                level_it = level_it == levels.begin() ? level_it : std::prev(level_it);
            } else {
                level_it = std::next(level_it);
            }
        }
    }

    constexpr void perform_post_order_processing(OrderBook& orderbook) noexcept {
        // TODO check where this is added redundantly
        if (is_matching_enabled()) {
            match(orderbook);
        }

        orderbook.reset_matching_prices();
    }

    template <OrderType type, OrderSide side, typename T>
    constexpr bool try_trigger_new_stop_order(OrderBook& orderbook, T&& order) noexcept {
        if (should_trigger()) {
            trigger_new_stop_order<type, side>(orderbook, std::forward<T>(order));
            return true;
        }
        return false;
    }

    // TODO remove all forwarding references for orders. You can't copy
    //  orders, and the only way you should get them is by rvalue ref or
    //  iterator. At the same time, don't "move" it too much, since this
    //  is just copying the order. Figure out a way to reduce copying.

    template <OrderType type, OrderSide side>
    constexpr void trigger_new_stop_order(OrderBook& orderbook, Order&& order) noexcept {
        constexpr auto triggered_type = get_triggered<type>();
        order.template mark_triggered<type>();

        if constexpr (is_market(type)) {
            if (order.time_in_force() != TimeInForce::FOK)
                order.set_time_in_force(TimeInForce::IOC);
        }

        event_handler().template on_trigger_stop_order<type, side>(orderbook, order);

        if constexpr (is_market(triggered_type)) {
            match_market_order<side>(orderbook, order);
            event_handler().template on_remove_order<triggered_type, side>(orderbook, order);
        } else if constexpr (is_limit(triggered_type)) {
            match_limit_order<side>(orderbook, order);
            try_add_limit_order<triggered_type, side>(orderbook, std::move(order));
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

    template <OrderSide opposite_side>
    constexpr void try_match_aon(OrderBook& orderbook, Order& order) noexcept {
        auto chain_volume = calculate_matching_chain<opposite_side>(orderbook, order.price(), order.leaves_quantity());
        if (chain_volume == Quantity { 0 }) return;
        execute_matching_chain<opposite_side>(orderbook, order.price(), chain_volume);
        event_handler().template on_execute_order<opposite_side>(orderbook, order, order.leaves_quantity(), order.price());
        // TODO remove this
        orderbook.template update_last_and_matching_price<opposite_side>(order.price());
        // Doesn't remove, just marks it as fully filled
        order.execute_quantity(order.leaves_quantity());
    }

    template <OrderSide side>
    constexpr void match_limit_order(OrderBook& orderbook, Order& order) noexcept {
        match_order<side>(orderbook, order);
    }

    template <OrderSide side>
    [[nodiscard]] constexpr bool should_trigger(OrderBook& orderbook, Order& order) const noexcept {
        constexpr auto opposite = opposite_side<side>();
        auto stop_trigger_price = orderbook.template get_market_price<opposite>();

        return prices_cross<opposite>(order.stop_price(), stop_trigger_price);
    }

    [[nodiscard]] constexpr bool is_symbol_taken(uint64_t id) const noexcept {
        return id < orderbooks().size() && orderbooks()[id].is_valid();
    }

    [[nodiscard]] constexpr bool is_symbol_taken(const SymbolId id) const noexcept {
        return is_symbol_taken(id.value);
    }

    template <OrderSide side1>
    static constexpr bool prices_cross(Price p1, Price p2) noexcept {
        // TODO consider uses of this method again. The passed template arg and method args are not clear
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
};

}
