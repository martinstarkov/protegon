#pragma once

#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <iostream>
#include <optional>
#include <ostream>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>

#include "debug/core/debug_break.h"

namespace ptgn::impl {

// Streamability concept (works for any type that can be piped to std::ostream)
template <class T>
concept Loggable = requires(std::ostream& os, T&& v) { os << std::forward<T>(v); };

// Save/restore ostream formatting state (RAII)
class OStreamStateGuard {
public:
	explicit OStreamStateGuard(std::ostream& os) :
		os_{ os }, flags_{ os.flags() }, precision_{ os.precision() }, fill_{ os.fill() } {}

	~OStreamStateGuard() {
		os_.flags(flags_);
		os_.precision(precision_);
		os_.fill(fill_);
	}

	OStreamStateGuard(const OStreamStateGuard&)			   = delete;
	OStreamStateGuard& operator=(const OStreamStateGuard&) = delete;

private:
	std::ostream& os_;
	std::ios_base::fmtflags flags_;
	std::streamsize precision_;
	char fill_;
};

// Compose any number of Loggable parts into a std::string
template <Loggable... Ts>
[[nodiscard]] inline std::string ToString(Ts&&... parts) {
	std::ostringstream oss;
	((oss << std::forward<Ts>(parts)), ...);
	return std::move(oss).str();
}

// Print to an ostream with optional precision and scientific formatting.
// precision == std::nullopt -> leave precision as-is.
inline void PrintImpl(
	std::ostream& os, std::optional<int> precision, bool scientific, auto&& write
) {
	OStreamStateGuard guard{ os };
	if (scientific) {
		os.setf(std::ios::scientific, std::ios::floatfield);
		if (precision) {
			os << std::setprecision(*precision);
		}
	} else if (precision) {
		os.setf(std::ios::fixed, std::ios::floatfield);
		os << std::setprecision(*precision);
	}
	write(os);
}

inline std::string Basename(std::string_view path) {
	try {
		return std::filesystem::path(path).filename().string();
	} catch (...) {
		return std::string(path);
	}
}

// Print any number of Loggable items to std::cout (no newline).
template <impl::Loggable... Ts>
inline void Print(Ts&&... items) {
	((std::cout << std::forward<Ts>(items)), ...);
}

// Print + newline
template <impl::Loggable... Ts>
inline void PrintLine(Ts&&... items) {
	Print(std::forward<Ts>(items)...);
	std::cout << '\n';
}

// Precision/scientific variants
template <impl::Loggable... Ts>
inline void PrintPrecise(std::optional<int> precision, bool scientific, Ts&&... items) {
	impl::PrintImpl(std::cout, precision, scientific, [&](std::ostream& os) {
		((os << std::forward<Ts>(items)), ...);
	});
}

template <impl::Loggable... Ts>
inline void PrintPreciseLine(std::optional<int> precision, bool scientific, Ts&&... items) {
	PrintPrecise(precision, scientific, std::forward<Ts>(items)...);
	std::cout << '\n';
}

inline void DebugMessage(
	std::string_view prefix, std::optional<std::string> message = std::nullopt,
	std::source_location where = std::source_location::current()
) {
	const auto file = impl::Basename(where.file_name());
	if (message && !message->empty()) {
		PrintLine(prefix, file, ':', where.line(), " in ", where.function_name(), ": ", *message);
	} else {
		PrintLine(prefix, file, ':', where.line(), " in ", where.function_name());
	}
}

// Convenience log levels (no location)
template <impl::Loggable... Ts>
inline void Info(Ts&&... parts) {
	Print("INFO: ");
	PrintLine(std::forward<Ts>(parts)...);
}

template <impl::Loggable... Ts>
inline void Warn(Ts&&... parts) {
	Print("WARN: ");
	PrintLine(std::forward<Ts>(parts)...);
}

template <impl::Loggable... Ts>
[[noreturn]] inline void Error(Ts&&... parts) {
	// Include location for errors.
	DebugMessage("ERROR: ", impl::ToString(std::forward<Ts>(parts)...));
	PTGN_ABORT();
}

} // namespace ptgn::impl

#define PTGN_LOG(...) ::ptgn::impl::PrintLine(__VA_ARGS__)
#define PTGN_LOG_PRECISE(precision, scientific, ...) \
	::ptgn::PrintPreciseLine((precision), (scientific)__VA_OPT__(, ) __VA_ARGS__)
#define PTGN_INFO(...)	::ptgn::impl::Info(__VA_ARGS__)
#define PTGN_WARN(...)	::ptgn::impl::Warn(__VA_ARGS__)
#define PTGN_ERROR(...) ::ptgn::impl::Error(__VA_ARGS__)
#define PTGN_ABORT()   \
	PTGN_DEBUGBREAK(); \
	std::abort()