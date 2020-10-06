#pragma once

#include <engine/ecs/ECS.h>
#include <engine/ecs/Components.h>

#include <engine/renderer/Color.h>

class DebugDisplay {
public:
	static std::vector<std::pair<AABB, engine::Color>>& rectangles() { static std::vector<std::pair<AABB, engine::Color>> v; return v; }
};