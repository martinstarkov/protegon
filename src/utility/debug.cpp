#include "utility/debug.h"

namespace ptgn {

namespace impl {

std::string TrimFunctionSignature(const std::string& signature) {
	const std::string begin{ "__cdecl" }; // trim return type
	const std::string end{ "(" };		  // trim function parameter list
	std::string new_signature{ signature };
	if (const auto pos = new_signature.find(begin); pos != std::string::npos) {
		new_signature = new_signature.substr(pos + begin.length() + 1);
	}
	if (const auto pos = new_signature.find(end); pos != std::string::npos) {
		new_signature = new_signature.substr(0, pos);
	}
	return new_signature;
}

} // namespace impl

namespace debug {

namespace impl {

std::uint64_t Allocations::total_allocated_{ 0 };
std::uint64_t Allocations::total_freed_{ 0 };

} // namespace impl

} // namespace debug

} // namespace ptgn

void* operator new(std::size_t size) {
	ptgn::debug::impl::Allocation(size);
	return std::malloc(size);
}

void* operator new[](std::size_t size) {
	ptgn::debug::impl::Allocation(size);
	return std::malloc(size);
}

void operator delete(void* memory, std::size_t size) {
	ptgn::debug::impl::Deallocation(size);
	std::free(memory);
}

void operator delete[](void* memory, std::size_t size) {
	ptgn::debug::impl::Deallocation(size);
	std::free(memory);
}