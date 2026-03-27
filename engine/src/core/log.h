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
#include <vector>

#include "platform/debug_break.h"

#define PTGN_ABORT()   \
	PTGN_DEBUGBREAK(); \
	std::abort()

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
	os << "[";
	for (std::size_t i = 0; i < vec.size(); ++i) {
		os << vec[i];
		if (i + 1 < vec.size()) {
			os << ", ";
		}
	}
	os << "]";
	return os;
}

namespace ptgn::impl {

// Streamability concept (works for any type that can be piped to std::ostream)
template <typename T>
concept Stream = std::is_convertible_v<T, std::ostream&>;

template <typename T>
concept Loggable = requires(std::ostream& os, T value) {
	{ os << value } -> Stream;
};

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
template <typename... Ts>
[[nodiscard]] inline std::string ToString(Ts&&... parts) {
	static_assert((impl::Loggable<Ts> && ...), "All items must be Loggable");
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
		if (precision.has_value()) {
			os << std::setprecision(*precision);
		}
	} else if (precision.has_value()) {
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
template <typename... Ts>
inline void Print(Ts&&... items) {
	static_assert((impl::Loggable<Ts> && ...), "All items must be Loggable");
	((std::cout << std::forward<Ts>(items)), ...);
}

// Print + newline
template <typename... Ts>
inline void PrintLine(Ts&&... items) {
	static_assert((impl::Loggable<Ts> && ...), "All items must be Loggable");
	Print(std::forward<Ts>(items)...);
	std::cout << '\n';
}

// Precision/scientific variants
template <typename... Ts>
inline void PrintPrecise(std::optional<int> precision, bool scientific, Ts&&... items) {
	static_assert((impl::Loggable<Ts> && ...), "All items must be Loggable");
	impl::PrintImpl(std::cout, precision, scientific, [&](std::ostream& os) {
		((os << std::forward<Ts>(items)), ...);
	});
}

template <typename... Ts>
inline void PrintPreciseLine(std::optional<int> precision, bool scientific, Ts&&... items) {
	static_assert((impl::Loggable<Ts> && ...), "All items must be Loggable");
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
template <typename... Ts>
inline void Info(Ts&&... parts) {
	static_assert((impl::Loggable<Ts> && ...), "All items must be Loggable");
	Print("INFO: ");
	PrintLine(std::forward<Ts>(parts)...);
}

template <typename... Ts>
inline void Warn(Ts&&... parts) {
	static_assert((impl::Loggable<Ts> && ...), "All items must be Loggable");
	Print("WARN: ");
	PrintLine(std::forward<Ts>(parts)...);
}

template <typename... Ts>
[[noreturn]] inline void Error(Ts&&... parts) {
	static_assert((impl::Loggable<Ts> && ...), "All items must be Loggable");
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