#pragma once

#include <iostream> // std::cout
#include <ostream> // std::ostream, std::endl;

#include "utils/TypeTraits.h"

// TODO: Eventually move logging into class which can print to console and log to file.

namespace engine {

// Print desired items to the console. If a newline is desired, use PrintLine() instead.
template <typename ...TArgs,
	type_traits::are_stream_writable_e<std::ostream, TArgs...> = true>
inline void Print(TArgs&&... items) {
	((std::cout << std::forward<TArgs>(items)), ...);
}

// Print desired items to the console and add a newline. if no newline is desired, use Print() instead.
template <typename ...TArgs,
	type_traits::are_stream_writable_e<std::ostream, TArgs...> = true>
inline void PrintLine(TArgs&&... items) {
	Print(std::forward<TArgs>(items)...);
	std::cout << std::endl;
}

} // namespace engine