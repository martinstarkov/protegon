#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace ptgn {

namespace impl {

template <typename Test, template <typename...> typename Ref>
struct is_specialization : std::false_type {};

template <template <typename...> typename Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref> : std::true_type {};

} // namespace impl

template <typename T, template <typename...> typename Ref>
concept SpecializationOf = impl::is_specialization<T, Ref>::value;

template <typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template <typename T>
concept EnumType = std::is_enum_v<T>;

template <typename T>
concept ScopedEnum = EnumType<T> && !std::is_convertible_v<T, int>;

template <typename From, typename To>
concept Narrowing = !requires(From f) { To{ f }; };

template <typename From, typename To>
concept NotNarrowing = !Narrowing<From, To>;

template <typename From, typename To>
concept NarrowingArithmetic = Arithmetic<From> && Arithmetic<To> && Narrowing<From, To>;

template <typename From, typename To>
concept NotNarrowingArithmetic = !NarrowingArithmetic<From, To>;

template <typename T>
concept ConvertibleToArithmetic = requires { static_cast<double>(std::declval<T>()); };

template <typename T>
concept MapLike = requires(T t, typename T::key_type key) {
	typename T::key_type;
	typename T::mapped_type;
	// requires T::value_type is like std::pair<const key_type, mapped_type>
	requires std::same_as<
		typename T::value_type, std::pair<const typename T::key_type, typename T::mapped_type>>;

	{ t.find(key) } -> std::same_as<typename T::iterator>;
	{ t[key] } -> std::same_as<typename T::mapped_type&>;
};

template <typename T, typename... Ts>
concept IsAnyOf = (std::is_same_v<T, Ts> || ...);

/// @brief No return type specified.

template <typename F, typename... Args>
concept Invocable = std::invocable<std::remove_cvref_t<F>, Args...>;

template <typename F, typename R, typename... Args>
concept InvocableR = std::regular_invocable<std::remove_cvref_t<F>, Args...> &&
					 std::same_as<std::invoke_result_t<std::remove_cvref_t<F>, Args...>, R>;

template <typename T, typename... TArgs>
concept BraceConstructible = requires(TArgs&&... args) { T{ std::forward<TArgs>(args)... }; };

} // namespace ptgn
