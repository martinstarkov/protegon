#include "runtime/graphics/shape.h"

#include <algorithm>
#include <optional>
#include <utility>
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
#include "core/util/type_info.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace {

template <typename Variant, typename... Ts>
std::optional<Variant> GetFirstMatchingVariant(Entity entity) {
	std::optional<Variant> result;

	(
		[&] {
			if (!result && entity.Has<Ts>()) {
				result = entity.Get<Ts>();
			}
		}(),
		...);

	return result;
}

} // namespace

template <ShapeType T>
void DrawShape(DrawContext& renderer, Entity entity) {
	PTGN_ASSERT(entity.Has<T>(), "Entity does not have shape: ", type_name<T>());

	const auto& shape{ entity.Get<T>() };
	auto draw_transform{ GetDrawTransform(entity) };
	auto tint{ GetTint(entity) };
	auto fill_style{ entity.GetOrDefault<FillStyle>() };
	auto draw_origin{ GetDrawOrigin(entity) };
	auto depth{ GetDepth(entity) };
	auto blend_mode{ GetBlendMode(entity) };
	auto entity_id{ entity.GetUUID() };

	// TODO: Fix.
	// renderer.DrawShape(
	//	shape, draw_transform, depth, tint, fill_style, draw_origin, blend_mode, entity_id
	//);
}

void RectDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Rect>(renderer, entity);
}

void RoundedRectDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<RoundedRect>(renderer, entity);
}

void PolygonDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Polygon>(renderer, entity);
}

void TriangleDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Triangle>(renderer, entity);
}

void CircleDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Circle>(renderer, entity);
}

void EllipseDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Ellipse>(renderer, entity);
}

void ArcDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Arc>(renderer, entity);
}

void LineDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Line>(renderer, entity);
}

void CapsuleDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Capsule>(renderer, entity);
}

Transform OffsetByOrigin(const Shape& shape, Transform transform, Entity entity) {
	if (!shape.HoldsAlternative<Rect>()) {
		return transform;
	}
	const auto& rect{ shape.Get<Rect>() };
	auto draw_origin{ GetDrawOrigin(entity) };
	return rect.Offset(transform, draw_origin);
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

template <typename TShapeDraw, ShapeType TShape>
Entity CreateShape(
	Scene& scene, V2_float position, TShape shape, Color color, FillStyle fill_style
) {
	auto entity{ scene.CreateEntity() };

	SetDraw<TShapeDraw>(entity);
	Show(entity, false);

	SetPosition(entity, position);
	entity.Add<TShape>(std::move(shape));

	SetTint(entity, color);
	entity.Add<FillStyle>(fill_style);

	return entity;
}

Entity CreateRect(
	Scene& scene, V2_float position, V2_float size, Color color, FillStyle fill_style, Origin origin
) {
	auto rect{ CreateShape<RectDraw>(scene, position, Rect{ size }, color, fill_style) };
	SetDrawOrigin(rect, origin);
	return rect;
}

Entity CreateRoundedRect(
	Scene& scene, V2_float position, V2_float size, float radius, Color color, FillStyle fill_style,
	Origin origin
) {
	auto rounded_rect{ CreateShape<RoundedRectDraw>(
		scene, position, RoundedRect{ size, radius }, color, fill_style
	) };
	SetDrawOrigin(rounded_rect, origin);
	return rounded_rect;
}

Entity CreatePolygon(
	Scene& scene, V2_float position, const std::vector<V2_float>& vertices, Color color,
	FillStyle fill_style
) {
	return CreateShape<PolygonDraw>(scene, position, Polygon{ vertices }, color, fill_style);
}

Entity CreateTriangle(
	Scene& scene, V2_float position, V2_float a, V2_float b, V2_float c, Color color,
	FillStyle fill_style
) {
	return CreateShape<TriangleDraw>(scene, position, Triangle{ a, b, c }, color, fill_style);
}

Entity CreateCircle(
	Scene& scene, V2_float position, float radius, Color color, FillStyle fill_style
) {
	return CreateShape<CircleDraw>(scene, position, Circle{ radius }, color, fill_style);
}

Entity CreateEllipse(
	Scene& scene, V2_float position, V2_float radii, Color color, FillStyle fill_style
) {
	return CreateShape<EllipseDraw>(scene, position, Ellipse{ radii }, color, fill_style);
}

Entity CreateArc(
	Scene& scene, V2_float position, float arc_radius, Degrees start_angle, Degrees end_angle,
	bool clockwise, Color color, FillStyle fill_style
) {
	return CreateShape<ArcDraw>(
		scene, position, Arc{ arc_radius, start_angle, end_angle, clockwise }, color, fill_style
	);
}

Entity CreateLine(
	Scene& scene, V2_float position, V2_float start, V2_float end, Color color, float width
) {
	return CreateShape<LineDraw>(scene, position, Line{ start, end }, color, width);
}

Entity CreateCapsule(
	Scene& scene, V2_float position, V2_float start, V2_float end, float radius, Color color,
	FillStyle fill_style
) {
	return CreateShape<CapsuleDraw>(
		scene, position, Capsule{ start, end, radius }, color, fill_style
	);
}

} // namespace ptgn