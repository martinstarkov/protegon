#pragma once

#include <iostream> // std::cout
#include <ostream> // std::ostream, std::endl;

namespace ptgn {

namespace type_traits {

namespace internal {

// Source: https://stackoverflow.com/a/49026811
template<typename Stream, typename Type, typename = void>
struct is_stream_writable : std::false_type {};
template<typename Stream, typename Type>
struct is_stream_writable<Stream, Type, std::void_t<decltype(std::declval<Stream&>() << std::declval<Type>()) >> : std::true_type {};

} // namespace internal

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

// Print desired items to the console. If a newline is desired, use PrintLine() instead.
template <typename ...TArgs,
	type_traits::are_stream_writable_e<std::ostream, TArgs...> = true>
inline void Print(TArgs&&... items) {
	((std::cout << std::forward<TArgs>(items)), ...);
}

// Print desired items to the console and add a newline. If no newline is desired, use Print() instead.
template <typename ...TArgs,
	type_traits::are_stream_writable_e<std::ostream, TArgs...> = true>
inline void PrintLine(TArgs&&... items) {
	Print(std::forward<TArgs>(items)...);
	std::cout << '\n';
}

inline void PrintLine() {
	std::cout << '\n';
}

} // namespace ptgn