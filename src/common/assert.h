#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "debug/debug.h"
#include "debug/log.h"

#ifdef PTGN_DEBUG

#define PTGN_ENABLE_ASSERTS

#endif

#ifdef PTGN_ENABLE_ASSERTS

#ifdef PTGN_PLATFORM_MACOS

#define PTGN_FUNC_CHOOSER(                                                                      \
	_f0, _f1, _f2, _f3, _f4, _f5, _f6, _f7, _f8, _f9, _f10, _f11, _f12, _f13, _f14, _f15, _f16, \
	...                                                                                         \
)                                                                                               \
	_f16
#define PTGN_FUNC_RECOMPOSER(args) PTGN_FUNC_CHOOSER args
#define PTGN_CHOOSE_FROM_ARG_COUNT(F, ...)                                                         \
	PTGN_FUNC_RECOMPOSER(                                                                          \
		(__VA_ARGS__, F##_16, F##_15, F##_14, F##_13, F##_12, F##_11, F##_10, F##_9, F##_8, F##_7, \
		 F##_6, F##_5, F##_4, F##_3, F##_2, F##_1, )                                               \
	)
#define PTGN_NO_ARG_EXPANDER(FUNC) , , , , , , , , , , , , , , , , FUNC##_0
#define PTGN_MACRO_CHOOSER(FUNC, ...) \
	PTGN_CHOOSE_FROM_ARG_COUNT(FUNC, NO_ARG_EXPANDER __VA_ARGS__(FUNC))
#define PTGN_MULTI_MACRO(FUNC, ...) PTGN_MACRO_CHOOSER(FUNC, __VA_ARGS__)(__VA_ARGS__)

#define PTGN_ASSERT(...)				   PTGN_MULTI_MACRO(PTGN_ASSERT, __VA_ARGS__)
#define PTGN_ASSERT_1(x)				   PTGN_ASSERT_2(x, "")
#define PTGN_ASSERT_2(x, y)				   PTGN_ASSERT_3(x, y, "")
#define PTGN_ASSERT_3(x, y, z)			   PTGN_ASSERT_4(x, y, z, "")
#define PTGN_ASSERT_4(x, y, z, w)		   PTGN_ASSERT_5(x, y, z, w, "")
#define PTGN_ASSERT_5(x, y, z, w, e)	   PTGN_ASSERT_6(x, y, z, w, e, "")
#define PTGN_ASSERT_6(x, y, z, w, e, f)	   PTGN_ASSERT_7(x, y, z, w, e, f, "")
#define PTGN_ASSERT_7(x, y, z, w, e, f, g) PTGN_ASSERT_8(x, y, z, w, e, f, g, "")
#define PTGN_ASSERT_8(x, y, z, w, e, f, g, h)                                       \
	{                                                                               \
		if (!(x)) {                                                                 \
			PTGN_INTERNAL_DEBUG_MESSAGE("ASSERTION FAILED: ", y, z, w, e, f, g, h); \
			PTGN_DEBUGBREAK();                                                      \
			std::abort();                                                           \
		}                                                                           \
	}

#else // Not MacOS.

#define PTGN_ASSERT(condition, ...)                                         \
	{                                                                       \
		if (!(condition)) {                                                 \
			PTGN_INTERNAL_DEBUG_MESSAGE("ASSERTION FAILED: ", __VA_ARGS__); \
			PTGN_DEBUGBREAK();                                              \
			std::abort();                                                   \
		}                                                                   \
	}
#endif

#else // No asserts.

#define PTGN_ASSERT(...) ((void)0)

#endif

// TODO: Upgrade this to allow error messages in the future.
#define PTGN_EXCEPTION(message) throw std::runtime_error(message)

#define PTGN_CHECK(condition, ...)                                      \
	{                                                                   \
		if (!(condition)) {                                             \
			PTGN_INTERNAL_DEBUG_MESSAGE("CHECK FAILED: ", __VA_ARGS__); \
			PTGN_DEBUGBREAK();                                          \
			PTGN_EXCEPTION("Check failed");                             \
		}                                                               \
	}
