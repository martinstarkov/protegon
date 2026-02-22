#pragma once

#include <cstddef>
#include <cstdint>

namespace ptgn::impl {

class Allocations {
public:
	/*
	 * @return Current heap allocated memory in bytes.
	 */
	[[nodiscard]] static std::uint64_t CurrentUsage();

	[[nodiscard]] static std::uint64_t Allocated();

	[[nodiscard]] static std::uint64_t Freed();

	// Notifies AllocationMetrics that an allocation has been made.
	static void Allocation(const std::size_t& size);

	// Notifies AllocationMetrics that a deallocation has been made.
	static void Deallocation(const std::size_t& size) noexcept;

private:
	inline static std::uint64_t total_allocated_{ 0 };
	inline static std::uint64_t total_freed_{ 0 };
};

} // namespace ptgn::impl