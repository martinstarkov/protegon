#include "core/math/geometry/shape.h"

#include <optional>
#include <variant>

#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/transform.h"
#include "runtime/ecs/components/shape.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

Transform OffsetByOrigin(const Shape& shape, const Transform& transform, const Entity& entity) {
	if (!std::holds_alternative<Rect>(shape)) {
		return transform;
	}
	const Rect& rect{ std::get<Rect>(shape) };
	// TODO: Fix.
	// auto draw_origin{ GetDrawOrigin(entity) };
	// return rect.Offset(transform, draw_origin);
	return {};
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
	// TODO: Fix.
	// if (entity.Has<TextureHandle>()) {
	//	return Rect{ Sprite{ entity }.GetDisplaySize() };
	//}
	return GetShape(entity);
}

std::optional<Shape> GetShape(const Entity& entity) {
	return GetFirstMatchingVariant<
		Shape, Rect, Circle, Polygon, Triangle, Line, Ellipse, RoundedRect, Arc, Capsule>(entity);
}

} // namespace ptgn