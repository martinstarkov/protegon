#pragma once

#include <array>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ptgn::tt {

namespace impl {

// Source: https://stackoverflow.com/a/44536046
template <typename T, typename U>
using equals_comparison_t = decltype(std::declval<T&>() == std::declval<U&>());

template <typename T, typename U, typename = std::void_t<>>
struct is_equals_comparable : std::false_type {};

template <typename T, typename U>
struct is_equals_comparable<T, U, std::void_t<equals_comparison_t<T, U>>> :
	std::is_same<equals_comparison_t<T, U>, bool> {};

template <typename T, typename U>
using not_equals_comparison_t = decltype(std::declval<T&>() != std::declval<U&>());

template <typename T, typename U, typename = std::void_t<>>
struct is_not_equals_comparable : std::false_type {};

template <typename T, typename U>
struct is_not_equals_comparable<T, U, std::void_t<not_equals_comparison_t<T, U>>> :
	std::is_same<not_equals_comparison_t<T, U>, bool> {};

template <typename T, typename U>
using less_than_comparison_t = decltype(std::declval<T&>() < std::declval<U&>());

template <typename T, typename U, typename = std::void_t<>>
struct is_less_than_comparable : std::false_type {};

template <typename T, typename U>
struct is_less_than_comparable<T, U, std::void_t<less_than_comparison_t<T, U>>> :
	std::is_same<less_than_comparison_t<T, U>, bool> {};

template <typename T, typename U>
using less_than_or_equal_comparison_t = decltype(std::declval<T&>() <= std::declval<U&>());

template <typename T, typename U, typename = std::void_t<>>
struct is_less_than_or_equal_comparable : std::false_type {};

template <typename T, typename U>
struct is_less_than_or_equal_comparable<T, U, std::void_t<less_than_or_equal_comparison_t<T, U>>> :
	std::is_same<less_than_or_equal_comparison_t<T, U>, bool> {};

template <typename T, typename U>
using greater_than_comparison_t = decltype(std::declval<T&>() > std::declval<U&>());

template <typename T, typename U, typename = std::void_t<>>
struct is_greater_than_comparable : std::false_type {};

template <typename T, typename U>
struct is_greater_than_comparable<T, U, std::void_t<greater_than_comparison_t<T, U>>> :
	std::is_same<greater_than_comparison_t<T, U>, bool> {};

template <typename T, typename U>
using greater_than_or_equal_comparison_t = decltype(std::declval<T&>() >= std::declval<U&>());

template <typename T, typename U, typename = std::void_t<>>
struct is_greater_than_or_equal_comparable : std::false_type {};

template <typename T, typename U>
struct is_greater_than_or_equal_comparable<
	T, U, std::void_t<greater_than_or_equal_comparison_t<T, U>>> :
	std::is_same<greater_than_or_equal_comparison_t<T, U>, bool> {};

// Source: https://stackoverflow.com/a/36272533
// pred_base selects the appropriate base type (true_type or false_type) to
// make defining our own predicates easier.
template <bool>
struct pred_base : std::false_type {};

template <>
struct pred_base<true> : std::true_type {};

// same_decayed
// -------------
// Are the decayed versions of "T" and "O" the same basic type?
// Gets around the fact that std::is_same will treat, say "bool" and "bool&" as
// different types and using std::decay all over the place gets really verbose
template <typename T, typename O>
struct same_decayed :
	pred_base<std::is_same_v<typename std::decay_t<T>, typename std::decay_t<O>>> {};

// is_numeric, i.e. true for floats and integrals but not bool
template <typename T>
struct is_numeric : pred_base<std::is_arithmetic_v<T> && !same_decayed<bool, T>::value> {};

// both - less verbose way to determine if two types both meet a single
// predicate
template <typename A, typename B, template <typename> typename PRED>
struct both : pred_base<PRED<A>::value && PRED<B>::value> {};

template <typename A, typename B>
struct both_numeric : both<A, B, is_numeric> {}; // Are both A and B numeric types

template <typename A, typename B>
struct both_floating :
	both<A, B, std::is_floating_point> {}; // Are both A and B floating point types

template <typename A, typename B>
struct both_integral : both<A, B, std::is_integral> {}; // Are both A and B integral types

template <typename A, typename B>
struct both_signed : both<A, B, std::is_signed> {}; // Are both A and B signed types

template <typename A, typename B>
struct both_unsigned : both<A, B, std::is_unsigned> {}; // Are both A and B unsigned types

// Returns true if both number types are signed or both are unsigned.
template <typename T, typename F>
struct same_signage : pred_base<(both_signed<T, F>::value) || (both_unsigned<T, F>::value)> {};

// Source: https://stackoverflow.com/a/36272533
// Obviously both src and dest must be numbers
// Floating dest: src must be integral or smaller/equal float-type
// Integral dest: src must be integral and (smaller/equal+same signage) or
// (smaller+different signage)
template <typename T, typename F>
struct is_safe_numeric_cast :
	pred_base<
		(both_numeric<T, F>::value && std::is_floating_point_v<T> && std::is_integral_v<F>) ||
		(sizeof(T) >= sizeof(F)) ||
		(both_integral<T, F>::value && ((sizeof(T) > sizeof(F)) || (sizeof(T) == sizeof(F))) &&
		 same_signage<T, F>::value)> {};

// Source: https://stackoverflow.com/a/49026811
template <typename Stream, typename Type, typename = void>
struct is_stream_writable : std::false_type {};

template <typename Stream, typename Type>
struct is_stream_writable<
	Stream, Type, std::void_t<decltype(std::declval<Stream&>() << std::declval<Type>())>> :
	std::true_type {};

// Source: https://stackoverflow.com/a/50473559
template <typename T, typename U, typename = void>
struct is_safely_castable : std::false_type {};

template <typename T, typename U>
struct is_safely_castable<T, U, std::void_t<decltype(static_cast<U>(std::declval<T>()))>> :
	std::true_type {};

template <typename T, typename U = void>
struct is_map_like_impl : std::false_type {};

template <typename T>
struct is_map_like_impl<
	T, std::void_t<
		   typename T::key_type, typename T::mapped_type,
		   decltype(std::declval<T&>()[std::declval<const typename T::key_type&>()])>> :
	std::true_type {};

template <typename T>
struct is_map_like : is_map_like_impl<T>::type {};

template <typename T>
struct is_string_like : public std::false_type {};

template <>
struct is_string_like<std::string> : public std::true_type {};

template <>
struct is_string_like<std::string_view> : public std::true_type {};

template <typename T>
struct is_std_vector : public std::false_type {};

template <typename T, typename Alloc>
struct is_std_vector<std::vector<T, Alloc>> : public std::true_type {};

template <typename T>
struct is_std_array : public std::false_type {};

template <typename T, std::size_t I>
struct is_std_array<std::array<T, I>> : public std::true_type {};

template <typename T, typename StreamWriterType, typename = void>
struct has_static_serialize : std::false_type {};

template <typename T, typename StreamWriterType>
struct has_static_serialize<
	T, StreamWriterType,
	std::void_t<decltype(T::Serialize(std::declval<StreamWriterType*>(), std::declval<const T&>())
	)>> : std::true_type {};

template <typename T, typename StreamReaderType, typename = void>
struct has_static_deserialize : std::false_type {};

template <typename T, typename StreamReaderType>
struct has_static_deserialize<
	T, StreamReaderType,
	std::void_t<decltype(T::Deserialize(std::declval<StreamReaderType*>(), std::declval<T&>()))>> :
	std::true_type {};

} // namespace impl

