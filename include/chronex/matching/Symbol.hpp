#pragma once
#include <cstdint>

namespace chronex {

struct SymbolId {
    uint32_t value;
    constexpr bool operator==(const SymbolId &) const noexcept = default;
};

struct Symbol {
    SymbolId id;
    char name[10];
};

}
