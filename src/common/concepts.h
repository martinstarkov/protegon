#pragma once

#include <type_traits>

namespace ptgn {

template <typename T>
concept Enum = std::is_enum_v<T>;

template <typename T>
concept ScopedEnum = std::is_enum_v<T> && !std::is_convertible_v<T, int>;

} // namespace ptgn
