#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "utility/log.h"
#include "utility/platform.h"

#ifndef NDEBUG
#define PTGN_DEBUG
#endif

#define PTGN_DEBUGBREAK() ((void)0)

#ifdef PTGN_DEBUG

#define PTGN_ENABLE_ASSERTS

#if defined(PTGN_PLATFORM_WINDOWS)

#undef PTGN_DEBUGBREAK
#define PTGN_DEBUGBREAK() __debugbreak()

#elif defined(PTGN_PLATFORM_LINUX)

#include <signal.h>
#undef PTGN_DEBUGBREAK
#define PTGN_DEBUGBREAK() raise(SIGTRAP)

#endif

#endif

#define PTGN_EXPAND_MACRO(x)	x
#define PTGN_STRINGIFY_MACRO(x) #x

// Function signature macro: PTGN_FULL_FUNCTION_SIGNATURE
#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || \
	(defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
#define PTGN_FULL_FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#elif defined(__DMC__) && (__DMC__ >= 0x810)
#define PTGN_FULL_FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#elif (defined(__FUNCSIG__) || (_MSC_VER))
#define PTGN_FULL_FUNCTION_SIGNATURE __FUNCSIG__
#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || \
	(defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#define PTGN_FULL_FUNCTION_SIGNATURE __FUNCTION__
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#define PTGN_FULL_FUNCTION_SIGNATURE __FUNC__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#define PTGN_FULL_FUNCTION_SIGNATURE __func__
#elif defined(__cplusplus) && (__cplusplus >= 201103)
#define PTGN_FULL_FUNCTION_SIGNATURE __func__
#else
#define PTGN_FULL_FUNCTION_SIGNATURE "PTGN_FULL_FUNCTION_SIGNATURE unknown!"
#endif

namespace ptgn::impl {

// Returns the name of the function with the return type and function parameter list trimmed away.
[[nodiscard]] std::string TrimFunctionSignature(std::string_view signature);

} // namespace ptgn::impl

#define PTGN_FUNCTION_NAME() ptgn::impl::TrimFunctionSignature(PTGN_FULL_FUNCTION_SIGNATURE)

#ifdef PTGN_ENABLE_ASSERTS

	#ifdef PTGN_PLATFORM_MACOS

		#define PTGN_FUNC_CHOOSER(_f0, _f1, _f2, _f3, _f4, _f5, _f6, _f7, _f8, _f9, _f10, _f11, _f12, _f13, _f14, _f15, _f16, ...) _f16
		#define PTGN_FUNC_RECOMPOSER(args) PTGN_FUNC_CHOOSER args
		#define PTGN_CHOOSE_FROM_ARG_COUNT(F, ...) PTGN_FUNC_RECOMPOSER((__VA_ARGS__, \
					F##_16, F##_15, F##_14, F##_13, F##_12, F##_11, F##_10, F##_9, F##_8,\
					F##_7, F##_6, F##_5, F##_4, F##_3, F##_2, F##_1, ))
		#define PTGN_NO_ARG_EXPANDER(FUNC) ,,,,,,,,,,,,,,,,FUNC ## _0
		#define PTGN_MACRO_CHOOSER(FUNC, ...) PTGN_CHOOSE_FROM_ARG_COUNT(FUNC, NO_ARG_EXPANDER __VA_ARGS__ (FUNC))
		#define PTGN_MULTI_MACRO(FUNC, ...) PTGN_MACRO_CHOOSER(FUNC, __VA_ARGS__)(__VA_ARGS__)

		#define PTGN_ASSERT(...) PTGN_MULTI_MACRO(PTGN_ASSERT, __VA_ARGS__)
		#define PTGN_ASSERT_1(x) PTGN_ASSERT_2(x, "")
		#define PTGN_ASSERT_2(x, y) PTGN_ASSERT_3(x, y, "")
		#define PTGN_ASSERT_3(x, y, z) PTGN_ASSERT_4(x, y, z, "")
		#define PTGN_ASSERT_4(x, y, z, w) PTGN_ASSERT_5(x, y, z, w, "")
		#define PTGN_ASSERT_5(x, y, z, w, e) PTGN_ASSERT_6(x, y, z, w, e, "")
		#define PTGN_ASSERT_6(x, y, z, w, e, f) PTGN_ASSERT_7(x, y, z, w, e, f, "")
		#define PTGN_ASSERT_7(x, y, z, w, e, f, g) PTGN_ASSERT_8(x, y, z, w, e, f, g, "")
		#define PTGN_ASSERT_8(x, y, z, w, e, f, g, h)                               \
			{                                                                       \
				if (!(x)) {                                                 \
					PTGN_INTERNAL_DEBUG_MESSAGE("ASSERTION FAILED: ", y, z, w, e, f, g, h); \
					PTGN_DEBUGBREAK();                                              \
					std::abort();                                                   \
				}                                                                   \
			}
		
	#else
		#define PTGN_ASSERT(condition, ...)                                         \
			{                                                                       \
				if (!(condition)) {                                                 \
					PTGN_INTERNAL_DEBUG_MESSAGE("ASSERTION FAILED: ", __VA_ARGS__); \
					PTGN_DEBUGBREAK();                                              \
					std::abort();                                                   \
				}                                                                   \
			}
	#endif
#else
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

namespace ptgn::debug {

namespace impl {

struct Allocations {
	static std::uint64_t total_allocated_;
	static std::uint64_t total_freed_;
};

// Notifies AllocationMetrics that an allocation has been made.
inline void Allocation(const std::size_t& size) {
	Allocations::total_allocated_ += size;
}

// Notifies AllocationMetrics that a deallocation has been made.
inline void Deallocation(const std::size_t& size) noexcept {
	Allocations::total_freed_ += size;
}

} // namespace impl

/*
 * @return Current heap allocated memory in bytes.
 */
// TODO: This seems to grow infinitely on Mac.
inline std::uint64_t CurrentUsage() {
	return impl::Allocations::total_allocated_ - impl::Allocations::total_freed_;
}

/*
inline std::uint64_t Allocated() {
	return impl::Allocations::total_allocated_;
}

inline std::uint64_t Freed() {
	return impl::Allocations::total_freed_;
}
*/

} // namespace ptgn::debug
