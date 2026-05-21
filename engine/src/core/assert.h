#pragma once

#include "core/config.h"

#ifdef PTGN_DEBUG
#define PTGN_ENABLE_ASSERTS
#endif

#ifdef PTGN_ENABLE_ASSERTS

#include <cstdlib>
#include <source_location>
#include <string>
#include <string_view>

#include "core/log.h"
#include "core/util/concepts_stream.h"
#include "core/util/string.h"
#include "platform/debug_break.h"

namespace ptgn::impl {

template <StreamWritable... Ts>
[[noreturn]] void AssertFail(
	std::string_view expr, const std::source_location& where, Ts&&... parts
) noexcept {
	auto msg{ ToString(std::forward<Ts>(parts)...) };

	ptgn::impl::DebugPrint(
		"ASSERTION FAILED: ", msg.empty() ? expr : std::string{ expr } + " | " + msg, where
	);

	PTGN_DEBUGBREAK();
	std::abort();
}

} // namespace ptgn::impl

/// @brief Usage:
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