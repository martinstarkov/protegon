#include "common/assert.h"

#include <cstdint>
#include <cstdlib>
#include <new>
#include <string>
#include <string_view>

namespace ptgn::impl {

std::string TrimFunctionSignature(std::string_view signature) {
	const std::string begin{ "__cdecl" }; // trim return type
	const std::string end{ "(" };		  // trim function parameter list
	std::string new_signature{ signature };
	if (const auto position{ new_signature.find(begin) }; position != std::string::npos) {
		new_signature = new_signature.substr(position + begin.length() + 1);
	}
	if (const auto position{ new_signature.find(end) }; position != std::string::npos) {
		new_signature = new_signature.substr(0, position);
	}
	return new_signature;
}

std::uint64_t Allocations::total_allocated_{ 0 };
std::uint64_t Allocations::total_freed_{ 0 };

} // namespace ptgn::impl

void* operator new(std::size_t size) {
	if (size == 0) {
		++size;
	}

	ptgn::impl::Allocation(size);

	if (void* ptr{ std::malloc(size) }) {
		return ptr;
	}

	throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
	if (size == 0) {
		++size;
	}

	ptgn::impl::Allocation(size);

	if (void* ptr{ std::malloc(size) }) {
		return ptr;
	}

	throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept {
	std::free(memory);
}

void operator delete(void* memory, std::size_t size) noexcept {
	ptgn::impl::Deallocation(size);
	std::free(memory);
}

void operator delete[](void* memory) noexcept {
	std::free(memory);
}

void operator delete[](void* memory, std::size_t size) noexcept {
	ptgn::impl::Deallocation(size);
	std::free(memory);
}