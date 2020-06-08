#pragma once

#include "common.h"

#include <memory>

class AllocationMetrics {
public:
	static std::uint32_t currentUsage();
	static void allocation(const std::size_t& size);
	static void deallocation(const std::size_t& size);
	static void printMemoryUsage();
private:
	static std::uint32_t _totalAllocated;
	static std::uint32_t _totalFreed;
};

