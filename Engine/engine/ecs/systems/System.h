#pragma once

#include <tuple> // std::tuple

#include "ecs/ECS.h"
#include "ecs/Components.h"

#include "renderer/Color.h"

class DebugDisplay {
public:
	static std::vector<std::pair<AABB, engine::Color>>& rectangles() { static std::vector<std::pair<AABB, engine::Color>> v; return v; }
	static std::vector<std::tuple<V2_double, V2_double, engine::Color>>& lines() { static std::vector<std::tuple<V2_double, V2_double, engine::Color>> v; return v; }
	static std::vector<std::tuple<V2_double, double, engine::Color>>& circles() { static std::vector<std::tuple<V2_double, double, engine::Color>> v; return v; }
};