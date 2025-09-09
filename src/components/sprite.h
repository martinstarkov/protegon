#pragma once

#include <array>

#include "components/drawable.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"
#include "renderer/texture.h"

namespace ptgn {

class Manager;

namespace impl {

class RenderData;

} // namespace impl

struct Sprite : public Entity {
	Sprite() = default;
	Sprite(const Entity& entity);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	Sprite& SetTextureKey(const TextureHandle& texture_key);

	[[nodiscard]] const impl::Texture& GetTexture() const;

	[[nodiscard]] impl::Texture& GetTexture();

	// @return Unscaled size of the entire texture in pixels.
	[[nodiscard]] V2_int GetTextureSize() const;

	// @return Unscaled size of the cropped texture in pixels.
	[[nodiscard]] V2_int GetSize() const;

	// @return Scaled size of the cropped texture in pixels.
	[[nodiscard]] V2_float GetDisplaySize() const;

	void SetDisplaySize(const V2_float& display_size);

	[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(bool flip_vertically) const;
};

PTGN_DRAWABLE_REGISTER(Sprite);

Sprite CreateSprite(
	Manager& manager, const TextureHandle& texture_key, const V2_float& position,
	Origin draw_origin = Origin::Center
);

} // namespace ptgn