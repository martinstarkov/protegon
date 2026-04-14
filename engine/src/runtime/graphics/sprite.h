#pragma once

#include <optional>
#include <string_view>
#include <variant>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class Scene;
class DrawContext;

namespace impl {

class AnimationData;

/// Component for a custom texture size to be used instead of the actual texture size. This can be
/// used for example to render a texture at a larger size.
struct TextureSize : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct TextureCrop {
	/// @brief Position and size are V2_float instead of V2_int to allow for smooth increase in
	/// display size (for example).

	/// @brief Top left position (in pixels) within the texture from which the crop starts.
	V2_float position;

	/// @brief Size of the crop in pixels. std::nullopt will use full size of the unscaled texture.
	std::optional<V2_float> size;

	bool operator==(const TextureCrop&) const = default;

	/// @brief Updates the crop size based on the animation data.
	void Update(const AnimationData& anim);
};

} // namespace impl

class Sprite : public Entity {
public:
	Sprite() = default;
	explicit Sprite(Entity entity);

	static void Draw(
		DrawContext& renderer, Entity entity, Origin offset_origin, V2_float offset_size, Camera,
		Color additional_tint
	);

	static void Draw(DrawContext& renderer, Entity entity, Camera camera);

	Sprite& SetTexture(TextureOrKey texture);
};

Sprite CreateSprite(
	Scene& scene, TextureOrKey texture, V2_float position = {}, Origin draw_origin = Origin::Center
);

PTGN_REGISTER_DRAWABLE(Sprite);

/// @return Unscaled size of the entire texture in pixels.
std::optional<V2_int> GetTextureSize(Entity entity);

/// @return Unscaled size of the cropped texture in pixels.
std::optional<V2_int> GetCroppedTextureSize(Entity entity);

/// @return Scaled size of the cropped texture in pixels.
std::optional<V2_float> GetDisplaySize(Entity entity);

/// @brief Overrides the scale of the entity.
void SetDisplaySize(Entity entity, V2_float display_size);

std::array<V2_float, 4> GetTextureCoordinates(Entity entity, bool flip_vertically);

} // namespace ptgn