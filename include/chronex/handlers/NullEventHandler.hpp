#pragma once

namespace chronex::handlers {

class NullEventHandler {
public:
    template <typename T>
    void on_order_creation(T&&) const noexcept { }
};

}