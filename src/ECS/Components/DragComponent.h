#pragma once

#include "Component.h"

#include "../../Vec2D.h"

struct DragComponent : public Component<DragComponent> {
	Vec2D _drag;
	DragComponent(Vec2D drag = Vec2D()) : _drag(drag) {}
	DragComponent(float drag) : _drag(drag, drag) {}
};