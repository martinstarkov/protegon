#include "debug/runtime/allocation.h"

#include <cstdint>
#include <exception>

namespace ptgn::impl {

std::uint64_t Allocations::CurrentUsage() {
	// TODO: This seems to grow infinitely on Mac.
	return total_allocated_ - total_freed_;
}

std::uint64_t Allocations::Allocated() {
	return total_allocated_;
}

std::uint64_t Allocations::Freed() {
	return total_freed_;
}

// Notifies AllocationMetrics that an allocation has been made.
void Allocations::Allocation(const std::size_t& size) {
	total_allocated_ += size;
}

// Notifies AllocationMetrics that a deallocation has been made.
void Allocations::Deallocation(const std::size_t& size) noexcept {
	total_freed_ += size;
}

} // namespace ptgn::impl

void* operator new(std::size_t size) {
	if (size == 0) {
		++size;
	}

	ptgn::impl::Allocations::Allocation(size);

	if (void* ptr{ std::malloc(size) }) {
		return ptr;
	}

	throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
	if (size == 0) {
		++size;
	}

	ptgn::impl::Allocations::Allocation(size);

	if (void* ptr{ std::malloc(size) }) {
		return ptr;
	}

	throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept {
	std::free(memory);
}

void operator delete(void* memory, std::size_t size) noexcept {
	ptgn::impl::Allocations::Deallocation(size);

	std::free(memory);
}

void operator delete[](void* memory) noexcept {
	std::free(memory);
}

void operator delete[](void* memory, std::size_t size) noexcept {
	ptgn::impl::Allocations::Deallocation(size);

	std::free(memory);
}