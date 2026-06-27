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
#include "renderer/draw_context.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
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

namespace impl {

ShapeDrawParams GetShapeDrawParams(Entity entity) {
	return { .depth{ GetDepth(entity) },
			 .fill_style{ entity.GetOrDefault<FillStyle>() },
			 .origin{ GetDrawOrigin(entity) },
			 .entity_id{ entity.GetUUID() },
			 .effects{ impl::GetEffectParams(entity) } };
}

} // namespace impl

template <ShapeType T>
void DrawShape(DrawContext& ctx, Entity entity) {
	PTGN_ASSERT(entity.Has<T>(), "Entity does not have shape: ", type_name<T>());
	PTGN_ASSERT(entity.Has<Color>(), "Shape does not have color: ", type_name<T>());

	const auto& shape{ entity.Get<T>() };
	auto draw_transform{ GetDrawTransform(entity) };
	auto color{ entity.Get<Color>() };
	auto blend_mode{ GetBlendMode(entity) };
	auto tint{ GetTint(entity) };
	auto color_final{ Color::Multiply(color, tint) };

	auto params{ impl::GetShapeDrawParams(entity) };

	ctx.SetBlendMode(blend_mode);
	ctx.DrawShape(draw_transform, shape, color_final, params);
}

void RectDraw::Draw(DrawContext& ctx, Entity entity) {
	DrawShape<Rect>(ctx, entity);
}

void RoundedRectDraw::Draw(DrawContext& ctx, Entity entity) {
	DrawShape<RoundedRect>(ctx, entity);
}

void PolygonDraw::Draw(DrawContext& ctx, Entity entity) {
	DrawShape<Polygon>(ctx, entity);
}

void TriangleDraw::Draw(DrawContext& ctx, Entity entity) {
	DrawShape<Triangle>(ctx, entity);
}

void CircleDraw::Draw(DrawContext& ctx, Entity entity) {
	DrawShape<Circle>(ctx, entity);
}

void EllipseDraw::Draw(DrawContext& ctx, Entity entity) {
	DrawShape<Ellipse>(ctx, entity);
}

void ArcDraw::Draw(DrawContext& ctx, Entity entity) {
	DrawShape<Arc>(ctx, entity);
}

void LineDraw::Draw(DrawContext& ctx, Entity entity) {
	DrawShape<Line>(ctx, entity);
}

void CapsuleDraw::Draw(DrawContext& ctx, Entity entity) {
	DrawShape<Capsule>(ctx, entity);
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
		return Rect{ display_size.value() };
	}
	return GetShape(entity);
}

std::optional<Shape> GetShape(Entity entity) {
	return GetFirstMatchingVariant<
		Shape, Rect, Circle, Polygon, Triangle, Line, Ellipse, RoundedRect, Arc, Capsule>(entity);
}

template <typename TShapeDraw, ShapeType TShape>
Entity CreateShape(
	Scene& scene, Transform transform, TShape shape, Color color, FillStyle fill_style
) {
	auto entity{ scene.CreateEntity() };

	SetDraw<TShapeDraw>(entity);
	SetTransform(entity, transform);

	entity.Add<TShape>(std::move(shape));
	entity.Add<Color>(color);
	entity.Add<FillStyle>(fill_style);
	entity.Add<impl::Visible>(true);

	return entity;
}

Entity CreateRect(
	Scene& scene, Transform transform, V2_float size, Color color, FillStyle fill_style,
	Origin origin
) {
	auto rect{ CreateShape<RectDraw>(scene, transform, Rect{ size }, color, fill_style) };
	PTGN_DEFAULT_NAME(rect, "Rect");
	SetDrawOrigin(rect, origin);
	return rect;
}

Entity CreateRoundedRect(
	Scene& scene, Transform transform, V2_float size, float radius, Color color,
	FillStyle fill_style, Origin origin
) {
	auto rounded_rect{ CreateShape<RoundedRectDraw>(
		scene, transform, RoundedRect{ size, radius }, color, fill_style
	) };
	PTGN_DEFAULT_NAME(rounded_rect, "Rounded Rect");
	SetDrawOrigin(rounded_rect, origin);
	return rounded_rect;
}

Entity CreatePolygon(
	Scene& scene, Transform transform, const std::vector<V2_float>& vertices, Color color,
	FillStyle fill_style
) {
	auto polygon{
		CreateShape<PolygonDraw>(scene, transform, Polygon{ vertices }, color, fill_style)
	};
	PTGN_DEFAULT_NAME(polygon, "Polygon");
	return polygon;
}

Entity CreateTriangle(
	Scene& scene, Transform transform, V2_float a, V2_float b, V2_float c, Color color,
	FillStyle fill_style
) {
	auto triangle{
		CreateShape<TriangleDraw>(scene, transform, Triangle{ a, b, c }, color, fill_style)
	};
	PTGN_DEFAULT_NAME(triangle, "Triangle");
	return triangle;
}

Entity CreateCircle(
	Scene& scene, Transform transform, float radius, Color color, FillStyle fill_style
) {
	auto circle{ CreateShape<CircleDraw>(scene, transform, Circle{ radius }, color, fill_style) };
	PTGN_DEFAULT_NAME(circle, "Circle");
	return circle;
}

Entity CreateEllipse(
	Scene& scene, Transform transform, V2_float radii, Color color, FillStyle fill_style
) {
	auto ellipse{ CreateShape<EllipseDraw>(scene, transform, Ellipse{ radii }, color, fill_style) };
	PTGN_DEFAULT_NAME(ellipse, "Ellipse");
	return ellipse;
}

Entity CreateArc(
	Scene& scene, Transform transform, float arc_radius, Degrees start_angle, Degrees end_angle,
	bool clockwise, Color color, FillStyle fill_style
) {
	auto arc{ CreateShape<ArcDraw>(
		scene, transform, Arc{ arc_radius, start_angle, end_angle, clockwise }, color, fill_style
	) };
	PTGN_DEFAULT_NAME(arc, "Arc");
	return arc;
}

Entity CreateLine(
	Scene& scene, Transform transform, V2_float start, V2_float end, Color color, float width
) {
	auto line{ CreateShape<LineDraw>(scene, transform, Line{ start, end }, color, width) };
	PTGN_DEFAULT_NAME(line, "Line");
	return line;
}

Entity CreateCapsule(
	Scene& scene, Transform transform, V2_float start, V2_float end, float radius, Color color,
	FillStyle fill_style
) {
	auto capsule{
		CreateShape<CapsuleDraw>(scene, transform, Capsule{ start, end, radius }, color, fill_style)
	};
	PTGN_DEFAULT_NAME(capsule, "Capsule");
	return capsule;
}

} // namespace ptgn