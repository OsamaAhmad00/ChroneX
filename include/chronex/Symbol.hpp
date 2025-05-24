#pragma once
#include <cstdint>
#include <limits>

namespace chronex {

struct SymbolId {
    uint32_t value;
    constexpr bool operator==(const SymbolId &) const noexcept = default;
    constexpr static SymbolId invalid() noexcept { return SymbolId{std::numeric_limits<decltype(value)>::max() }; }
};

struct Symbol {
    SymbolId id;
    char name[8];
};

}
