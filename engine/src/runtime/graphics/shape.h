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
#include "runtime/graphics/camera.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class Scene;
class DrawContext;

struct CapsuleDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct CircleDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct EllipseDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct ArcDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct PolygonDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct RectDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct RoundedRectDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct TriangleDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct LineDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

[[nodiscard]] Transform OffsetByOrigin(const Shape& shape, Transform transform, Entity entity);

/// @return The shape of the entity, if it has one.
std::optional<Shape> GetShape(Entity entity);

/// @return The display size of the entity sprite (if it has a TextureHandle), or its shape, if it
/// has one.
std::optional<Shape> GetSpriteOrShape(Entity entity);

/// @brief Creates a rectangle entity in the manager.
///
/// @param position    The position of the rectangle relative to its parent camera.
/// @param size        The width and height of the rectangle.
/// @param color       The tint color of the rectangle.
/// @param origin      The origin of the rectangle position (e.g., center, top-left).
/// @return Entity     A handle to the newly created rectangle entity.
Entity CreateRect(
	Scene& scene, V2_float position, V2_float size, Color color, FillStyle fill_style = Solid{},
	Origin origin = Origin::Center
);

/// @brief Creates a polygon entity in the scene at the specified position.
/// @param scene The scene in which to create the polygon entity.
/// @param position The position of the polygon in 2D space.
/// @param vertices A collection of 2D vertices that define the polygon's shape.
/// @param color The color of the polygon.
/// @return The newly created polygon entity.
Entity CreatePolygon(
	Scene& scene, V2_float position, const std::vector<V2_float>& vertices, Color color,
	FillStyle fill_style = Solid{}
);

/// @brief Creates a circle entity in the manager.
/// @param position    The position of the circle relative to its parent camera.
/// @param radius        The radius of the circle.
/// @param color       The tint color of the circle.
/// @return Entity     A handle to the newly created circle entity.
Entity CreateCircle(
	Scene& scene, V2_float position, float radius, Color color, FillStyle fill_style = Solid{}
);

Entity CreateArc(
	Scene& scene, V2_float position, float arc_radius, Degrees start_angle, Degrees end_angle,
	bool clockwise, Color color, FillStyle fill_style = Solid{}
);

PTGN_REGISTER_DRAWABLE(CapsuleDraw);
PTGN_REGISTER_DRAWABLE(CircleDraw);
PTGN_REGISTER_DRAWABLE(EllipseDraw);
PTGN_REGISTER_DRAWABLE(ArcDraw);
PTGN_REGISTER_DRAWABLE(PolygonDraw);
PTGN_REGISTER_DRAWABLE(RectDraw);
PTGN_REGISTER_DRAWABLE(RoundedRectDraw);
PTGN_REGISTER_DRAWABLE(TriangleDraw);
PTGN_REGISTER_DRAWABLE(LineDraw);

} // namespace ptgn