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

} // namespace internal

// Custom template helpers.

// Template qualifier of whether or not Type is the same as one or more of Types.
template <typename Type, typename ...Types>
using is_one_of_e = std::enable_if_t<(std::is_same_v<Type, Types> || ...), bool>;

// True if Derived derives from a template Base, false otherwise.
template <template <typename...> class Base, typename Derived>
bool constexpr is_base_of_template_v{ internal::is_base_of_template<Base, Derived>::value };

// Template qualifier of whether or not Dervied derives from a template Base.
template <template <typename...> class Base, typename Derived>
using is_base_of_template_e = std::enable_if_t<is_base_of_template_v<Base, Derived>, bool>;

// True if Type has a function named Invoke, false otherwise.
template <typename Type>
bool constexpr has_invoke_v{ !std::is_same<decltype(internal::has_invoke_helper(std::declval<Type>(), 0)), void*>::value };

// True if Type has a static function named Invoke, false otherwise.
template <typename Type>
bool constexpr has_static_invoke_v{
	has_invoke_v<Type> && 
	!std::is_member_function_pointer_v<decltype(internal::has_invoke_helper(std::declval<Type>(), 0))>
};

// Template qualifier of whether or not Type has a static function named Invoke.
template <typename Type>
using has_static_invoke_e = std::enable_if_t<has_static_invoke_v<Type>, bool>;

} // namespace type_traits

} // namespace ptgn