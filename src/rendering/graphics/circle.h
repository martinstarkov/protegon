#pragma once

#include "components/drawable.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "serialization/serializable.h"

namespace ptgn {

class Scene;

namespace impl {

class RenderData;

} // namespace impl

struct Circle : public Drawable<Circle> {
	Circle() = default;

	Circle(float circle_radius);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	float radius{ 0.0f };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Circle, radius)
};

/**
 * @brief Creates a circle entity in the scene.
 *
 * @param scene       Reference to the scene where the circle will be created.
 * @param position    The position of the circle relative to its parent camera.
 * @param radius        The radius of the circle.
 * @param color       The tint color of the circle.
 * @param line_width  Optional outline width. If -1.0f, the circle is filled. If positive, an
 * outlined circle is created.
 * @return Entity     A handle to the newly created circle entity.
 */
Entity CreateCircle(
	Scene& scene, const V2_float& position, float radius, const Color& color,
	float line_width = -1.0f
);

} // namespace ptgn