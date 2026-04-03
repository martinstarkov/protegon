#include "core/math/geometry/shape.h"

#include <ranges>
#include <type_traits>
#include <vector>

#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/geometry_utils.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"

namespace ptgn {

// TODO: Fix shape type serialization.

/*
void to_json(json& j, const InteractiveShape& shape) {
	// nlohmann::adl_serializer<impl::InteractiveType>::to_json(j, shape);
}

void from_json(const json& j, InteractiveShape& shape) {
	// nlohmann::adl_serializer<impl::InteractiveType>::from_json(j, shape);
}

void to_json(json& j, const ColliderShape& shape) {
	// nlohmann::adl_serializer<impl::ColliderType>::to_json(j, shape);
}

void from_json(const json& j, ColliderShape& shape) {
	// nlohmann::adl_serializer<impl::ColliderType>::from_json(j, shape);
}

void to_json(json& j, const Shape& shape) {
	// nlohmann::adl_serializer<impl::ShapeType>::to_json(j, shape);
}

void from_json(const json& j, Shape& shape) {
	// nlohmann::adl_serializer<impl::ShapeType>::from_json(j, shape);
}
*/

std::vector<V2_float> GetWorldVertices(const Shape& shape, Transform transform) {
	return shape.Visit([&]<typename T>(const T& s) -> std::vector<V2_float> {
		if constexpr (IsAnyOf<T, Rect, Polygon, Triangle, Line>) {
			return std::ranges::to<std::vector>(s.GetWorldVertices(transform));
		} else if constexpr (IsAnyOf<T, RoundedRect, Ellipse, Circle, Arc, Capsule>) {
			return std::ranges::to<std::vector>(s.GetWorldQuadVertices(transform));
		} else if constexpr (std::is_same_v<T, V2_float>) {
			return std::ranges::to<std::vector>(Rect{ V2_float{ 1.0f } }.GetWorldVertices(transform)
			);
		} else {
			static_assert(false, "Incomplete visitor!");
		}
	});
}

EdgeInfo GetEdges(const Shape& shape, Transform transform) {
	return shape.Visit([&]<typename T>(const T& s) {
		EdgeInfo info;

		if constexpr (IsAnyOf<T, RoundedRect, Ellipse, Circle>) {
			info.quad_approximation = true;
		}

		auto world_vertices{ GetWorldVertices(s, transform) };

		info.edges = PointsToLines(world_vertices, true);

		return info;
	});
}

} // namespace ptgn