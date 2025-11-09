#include "math/geometry/shape.h"

#include <array>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include "core/ecs/components/draw.h"
#include "core/ecs/components/sprite.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/log.h"
#include "core/util/concepts.h"
#include "core/util/span.h"
#include "math/geometry/arc.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/ellipse.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/rounded_rect.h"
#include "math/geometry/triangle.h"
#include "math/geometry_utils.h"
#include "math/vector2.h"
#include "renderer/material/texture.h"

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

Transform OffsetByOrigin(const Shape& shape, const Transform& transform, const Entity& entity) {
	if (!std::holds_alternative<Rect>(shape)) {
		return transform;
	}
	const Rect& rect{ std::get<Rect>(shape) };
	auto draw_origin{ GetDrawOrigin(entity) };
	return rect.Offset(transform, draw_origin);
}

template <typename Variant, typename... Ts>
static std::optional<Variant> GetFirstMatchingVariant(const Entity& entity) {
	std::optional<Variant> result;

	(
		[&] {
			if (!result && entity.Has<Ts>()) {
				result = entity.Get<Ts>();
			}
		}(),
		...
	);

	return result;
}

std::optional<Shape> GetSpriteOrShape(const Entity& entity) {
	if (entity.Has<TextureHandle>()) {
		return Rect{ Sprite{ entity }.GetDisplaySize() };
	}
	return GetShape(entity);
}

std::vector<V2_float> GetWorldVertices(const Shape& shape, const Transform& transform) {
	return std::visit(
		[&](const auto& s) -> std::vector<V2_float> {
			using T = std::decay_t<decltype(s)>;

			if constexpr (IsAnyOf<T, Rect, Polygon, Triangle, Line>) {
				return ToVector(s.GetWorldVertices(transform));
			} else if constexpr (IsAnyOf<T, RoundedRect, Ellipse, Circle>) {
				return ToVector(s.GetWorldQuadVertices(transform));
			} else if constexpr (std::is_same_v<T, V2_float>) {
				return ToVector(Rect{ V2_float{ 1.0f } }.GetWorldVertices(transform));
			} else {
				PTGN_ERROR("Unknown shape type");
			}
		},
		shape
	);
}

std::optional<Shape> GetShape(const Entity& entity) {
	return GetFirstMatchingVariant<
		Shape, Rect, Circle, Polygon, Triangle, Line, Ellipse, RoundedRect, Arc, Capsule>(entity);
}

EdgeInfo GetEdges(const Shape& shape, const Transform& transform) {
	return std::visit(
		[&](const auto& s) {
			using T = std::decay_t<decltype(s)>;

			EdgeInfo info;

			if constexpr (IsAnyOf<T, RoundedRect, Ellipse, Circle>) {
				info.quad_approximation = true;
			}

			auto world_vertices{ GetWorldVertices(shape, transform) };

			info.edges = PointsToLines(world_vertices, true);

			return info;
		},
		shape
	);
}

} // namespace ptgn