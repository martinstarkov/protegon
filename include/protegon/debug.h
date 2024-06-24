#pragma once

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <sstream>
#include <tuple>

#include "protegon/log.h"
#include "protegon/platform.h"

#ifndef NDEBUG
#define PTGN_DEBUG
#endif

#ifdef PTGN_DEBUG
	#if defined(PTGN_PLATFORM_WINDOWS)
		#define PTGN_DEBUGBREAK() __debugbreak()
	#elif defined(PTGN_PLATFORM_LINUX)
		#include <signal.h>
		#define PTGN_DEBUGBREAK() raise(SIGTRAP)
	#endif
#else
	#define PTGN_DEBUGBREAK()
#endif

#define PTGN_EXPAND_MACRO(x) x
#define PTGN_STRINGIFY_MACRO(x) #x
#define PTGN_GET_MACRO(_1, NAME) NAME

#define PTGN_NUMBER_OF_ARGS(...) std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value

#define PTGN_INTERNAL_CHECK_WITHOUT_MSG(ss) ((void)0)
#define PTGN_INTERNAL_CHECK_WITH_MSG(ss, ...) ptgn::impl::PrintImpl(ss, __VA_ARGS__)

#define PTGN_INTERNAL_CHECK_GET_MACRO(ss, ...) PTGN_EXPAND_MACRO( PTGN_GET_MACRO(ss, __VA_ARGS__, PTGN_INTERNAL_CHECK_WITH_MSG, PTGN_INTERNAL_CHECK_WITHOUT_MSG) )

#define PTGN_CHECK(condition, ...) \
{ \
	if (!(condition)) { \
		std::stringstream ss; \
		ss << "Check '" << PTGN_STRINGIFY_MACRO(condition) << "' failed at " << std::filesystem::path(__FILE__).filename().string() << ":" << __LINE__; \
		ptgn::impl::PrintImpl(ss, ##__VA_ARGS__); \
		throw std::runtime_error(ss.str()); \
	} \
}

#ifdef PTGN_ENABLE_ASSERTS
	#define PTGN_ASSERT(condition, ...) \
	{ \
		if (!(condition)) { \
			std::stringstream ss; \
			ss << "Assertion '" << PTGN_STRINGIFY_MACRO(condition) << "' failed at " << std::filesystem::path(__FILE__).filename().string() << ":" << __LINE__; \
			ptgn::impl::PrintImpl(ss, ##__VA_ARGS__); \
			ptgn::debug::Print(ss); \
			PTGN_DEBUGBREAK(); \
			std::abort(); \
		} \
	}
#else
	#define PTGN_ASSERT(...) ((void)0)
#endif

namespace ptgn {

namespace debug {

namespace impl {

struct Allocations {
	static std::uint64_t total_allocated_;
	static std::uint64_t total_freed_;
};

// Notifies AllocationMetrics that an allocation has been made.
inline void Allocation(const std::size_t& size) {
	Allocations::total_allocated_ += static_cast<std::uint64_t>(size);
}

// Notifies AllocationMetrics that a deallocation has been made.
inline void Deallocation(const std::size_t& size) {
	Allocations::total_freed_ += static_cast<std::uint64_t>(size);
}

} // namespace impl

/*
 * @return Current heap allocated memory in bytes.
 */
// TODO: This seems to grow infinitely on Mac.
inline std::uint64_t CurrentUsage() {
	return impl::Allocations::total_allocated_ -
		   impl::Allocations::total_freed_;
}

/*
inline std::uint64_t Allocated() {
	return impl::Allocations::total_allocated_;
}

inline std::uint64_t Freed() {
	return impl::Allocations::total_freed_;
}
*/

} // namespace debug

} // namespace ptgn
