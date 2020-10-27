#pragma once

#include "renderer/Ray2D.h"
#include "renderer/Shape.h"

#include "utils/Vector2.h"

struct AABB : Shape<AABB> {
	V2_double position;
	V2_double size;
	AABB() : position{ 0.0, 0.0 }, size{ 0.0, 0.0 } {}
	AABB(V2_double position, V2_double size) : position{ position }, size{ size } {}
	AABB(int x, int y, int w, int h) : position{ x, y }, size{ w, h } {}
	AABB ExpandedBy(const AABB& other) const {
		return { position - other.size / 2.0, size + other.size };
	}
	V2_double Center() {
		return position + size / 2.0;
	}
	V2_double Center() const {
		return position + size / 2.0;
	}
	static AABB MinkowskiDifference(const AABB& a, const AABB& b) {
		return AABB{ a.position - b.position + b.size, a.size + b.size };
	}
	bool MinkowskiOverlap() {
		return (position.x <= 0.0 &&
				position.x + size.x >= 0.0 &&
				position.y <= 0.0 &&
				position.y + size.y >= 0.0);
	}
	V2_double MinkowskiPenetration(V2_double point = { 0.0, 0.0 }) const {
		auto max = position + size;
		double min_distance = std::abs(point.x - position.x);
		V2_double penetration = { position.x, point.y };
		if (std::abs(max.x - point.x) < min_distance) {
			min_distance = std::abs(max.x - point.x);
			penetration = { max.x, point.y };
		}
		if (std::abs(max.y - point.y) < min_distance) {
			min_distance = std::abs(max.y - point.y);
			penetration = { point.x, max.y };
		}
		if (std::abs(position.y - point.y) < min_distance) {
			min_distance = std::abs(position.y - point.y);
			penetration = { point.x, position.y };
		}
		return penetration;
	}
	operator bool() const {
		return position || size;
	}
	friend std::ostream& operator<<(std::ostream& os, const AABB& obj) {
		os << '[' << obj.position << ',' << obj.size << ']';
		return os;
	}
};

namespace engine {

namespace math {

// Determine if a point lies inside an AABB.
inline bool PointVsAABB(const V2_double& point, const AABB& a) {
	return (point.x >= a.position.x &&
			point.y >= a.position.y &&
			point.x < a.position.x + a.size.x &&
			point.y < a.position.y + a.size.y);
}

} // namespace math

} // namespace engine