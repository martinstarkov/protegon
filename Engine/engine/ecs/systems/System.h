#pragma once

#include <tuple> // std::tuple

#include "ecs/ECS.h"
#include "ecs/Components.h"

#include "utils/Matrix.h"

#include "renderer/Color.h"

class DebugDisplay {
public:
	// aabb, color
	static std::vector<std::pair<AABB, engine::Color>>& rectangles() { static std::vector<std::pair<AABB, engine::Color>> v; return v; }
	// position, vertices, rotation_matrix, color
	static std::vector<std::tuple<V2_double, std::vector<V2_double>, Matrix<double, 2, 2>, engine::Color>>& polygons() { static std::vector<std::tuple<V2_double, std::vector<V2_double>, Matrix<double, 2, 2>, engine::Color>> v; return v; }
	// origin, destination, color
	static std::vector<std::tuple<V2_double, V2_double, engine::Color>>& lines() { static std::vector<std::tuple<V2_double, V2_double, engine::Color>> v; return v; }
	// center, radius, color
	static std::vector<std::tuple<V2_double, double, engine::Color>>& circles() { static std::vector<std::tuple<V2_double, double, engine::Color>> v; return v; }
};