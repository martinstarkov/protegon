#include "AllocationMetrics.h"

void* operator new(std::size_t size) {
	ptgn::AllocationMetrics::Allocation(size);
	return std::malloc(size);
}

void* operator new[](std::size_t size) {
	ptgn::AllocationMetrics::Allocation(size);
	return std::malloc(size);
}

void operator delete(void* memory, std::size_t size) {
	ptgn::AllocationMetrics::Deallocation(size);
	std::free(memory);
}

void  operator delete[](void* memory, std::size_t size) {
	ptgn::AllocationMetrics::Deallocation(size);
	std::free(memory);
}

namespace ptgn {

std::uint32_t AllocationMetrics::total_allocated_{ 0 };
std::uint32_t AllocationMetrics::total_freed_{ 0 };

} // namespace ptgn