template <typename T, typename StreamWriterType>
inline constexpr bool is_serializable_v{ impl::has_static_serialize<T, StreamWriterType>::value };

template <typename T, typename StreamReaderType>
inline constexpr bool is_deserializable_v{
	impl::has_static_deserialize<T, StreamReaderType>::value
};

template <typename T>
inline constexpr bool is_std_array_or_vector_v{ impl::is_std_array<T>::value ||
												impl::is_std_vector<T>::value };

template <typename T>
inline constexpr bool is_std_array_v{ impl::is_std_array<T>::value };

template <typename T>
inline constexpr bool is_std_vector_v{ impl::is_std_vector<T>::value };

template <typename T, typename U>
inline constexpr bool is_equals_comparable_v{ impl::is_equals_comparable<T, U>::value };

template <typename T, typename U>
inline constexpr bool is_not_equals_comparable_v{ impl::is_not_equals_comparable<T, U>::value };

template <typename T, typename U>
inline constexpr bool is_less_than_comparable_v{ impl::is_less_than_comparable<T, U>::value };

template <typename T, typename U>
inline constexpr bool is_less_than_or_equal_comparable_v{
	impl::is_less_than_or_equal_comparable<T, U>::value
};

template <typename T, typename U>
inline constexpr bool is_greater_than_comparable_v{ impl::is_greater_than_comparable<T, U>::value };

