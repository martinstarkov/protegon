#include "protegon/debug.h"

namespace ptgn {

namespace debug {

namespace impl {

std::uint64_t Allocations::total_allocated_{ 0 };
std::uint64_t Allocations::total_freed_{ 0 };

} // namespace impl

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