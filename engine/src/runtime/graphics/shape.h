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
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class Scene;
class DrawContext;

struct RectDraw {
	static void Draw(DrawContext& renderer, Entity entity);
};

struct RoundedRectDraw {
	static void Draw(DrawContext& renderer, Entity entity);
};

struct PolygonDraw {
	static void Draw(DrawContext& renderer, Entity entity);
};

struct CircleDraw {
	static void Draw(DrawContext& renderer, Entity entity);
};

struct EllipseDraw {
	static void Draw(DrawContext& renderer, Entity entity);
};

struct ArcDraw {
	static void Draw(DrawContext& renderer, Entity entity);
};

struct TriangleDraw {
	static void Draw(DrawContext& renderer, Entity entity);
};

struct LineDraw {
	static void Draw(DrawContext& renderer, Entity entity);
};

struct CapsuleDraw {
	static void Draw(DrawContext& renderer, Entity entity);
};

[[nodiscard]] Transform OffsetByOrigin(const Shape& shape, Transform transform, Entity entity);

/// @return The shape of the entity, if it has one.
std::optional<Shape> GetShape(Entity entity);

/// @return The display size of the entity sprite (if it has a TextureHandle), or its shape, if it
/// has one.
std::optional<Shape> GetSpriteOrShape(Entity entity);

Entity CreateRect(
	Scene& scene, V2_float position, V2_float size, Color color, FillStyle fill_style = Solid{},
	Origin origin = Origin::Center
);

Entity CreateRoundedRect(
	Scene& scene, V2_float position, V2_float size, float radius, Color color,
	FillStyle fill_style = Solid{}, Origin origin = Origin::Center
);

Entity CreatePolygon(
	Scene& scene, V2_float position, const std::vector<V2_float>& vertices, Color color,
	FillStyle fill_style = Solid{}
);

Entity CreateTriangle(
	Scene& scene, V2_float position, V2_float a, V2_float b, V2_float c, Color color,
	FillStyle fill_style = Solid{}
);

Entity CreateCircle(
	Scene& scene, V2_float position, float radius, Color color, FillStyle fill_style = Solid{}
);

Entity CreateEllipse(
	Scene& scene, V2_float position, V2_float radii, Color color, FillStyle fill_style = Solid{}
);

Entity CreateArc(
	Scene& scene, V2_float position, float arc_radius, Degrees start_angle, Degrees end_angle,
	bool clockwise, Color color, FillStyle fill_style = Solid{}
);

Entity CreateLine(
	Scene& scene, V2_float position, V2_float start, V2_float end, Color color, float width = 1.0f
);

Entity CreateCapsule(
	Scene& scene, V2_float position, V2_float start, V2_float end, float radius, Color color,
	FillStyle fill_style = Solid{}
);

PTGN_REGISTER_DRAWABLE(RectDraw);
PTGN_REGISTER_DRAWABLE(RoundedRectDraw);
PTGN_REGISTER_DRAWABLE(PolygonDraw);
PTGN_REGISTER_DRAWABLE(TriangleDraw);
PTGN_REGISTER_DRAWABLE(CircleDraw);
PTGN_REGISTER_DRAWABLE(EllipseDraw);
PTGN_REGISTER_DRAWABLE(ArcDraw);
PTGN_REGISTER_DRAWABLE(LineDraw);
PTGN_REGISTER_DRAWABLE(CapsuleDraw);

} // namespace ptgn