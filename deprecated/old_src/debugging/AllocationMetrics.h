#pragma once

#include <cstdlib> // std::size_t, std::malloc, std::free
#include <cstdint> // std::uint32_t

namespace ptgn {

namespace debug {

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
	// Prints the current memory usage in bytes to the console.
	static void PrintMemoryUsage();
private:
	static std::uint32_t total_allocated_;
	static std::uint32_t total_freed_;
};

} // namespace debug

} // namespace ptgn