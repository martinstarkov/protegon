#include "AllocationMetrics.h"

#include <iostream>

void* operator new(std::size_t size) {
	AllocationMetrics::Allocation(size);
	return malloc(size);
}
void* operator new[](std::size_t size) {
	AllocationMetrics::Allocation(size);
	return malloc(size);
}
void operator delete(void* memory, std::size_t size) {
	AllocationMetrics::Deallocation(size);
	free(memory);
}
void  operator delete[](void* memory, std::size_t size) {
	AllocationMetrics::Deallocation(size);
	free(memory);
}

std::uint32_t AllocationMetrics::_totalAllocated = 0;
std::uint32_t AllocationMetrics::_totalFreed = 0;

std::uint32_t AllocationMetrics::CurrentUsage() {
	return _totalAllocated - _totalFreed;
}

void AllocationMetrics::Allocation(const std::size_t& size) {
	_totalAllocated += static_cast<std::uint32_t>(size);
}

void AllocationMetrics::Deallocation(const std::size_t& size) {
	_totalFreed += static_cast<std::uint32_t>(size);
}

void AllocationMetrics::PrintMemoryUsage() {
	std::cout << "Memory Usage: " << CurrentUsage() << " bytes" << std::endl;
}