template <typename T, typename U>
inline constexpr bool is_greater_than_or_equal_comparable_v{
	impl::is_greater_than_or_equal_comparable<T, U>::value
};

template <typename From, typename To>
inline constexpr bool is_narrowing_v{
	!impl::is_safe_numeric_cast<To, From>::value
	/* Or perhaps alternatively: std::is_convertible_v<From, To> &&
	  !std::is_same_v<From, To> &&
	  !std::is_constructible_v<To, From>*/
};

template <typename Stream, typename Type>
inline constexpr bool is_stream_writable_v{ impl::is_stream_writable<Stream, Type>::value };

template <typename From, typename To>
inline constexpr bool is_safely_castable_v{ impl::is_safely_castable<From, To>::value };

template <typename T, typename... Ts>
inline constexpr bool is_any_of_v{ (std::is_same_v<T, Ts> || ...) };

template <typename T, typename... Ts>
inline constexpr bool is_not_any_of_v{ (!std::is_same_v<T, Ts> && ...) };

template <typename T, typename... Ts>
inline constexpr bool is_safely_castable_to_one_of_v{ (is_safely_castable_v<T, Ts> || ...) };

template <typename T>
inline constexpr bool is_string_like_v{ impl::is_string_like<T>::value };

template <typename T>
inline constexpr bool is_map_like_v{ impl::is_map_like<T>{} };

// SFINAE helpers.

template <bool Condition>
using enable = std::enable_if_t<Condition, bool>;

template <typename T, typename StreamWriterType>
using serializable = enable<is_serializable_v<T, StreamWriterType>>;

template <typename T, typename StreamReaderType>
using deserializable = enable<is_deserializable_v<T, StreamReaderType>>;

template <typename T>
using string_like = enable<is_string_like_v<T>>;

template <typename T>
using map_like = enable<is_map_like_v<T>>;

template <typename T, typename U>
using equals_comparable = enable<is_equals_comparable_v<T, U>>;

template <typename T, typename U>
using not_equals_comparable = enable<is_not_equals_comparable_v<T, U>>;

template <typename T, typename U>
using less_than_comparable = enable<is_less_than_comparable_v<T, U>>;

template <typename T, typename U>
using less_than_or_equal_comparable = enable<is_less_than_or_equal_comparable_v<T, U>>;

template <typename T, typename U>
using greater_than_comparable = enable<is_greater_than_comparable_v<T, U>>;

template <typename T, typename U>
using greater_than_or_equal_comparable = enable<is_greater_than_or_equal_comparable_v<T, U>>;

template <typename From, typename To>
using convertible = enable<std::is_convertible_v<From, To>>;

template <typename T>
using arithmetic = enable<std::is_arithmetic_v<T>>;

template <typename T>
using integral = enable<std::is_integral_v<T>>;

template <typename T>
using floating_point = enable<std::is_floating_point_v<T>>;

template <typename From, typename To>
using narrowing = enable<is_narrowing_v<From, To>>;

template <typename From, typename To>
using not_narrowing = enable<!is_narrowing_v<From, To>>;

template <typename T>
using copy_constructible = enable<std::is_copy_constructible_v<T>>;

template <typename From, typename To>
using is_safely_castable = enable<is_safely_castable_v<From, To>>;

template <typename T, typename... TArgs>
using constructible =
	enable<std::is_constructible_v<T, TArgs...> || std::is_trivially_constructible_v<T, TArgs...>>;

template <typename Stream, typename... Types>
using stream_writable = enable<std::conjunction_v<impl::is_stream_writable<Stream, Types>...>>;

template <typename Type, typename... Types>
using type = enable<std::conjunction_v<std::is_same<Type, Types>...>>;

template <typename TypeA, typename TypeB>
using same = enable<std::is_same_v<TypeA, TypeB>>;

template <typename TypeA, typename TypeB>
using not_same = enable<!std::is_same_v<TypeA, TypeB>>;

template <typename ChildClass, typename ParentClass>
using is_base_of = enable<std::is_base_of_v<ParentClass, ChildClass>>;

template <typename T, typename... Ts>
using is_any_of = enable<is_any_of_v<T, Ts...>>;

template <typename T, typename... Ts>
using is_safely_castable_to_one_of = enable<is_safely_castable_to_one_of_v<T, Ts...>>;

} // namespace ptgn::tt