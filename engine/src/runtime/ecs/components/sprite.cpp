#include "runtime/ecs/components/sprite.h"

#include "app/context.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/camera/camera.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/components/camera_component.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace impl {

void SpriteDraw::Draw(Renderer& renderer, Entity entity) {
	auto camera{ entity.Has<Camera>() ? entity : entity.GetScene().camera };
	renderer.SetViewProjection(GetViewProjection(camera));
	renderer.SetBlend(GetBlendMode(entity));
	// TODO: Add rotation and flip here.
	PTGN_ASSERT(entity.Has<Texture>());
	renderer.DrawTexture(
		entity.Get<Texture>(), GetPosition(entity), GetTextureSize(entity), GetTint(entity)
	);
}

} // namespace impl

Entity SetTexture(Entity sprite, Texture texture) {
	sprite.Add<Texture>(texture);
	return sprite;
}

Entity CreateSprite(Scene& scene, Texture texture, V2_float position, Origin draw_origin) {
	auto sprite{ scene.CreateEntity() };
	SetDraw<impl::SpriteDraw>(sprite);
	Show(sprite);
	SetTexture(sprite, texture);
	SetPosition(sprite, position);
	SetDrawOrigin(sprite, draw_origin);
	return sprite;
}

Entity CreateSprite(Scene& scene, const path& asset_path, V2_float position, Origin draw_origin) {
	return CreateSprite(scene, scene.app().assets.CreateTexture(asset_path), position, draw_origin);
}

} // namespace ptgn