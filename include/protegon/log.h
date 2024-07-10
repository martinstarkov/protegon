#pragma once

#include <iostream>
#include <ostream>

#include "type_traits.h"

namespace ptgn {

namespace impl {

template <
	typename... TArgs,
	type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void PrintImpl(std::ostream& ostream, TArgs&&... items) {
	((ostream << std::forward<TArgs>(items)), ...);
}

template <
	typename... TArgs,
	type_traits::stream_writable<std::ostream, TArgs...> = true>
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
template <
	typename... TArgs,
	type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void Print(TArgs&&... items) {
	impl::PrintImpl(std::cout, std::forward<TArgs>(items)...);
}

// Print desired items to the console and add a newline. If no newline is
// desired, use Print() instead.
template <
	typename... TArgs,
	type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void PrintLine(TArgs&&... items) {
	impl::PrintLineImpl(std::cout, std::forward<TArgs>(items)...);
}

inline void PrintLine() {
	impl::PrintLineImpl(std::cout);
}

namespace debug {

// Print desired items to the console. If a newline is desired, use PrintLine()
// instead.
template <
	typename... TArgs,
	type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void Print(TArgs&&... items) {
	ptgn::impl::PrintImpl(std::cerr, std::forward<TArgs>(items)...);
}

// Print desired items to the console and add a newline. If no newline is
// desired, use Print() instead.
template <
	typename... TArgs,
	type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void PrintLine(TArgs&&... items) {
	ptgn::impl::PrintLineImpl(std::cerr, std::forward<TArgs>(items)...);
}

inline void PrintLine() {
	ptgn::impl::PrintLineImpl(std::cerr);
}

} // namespace debug

} // namespace ptgn

#define PTGN_LOG(...) ptgn::PrintLine(__VA_ARGS__)
#define PTGN_INFO(...) ptgn::PrintLine("INFO: ", __VA_ARGS__)
#define PTGN_WARN(...) ptgn::debug::PrintLine("WARN: ", __FILE__, "#", __LINE__, ": ", __VA_ARGS__)
#define PTGN_ERROR(...)	ptgn::debug::PrintLine("ERROR: ", __FILE__, "#", __LINE__, ": ", __VA_ARGS__)