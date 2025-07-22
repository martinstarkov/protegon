#pragma once

#include "components/drawable.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "serialization/serializable.h"

namespace ptgn {

class Scene;

namespace impl {

class RenderData;

Entity CreateRect(
	Manager& manager, const V2_float& position, const V2_float& size, const Color& color,
	float line_width, Origin origin
);

} // namespace impl

struct Rect : public Drawable<Rect> {
	Rect() = default;

	Rect(const V2_float& rect_size);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	V2_float size;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Rect, size)
};

/**
 * @brief Creates a rectangle entity in the scene.
 *
 * @param scene       Reference to the scene where the rectangle will be created.
 * @param position    The position of the rectangle relative to its parent camera.
 * @param size        The width and height of the rectangle.
 * @param color       The tint color of the rectangle.
 * @param line_width  Optional outline width. If -1.0f, the rectangle is filled. If positive, an
 * outlined rectangle is created.
 * @param origin      The origin of the rectangle position (e.g., center, top-left).
 * @return Entity     A handle to the newly created rectangle entity.
 */
Entity CreateRect(
	Scene& scene, const V2_float& position, const V2_float& size, const Color& color,
	float line_width = -1.0f, Origin origin = Origin::Center
);

} // namespace ptgn