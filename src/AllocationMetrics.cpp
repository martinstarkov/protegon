#include "AllocationMetrics.h"

void* operator new(std::size_t size) {
	AllocationMetrics::allocation(size);
	return malloc(size);
}

void operator delete(void* memory, std::size_t size) {
	AllocationMetrics::deallocation(size);
	free(memory);
}

std::uint32_t AllocationMetrics::_totalAllocated = 0;
std::uint32_t AllocationMetrics::_totalFreed = 0;

std::uint32_t AllocationMetrics::currentUsage() {
	return _totalAllocated - _totalFreed;
}

void AllocationMetrics::allocation(const std::size_t& size) {
	_totalAllocated += size;
}

void AllocationMetrics::deallocation(const std::size_t& size) {
	_totalFreed += size;
}

void AllocationMetrics::printMemoryUsage() {
	LOG("Memory Usage: " << currentUsage() << " bytes");
}

void AllocationMetrics::printMemoryBreakdown() {
	LOG("Allocated: " << _totalAllocated << " | Deallocated: " << _totalFreed);
}
