#pragma once

#include <cstdlib> // std::size_t, std::malloc, std::free
#include <cstdint> // std::uint64_t

namespace ptgn {

namespace debug {

namespace impl {

struct Allocations {
	std::uint64_t total_allocated_{ 0 };
	std::uint64_t total_freed_{ 0 };
};

static Allocations allocations;

// Notifies AllocationMetrics that an allocation has been made.
inline void Allocation(const std::size_t& size) {
	allocations.total_allocated_ += static_cast<std::uint64_t>(size);
}

// Notifies AllocationMetrics that a deallocation has been made.
inline void Deallocation(const std::size_t& size) {
	allocations.total_freed_ += static_cast<std::uint64_t>(size);
}

} // namespace impl

/*
* @return Current heap allocated memory in bytes.
*/
inline std::uint64_t CurrentUsage() {
	return impl::allocations.total_allocated_ - impl::allocations.total_freed_;
}

/*
inline std::uint64_t Allocated() {
	return impl::allocations.total_allocated_;
}

inline std::uint64_t Freed() {
	return impl::allocations.total_freed_;
}
*/

} // namespace debug

} // namespace ptgn

void* operator new(std::size_t size) {
	ptgn::debug::impl::Allocation(size);
	return std::malloc(size);
}

void* operator new[](std::size_t size) {
	ptgn::debug::impl::Allocation(size);
	return std::malloc(size);
}

void operator delete(void* memory, std::size_t size) {
	ptgn::debug::impl::Deallocation(size);
	std::free(memory);
}

void operator delete[](void* memory, std::size_t size) {
	ptgn::debug::impl::Deallocation(size);
	std::free(memory);
}