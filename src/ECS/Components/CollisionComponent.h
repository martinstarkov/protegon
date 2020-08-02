#pragma once

#include "Component.h"

#include "../../AABB.h"
#include "../../Circle.h"

struct CollisionComponent : public Component<CollisionComponent> {
	AABB collider;
	CollisionComponent(AABB collider = AABB()) : collider(collider) {}
	CollisionComponent(Vec2D size) : collider({}, size) {}
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