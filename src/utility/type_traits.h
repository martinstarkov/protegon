#pragma once

#include <type_traits>
#include <utility>

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
template <class T, class O>
struct same_decayed :
	pred_base<std::is_same_v<typename std::decay_t<T>, typename std::decay_t<O>>> {};

// is_numeric, i.e. true for floats and integrals but not bool
template <class T>
struct is_numeric : pred_base<std::is_arithmetic_v<T> && !same_decayed<bool, T>::value> {};

// both - less verbose way to determine if two types both meet a single
// predicate
template <class A, class B, template <typename> class PRED>
struct both : pred_base<PRED<A>::value && PRED<B>::value> {};

template <class A, class B>
struct both_numeric : both<A, B, is_numeric> {}; // Are both A and B numeric types

template <class A, class B>
struct both_floating :
	both<A, B, std::is_floating_point> {}; // Are both A and B floating point types

template <class A, class B>
struct both_integral : both<A, B, std::is_integral> {}; // Are both A and B integral types

template <class A, class B>
struct both_signed : both<A, B, std::is_signed> {}; // Are both A and B signed types

template <class A, class B>
struct both_unsigned : both<A, B, std::is_unsigned> {}; // Are both A and B unsigned types

// Returns true if both number types are signed or both are unsigned.
template <class T, class F>
struct same_signage : pred_base<(both_signed<T, F>::value) || (both_unsigned<T, F>::value)> {};

// Source: https://stackoverflow.com/a/36272533
// Obviously both src and dest must be numbers
// Floating dest: src must be integral or smaller/equal float-type
// Integral dest: src must be integral and (smaller/equal+same signage) or
// (smaller+different signage)
template <class T, class F>
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
struct is_mappish_impl : std::false_type {};

template <typename T>
struct is_mappish_impl<
	T, std::void_t<
		   typename T::key_type, typename T::mapped_type,
		   decltype(std::declval<T&>()[std::declval<const typename T::key_type&>()])>> :
	std::true_type {};

template <typename T>
struct is_mappish : is_mappish_impl<T>::type {};

} // namespace impl

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
inline constexpr bool is_narrowing_v{ !impl::is_safe_numeric_cast<To, From>::value 
									/* Or perhaps alternatively: std::is_convertible_v<From, To> &&
                                      !std::is_same_v<From, To> &&
                                      !std::is_constructible_v<To, From>*/ };
									  
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

template <class T>
inline constexpr bool is_map_type_v{ impl::is_mappish<T>{} };

template <typename T, typename U>
using equals_comparable = std::enable_if_t<is_equals_comparable_v<T, U>, bool>;
template <typename T, typename U>
using not_equals_comparable = std::enable_if_t<is_not_equals_comparable_v<T, U>, bool>;
template <typename T, typename U>
using less_than_comparable = std::enable_if_t<is_less_than_comparable_v<T, U>, bool>;
template <typename T, typename U>
using less_than_or_equal_comparable =
	std::enable_if_t<is_less_than_or_equal_comparable_v<T, U>, bool>;
template <typename T, typename U>
using greater_than_comparable = std::enable_if_t<is_greater_than_comparable_v<T, U>, bool>;
template <typename T, typename U>
using greater_than_or_equal_comparable =
	std::enable_if_t<is_greater_than_or_equal_comparable_v<T, U>, bool>;
template <typename From, typename To>
using convertible = std::enable_if_t<std::is_convertible_v<From, To>, bool>;
template <typename T>
using arithmetic = std::enable_if_t<std::is_arithmetic_v<T>, bool>;
template <typename T>
using integral = std::enable_if_t<std::is_integral_v<T>, bool>;
template <typename T>
using floating_point = std::enable_if_t<std::is_floating_point_v<T>, bool>;
template <typename From, typename To>
using narrowing = std::enable_if_t<is_narrowing_v<From, To>, bool>;
template <typename From, typename To>
using not_narrowing = std::enable_if_t<!is_narrowing_v<From, To>, bool>;
template <typename T>
using copy_constructible = std::enable_if_t<std::is_copy_constructible_v<T>, bool>;
template <typename From, typename To>
using is_safely_castable = std::enable_if_t<is_safely_castable_v<From, To>, bool>;
template <typename T, typename... TArgs>
using constructible = std::enable_if_t<
	std::is_constructible_v<T, TArgs...> || std::is_trivially_constructible_v<T, TArgs...>, bool>;
template <typename Stream, typename... Types>
using stream_writable =
	std::enable_if_t<std::conjunction_v<impl::is_stream_writable<Stream, Types>...>, bool>;
template <typename Type, typename... Types>
using type = std::enable_if_t<std::conjunction_v<std::is_same<Type, Types>...>, bool>;
template <typename TypeA, typename TypeB>
using same = std::enable_if_t<std::is_same_v<TypeA, TypeB>, bool>;
template <typename TypeA, typename TypeB>
using not_same = std::enable_if_t<!std::is_same_v<TypeA, TypeB>, bool>;
template <typename ChildClass, typename ParentClass>
using is_base_of = std::enable_if_t<std::is_base_of_v<ParentClass, ChildClass>, bool>;
template <typename T, typename... Ts>
using is_any_of = std::enable_if_t<is_any_of_v<T, Ts...>, bool>;
template <typename T, typename... Ts>
using is_safely_castable_to_one_of =
	std::enable_if_t<is_safely_castable_to_one_of_v<T, Ts...>, bool>;
template <bool CONDITION>
using enable = std::enable_if_t<CONDITION, bool>;

} // namespace ptgn::tt
