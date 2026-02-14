#include "runtime/ecs/components/sprite.h"

#include "app/context.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset.h"
#include "runtime/asset/asset_handle.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/components/transform.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"

namespace ptgn {

Entity SetTexture(Entity sprite, Handle<Asset::Texture> texture) {
	sprite.Add<Handle<Asset::Texture>>(texture);
	return sprite;
}

V2_int GetTextureSize(Entity sprite) {
	const auto& renderer{ sprite.GetScene().app().renderer };
	const auto& texture{ sprite.Get<Handle<Asset::Texture>>().Get() };
	return renderer.GetTextureSize(texture);
}

// TODO: Fix.
// V2_int Sprite::GetSize() const {
// return impl::GetCroppedSize(*this);
//}

// TODO: Fix.
// V2_float Sprite::GetDisplaySize() const {
// return impl::GetDisplaySize(*this);
//}

// TODO: Fix.
// void Sprite::SetDisplaySize(const V2_float& display_size) {
// impl::SetDisplaySize(*this, display_size);
//}

// TODO: Fix.
// std::array<V2_float, 4> Sprite::GetTextureCoordinates(bool flip_vertically) const {
// return impl::GetTextureCoordinates(*this, flip_vertically);
//}

Entity CreateSprite(
	Scene& scene, Handle<Asset::Texture> texture, V2_float position //, Origin draw_origin
) {
	auto sprite{ scene.CreateEntity() };
	// TODO: Fix.
	// SetDraw<Sprite>(sprite);
	// Show(sprite);
	SetTexture(sprite, texture);
	SetPosition(sprite, position);
	// TODO: Fix.
	// SetDrawOrigin(sprite, draw_origin);
	return sprite;
}

Entity CreateSprite(Scene& scene, const path& asset_path, V2_float position) {
	return CreateSprite(scene, scene.app().assets.LoadTexture(asset_path), position);
}

} // namespace ptgn