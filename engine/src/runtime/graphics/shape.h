#pragma once

#include <optional>
#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/draw_context.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_creation_context.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class Scene;
class DrawContext;
class EntityCreationContext;

namespace impl {

ShapeDrawParams GetShapeDrawParams(Entity entity);

} // namespace impl

struct RectDraw {
	static void Draw(DrawContext& ctx, Entity entity);
};

struct RoundedRectDraw {
	static void Draw(DrawContext& ctx, Entity entity);
};

struct PolygonDraw {
	static void Draw(DrawContext& ctx, Entity entity);
};

struct CircleDraw {
	static void Draw(DrawContext& ctx, Entity entity);
};

struct EllipseDraw {
	static void Draw(DrawContext& ctx, Entity entity);
};

struct ArcDraw {
	static void Draw(DrawContext& ctx, Entity entity);
};

struct TriangleDraw {
	static void Draw(DrawContext& ctx, Entity entity);
};

struct LineDraw {
	static void Draw(DrawContext& ctx, Entity entity);
};

struct CapsuleDraw {
	static void Draw(DrawContext& ctx, Entity entity);
};

[[nodiscard]] Transform OffsetByOrigin(const Shape& shape, Transform transform, Entity entity);

/// @return The shape of the entity, if it has one.
std::optional<Shape> GetShape(Entity entity);

Entity CreateRect(
	EntityCreationContext context, Transform transform, V2_float size, Color color,
	FillStyle fill_style = Solid{}, Origin origin = Origin::Center
);

Entity CreateRoundedRect(
	EntityCreationContext context, Transform transform, V2_float size, float radius, Color color,
	FillStyle fill_style = Solid{}, Origin origin = Origin::Center
);

Entity CreatePolygon(
	EntityCreationContext context, Transform transform, const std::vector<V2_float>& vertices,
	Color color, FillStyle fill_style = Solid{}
);

Entity CreateTriangle(
	EntityCreationContext context, Transform transform, V2_float a, V2_float b, V2_float c,
	Color color, FillStyle fill_style = Solid{}
);

Entity CreateCircle(
	EntityCreationContext context, Transform transform, float radius, Color color,
	FillStyle fill_style = Solid{}
);

Entity CreateEllipse(
	EntityCreationContext context, Transform transform, V2_float radii, Color color,
	FillStyle fill_style = Solid{}
);

Entity CreateArc(
	EntityCreationContext context, Transform transform, float arc_radius, Degrees start_angle,
	Degrees end_angle, bool clockwise, Color color, FillStyle fill_style = Solid{}
);

Entity CreateLine(
	EntityCreationContext context, Transform transform, V2_float start, V2_float end, Color color,
	float width = 1.0f
);

Entity CreateCapsule(
	EntityCreationContext context, Transform transform, V2_float start, V2_float end, float radius,
	Color color, FillStyle fill_style = Solid{}
);

PTGN_REGISTER_DRAWABLE(RectDraw, { .name = "Rect", .group = "Shape" });
PTGN_REGISTER_DRAWABLE(RoundedRectDraw, { .name = "Rounded Rect", .group = "Shape" });
PTGN_REGISTER_DRAWABLE(PolygonDraw, { .name = "Polygon", .group = "Shape" });
PTGN_REGISTER_DRAWABLE(TriangleDraw, { .name = "Triangle", .group = "Shape" });
PTGN_REGISTER_DRAWABLE(CircleDraw, { .name = "Circle", .group = "Shape" });
PTGN_REGISTER_DRAWABLE(EllipseDraw, { .name = "Ellipse", .group = "Shape" });
PTGN_REGISTER_DRAWABLE(ArcDraw, { .name = "Arc", .group = "Shape" });
PTGN_REGISTER_DRAWABLE(LineDraw, { .name = "Line", .group = "Shape" });
PTGN_REGISTER_DRAWABLE(CapsuleDraw, { .name = "Capsule", .group = "Shape" });

} // namespace ptgn
