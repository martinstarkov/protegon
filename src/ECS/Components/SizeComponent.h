#pragma once

#include "Component.h"

#include "../../Vec2D.h"

struct SizeComponent : public Component<SizeComponent> {
	Vec2D size;
	SizeComponent(Vec2D size = Vec2D()) : size(size) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const SizeComponent& o) {
	j["size"] = o.size;
}

inline void from_json(const nlohmann::json& j, SizeComponent& o) {
	o = SizeComponent(
		j.at("size").get<Vec2D>()
	);
}