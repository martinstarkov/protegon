#pragma once

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <sstream>
#include <tuple>

#include "protegon/log.h"
#include "utility/platform.h"
#include "utility/type_traits.h"

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
#define PTGN_ENABLE_ASSERTS
#else
#define PTGN_DEBUGBREAK()
#endif

#define PTGN_EXPAND_MACRO(x)			  x
#define PTGN_STRINGIFY_MACRO(x)			  #x
#define PTGN_GET_MACRO(_1, _2, NAME, ...) NAME

#define PTGN_NUMBER_OF_ARGS(...) std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value

#define PTGN_INTERNAL_PRINT_WITHOUT_MSG(ss)	  ((void)0)
#define PTGN_INTERNAL_PRINT_WITH_MSG(ss, ...) ptgn::impl::PrintImpl(ss, __VA_ARGS__)

#define PTGN_INTERNAL_PRINT_GET_MACRO(ss, ...)                                         \
	PTGN_EXPAND_MACRO(PTGN_GET_MACRO(                                                  \
		ss, __VA_ARGS__, PTGN_INTERNAL_PRINT_WITH_MSG, PTGN_INTERNAL_PRINT_WITHOUT_MSG \
	))                                                                                 \
	(ss, __VA_ARGS__)

namespace ptgn {

namespace impl {

// This class exists so that printline(ss, __VA_ARGS__) does not fail with 0 args.
struct StringStreamWriter {
	StringStreamWriter() = default;

	template <typename... TArgs, type_traits::stream_writable<std::stringstream, TArgs...> = true>
	void Write(TArgs&&... items) {
		((ss << std::forward<TArgs>(items)), ...);
	}

	template <typename... TArgs, type_traits::stream_writable<std::stringstream, TArgs...> = true>
	void WriteLine(TArgs&&... items) {
		Write(std::forward<TArgs>(items)...);
		ss << std::endl;
	}

	std::string Get() const {
		return ss.str();
	}

	std::stringstream ss;
};

} // namespace impl

} // namespace ptgn

#ifdef PTGN_ENABLE_ASSERTS
#define PTGN_ASSERT(condition, ...)                                                 \
	{                                                                               \
		if (!(condition)) {                                                         \
			ptgn::debug::Print(                                                     \
				"Assertion '", PTGN_STRINGIFY_MACRO(condition), "' failed at ",     \
				std::filesystem::path(__FILE__).filename().string(), ":", __LINE__, \
				[&]() -> const char* {                                              \
					if constexpr (PTGN_NUMBER_OF_ARGS(__VA_ARGS__) > 0) {           \
						return ": ";                                                \
					} else {                                                        \
						return "";                                                  \
					}                                                               \
				}()                                                                 \
			);                                                                      \
			ptgn::debug::PrintLine(__VA_ARGS__);                                    \
			PTGN_DEBUGBREAK();                                                      \
			std::abort();                                                           \
		}                                                                           \
	}
#else
#define PTGN_ASSERT(...) ((void)0)
#endif

#define PTGN_EXCEPTION(msg) throw std::runtime_error(#msg)

#define PTGN_CHECK(condition, ...)                                                  \
	{                                                                               \
		if (!(condition)) {                                                         \
			ptgn::impl::StringStreamWriter s;                                       \
			s.Write(                                                                \
				"Check '", PTGN_STRINGIFY_MACRO(condition), "' failed at ",         \
				std::filesystem::path(__FILE__).filename().string(), ":", __LINE__, \
				[&]() -> const char* {                                              \
					if constexpr (PTGN_NUMBER_OF_ARGS(__VA_ARGS__) > 0) {           \
						return ": ";                                                \
					} else {                                                        \
						return "";                                                  \
					}                                                               \
				}()                                                                 \
			);                                                                      \
			s.WriteLine(__VA_ARGS__);                                               \
			PTGN_EXCEPTION(s.Get());                                                \
		}                                                                           \
	}

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
