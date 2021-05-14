#pragma once

#include <vector> // std::vector

#include "ecs/ECS.h"

#include "math/Hasher.h"

namespace engine {

struct TagComponent {
	TagComponent(int id) : id{ id } {}
	int id;
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
static bool HasExcludedTag(const ecs::Entity& entity, const std::vector<T>& tags) {
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