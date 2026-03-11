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

void Sprite::Draw(DrawContext& renderer, Entity entity, [[maybe_unused]] Camera) {
	PTGN_ASSERT(entity.Has<Texture>());
	renderer.DrawTexture(
		entity.Get<Texture>(), GetDrawTransform(entity), GetCroppedTextureSize(entity),
		GetDrawOrigin(entity), GetTint(entity), GetDepth(entity),
		GetTextureCoordinates(entity, false), GetBlendMode(entity)
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