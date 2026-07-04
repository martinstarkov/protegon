#include "runtime/graphics/sprite.h"

#include <array>
#include <optional>
#include <string_view>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/draw_context.h"
#include "renderer/resources/texture.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
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

TextureDrawParams GetTextureDrawParams(
	Entity entity, V2_float size, bool flip_y, Color additional_tint
) {
	return { .depth{ GetDepth(entity) },
			 .size{ size },
			 .origin{ GetDrawOrigin(entity) },
			 .tint{ Color::Multiply(GetTint(entity), additional_tint) },
			 .texture_coordinates{ GetTextureCoordinates(entity, flip_y) },
			 .effects{ impl::GetEffectParams(entity) },
			 .entity_id{ entity.GetUUID() } };
}

} // namespace impl

Sprite::Sprite(Entity entity) : Entity{ entity } {}

void Sprite::Draw(
	DrawContext& ctx, Entity entity, Origin offset_origin, V2_float offset_size,
	Color additional_tint
) {
	PTGN_ASSERT(entity.Has<Texture>(), "Sprites must have a texture");

	const auto& texture{ entity.Get<Texture>() };
	auto texture_size{ GetDisplaySize(entity) };
	PTGN_ASSERT(texture_size.has_value(), "Sprite texture does not have a valid texture size");

	auto draw_transform{ GetDrawTransform(entity) };
	auto scale{ draw_transform.scale };

	auto scaled_offset{ offset_size * Abs(scale) };
	V2_float offset{ GetOffset(offset_origin, scaled_offset) };
	draw_transform.Translate(offset);

	// GetDisplaySize already handles the scaling.
	PTGN_ASSERT(!scale.HasZero(), "Scale cannot have a zero component");
	// Maintain scale sign as this is used to flip the direction of a sprite.
	draw_transform.Scale(1.0f / Abs(scale));

	auto blend_mode{ GetBlendMode(entity) };

	auto params{ impl::GetTextureDrawParams(entity, texture_size.value(), false, additional_tint) };

	ctx.SetBlendMode(blend_mode);
	ctx.DrawTexture(draw_transform, texture, params);
}

void Sprite::Draw(DrawContext& ctx, Entity entity) {
	Sprite::Draw(ctx, entity, Origin::Center, {}, impl::Tint{});
}

Sprite& Sprite::SetTexture(std::string_view texture_key) {
	auto& scene{ GetScene() };
	auto& assets{ scene.ctx().asset };

	auto resolved_texture{ impl::AssetAccessor{ assets }.Get<Texture>(texture_key) };

	Add<Texture>(resolved_texture);
	return *this;
}

Sprite CreateSprite(
	Scene& scene, Transform transform, std::string_view texture_key, Origin origin
) {
	Sprite sprite{ scene.CreateEntity() };

	PTGN_DEFAULT_NAME(sprite, "Sprite");
	SetDraw<Sprite>(sprite);
	sprite.Add<impl::Visible>(true);

	if (!texture_key.empty()) {
		sprite.SetTexture(texture_key);
	}

	SetTransform(sprite, transform);
	SetDrawOrigin(sprite, origin);

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
		PTGN_ASSERT(!crop->size.value().IsZero(), "Cropped texture does not have a valid size");
		if (crop->size.has_value()) {
			return crop->size.value();
		}
		return std::nullopt;
	}
	return GetTextureSize(entity);
}

void SetDisplaySize(Entity entity, std::optional<V2_float> display_size) {
	if (display_size.has_value()) {
		entity.Add<impl::TextureSize>(display_size.value());
	} else {
		entity.Remove<impl::TextureSize>();
	}
}

std::optional<V2_float> GetDisplaySize(Entity entity) {
	if (auto texture_size{ entity.TryGet<impl::TextureSize>() }) {
		return *texture_size;
	}
	auto cropped_size{ GetCroppedTextureSize(entity) };
	if (cropped_size.has_value()) {
		return cropped_size.value() * Abs(GetWorldScale(entity));
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

	if (auto crop{ entity.TryGet<impl::TextureCrop>() }) {
		auto crop_size{ crop->size.value_or(texture_size.value()) };
		return impl::GetTextureCoordinates(
			crop->position, crop_size, texture_size.value(), flip_vertically, true
		);
	}

	return impl::GetTextureCoordinates(
		{}, texture_size.value(), texture_size.value(), flip_vertically, true
	);
}

} // namespace ptgn