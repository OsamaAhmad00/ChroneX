#pragma once

#include <cstddef>
#include <concepts>
#include <iterator>

namespace chronex::concepts {

template <typename ListType>
concept List = requires(ListType l) {
    typename ListType::value_type;
    typename ListType::reference;
    typename ListType::const_reference;
    typename ListType::iterator;
    typename ListType::const_iterator;
    typename ListType::difference_type;
    typename ListType::size_type;
    
    { l.begin() } -> std::same_as<typename ListType::iterator>;
    { l.end() } -> std::same_as<typename ListType::iterator>;
    { l.cbegin() } -> std::same_as<typename ListType::const_iterator>;
    { l.cend() } -> std::same_as<typename ListType::const_iterator>;
    { l.size() } -> std::convertible_to<typename ListType::size_type>;
    { l.empty() } -> std::convertible_to<bool>;
};

}
