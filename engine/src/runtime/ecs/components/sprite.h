#pragma once

#include "core/component.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/components/drawable.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Scene;
class Renderer;

namespace impl {

struct SpriteDraw {
	static void Draw(Renderer& renderer, Entity entity);
};

/// Component for a custom texture size to be used instead of the actual texture size. This can be
/// used for example to render a texture at a larger size.
struct TextureSize : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct TextureCrop {
	// Position and size are V2_float instead of V2_int to allow for smooth increase in display size
	// (for example).

	// Top left position (in pixels) within the texture from which the crop starts.
	V2_float position;

	// Size of the crop in pixels. Zero size will use full size of texture.
	V2_float size;

	bool operator==(const TextureCrop&) const = default;
};

} // namespace impl

Entity SetTexture(Entity sprite, Texture texture);

Entity CreateSprite(
	Scene& scene, Texture texture, V2_float position = {}, Origin draw_origin = Origin::Center
);

Entity CreateSprite(
	Scene& scene, const path& asset_path, V2_float position = {},
	Origin draw_origin = Origin::Center
);

PTGN_REGISTER_DRAWABLE(impl::SpriteDraw);

} // namespace ptgn