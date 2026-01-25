// #include "ecs/components/sprite.h"
//
// #include <array>
//
// #include "core/app/manager.h"
// #include "core/asset/asset_handle.h"
// #include "ecs/components/draw.h"
// #include "ecs/components/transform.h"
// #include "ecs/entity.h"
// #include "math/vector2.h"
//
// namespace ptgn {
//
// Sprite::Sprite(const Entity& entity) : Entity{ entity } {}
//
// void Sprite::Draw(const Entity& entity) {
//	impl::DrawTexture(entity, false);
// }
//
// Sprite& Sprite::SetTexture(const Handle<Texture>& texture) {
//	if (Has<Handle<Texture>>()) {
//		Get<Handle<Texture>>() = texture;
//	} else {
//		Add<Handle<Texture>>(texture);
//	}
//	return *this;
// }
//
// V2_int Sprite::GetTextureSize() const {
//	return impl::GetTextureSize(*this);
// }
//
// V2_int Sprite::GetSize() const {
//	return impl::GetCroppedSize(*this);
// }
//
// V2_float Sprite::GetDisplaySize() const {
//	return impl::GetDisplaySize(*this);
// }
//
// void Sprite::SetDisplaySize(const V2_float& display_size) {
//	impl::SetDisplaySize(*this, display_size);
// }
//
// std::array<V2_float, 4> Sprite::GetTextureCoordinates(bool flip_vertically) const {
//	return impl::GetTextureCoordinates(*this, flip_vertically);
// }
//
// Sprite CreateSprite(
//	Manager& manager, const Handle<Texture>& texture, const V2_float& position, Origin draw_origin
//) {
//	Sprite sprite{ manager.CreateEntity() };
//	SetDraw<Sprite>(sprite);
//	sprite.SetTexture(texture);
//	Show(sprite);
//	SetPosition(sprite, position);
//	SetDrawOrigin(sprite, draw_origin);
//	return sprite;
// }
//
// } // namespace ptgn