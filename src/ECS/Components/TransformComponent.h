#pragma once

#include "Component.h"

#include "../../Vec2D.h"

struct TransformComponent : public Component<TransformComponent> {
	Vec2D _position; // 8
	float _scale; // 4
	float _rotation; // 4
	TransformComponent(Vec2D position = Vec2D(), float scale = 1.0f, float rotation = 0.0f) : _position(position), _rotation(rotation), _scale(scale) {}
};