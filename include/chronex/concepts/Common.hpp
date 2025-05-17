#pragma once

namespace chronex::concepts {

template <typename Comp, typename T, typename U>
concept Comparator = requires(Comp comp, T a, U b) {
    { comp(a, b) } noexcept -> std::convertible_to<bool>;
};

template <typename Comp, typename T>
concept UniTypeComparator = Comparator<Comp, T, T>;

template <typename T, typename U>
concept SameAfterRemovingCVRef = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

}