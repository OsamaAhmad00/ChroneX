#pragma once

namespace chronex::concepts {

template <typename OrderBookType>
concept OrderBook = requires (OrderBookType orderbook) {
    // TODO Write different types of orderbook concepts
    orderbook.bids();
    orderbook.asks();
};

}