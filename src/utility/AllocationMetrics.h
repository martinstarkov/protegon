#pragma once

#include <cstdlib> // std::size_t, std::malloc, std::free
#include <cstdint> // std::uint32_t

namespace ptgn {

class AllocationMetrics {
public:
	/*
	* @return Current heap allocated memory in bytes.
	*/
	static std::uint32_t CurrentUsage();
	// Notifies AllocationMetrics that an allocation has been made.
	static void Allocation(const std::size_t& size);
	// Notifies AllocationMetrics that a deallocation has been made.
	static void Deallocation(const std::size_t& size);
private:
	static std::uint32_t total_allocated_;
	static std::uint32_t total_freed_;
};

} // namespace ptgn