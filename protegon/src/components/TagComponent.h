#pragma once

#include "ecs/ECS.h"
#include "math/Hasher.h"

namespace engine {

struct TagComponent {
	TagComponent() = delete;
	TagComponent(std::size_t id) : id{ id } {}
	~TagComponent() = default;
	std::size_t id{ 0 };

	// Comparison between tag id and const char* tag.
	// These use the hasher internally.

	friend bool operator==(const TagComponent& lhs, const char* rhs) {
		return lhs.id == Hasher::HashCString(rhs);
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

} // namespace engine