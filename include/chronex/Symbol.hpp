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
    constexpr Symbol(SymbolId _id, const char* _name) : id(_id), name{ } {
        // TODO should we store the null terminator? This makes
        //  it convenient for using it as a cstr, but we already
        //  know that it has a certain size
        int size = sizeof(this->name) - 1;
        int i = 0;
        while (i < size && _name[i] != '\0') {
            this->name[i] = _name[i];
            i++;
        }
        this->name[size] = '\0';
        assert(_name[i] == '\0' && "Symbol name exceeds the expected size");
    }

    constexpr Symbol(uint32_t _id, const char* _name) : Symbol(SymbolId{ _id }, _name) { }

    constexpr Symbol(SymbolId _id, std::string_view _name) : Symbol(_id, _name.data()) { }

    constexpr Symbol(uint32_t _id, std::string_view _name) : Symbol(_id, _name.data()) { }

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