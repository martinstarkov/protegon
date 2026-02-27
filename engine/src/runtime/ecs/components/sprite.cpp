#include "runtime/ecs/components/sprite.h"

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
	Show(sprite);
	sprite.SetTexture(texture);
	SetPosition(sprite, position);
	SetDrawOrigin(sprite, draw_origin);
	return sprite;
}

Sprite CreateSprite(Scene& scene, const path& asset_path, V2_float position, Origin draw_origin) {
	return CreateSprite(scene, scene.app().assets.CreateTexture(asset_path), position, draw_origin);
}

} // namespace ptgn