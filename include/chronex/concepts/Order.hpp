#pragma once

#include <concepts>

#include <chronex/concepts/Common.hpp>

namespace chronex {

struct OrderId;

};

namespace chronex::concepts {

template <typename OrderType>
concept Order = requires (OrderType order) {
    { order.id() } -> SameAfterRemovingCVRef<OrderId>;
};

}
