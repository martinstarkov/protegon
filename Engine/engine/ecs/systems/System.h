#pragma once

#include "ecs/ECS.h"
#include "ecs/Components.h"

#include "renderer/Color.h"

class DebugDisplay {
public:
	static std::vector<std::pair<AABB, engine::Color>>& rectangles() { static std::vector<std::pair<AABB, engine::Color>> v; return v; }
};