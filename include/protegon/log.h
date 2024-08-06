#pragma once

#include <filesystem>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>

#include "utility/type_traits.h"

namespace ptgn {

namespace impl {

template <typename... T>
inline constexpr size_t NumberOfArgs(T... a) {
	return sizeof...(a);
}

} // namespace impl

} // namespace ptgn

#define PTGN_NUMBER_OF_ARGS(...) ptgn::impl::NumberOfArgs(__VA_ARGS__)

namespace ptgn {

namespace impl {

template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void PrintImpl(std::ostream& ostream, TArgs&&... items) {
	((ostream << std::forward<TArgs>(items)), ...);
}

template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void PrintLineImpl(std::ostream& ostream, TArgs&&... items) {
	PrintImpl(ostream, std::forward<TArgs>(items)...);
	ostream << '\n';
}

inline void PrintLineImpl(std::ostream& ostream) {
	ostream << '\n';
}

} // namespace impl

// Print desired items to the console. If a newline is desired, use PrintLine()
// instead.
template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void Print(TArgs&&... items) {
	impl::PrintImpl(std::cout, std::forward<TArgs>(items)...);
}

// Print desired items to the console and add a newline. If no newline is
// desired, use Print() instead.
template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void PrintLine(TArgs&&... items) {
	impl::PrintLineImpl(std::cout, std::forward<TArgs>(items)...);
}

inline void PrintLine() {
	impl::PrintLineImpl(std::cout);
}

namespace debug {

// Print desired items to the console. If a newline is desired, use PrintLine()
// instead.
template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void Print(TArgs&&... items) {
	ptgn::impl::PrintImpl(std::cerr, std::forward<TArgs>(items)...);
}

// Print desired items to the console and add a newline. If no newline is
// desired, use Print() instead.
template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void PrintLine(TArgs&&... items) {
	ptgn::impl::PrintLineImpl(std::cerr, std::forward<TArgs>(items)...);
}

inline void PrintLine() {
	ptgn::impl::PrintLineImpl(std::cerr);
}

} // namespace debug

} // namespace ptgn

#define PTGN_LOG(...) ptgn::PrintLine(__VA_ARGS__);
#define PTGN_INFO(...)                \
	{                                 \
		ptgn::Print("INFO: ");        \
		ptgn::PrintLine(__VA_ARGS__); \
	}

#define PTGN_INTERNAL_DEBUG_MESSAGE(prefix, ...)                                            \
	{                                                                                       \
		ptgn::debug::Print(                                                                 \
				prefix, std::filesystem::path(__FILE__).filename().string(), ":", __LINE__, \
				[&]() -> const char* {                                                      \
					if (PTGN_NUMBER_OF_ARGS(__VA_ARGS__) > 0) {                             \
						return ": ";                                                        \
					} else {                                                                \
						return "";                                                          \
					}                                                                       \
				}()                                                                         \
		);                                                                                  \
		ptgn::debug::PrintLine(__VA_ARGS__);                                                \
	}

#define PTGN_WARN(...) PTGN_INTERNAL_DEBUG_MESSAGE("WARN: ", __VA_ARGS__)

#define PTGN_ERROR(...)                                      \
	{                                                        \
		PTGN_INTERNAL_DEBUG_MESSAGE("ERROR: ", __VA_ARGS__); \
		PTGN_EXCEPTION("Error");                             \
	}