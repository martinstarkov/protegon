#pragma once

#include <memory>

class AllocationMetrics {
public:
	static std::uint32_t CurrentUsage();
	static void Allocation(const std::size_t& size);
	static void Deallocation(const std::size_t& size);
	static void PrintMemoryUsage();
private:
	static std::uint32_t _totalAllocated;
	static std::uint32_t _totalFreed;
};