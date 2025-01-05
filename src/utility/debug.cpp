#include "utility/debug.h"

#include <cstdint>
#include <cstdlib>
#include <new>
#include <string>
#include <string_view>

namespace ptgn {

namespace impl {

std::string TrimFunctionSignature(std::string_view signature) {
	const std::string begin{ "__cdecl" }; // trim return type
	const std::string end{ "(" };		  // trim function parameter list
	std::string new_signature{ signature };
	if (const auto position = new_signature.find(begin); position != std::string::npos) {
		new_signature = new_signature.substr(position + begin.length() + 1);
	}
	if (const auto position = new_signature.find(end); position != std::string::npos) {
		new_signature = new_signature.substr(0, position);
	}
	return new_signature;
}

} // namespace impl

namespace debug::impl {

std::uint64_t Allocations::total_allocated_{ 0 };
std::uint64_t Allocations::total_freed_{ 0 };

} // namespace debug::impl

} // namespace ptgn

void* operator new(std::size_t size) {
	if (size == 0) {
		++size;
	}

	ptgn::debug::impl::Allocation(size);

	if (void* ptr = std::malloc(size)) {
		return ptr;
	}

	throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
	if (size == 0) {
		++size;
	}

	ptgn::debug::impl::Allocation(size);

	if (void* ptr = std::malloc(size)) {
		return ptr;
	}

	throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept {
	std::free(memory);
}

void operator delete(void* memory, std::size_t size) noexcept {
	ptgn::debug::impl::Deallocation(size);
	std::free(memory);
}

void operator delete[](void* memory) noexcept {
	std::free(memory);
}

void operator delete[](void* memory, std::size_t size) noexcept {
	ptgn::debug::impl::Deallocation(size);
	std::free(memory);
}