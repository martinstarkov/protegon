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
#include "runtime/graphics/drawable.h"

namespace ptgn {

class Scene;
class DrawContext;

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
	Scene& scene, Transform transform, V2_float size, Color color, FillStyle fill_style = Solid{},
	Origin origin = Origin::Center
);

Entity CreateRoundedRect(
	Scene& scene, Transform transform, V2_float size, float radius, Color color,
	FillStyle fill_style = Solid{}, Origin origin = Origin::Center
);

Entity CreatePolygon(
	Scene& scene, Transform transform, const std::vector<V2_float>& vertices, Color color,
	FillStyle fill_style = Solid{}
);

Entity CreateTriangle(
	Scene& scene, Transform transform, V2_float a, V2_float b, V2_float c, Color color,
	FillStyle fill_style = Solid{}
);

Entity CreateCircle(
	Scene& scene, Transform transform, float radius, Color color, FillStyle fill_style = Solid{}
);

Entity CreateEllipse(
	Scene& scene, Transform transform, V2_float radii, Color color, FillStyle fill_style = Solid{}
);

Entity CreateArc(
	Scene& scene, Transform transform, float arc_radius, Degrees start_angle, Degrees end_angle,
	bool clockwise, Color color, FillStyle fill_style = Solid{}
);

Entity CreateLine(
	Scene& scene, Transform transform, V2_float start, V2_float end, Color color, float width = 1.0f
);

Entity CreateCapsule(
	Scene& scene, Transform transform, V2_float start, V2_float end, float radius, Color color,
	FillStyle fill_style = Solid{}
);

PTGN_REGISTER_DRAWABLE_NAMED(RectDraw, "Rect");
PTGN_REGISTER_DRAWABLE_NAMED(RoundedRectDraw, "Rounded Rect");
PTGN_REGISTER_DRAWABLE_NAMED(PolygonDraw, "Polygon");
PTGN_REGISTER_DRAWABLE_NAMED(TriangleDraw, "Triangle");
PTGN_REGISTER_DRAWABLE_NAMED(CircleDraw, "Circle");
PTGN_REGISTER_DRAWABLE_NAMED(EllipseDraw, "Ellipse");
PTGN_REGISTER_DRAWABLE_NAMED(ArcDraw, "Arc");
PTGN_REGISTER_DRAWABLE_NAMED(LineDraw, "Line");
PTGN_REGISTER_DRAWABLE_NAMED(CapsuleDraw, "Capsule");

} // namespace ptgn