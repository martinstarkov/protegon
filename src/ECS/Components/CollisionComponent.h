#pragma once

#include "Component.h"

#include "../../AABB.h"

// TODO: Figure out what data belongs here

struct CollisionComponent : public Component<CollisionComponent> {
	bool colliding;
	CollisionComponent(bool colliding = false) : colliding(colliding) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const CollisionComponent& o) {
	j["colliding"] = o.colliding;
}

inline void from_json(const nlohmann::json& j, CollisionComponent& o) {
	o = CollisionComponent(
		j.at("colliding").get<bool>()
	);
}