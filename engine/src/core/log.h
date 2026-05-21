#pragma once

#include <cstdlib>
#include <iostream>
#include <ostream>
#include <source_location>
#include <string_view>

#include "core/util/concepts_stream.h"
#include "core/util/string.h"
#include "platform/debug_break.h"

namespace ptgn {

template <StreamWritable... Ts>
void Print(Ts&&... items) {
	((std::cout << std::forward<Ts>(items)), ...);
}

template <StreamWritable... Ts>
void PrintLine(Ts&&... items) {
	Print(std::forward<Ts>(items)...);
	std::cout << '\n';
}

namespace impl {

void DebugPrint(
	std::string_view prefix, std::string_view message = "",
	std::source_location where = std::source_location::current()
);

template <StreamWritable... Ts>
void Info(Ts&&... parts) {
	Print("INFO: ");
	PrintLine(std::forward<Ts>(parts)...);
}

template <StreamWritable... Ts>
void Warn(Ts&&... parts) {
	Print("WARN: ");
	PrintLine(std::forward<Ts>(parts)...);
}

template <StreamWritable... Ts>
[[noreturn]] void Error(Ts&&... parts) {
	DebugPrint("ERROR: ", ToString(std::forward<Ts>(parts)...));
	PTGN_DEBUGBREAK();
	std::abort();
}

} // namespace impl

} // namespace ptgn

#define PTGN_LOG(...)	::ptgn::PrintLine(__VA_ARGS__)
#define PTGN_INFO(...)	::ptgn::impl::Info(__VA_ARGS__)
#define PTGN_WARN(...)	::ptgn::impl::Warn(__VA_ARGS__)
#define PTGN_ERROR(...) ::ptgn::impl::Error(__VA_ARGS__)