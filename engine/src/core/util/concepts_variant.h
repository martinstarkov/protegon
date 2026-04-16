#pragma once

#include <type_traits>
#include <variant>

namespace ptgn {

namespace impl {

template <typename T, typename Variant>
struct variant_contains : std::false_type {};

template <typename T, typename... Ts>
struct variant_contains<T, std::variant<Ts...>> :
	std::bool_constant<(std::is_same_v<std::remove_cvref_t<T>, Ts> || ...)> {};

template <typename T>
struct is_variant : std::false_type {};

template <typename... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type {};

} // namespace impl

template <typename T>
concept VariantType = impl::is_variant<std::remove_cvref_t<T>>::value;

template <typename T, typename Variant>
concept VariantContains = impl::variant_contains<T, std::remove_cvref_t<Variant>>::value;

} // namespace ptgn
