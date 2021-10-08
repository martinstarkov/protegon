#pragma once

#include <cstdlib> // std::size_t

#include "math/Math.h"

namespace ptgn {

struct TagComponent {
	TagComponent() = delete;
	~TagComponent() = default;
	TagComponent(std::size_t id) : id{ id } {}
	std::size_t id{ 0 };

	// Comparison between tag id and const char* tag.
	// These use the hasher internally.

	friend bool operator==(const TagComponent& lhs, const char* rhs) {
		return lhs.id == math::Hash(rhs);
	}
	friend bool operator==(const char* lhs, const TagComponent& rhs) {
		return rhs == lhs;
	}
	friend bool operator!=(const TagComponent& lhs, const char* rhs) {
		return !(lhs == rhs);
	}
	friend bool operator!=(const char* lhs, const TagComponent& rhs) {
		return !(rhs == lhs);
	}
};

} // namespace ptgn