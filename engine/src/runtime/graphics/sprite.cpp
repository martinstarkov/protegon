#include "runtime/graphics/sprite.h"

#include <array>
#include <optional>
#include <span>
#include <string_view>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "renderer/vertex/vertex.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

namespace impl {

void TextureCrop::Update(const AnimationData& anim) {
	position = anim.GetCurrentFramePosition();
	size	 = anim.config.frame_size;
}

} // namespace impl

Sprite::Sprite(Entity entity) : Entity{ entity } {}

void Sprite::Draw(
	DrawContext& ctx, Entity entity, Origin offset_origin, V2_float offset_size,
	Color additional_tint
) {
	PTGN_ASSERT(entity.Has<Texture>());
	const auto& texture{ entity.Get<Texture>() };
	auto texture_size{ GetDisplaySize(entity) };
	PTGN_ASSERT(texture_size.has_value(), "Sprite texture does not have a valid texture size");

	auto draw_transform{ GetDrawTransform(entity) };
	auto scale{ draw_transform.GetScale() };

	auto scaled_offset{ offset_size * Abs(scale) };
	V2_float offset{ -GetOriginOffset(offset_origin, scaled_offset) };
	draw_transform.Translate(offset);

	// GetDisplaySize already handles the scaling.
	PTGN_ASSERT(!scale.HasZero(), "Scale cannot have a zero component");
	draw_transform.SetScale(scale / Abs(scale));

	auto tint{ GetTint(entity) };
	Color final_tint{ tint.Normalized() * additional_tint.Normalized() };

	auto draw_origin{ GetDrawOrigin(entity) };
	auto depth{ GetDepth(entity) };
	auto tex_coords{ GetTextureCoordinates(entity, false) };
	auto blend_mode{ GetBlendMode(entity) };
	auto entity_id{ entity.GetUUID() };
	auto effects{ impl::GetEffectParams(entity) };

	ctx.WithBlendMode(blend_mode, [&]() {
		ctx.DrawTexture(
			texture, draw_transform, depth, *texture_size, draw_origin, tint, tex_coords, effects,
			entity_id
		);
	});
}

void Sprite::Draw(DrawContext& ctx, Entity entity) {
	Sprite::Draw(ctx, entity, Origin::Center, {}, impl::Tint{});
}

Sprite& Sprite::SetTexture(std::string_view texture_key) {
	const auto& scene{ GetScene() };
	const auto& assets{ scene.ctx().asset };

	auto resolved_texture{ assets.Get<Texture>(texture_key) };

	Add<Texture>(resolved_texture);
	return *this;
}

Sprite CreateSprite(
	Scene& scene, std::string_view texture_key, V2_float position, Origin draw_origin
) {
	Sprite sprite{ scene.CreateEntity() };

	SetDraw<Sprite>(sprite);
	Show(sprite, false);

	sprite.SetTexture(texture_key);

	SetPosition(sprite, position);
	SetDrawOrigin(sprite, draw_origin);

	return sprite;
}

std::optional<V2_int> GetTextureSize(Entity entity) {
	if (auto texture{ entity.TryGet<Texture>() }) {
		auto size{ texture->GetSize() };
		// TODO: Re-enable when text is fixed.
		// PTGN_ASSERT(!size.IsZero(), "Texture does not have a valid size");
		if (size.IsZero()) {
			return std::nullopt;
		}
		return size;
	}
	return std::nullopt;
}

std::optional<V2_int> GetCroppedTextureSize(Entity entity) {
	if (auto crop{ entity.TryGet<impl::TextureCrop>() }) {
		if (!crop->size.has_value()) {
			return GetTextureSize(entity);
		}
		PTGN_ASSERT(!crop->size->IsZero(), "Cropped texture does not have a valid size");
		if (crop->size.has_value()) {
			return *crop->size;
		}
		return std::nullopt;
	}
	return GetTextureSize(entity);
}

void SetDisplaySize(Entity entity, V2_float display_size) {
	entity.Add<impl::TextureSize>(display_size);
}

std::optional<V2_float> GetDisplaySize(Entity entity) {
	if (auto texture_size{ entity.TryGet<impl::TextureSize>() }) {
		return *texture_size;
	}
	auto cropped_size{ GetCroppedTextureSize(entity) };
	if (cropped_size.has_value()) {
		return *cropped_size * GetWorldScale(entity);
	}
	return std::nullopt;
}

std::array<V2_float, 4> GetTextureCoordinates(Entity entity, bool flip_vertically) {
	if (!entity) {
		return impl::GetDefaultTextureCoordinates(flip_vertically);
	}

	auto texture_size{ GetTextureSize(entity) };

	if (!texture_size.has_value()) {
		return impl::GetDefaultTextureCoordinates(flip_vertically);
	}

	std::array<V2_float, 4> tex_coords;

	if (auto crop{ entity.TryGet<impl::TextureCrop>() }) {
		auto crop_size{ crop->size.value_or(*texture_size) };
		tex_coords = impl::GetTextureCoordinates(
			crop->position, crop_size, *texture_size, flip_vertically, true
		);
	} else {
		tex_coords =
			impl::GetTextureCoordinates({}, *texture_size, *texture_size, flip_vertically, true);
	}

	auto scale{ GetWorldScale(entity) };

	impl::FlipTextureCoordinates(tex_coords, scale);

	return tex_coords;
}

} // namespace ptgn