#include "runtime/ecs/components/sprite.h"

#include <string_view>

#include "app/context.h"
#include "core/assert.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"

namespace ptgn {

Sprite::Sprite(Entity entity) : Entity{ entity } {}

void Sprite::Draw(Renderer& renderer, Entity entity) {
	PTGN_ASSERT(entity.Has<Texture>());
	impl::DrawQuadTexture(
		renderer, entity.Get<Texture>(), GetDrawTransform(entity), GetTextureSize(entity),
		GetDrawOrigin(entity), GetTint(entity), GetDepth(entity), GetBlendMode(entity),
		GetTextureCoordinates(entity, false)
	);
}

Sprite& Sprite::SetTexture(Texture texture) {
	Add<Texture>(texture);
	return *this;
}

Sprite CreateSprite(Scene& scene, Texture texture, V2_float position, Origin draw_origin) {
	Sprite sprite{ scene.CreateEntity() };
	SetDraw<Sprite>(sprite);
	Show(sprite, false);
	sprite.SetTexture(texture);
	SetPosition(sprite, position);
	SetDrawOrigin(sprite, draw_origin);
	return sprite;
}

Sprite CreateSprite(
	Scene& scene, std::string_view texture_key, V2_float position, Origin draw_origin
) {
	PTGN_ASSERT(
		scene.app().asset.HasTexture(texture_key),
		"Texture key must be loaded in the asset manager before creating an entity with it"
	);
	auto texture{ *scene.app().asset.GetTexture(texture_key) };
	return CreateSprite(scene, texture, position, draw_origin);
}

} // namespace ptgn