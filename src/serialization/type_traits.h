#pragma once

#include "utility/macro.h"

PTGN_HAS_TEMPLATE_FUNCTION(Serialize);
PTGN_HAS_TEMPLATE_FUNCTION(Deserialize);

namespace ptgn::tt {

namespace impl {

template <typename, typename = void>
struct has_friend_to_json : std::false_type {};

template <typename T>
struct has_friend_to_json<
	T, std::void_t<decltype(to_json(std::declval<json&>(), std::declval<const T&>()))>> :
	std::true_type {};

template <typename T>
inline constexpr bool has_friend_to_json_v = has_friend_to_json<T>::value;

template <typename, typename = void>
struct has_friend_from_json : std::false_type {};

template <typename T>
struct has_friend_from_json<
	T, std::void_t<decltype(from_json(std::declval<const json&>(), std::declval<T&>()))>> :
	std::true_type {};

template <typename T>
inline constexpr bool has_friend_from_json_v = has_friend_from_json<T>::value;

} // namespace impl

template <typename T>
inline constexpr bool is_to_json_convertible_v{ impl::has_friend_to_json_v<T> };

template <typename T>
inline constexpr bool is_from_json_convertible_v{ impl::has_friend_from_json_v<T> };

template <typename T>
inline constexpr bool is_json_convertible_v{ is_to_json_convertible_v<T> &&
											 is_from_json_convertible_v<T> };

} // namespace ptgn::tt