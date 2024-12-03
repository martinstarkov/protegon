#pragma once

#include <filesystem>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <ostream>
#include <string_view>
#include <type_traits>

#include "utility/stats.h"
#include "utility/string.h"
#include "utility/time.h"
#include "utility/type_traits.h"

namespace ptgn::impl {

template <typename... T>
constexpr size_t NumberOfArgs(T... a) {
	return sizeof...(a);
}

} // namespace ptgn::impl

#define PTGN_NUMBER_OF_ARGS(...) ptgn::impl::NumberOfArgs(__VA_ARGS__)

namespace ptgn {

namespace impl {

// @param precision -1 for default precision.
template <typename... TArgs>
inline void PrintImpl(std::ostream& ostream, int precision, bool scientific, TArgs&&... items) {
	// TODO: Figure out how to add this since PTGN_ASSERT requires print.
	// PTGN_ASSERT(precision == -1 || precision >= 0, "Invalid print precision");
	using ptgn::operator<<;
	/*static_assert(
		(tt::is_stream_writable_v<std::ostream, TArgs> && ...),
		"PTGN_* argument must be stream writeable"
	);*/
	std::ios state{ nullptr };
	state.copyfmt(ostream);
	if (scientific) {
		if (precision != -1) {
			ostream << std::scientific << std::setprecision(precision);
		} else {
			ostream << std::scientific;
		}
	} else if (precision != -1) {
		ostream << std::fixed << std::setprecision(precision);
	}
	((ostream << std::forward<TArgs>(items)), ...);
	std::cout.copyfmt(state);
}

} // namespace impl

// Print desired items to the console. If a newline is desired, use PrintLine()
// instead.
template <typename... TArgs>
inline void Print(TArgs&&... items) {
	impl::PrintImpl(std::cout, -1, false, std::forward<TArgs>(items)...);
}

// Print desired items to the console and add a newline. If no newline is
// desired, use Print() instead.
template <typename... TArgs>
inline void PrintLine(TArgs&&... items) {
	Print(std::forward<TArgs>(items)...);
	std::cout << "\n";
}

inline void PrintLine() {
	std::cout << "\n";
}

// Print desired items to the console. If a newline is desired, use PrintLine()
// instead.
template <typename... TArgs>
inline void PrintPrecise(int precision, bool scientific, TArgs&&... items) {
	impl::PrintImpl(std::cout, precision, scientific, std::forward<TArgs>(items)...);
}

// Print desired items to the console and add a newline. If no newline is
// desired, use Print() instead.
template <typename... TArgs>
inline void PrintPreciseLine(int precision, bool scientific, TArgs&&... items) {
	impl::PrintImpl(std::cout, precision, scientific, std::forward<TArgs>(items)...);
	std::cout << "\n";
}

inline void PrintPreciseLine(
	[[maybe_unused]] int precision = -1, [[maybe_unused]] bool scientific = false
) {
	std::cout << "\n";
}

namespace debug {

// Print desired items to the console. If a newline is desired, use PrintLine()
// instead.
template <typename... TArgs, tt::stream_writable<std::ostream, TArgs...> = true>
inline void Print(TArgs&&... items) {
	ptgn::impl::PrintImpl(std::cerr, -1, false, std::forward<TArgs>(items)...);
}

// Print desired items to the console and add a newline. If no newline is
// desired, use Print() instead.
template <typename... TArgs, tt::stream_writable<std::ostream, TArgs...> = true>
inline void PrintLine(TArgs&&... items) {
	Print(std::forward<TArgs>(items)...);
	std::cerr << "\n";
}

inline void PrintLine() {
	std::cerr << "\n";
}

// Print desired items to the console. If a newline is desired, use PrintLine()
// instead.
template <typename... TArgs, tt::stream_writable<std::ostream, TArgs...> = true>
inline void PrintPrecise(int precision, bool scientific, TArgs&&... items) {
	ptgn::impl::PrintImpl(std::cerr, precision, scientific, std::forward<TArgs>(items)...);
}

// Print desired items to the console and add a newline. If no newline is
// desired, use Print() instead.
template <typename... TArgs, tt::stream_writable<std::ostream, TArgs...> = true>
inline void PrintPreciseLine(int precision, bool scientific, TArgs&&... items) {
	PrintPrecise(precision, scientific, std::forward<TArgs>(items)...);
	std::cerr << "\n";
}

inline void PrintPreciseLine() {
	std::cerr << "\n";
}

} // namespace debug

} // namespace ptgn

#define PTGN_LOG(...) ptgn::PrintLine(__VA_ARGS__);
#define PTGN_LOG_PRECISE(precision, scientific, ...) \
	ptgn::PrintPreciseLine(precision, scientific, __VA_ARGS__);
#define PTGN_INFO(...)                \
	{                                 \
		ptgn::Print("INFO: ");        \
		ptgn::PrintLine(__VA_ARGS__); \
	}

#define PTGN_INTERNAL_DEBUG_MESSAGE(prefix, ...)                                        \
	{                                                                                   \
		ptgn::debug::Print(                                                             \
			prefix, std::filesystem::path(__FILE__).filename().string(), ":", __LINE__, \
			std::invoke([&]() -> std::string_view {                                     \
				if (PTGN_NUMBER_OF_ARGS(__VA_ARGS__) > 0) {                             \
					return ": ";                                                        \
				} else {                                                                \
					return "";                                                          \
				}                                                                       \
			})                                                                          \
		);                                                                              \
		ptgn::debug::PrintLine(__VA_ARGS__);                                            \
	}

#define PTGN_WARN(...)                \
	{                                 \
		ptgn::Print("WARN: ");        \
		ptgn::PrintLine(__VA_ARGS__); \
	}

#define PTGN_ERROR(...)                                      \
	{                                                        \
		PTGN_INTERNAL_DEBUG_MESSAGE("ERROR: ", __VA_ARGS__); \
		PTGN_EXCEPTION("Error");                             \
	}