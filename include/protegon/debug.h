#pragma once

#include <cstdlib> // std::size_t, std::malloc, std::free
#include <cstdint> // std::uint64_t

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