#pragma once

#include "Component.h"

#include <engine/renderer/AABB.h>
#include <engine/renderer/Circle.h>

// TODO: Consider holding a pointer to the TransformComponent here instead of an AABB somehow?
// CONSIDERATIONS ^: How can you default construct a CollisionComponent? For serialization..

struct CollisionComponent {
	AABB collider;
	CollisionComponent(AABB collider = AABB()) : collider(collider) {}
	CollisionComponent(Vec2D position, Vec2D size) : collider(position, size) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const CollisionComponent& o) {
	j["collider"]["size"] = o.collider.size;
}

inline void from_json(const nlohmann::json& j, CollisionComponent& o) {
	if (j.find("collider") != j.end()) {
		if (j.find("collider")->find("size") != j.find("collider")->end()) {
			o.collider.size = j.at("collider").at("size").get<Vec2D>();
		}
	}
}