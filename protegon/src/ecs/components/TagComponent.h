#pragma once

#include <vector> // std::vector

#include "ecs/ECS.h"

struct TagComponent {
	TagComponent(int id) : id{ id } {}
	int id;
};

template <typename T>
static bool HasExcludedTag(const ecs::Entity& entity, const std::vector<T>& tags) {
	if (tags.size() > 0 && entity.HasComponent<TagComponent>()) {
		const auto id = entity.GetComponent<TagComponent>().id;
		for (const auto tag : tags) {
			if (id == tag) {
				return true;
			}
		}
	}
	return false;
}