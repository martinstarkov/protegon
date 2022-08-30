#pragma once

#include <iostream> // std::cout
#include <ostream> // std::ostream, std::endl;

#include "type_traits.h"

namespace ptgn {

// Print desired items to the console. If a newline is desired, use PrintLine() instead.
template <typename ...TArgs,
	type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void Print(TArgs&&... items) {
	((std::cout << std::forward<TArgs>(items)), ...);
}

// Print desired items to the console and add a newline. If no newline is desired, use Print() instead.
template <typename ...TArgs,
	type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void PrintLine(TArgs&&... items) {
	Print(std::forward<TArgs>(items)...);
	std::cout << '\n';
}

inline void PrintLine() {
	std::cout << '\n';
}

} // namespace ptgn