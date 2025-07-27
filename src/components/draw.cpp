#include "components/draw.h"

#include <array>
#include <functional>

#include "components/sprite.h"
#include "core/entity.h"
#include "core/game.h"
#include "debug/log.h"
#include "math/vector2.h"
#include "rendering/api/flip.h"
#include "rendering/render_data.h"
#include "rendering/resources/shader.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"

namespace ptgn {

namespace impl {

void DrawTexture(impl::RenderData& ctx, const Entity& entity, bool flip_texture) {
	Sprite sprite{ entity };

	const auto& texture{ sprite.GetTexture() };
	auto display_size{ sprite.GetSize() };
	auto texture_coordinates{ sprite.GetTextureCoordinates(flip_texture) };

	auto transform{ entity.GetDrawTransform() };
	auto origin{ entity.GetOrigin() };
	auto tint{ entity.GetTint() };
	auto depth{ entity.GetDepth() };

	impl::RenderState state;
	state.blend_mode  = entity.GetBlendMode();
	state.shader_pass = game.shader.Get<ShapeShader::Quad>();
	state.camera	  = entity.GetOrParentOrDefault<Camera>();
	state.post_fx	  = entity.GetOrDefault<impl::PostFX>();
	auto pre_fx{ entity.GetOrDefault<impl::PreFX>() };

	ctx.AddTexturedQuad(
		texture, transform, display_size, origin, tint, depth, texture_coordinates, state, pre_fx
	);
}

V2_int GetTextureSize(const Entity& entity) {
	if (entity.Has<TextureHandle>()) {
		return entity.Get<TextureHandle>().GetSize(entity);
	}

	PTGN_ERROR("Texture does not have a valid size");
}

V2_int GetCroppedSize(const Entity& entity) {
	if (entity.Has<TextureCrop>()) {
		const auto& crop{ entity.Get<TextureCrop>() };
		return crop.size;
	}
	return GetTextureSize(entity);
}

V2_float GetDisplaySize(const Entity& entity) {
	if (!entity.Has<TextureHandle>() && !entity.Has<TextureCrop>()) {
		return {};
	}
	return GetCroppedSize(entity) * entity.GetScale();
}

std::array<V2_float, 4> GetTextureCoordinates(const Entity& entity, bool flip_vertically) {
	auto tex_coords{ impl::GetDefaultTextureCoordinates() };

	auto check_vertical_flip = [flip_vertically, &tex_coords]() {
		if (flip_vertically) {
			impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
		}
	};

	if (!entity) {
		std::invoke(check_vertical_flip);
		return tex_coords;
	}

	V2_int texture_size{ GetTextureSize(entity) };

	if (texture_size.IsZero()) {
		std::invoke(check_vertical_flip);
		return tex_coords;
	}

	if (entity.Has<TextureCrop>()) {
		const auto& crop{ entity.Get<TextureCrop>() };
		if (crop != TextureCrop{}) {
			tex_coords = impl::GetTextureCoordinates(crop.position, crop.size, texture_size);
		}
	}

	auto scale{ entity.GetScale() };

	bool flip_x{ scale.x < 0.0f };
	bool flip_y{ scale.y < 0.0f };

	if (flip_x && flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Both);
	} else if (flip_x) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Horizontal);
	} else if (flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
	}

	// TODO: Consider if this is necessary given entity scale already flips a texture.
	if (entity.Has<Flip>()) {
		impl::FlipTextureCoordinates(tex_coords, entity.Get<Flip>());
	}

	std::invoke(check_vertical_flip);

	return tex_coords;
}

} // namespace impl

} // namespace ptgn