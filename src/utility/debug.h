#pragma once

#include <cstdlib>
#include <cstdint>
#include <exception>
#include <sstream>

#include "protegon/log.h"
#include "platform.h"

#ifndef NDEBUG
#define PTGN_DEBUG
#endif

#define PTGN_INTERNAL_EXCEPTION_IMPL(...) \
	{ \
		std::stringstream ss; \
		PTGN_ERROR(ss, __VA_ARGS__); \
		throw std::exception(ss.str(), __FILE__, __LINE__); \
	}

#ifdef PTGN_DEBUG
	#if defined(PTGN_PLATFORM_WINDOWS)
		#define PTGN_DEBUGBREAK() __debugbreak()
	#elif defined(PTGN_PLATFORM_LINUX)
		#include <signal.h>
		#define PTGN_DEBUGBREAK() raise(SIGTRAP)
	#else
		// "Platform doesn't support debugbreak yet!"
	#endif
	#ifdef PTGN_DEBUGBREAK
		#undef PTGN_INTERNAL_EXCEPTION_IMPL
		#define PTGN_INTERNAL_EXCEPTION_IMPL(...) \
			{ \
				PTGN_ERROR(__VA_ARGS__); \
				PTGN_DEBUGBREAK(); \
			}
	#endif
	#define PTGN_ENABLE_ASSERTS
#else
	#define PTGN_DEBUGBREAK()
#endif

#define PTGN_CHECK(condition, ...) \
	if (!(condition)) { \
		PTGN_INTERNAL_EXCEPTION_IMPL(__VA_ARGS__); \
	}
#ifdef PTGN_ENABLE_ASSERTS
#define PTGN_ASSERT(condition, ...) \
    if (!(condition)) {          \
		PTGN_ERROR(__VA_ARGS__); \
        std::abort();            \
    }
#else
#define PTGN_ASSERT(condition, ...) do {} while(false)
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

} // namespace debug

} // namespace ptgn
