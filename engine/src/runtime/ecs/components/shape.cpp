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
#include "runtime/scene/scene.h"

namespace ptgn {

Transform OffsetByOrigin(const Shape& shape, Transform transform, Entity entity) {
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
static std::optional<Variant> GetFirstMatchingVariant(Entity entity) {
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

std::optional<Shape> GetSpriteOrShape(Entity entity) {
	// TODO: Fix.
	// if (entity.Has<TextureHandle>()) {
	//	return Rect{ Sprite{ entity }.GetDisplaySize() };
	//}
	return GetShape(entity);
}

std::optional<Shape> GetShape(Entity entity) {
	return GetFirstMatchingVariant<
		Shape, Rect, Circle, Polygon, Triangle, Line, Ellipse, RoundedRect, Arc, Capsule>(entity);
}

// TODO: Fix.
// Entity CreateRect(
//	Scene& scene, V2_float position, V2_float size, Color color,
//	float line_width, Origin origin
//) {
//	auto rect{ scene.CreateEntity() };
//
//	SetDraw<Rect>(rect);
//	Show(rect);
//
//	SetPosition(rect, position);
//	rect.Add<Rect>(size);
//	SetDrawOrigin(rect, origin);
//
//	SetTint(rect, color);
//	rect.Add<LineWidth>(line_width);
//
//	return rect;
//}
//
// TODO: Fix.
// Entity CreatePolygon(
//	Scene& scene, V2_float position, const std::vector<V2_float>& vertices,
//	Color color, float line_width
//) {
//	auto polygon{ scene.CreateEntity() };
//
//	SetDraw<Polygon>(polygon);
//	Show(polygon);
//
//	SetPosition(polygon, position);
//	polygon.Add<Polygon>(vertices);
//
//	SetTint(polygon, color);
//	polygon.Add<LineWidth>(line_width);
//
//	return polygon;
//}
//
// TODO: Fix.
// Entity CreateCircle(
//	Scene& scene, V2_float position, float radius, Color color, float line_width
//) {
//	auto circle{ scene.CreateEntity() };
//
//	SetDraw<Circle>(circle);
//	Show(circle);
//
//	SetPosition(circle, position);
//	circle.Add<Circle>(radius);
//
//	SetTint(circle, color);
//	circle.Add<LineWidth>(line_width);
//
//	return circle;
//}

} // namespace ptgn