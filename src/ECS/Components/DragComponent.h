#pragma once

#include "Component.h"

#include "../../Vec2D.h"

struct DragComponent : public Component<DragComponent> {
	Vec2D drag;
	DragComponent(Vec2D drag = Vec2D()) : drag(drag) {}
	DragComponent(double drag) : drag(drag) {}
};