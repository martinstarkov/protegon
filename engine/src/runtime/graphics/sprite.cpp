#include "runtime/graphics/sprite.h"

#include <optional>

#include "core/assert.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
#include "runtime/asset/asset.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"


namespace ptgn {

Sprite::Sprite(Entity entity) : Entity{ entity } {}

void Sprite::Draw(DrawContext& renderer, Entity entity, Camera, Color additional_tint) {
	PTGN_ASSERT(entity.Has<Texture>());
	const auto& texture{ entity.Get<Texture>() };
	auto texture_size{ GetDisplaySize(entity) };
	PTGN_ASSERT(texture_size.has_value(), "Sprite texture does not have a valid texture size");

	auto draw_transform{ GetDrawTransform(entity) };
	// GetDisplaySize already handles the scaling.
	auto scale{ draw_transform.GetScale() };
	PTGN_ASSERT(!scale.HasZero(), "Scale cannot have a zero component");
	draw_transform.SetScale(scale / Abs(scale));

	auto tint{ GetTint(entity) };
	impl::Tint final_tint{ tint.Normalized() * additional_tint.Normalized() };

	auto draw_origin{ GetDrawOrigin(entity) };
	auto depth{ GetDepth(entity) };
	auto tex_coords{ GetTextureCoordinates(entity, false) };
	auto blend_mode{ GetBlendMode(entity) };

	renderer.DrawTexture(
		texture, draw_transform, *texture_size, draw_origin, final_tint, depth, tex_coords,
		blend_mode
	);
}

void Sprite::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	Sprite::Draw(renderer, entity, camera, impl::Tint{});
}

Sprite& Sprite::SetTexture(TextureOrKey texture) {
	const auto& scene{ GetScene() };
	const auto& assets{ scene.ctx().asset };

	Texture resolved_texture{ texture.Get(assets) };

	Add<Texture>(resolved_texture);
	return *this;
}

Sprite CreateSprite(Scene& scene, TextureOrKey texture, V2_float position, Origin draw_origin) {
	Sprite sprite{ scene.CreateEntity() };

	SetDraw<Sprite>(sprite);
	Show(sprite, false);

	sprite.SetTexture(texture);

	SetPosition(sprite, position);
	SetDrawOrigin(sprite, draw_origin);

	return sprite;
}

} // namespace ptgn