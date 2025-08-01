#pragma once

#include <array>

#include "components/drawable.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "renderer/texture.h"

namespace ptgn {

class Scene;

namespace impl {

class RenderData;

} // namespace impl

struct Sprite : public Entity, public Drawable<Sprite> {
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

	[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(bool flip_vertically) const;
};

Sprite CreateSprite(Scene& scene, const TextureHandle& texture_key);

} // namespace ptgn