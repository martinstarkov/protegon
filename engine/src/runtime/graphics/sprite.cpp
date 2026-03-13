#include "runtime/graphics/sprite.h"

#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>

#include "app/context.h"
#include "core/assert.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/primitives/texture.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"

namespace ptgn {

Sprite::Sprite(Entity entity) : Entity{ entity } {}

void Sprite::Draw(DrawContext& renderer, Entity entity, Camera) {
	PTGN_ASSERT(entity.Has<Texture>());
	auto draw_transform{ GetDrawTransform(entity) };
	auto texture_size{ GetDisplaySize(entity) };
	auto draw_origin{ GetDrawOrigin(entity) };
	auto tint{ GetTint(entity) };
	auto depth{ GetDepth(entity) };
	auto tex_coords{ GetTextureCoordinates(entity, false) };
	auto blend_mode{ GetBlendMode(entity) };
	const auto& texture{ entity.Get<Texture>() };
	PTGN_ASSERT(texture_size.has_value(), "Sprite texture does not have a valid texture size");
	renderer.DrawTexture(
		texture, draw_transform, *texture_size, draw_origin, tint, depth, tex_coords, blend_mode
	);
}

Sprite& Sprite::SetTexture(Texture texture) {
	Add<Texture>(texture);
	return *this;
}

Sprite CreateSprite(
	Scene& scene, std::variant<Texture, std::string_view> texture, V2_float position,
	Origin draw_origin
) {
	Texture resolved_texture{ scene.app().asset.ToTexture(texture) };

	Sprite sprite{ scene.CreateEntity() };
	SetDraw<Sprite>(sprite);
	Show(sprite, false);

	sprite.SetTexture(resolved_texture);

	SetPosition(sprite, position);
	SetDrawOrigin(sprite, draw_origin);

	return sprite;
}

} // namespace ptgn