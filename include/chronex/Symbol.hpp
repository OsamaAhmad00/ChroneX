#pragma once

#include <cstdint>
#include <limits>
#include <functional>

namespace chronex {

struct SymbolId {
    uint32_t value;
    constexpr bool operator==(const SymbolId &) const noexcept = default;
    constexpr static SymbolId invalid() noexcept { return SymbolId{std::numeric_limits<decltype(value)>::max() }; }
};

struct Symbol {
    SymbolId id;
    char name[8];
    constexpr static Symbol invalid() noexcept { return Symbol { SymbolId::invalid(), "" }; }
};

}

namespace std {
    template <>
    struct hash<chronex::SymbolId> {
        size_t operator()(const chronex::SymbolId& id) const noexcept {
            return std::hash<decltype(id.value)>{}(id.value);
        }
    };

    template <>
    struct hash<chronex::Symbol> {
        size_t operator()(const chronex::Symbol& symbol) const noexcept {
            return std::hash<decltype(symbol.id)>{}(symbol.id);
        }
    };
}