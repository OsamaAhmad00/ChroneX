#pragma once

#include <cassert>
#include <cstdint>
#include <limits>
#include <functional>
#include <string_view>

namespace chronex {

struct SymbolId {
    uint32_t value;
    constexpr bool operator==(const SymbolId &) const noexcept = default;
    constexpr static SymbolId invalid() noexcept { return SymbolId{std::numeric_limits<decltype(value)>::max() }; }
};

struct Symbol {
    constexpr Symbol(SymbolId id, const char* name) : id(id), name{ } {
        // TODO should we store the null terminator? This makes
        //  it convenient for using it as a cstr, but we already
        //  know that it has a certain size
        int size = sizeof(this->name) - 1;
        int i = 0;
        while (i < size && name[i] != '\0') {
            this->name[i] = name[i];
            i++;
        }
        this->name[size] = '\0';
        assert(name[i] == '\0' && "Symbol name exceeds the expected size");
    }

    constexpr Symbol(uint32_t id, const char* name) : Symbol(SymbolId{ id }, name) { }

    constexpr Symbol(SymbolId id, std::string_view name) : Symbol(id, name.data()) { }

    constexpr Symbol(uint32_t id, std::string_view name) : Symbol(id, name.data()) { }

    constexpr static Symbol invalid() noexcept { return Symbol { SymbolId::invalid(), "" }; }

    SymbolId id;
    char name[8];
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