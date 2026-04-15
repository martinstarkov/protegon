#include "runtime/graphics/shape.h"

#include <optional>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/geometry/triangle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"

namespace ptgn {

template <ShapeType T>
void DrawShape(DrawContext& renderer, Entity entity, Camera) {
	PTGN_ASSERT(entity.Has<T>(), "Entity does not have shape: ", type_name<T>());

	const auto& shape{ entity.Get<T>() };
	auto draw_transform{ GetDrawTransform(entity) };
	auto tint{ GetTint(entity) };
	auto fill_style{ entity.GetOrDefault<FillStyle>() };
	auto draw_origin{ GetDrawOrigin(entity) };
	auto depth{ GetDepth(entity) };
	auto blend_mode{ GetBlendMode(entity) };

	renderer.DrawShape(shape, draw_transform, tint, fill_style, draw_origin, depth, blend_mode);
}

void CapsuleDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Capsule>(renderer, entity, camera);
}

void CircleDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Circle>(renderer, entity, camera);
}

void EllipseDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Ellipse>(renderer, entity, camera);
}

void ArcDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Arc>(renderer, entity, camera);
}

void PolygonDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Polygon>(renderer, entity, camera);
}

void RectDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Rect>(renderer, entity, camera);
}

void RoundedRectDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<RoundedRect>(renderer, entity, camera);
}

void TriangleDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Triangle>(renderer, entity, camera);
}

void LineDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Line>(renderer, entity, camera);
}

Transform OffsetByOrigin(const Shape& shape, Transform transform, Entity entity) {
	if (!shape.HoldsAlternative<Rect>()) {
		return transform;
	}
	const auto& rect{ shape.Get<Rect>() };
	auto draw_origin{ GetDrawOrigin(entity) };
	return rect.Offset(transform, draw_origin);
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
	if (entity.Has<Texture>()) {
		auto display_size{ GetDisplaySize(entity) };
		PTGN_ASSERT(display_size.has_value(), "Entity with texture must have a display size");
		return Rect{ *display_size };
	}
	return GetShape(entity);
}

std::optional<Shape> GetShape(Entity entity) {
	return GetFirstMatchingVariant<
		Shape, Rect, Circle, Polygon, Triangle, Line, Ellipse, RoundedRect, Arc, Capsule>(entity);
}

Entity CreateRect(
	Scene& scene, V2_float position, V2_float size, Color color, FillStyle fill_style, Origin origin
) {
	auto rect{ scene.CreateEntity() };

	SetDraw<RectDraw>(rect);
	Show(rect, false);

	SetPosition(rect, position);
	rect.Add<Rect>(size);
	SetDrawOrigin(rect, origin);

	SetTint(rect, color);
	rect.Add<FillStyle>(fill_style);

	return rect;
}

Entity CreatePolygon(
	Scene& scene, V2_float position, const std::vector<V2_float>& vertices, Color color,
	FillStyle fill_style
) {
	auto polygon{ scene.CreateEntity() };

	SetDraw<PolygonDraw>(polygon);
	Show(polygon, false);

	SetPosition(polygon, position);
	polygon.Add<Polygon>(vertices);

	SetTint(polygon, color);
	polygon.Add<FillStyle>(fill_style);

	return polygon;
}

Entity CreateCircle(
	Scene& scene, V2_float position, float radius, Color color, FillStyle fill_style
) {
	auto circle{ scene.CreateEntity() };

	SetDraw<CircleDraw>(circle);
	Show(circle, false);

	SetPosition(circle, position);
	circle.Add<Circle>(radius);

	SetTint(circle, color);
	circle.Add<FillStyle>(fill_style);

	return circle;
}

Entity CreateArc(
	Scene& scene, V2_float position, float arc_radius, Degrees start_angle, Degrees end_angle,
	bool clockwise, Color color, FillStyle fill_style
) {
	auto arc{ scene.CreateEntity() };

	SetDraw<ArcDraw>(arc);
	Show(arc, false);

	SetPosition(arc, position);
	arc.Add<Arc>(arc_radius, start_angle, end_angle, clockwise);

	SetTint(arc, color);
	arc.Add<FillStyle>(fill_style);

	return arc;
}

} // namespace ptgn