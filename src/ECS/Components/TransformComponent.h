#pragma once

#include "Component.h"

#include "../../Vec2D.h"

struct TransformComponent : public Component<TransformComponent> {
	Vec2D position; // 8
	double scale; // 4
	double rotation; // 4
	TransformComponent(Vec2D position = Vec2D(), double scale = 1.0, double rotation = 0.0) : position(position), rotation(rotation), scale(scale) {}
};