#pragma once

#include <cstdlib> // std::size_t
#include <cstdint> // std::uint32_t

class AllocationMetrics {
public:
	static std::uint32_t CurrentUsage();
	static void Allocation(const std::size_t& size);
	static void Deallocation(const std::size_t& size);
	static void PrintMemoryUsage();
private:
	static std::uint32_t total_allocated_;
	static std::uint32_t total_freed_;
};