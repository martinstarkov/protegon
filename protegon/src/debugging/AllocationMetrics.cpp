#include "AllocationMetrics.h"

#include <iostream> // std::cout

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

std::uint32_t AllocationMetrics::total_allocated_{ 0 };
std::uint32_t AllocationMetrics::total_freed_{ 0 };

std::uint32_t AllocationMetrics::CurrentUsage() {
	return total_allocated_ - total_freed_;
}

void AllocationMetrics::Allocation(const std::size_t& size) {
	total_allocated_ += static_cast<std::uint32_t>(size);
}

void AllocationMetrics::Deallocation(const std::size_t& size) {
	total_freed_ += static_cast<std::uint32_t>(size);
}

void AllocationMetrics::PrintMemoryUsage() {
	std::cout << "Memory usage: " << CurrentUsage() << " bytes" << std::endl;
}