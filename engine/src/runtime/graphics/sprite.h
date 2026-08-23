#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/draw_context.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class DrawContext;

namespace impl {

/// @return The texture id of the entity retrieved from its Texture or TextureKey components.
/// Texture might be 0 if no valid texture is found.
TextureId GetTexture(Entity entity);

class AnimationData;

TextureDrawParams GetTextureDrawParams(
	Entity entity, V2_float size, bool flip_y, Color additional_tint
);

/// Component for a custom texture size to be used instead of the actual texture size. This can be
/// used for example to render a texture at a larger size.
struct TextureSize {
	V2_float value{};

	operator V2_float() const { // NOSONAR
		return value;
	}

	PTGN_REFLECT_VALUE(TextureSize, value)
};

struct TextureCrop {
	/// @brief Position and size are V2_float instead of V2_int to allow for smooth increase in
	/// display size (for example).

	/// @brief Top left position (in pixels) within the texture from which the crop starts.
	V2_float position{};

	/// @brief Size of the crop in pixels. std::nullopt will use full size of the unscaled texture.
	std::optional<V2_float> size{};

	constexpr bool operator==(const TextureCrop&) const = default;

	/// @brief Updates the crop size based on the animation data.
	void Update(const AnimationData& anim, std::optional<V2_int> texture_size);

	PTGN_REFLECT(TextureCrop, position, size)
};

} // namespace impl

class Sprite : public Entity {
public:
	Sprite() = default;
	explicit Sprite(Entity entity);

	static void Draw(
		DrawContext& ctx, Entity entity, Origin offset_origin, V2_float offset_size,
		Color additional_tint
	);

	static void Draw(DrawContext& ctx, Entity entity);

	Sprite& SetTexture(TextureKey texture_key);
};

Sprite CreateSprite(
	Scene& scene, Transform transform = {}, TextureKey texture_key = {},
	Origin origin = Origin::Center
);

PTGN_REGISTER_DRAWABLE(Sprite);

/// @return Unscaled size of the entire texture in pixels.
std::optional<V2_int> GetTextureSize(Entity entity);

/// @return Unscaled size of the cropped texture in pixels.
std::optional<V2_int> GetCroppedTextureSize(Entity entity);

/// @return Scaled size of the cropped texture in pixels.
std::optional<V2_float> GetDisplaySize(Entity entity);

/// @brief Overrides the scale of the entity. If nullopt, removes the override and uses the actual
/// texture size.
void SetDisplaySize(Entity entity, std::optional<V2_float> display_size);

std::array<V2_float, 4> GetTextureCoordinates(Entity entity, bool flip_vertically);

} // namespace ptgn