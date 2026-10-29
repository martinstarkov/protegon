#include "runtime/graphics/shape.h"

#include <algorithm>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
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
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_creation_context.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
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
			 .origin	= entity.GetOrDefault<Origin>(),
			 .entity_id = entity.Get<UUID>(),
			 .effects{ impl::GetEffectParams(entity) } };
}

} // namespace impl

template <ShapeType T>
void DrawShape(DrawContext& ctx, Entity entity) {
	if (!entity.Has<T>()) {
		PTGN_WARN(
			"Shape entity cannot be drawn without ", type_name_without_namespaces<T>(), " component"
		);
		return;
	}

	const auto& shape{ entity.Get<T>() };
	auto draw_transform{ GetDrawTransform(entity) };
	auto color{ entity.GetOrDefault<Color>(color::White) };
	auto blend_mode{ GetBlendMode(entity) };
	auto tint{ GetTint(entity) };
	auto color_final{ Color::Multiply(color, tint) };

	auto params{ impl::GetShapeDrawParams(entity) };

	if constexpr (std::is_same_v<T, Line>) {
		if (!entity.Has<FillStyle>()) {
			params.fill_style = FillStyle{ 1.0f };
		}
	}

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
	auto origin{ entity.GetOrDefault<Origin>() };
	return rect.Offset(transform, origin);
}

std::optional<Shape> GetShape(Entity entity) {
	return GetFirstMatchingVariant<
		Shape, Rect, Circle, Polygon, Triangle, Line, Ellipse, RoundedRect, Arc, Capsule>(entity);
}

template <typename TShapeDraw, ShapeType TShape>
Entity CreateShape(
	EntityCreationContext context, Transform transform, TShape shape, Color color,
	FillStyle fill_style
) {
	auto entity{ context.CreateEntity() };

	entity.Add<Transform>(transform);
	entity.Add<TShape>(std::move(shape));
	entity.Add<Color>(color);
	entity.Add<FillStyle>(fill_style);
	entity.Add<Visible>(true);

	SetDraw<TShapeDraw>(entity);

	return entity;
}

Entity CreateRect(
	EntityCreationContext context, Transform transform, V2_float size, Color color,
	FillStyle fill_style, Origin origin
) {
	auto rect{ CreateShape<RectDraw>(context, transform, Rect{ size }, color, fill_style) };
	rect.Add<Tag>("Rect");
	rect.Add<Origin>(origin);
	return rect;
}

Entity CreateRoundedRect(
	EntityCreationContext context, Transform transform, V2_float size, float radius, Color color,
	FillStyle fill_style, Origin origin
) {
	auto rounded_rect{ CreateShape<RoundedRectDraw>(
		context, transform, RoundedRect{ size, radius }, color, fill_style
	) };
	rounded_rect.Add<Tag>("Rounded Rect");
	rounded_rect.Add<Origin>(origin);
	return rounded_rect;
}

// Using vector to enable initialization with a list of vertices, e.g. { {0, 0}, {1, 0}, {0, 1} }.
Entity CreatePolygon(
	EntityCreationContext context, Transform transform, const std::vector<V2_float>& vertices,
	Color color, FillStyle fill_style
) {
	auto polygon{
		CreateShape<PolygonDraw>(context, transform, Polygon{ vertices }, color, fill_style)
	};
	polygon.Add<Tag>("Polygon");
	return polygon;
}

Entity CreateTriangle(
	EntityCreationContext context, Transform transform, V2_float a, V2_float b, V2_float c,
	Color color, FillStyle fill_style
) {
	auto triangle{
		CreateShape<TriangleDraw>(context, transform, Triangle{ a, b, c }, color, fill_style)
	};
	triangle.Add<Tag>("Triangle");
	return triangle;
}

Entity CreateCircle(
	EntityCreationContext context, Transform transform, float radius, Color color,
	FillStyle fill_style
) {
	auto circle{ CreateShape<CircleDraw>(context, transform, Circle{ radius }, color, fill_style) };
	circle.Add<Tag>("Circle");
	return circle;
}

Entity CreateEllipse(
	EntityCreationContext context, Transform transform, V2_float radii, Color color,
	FillStyle fill_style
) {
	auto ellipse{
		CreateShape<EllipseDraw>(context, transform, Ellipse{ radii }, color, fill_style)
	};
	ellipse.Add<Tag>("Ellipse");
	return ellipse;
}

Entity CreateArc(
	EntityCreationContext context, Transform transform, float arc_radius, Degrees start_angle,
	Degrees end_angle, bool clockwise, Color color, FillStyle fill_style
) {
	auto arc{ CreateShape<ArcDraw>(
		context, transform, Arc{ arc_radius, start_angle, end_angle, clockwise }, color, fill_style
	) };
	arc.Add<Tag>("Arc");
	return arc;
}

Entity CreateLine(
	EntityCreationContext context, Transform transform, V2_float start, V2_float end, Color color,
	float width
) {
	auto line{ CreateShape<LineDraw>(context, transform, Line{ start, end }, color, width) };
	line.Add<Tag>("Line");
	return line;
}

Entity CreateCapsule(
	EntityCreationContext context, Transform transform, V2_float start, V2_float end, float radius,
	Color color, FillStyle fill_style
) {
	auto capsule{ CreateShape<CapsuleDraw>(
		context, transform, Capsule{ start, end, radius }, color, fill_style
	) };
	capsule.Add<Tag>("Capsule");
	return capsule;
}

} // namespace ptgn
