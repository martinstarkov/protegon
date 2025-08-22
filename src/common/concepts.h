#pragma once

#include <concepts>
#include <type_traits>

namespace ptgn {

template <typename T>
concept Enum = std::is_enum_v<T>;

template <typename T>
concept ScopedEnum = std::is_enum_v<T> && !std::is_convertible_v<T, int>;

template <typename T, typename BaseType>
concept IsOrDerivedFrom = std::is_same_v<T, BaseType> || std::derived_from<T, BaseType>;

} // namespace ptgn
