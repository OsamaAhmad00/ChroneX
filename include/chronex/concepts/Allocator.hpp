#pragma once

#include <memory>
#include <concepts>

namespace chronex::concepts {

template<typename AllocatorType>
concept Allocator = requires(AllocatorType a, typename AllocatorType::value_type* ptr, size_t n) {
    typename AllocatorType::value_type;
    typename AllocatorType::size_type;
    typename AllocatorType::difference_type;

    // Standard allocator requirements
    { a.allocate(n) } -> std::same_as<typename AllocatorType::value_type*>;
    { a.deallocate(ptr, n) } -> std::same_as<void>;

    // Additional requirements that can be provided by allocator_traits
    requires std::is_default_constructible_v<AllocatorType>;
    requires std::is_copy_constructible_v<AllocatorType>;
    requires std::is_destructible_v<AllocatorType>;
};

}