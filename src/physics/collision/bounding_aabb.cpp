#include "physics/collision/bounding_aabb.h"

#include <algorithm>
#include <type_traits>
#include <variant>
#include <vector>

#include "components/transform.h"
#include "math/geometry.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/triangle.h"
#include "math/vector2.h"

namespace ptgn {

bool BoundingAABB::Overlaps(const BoundingAABB& other) const {
	return !(
		max.x < other.min.x || min.x > other.max.x || max.y < other.min.y || min.y > other.max.y
	);
}

bool BoundingAABB::Overlaps(const V2_float& point) const {
	return !(max.x < point.x || min.x > point.x || max.y < point.y || min.y > point.y);
}

BoundingAABB GetBoundingAABB(const Shape& shape, const Transform& transform) {
	return std::visit(
		[&](const auto& s) -> BoundingAABB {
			using T = std::decay_t<decltype(s)>;

			std::vector<V2_float> vertices;
			if constexpr (std::is_same_v<T, Circle>) {
				auto v = s.GetExtents(transform);
				vertices.assign(v.begin(), v.end());
			} else if constexpr (std::is_same_v<T, Rect>) {
				auto v = s.GetWorldVertices(transform);
				vertices.assign(v.begin(), v.end());
			} else if constexpr (std::is_same_v<T, Polygon>) {
				vertices = s.GetWorldVertices(transform);
			} else if constexpr (std::is_same_v<T, Triangle>) {
				auto v = s.GetWorldVertices(transform);
				vertices.assign(v.begin(), v.end());
			} else if constexpr (std::is_same_v<T, Capsule>) {
				auto v = s.GetWorldVertices(transform);
				V2_float r{ s.GetRadius(transform) };
				// Treat capsule as two circles and a rectangle between them
				vertices.emplace_back(v[0] - r);
				vertices.emplace_back(v[0] + r);
				vertices.emplace_back(v[1] - r);
				vertices.emplace_back(v[1] + r);
			} else if constexpr (std::is_same_v<T, Line>) {
				auto v = s.GetWorldVertices(transform);
				vertices.assign(v.begin(), v.end());
			} else if constexpr (std::is_same_v<T, Point>) {
				// Assume Point is a single position with no size
				V2_float p = transform.position;
				vertices.emplace_back(p);
			}

			V2_float min{ vertices[0] };
			V2_float max{ vertices[0] };

			for (const auto& v : vertices) {
				min.x = std::min(min.x, v.x);
				min.y = std::min(min.y, v.y);
				max.x = std::max(max.x, v.x);
				max.y = std::max(max.y, v.y);
			}

			return { min, max };
		},
		shape
	);
}

} // namespace ptgn