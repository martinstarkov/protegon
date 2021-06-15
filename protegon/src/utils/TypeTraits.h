#pragma once

#include <type_traits> // std::enable_if_t, std::is_arithmetic_v, std::is_floating_point_v, etc.
#include <utility> // std::forward

namespace ptgn {

namespace type_traits {

// Internal type trait implementations and helpers.
namespace internal {

// Checks whether or not T has a function called Invoke.
template <typename T>
constexpr auto has_invoke_helper(const T&, int)
-> decltype(&T::Invoke, &T::Invoke);
template <typename T>
constexpr void* has_invoke_helper(const T&, long) {
	return nullptr;
}

// Source: https://stackoverflow.com/a/34672753/4384023
template <template <typename...> class Base, typename Derived>
struct is_base_of_template_impl {
	template<typename... Ts>
	static constexpr std::true_type  test(const Base<Ts...>*);
	static constexpr std::false_type test(...);
	using type = decltype(test(std::declval<Derived*>()));
};
template <template <typename...> class Base, typename Derived>
using is_base_of_template = typename is_base_of_template_impl<Base, Derived>::type;

// Source: https://stackoverflow.com/a/49026811
template<typename Stream, typename Type, typename = void>
struct is_stream_writable : std::false_type {};
template<typename Stream, typename Type>
struct is_stream_writable<Stream, Type, std::void_t<decltype(std::declval<Stream&>() << std::declval<Type>()) >> : std::true_type {};

} // namespace internal

// Custom template helpers.

// Template qualifier of whether or not Type is an integer OR float point number.
// This includes bool, char, char8_t, char16_t, char32_t, wchar_t, short, int, long, long long, float, double, and long double.
template <typename Type>
using is_number_e = std::enable_if_t<std::is_arithmetic_v<Type>, bool>;

// Template qualifier of whether or not Type is an integer number.
// This includes bool, char, char8_t, char16_t, char32_t, wchar_t, short, int, long, long long.
template <typename Type>
using is_integral_e = std::enable_if_t<std::is_integral_v<Type>, bool>;

// Template qualifier of whether or not Type is a float point number.
// This includes float, double, and long double.
template <typename Type>
using is_floating_point_e = std::enable_if_t<std::is_floating_point_v<Type>, bool>;

// Template qualifier of whether or not From is convertible to To (double to int, int to float, etc).
template <typename From, typename To>
using is_convertible_e = std::enable_if_t<std::is_convertible_v<From, To>, bool>;

// Template qualifier of whether or not the Types are the same.
template <typename Type1, typename Type2>
using is_same_as_e = std::enable_if_t<std::is_same_v<Type1, Type2>, bool>;

// Template qualifier of whether or not the Derived inherits from Base.
template <typename Base, typename Derived>
using is_base_of_e = std::enable_if_t<std::is_base_of_v<Base, Derived>, bool>;

// Template qualifier of whether or not Type is default constructible.
template <typename Type>
using is_default_constructible_e = std::enable_if_t<std::is_default_constructible_v<Type>, bool>;

// Template qualifier of whether or not Type is constructible from TArgs.
template <typename Type, typename ...TArgs>
using is_constructible_e = std::enable_if_t<std::is_constructible_v<Type, TArgs...>, bool>;

// Template qualifier of whether or not Type is the same as one or more of Types.
template <typename Type, typename ...Types>
using is_one_of_e = std::enable_if_t<(std::is_same_v<Type, Types> || ...), bool>;

// True if Derived derives from a template Base, false otherwise.
template <template <typename...> class Base, typename Derived>
bool constexpr is_base_of_template_v{
	internal::is_base_of_template<Base, Derived>::value
};

// Template qualifier of whether or not Dervied derives from a template Base.
template <template <typename...> class Base, typename Derived>
using is_base_of_template_e = std::enable_if_t<is_base_of_template_v<Base, Derived>, bool>;

// Template qualifier of whether or not all Types are same as a given Type.
template <typename Type, typename ...Types>
using are_type_e = std::enable_if_t<std::conjunction_v<std::is_same<Type, Types>...>, bool>;

// True if Type has a function named Invoke, false otherwise.
template <typename Type>
bool constexpr has_invoke_v{
	!std::is_same<decltype(internal::has_invoke_helper(std::declval<Type>(), 0)), void*>::value
};

// True if Type has a static function named Invoke, false otherwise.
template <typename Type>
bool constexpr has_static_invoke_v{
	has_invoke_v<Type> && 
	!std::is_member_function_pointer_v<decltype(internal::has_invoke_helper(std::declval<Type>(), 0))>
};

// Template qualifier of whether or not Type has a static function named Invoke.
template <typename Type>
using has_static_invoke_e = std::enable_if_t<has_static_invoke_v<Type>, bool>;

// True if Type is writeable to the stream, false otherwise.
template <typename Stream, typename Type>
bool constexpr is_stream_writable_v{ internal::is_stream_writable<Stream, Type>::value };

// Template qualifier of whether or not the Type is writeable to the stream.
template <typename Stream, typename Type>
using is_stream_writable_e = std::enable_if_t<is_stream_writable_v<Stream, Type>, bool>;

// Template qualifier of whether or not Types are all writeable to the stream.
template <typename Stream, typename ...Types>
using are_stream_writable_e = std::enable_if_t<std::conjunction_v<internal::is_stream_writable<Stream, Types>...>, bool>;

} // namespace type_traits

} // namespace ptgn