#pragma once

#include <cstdlib> // std::size_t

#include "math/Hash.h"

namespace ptgn {

struct Tag {
	Tag() = delete;
	~Tag() = default;
	Tag(std::size_t id) : id{ id } {}
	std::size_t id{ 0 };

	// Comparison between tag id and const char* tag.
	// These use the hasher internally.

	bool operator==(const char* rhs) const {
		return id == math::Hash(rhs);
	}
	bool operator!=(const char* rhs) const {
		return !operator==(rhs);
	}
};

} // namespace ptgn

bool operator==(const char* lhs, const ptgn::Tag& rhs) {
	return rhs == lhs;
}

bool operator!=(const char* lhs, const ptgn::Tag& rhs) {
	return !(rhs == lhs);
}