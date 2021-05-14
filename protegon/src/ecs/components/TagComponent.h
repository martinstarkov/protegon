#pragma once

#include <unordered_set> // std::unordered_set

#include "ecs/ECS.h"

#include "math/Hasher.h"

namespace engine {

struct TagComponent {
	TagComponent(std::size_t id) : id{ id } {}
	std::size_t id{ 0 };
};

// Comparison between tag id and const char* tag.
// This uses the hasher internally.

inline bool operator==(const TagComponent& lhs, const char* rhs) {
	return lhs.id == Hasher::HashCString(rhs);
}
inline bool operator==(const char* lhs, const TagComponent& rhs) {
	return rhs == lhs;
}

/*
* @return True if tag list contains the entity tag component id, false otherwise.
*/
template <typename T>
static bool HasExcludedTag(const ecs::Entity& entity, const std::unordered_set<T>& tags) {
	if (tags.size() > 0 && entity.HasComponent<TagComponent>()) {
		const auto id{ entity.GetComponent<TagComponent>().id };
		for (const auto tag : tags) {
			if (id == tag) {
				return true;
			}
		}
	}
	return false;
}

}