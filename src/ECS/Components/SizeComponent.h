#pragma once

#include "Component.h"

#include "../../Vec2D.h"

struct SizeComponent : public Component<SizeComponent> {
	Vec2D _size;
	SizeComponent(Vec2D size = Vec2D()) : _size(size) {}
};