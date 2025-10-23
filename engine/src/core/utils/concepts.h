#pragma once

#include <concepts>
#include <istream>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ptgn {

template <typename... Ts>
concept NonEmptyPack = (sizeof...(Ts) > 0);

template <typename T>
concept Enum = std::is_enum_v<T>;

template <typename T>
concept ScopedEnum = std::is_enum_v<T> && !std::is_convertible_v<T, int>;

template <typename T, typename BaseType>
concept IsOrDerivedFrom = std::is_same_v<T, BaseType> || std::derived_from<T, BaseType>;

template <typename From, typename To>
concept Narrowing = !requires(From f) { To{ f }; };

template <typename From, typename To>
concept NotNarrowing = !Narrowing<From, To>;

template <typename From, typename To>
concept NarrowingArithmetic =
	std::is_arithmetic_v<From> && std::is_arithmetic_v<To> && Narrowing<From, To>;

template <typename From, typename To>
concept NotNarrowingArithmetic = !NarrowingArithmetic<From, To>;

template <typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template <typename T>
concept ConvertibleToArithmetic = requires { static_cast<double>(std::declval<T>()); };

template <typename T>
concept StreamWritable = requires(std::ostream& os, T value) {
	{ os << value } -> std::same_as<std::ostream&>;
};

template <typename T>
concept StreamReadable = requires(std::istream& is, T& value) {
	{ is >> value } -> std::same_as<std::istream&>;
};

template <typename T>
concept Streamable = StreamWritable<T> && StreamReadable<T>;

template <typename T>
concept StringLike = requires(T a) {
	{ std::string_view(a) }; // constructible from T (implicit or explicit)
};

template <typename T>
concept MapLike = requires(T t, typename T::key_type key) {
	typename T::key_type;
	typename T::mapped_type;
	// requires T::value_type is like std::pair<const key_type, mapped_type>
	requires std::same_as<
		typename T::value_type, std::pair<const typename T::key_type, typename T::mapped_type> >;

	{ t.find(key) } -> std::same_as<typename T::iterator>;
	{ t[key] } -> std::same_as<typename T::mapped_type&>;
};

template <typename From, typename To>
concept IsSafelyCastable = std::is_convertible_v<From, To>;

template <typename T, typename... Ts>
concept IsSafelyCastableToOneOf = (IsSafelyCastable<T, Ts> || ...);

template <typename Type, typename... Types>
concept AllSameAs = std::conjunction_v<std::is_same<Type, Types>...>;

template <typename T, typename... Ts>
concept IsAnyOf = (std::is_same_v<T, Ts> || ...);

} // namespace ptgn
