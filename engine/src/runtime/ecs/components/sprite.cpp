#include "runtime/ecs/components/sprite.h"

#include "app/context.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/camera/camera.h"
#include "renderer/primitives/draw.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset.h"
#include "runtime/asset/asset_handle.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/components/entity_transform.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace impl {

void Sprite::Draw(Renderer& renderer, Entity entity) {
	auto camera{ entity.Has<Camera>() ? entity : entity.GetScene().camera };
	renderer.SetViewProjection(GetViewProjection(camera));
	renderer.SetBlend(GetBlendMode(entity));
	// TODO: Add rotation and flip here.
	renderer.DrawTexture(
		entity.Get<Handle<Asset::Texture>>().Get(), GetPosition(entity), GetTextureSize(entity),
		GetTint(entity)
	);
}

} // namespace impl

Entity SetTexture(Entity sprite, Handle<Asset::Texture> texture) {
	sprite.Add<Handle<Asset::Texture>>(texture);
	return sprite;
}

Entity CreateSprite(
	Scene& scene, Handle<Asset::Texture> texture, V2_float position, Origin draw_origin
) {
	auto sprite{ scene.CreateEntity() };
	SetDraw<impl::Sprite>(sprite);
	Show(sprite);
	SetTexture(sprite, texture);
	SetPosition(sprite, position);
	SetDrawOrigin(sprite, draw_origin);
	return sprite;
}

Entity CreateSprite(Scene& scene, const path& asset_path, V2_float position, Origin draw_origin) {
	return CreateSprite(scene, scene.app().assets.LoadTexture(asset_path), position, draw_origin);
}

} // namespace ptgn