#pragma once

#include <cstdlib> // std::size_t

#include "math/Hash.h"

namespace ptgn {

namespace component {

struct Tag {
	Tag() = delete;
	~Tag() = default;
	Tag(std::size_t id) : id{ id } {}
	std::size_t id{ 0 };

	// Comparison between tag id and const char* tag.
	// These use the hasher internally.

	friend bool operator==(const Tag& lhs, const char* rhs) {
		return lhs.id == math::Hash(rhs);
	}
	friend bool operator==(const char* lhs, const Tag& rhs) {
		return rhs == lhs;
	}
	friend bool operator!=(const Tag& lhs, const char* rhs) {
		return !(lhs == rhs);
	}
	friend bool operator!=(const char* lhs, const Tag& rhs) {
		return !(rhs == lhs);
	}
};

} // namespace component

} // namespace ptgn