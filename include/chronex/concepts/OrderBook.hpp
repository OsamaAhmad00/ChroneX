#pragma once

namespace chronex::concepts {

template <typename OrderBookType>
concept OrderBook = requires (OrderBookType orderbook) {
    // TODO Write different types of orderbook concepts
    { orderbook.size() } noexcept -> std::convertible_to<std::size_t>;
};

}