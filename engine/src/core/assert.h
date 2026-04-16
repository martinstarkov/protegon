#pragma once

#include <cstdlib>
#include <optional>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>

#include "core/config.h"
#include "core/log.h"
#include "platform/debug_break.h"

#ifdef PTGN_DEBUG
#define PTGN_ENABLE_ASSERTS
#endif

#ifdef PTGN_ENABLE_ASSERTS

namespace ptgn::impl {

template <Loggable... Ts>
[[noreturn]] inline void AssertFail(
	std::string_view expr, const std::source_location& where, Ts&&... parts
) noexcept {
	std::ostringstream oss;
	((oss << std::forward<Ts>(parts)), ...); // stream all extra parts (if any)
	auto msg{ oss.str() };

	auto composed = msg.empty() ? std::string{ expr } : (std::string{ expr } + " | " + msg);

	DebugMessage("ASSERTION FAILED: ", std::optional<std::string>{ composed }, where);

	PTGN_DEBUGBREAK();
	std::abort();
}

} // namespace ptgn::impl

/// Usage:
///   PTGN_ASSERT(x > 0);
///   PTGN_ASSERT(ptr, "null ptr for key=", key);
///   PTGN_ASSERT(a == b, "a=", a, " b=", b);
#define PTGN_ASSERT(condition, ...)                                                    \
	do {                                                                               \
		if (!(condition)) [[unlikely]] {                                               \
			::ptgn::impl::AssertFail(                                                  \
				#condition, std::source_location::current() __VA_OPT__(, ) __VA_ARGS__ \
			);                                                                         \
		}                                                                              \
	} while (0)

#else // !PTGN_ENABLE_ASSERTS

#define PTGN_ASSERT(...) ((void)0)

#endif // PTGN_ENABLE_ASSERTS