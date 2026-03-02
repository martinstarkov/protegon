#pragma once

#include <optional>
#include <vector>

#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Scene;

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
/// @param line_width  Optional outline width. If -1.0f, the rectangle is filled. If positive, an
/// outlined rectangle is created.
/// @param origin      The origin of the rectangle position (e.g., center, top-left).
/// @return Entity     A handle to the newly created rectangle entity.
Entity CreateRect(
	Scene& scene, V2_float position, V2_float size, Color color, float line_width = -1.0f,
	Origin origin = Origin::Center
);

/// @brief Creates a polygon entity in the scene at the specified position.
/// @param scene The scene in which to create the polygon entity.
/// @param position The position of the polygon in 2D space.
/// @param vertices A collection of 2D vertices that define the polygon's shape.
/// @param color The color of the polygon.
/// @param line_width The width of the polygon's outline. If -1.0f, the polygon is filled.
/// @return The newly created polygon entity.
Entity CreatePolygon(
	Scene& scene, V2_float position, const std::vector<V2_float>& vertices, Color color,
	float line_width = -1.0f
);

/// @brief Creates a circle entity in the manager.
/// @param position    The position of the circle relative to its parent camera.
/// @param radius        The radius of the circle.
/// @param color       The tint color of the circle.
/// @param line_width  Optional outline width. If -1.0f, the circle is filled. If positive, an
/// outlined circle is created.
/// @return Entity     A handle to the newly created circle entity.
Entity CreateCircle(
	Scene& scene, V2_float position, float radius, Color color, float line_width = -1.0f
);

} // namespace ptgn