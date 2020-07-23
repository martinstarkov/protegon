#pragma once

#include "Component.h"

#include "../../Vec2D.h"

struct DragComponent : public Component<DragComponent> {
	Vec2D drag;
	DragComponent(Vec2D drag = Vec2D()) : drag(drag) {}
	DragComponent(double drag) : drag(drag) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const DragComponent& o) {
	j["drag"] = o.drag;
}

inline void from_json(const nlohmann::json& j, DragComponent& o) {
	o = DragComponent(
		j.at("drag").get<Vec2D>()
	);
}