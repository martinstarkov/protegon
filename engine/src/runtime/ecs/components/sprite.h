#pragma once

#include <string_view>

#include "core/component.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/components/drawable.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Scene;
class Renderer;

namespace impl {

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

class Sprite : public Entity {
public:
	Sprite() = default;
	explicit Sprite(Entity entity);

	static void Draw(Renderer& renderer, Entity entity);

	Sprite& SetTexture(Texture texture);
};

Sprite CreateSprite(
	Scene& scene, Texture texture, V2_float position = {}, Origin draw_origin = Origin::Center
);

Sprite CreateSprite(
	Scene& scene, std::string_view texture_key, V2_float position = {},
	Origin draw_origin = Origin::Center
);

PTGN_REGISTER_DRAWABLE(Sprite);

} // namespace ptgn