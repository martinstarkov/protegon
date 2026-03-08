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
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene.h"

namespace ptgn {

Sprite::Sprite(Entity entity) : Entity{ entity } {}

void Sprite::Draw(RenderContext& renderer, Entity entity) {
	PTGN_ASSERT(entity.Has<Texture>());
	impl::DrawQuadTexture(
		renderer, entity.Get<Texture>(), GetDrawTransform(entity), GetCroppedTextureSize(entity),
		GetDrawOrigin(entity), GetTint(entity), GetDepth(entity), GetBlendMode(entity),
		GetTextureCoordinates(entity, false)
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
	Texture resolved_texture;

	std::visit(
		[&](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, Texture>) {
				resolved_texture = arg;
			} else if constexpr (std::is_same_v<T, std::string_view>) {
				PTGN_ASSERT(
					scene.app().asset.HasTexture(arg),
					"Texture key must be loaded in the asset manager before creating sprite"
				);

				resolved_texture = *scene.app().asset.GetTexture(arg);
			}
		},
		texture
	);

	Sprite sprite{ scene.CreateEntity() };
	SetDraw<Sprite>(sprite);
	Show(sprite, false);

	sprite.SetTexture(resolved_texture);

	SetPosition(sprite, position);
	SetDrawOrigin(sprite, draw_origin);

	return sprite;
}

} // namespace ptgn