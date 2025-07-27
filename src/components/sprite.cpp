#include "components/sprite.h"

#include <array>
#include <utility>

#include "components/draw.h"
#include "core/entity.h"
#include "debug/log.h"
#include "math/vector2.h"
#include "rendering/buffers/frame_buffer.h"
#include "rendering/render_data.h"
#include "rendering/resources/texture.h"
#include "scene/scene.h"

namespace ptgn {

Sprite CreateSprite(Scene& scene, const TextureHandle& texture_key) {
	Sprite sprite{ scene.CreateEntity() };
	sprite.SetDraw<Sprite>();
	sprite.SetTextureKey(texture_key);
	sprite.Show();
	return sprite;
}

Sprite::Sprite(const Entity& entity) : Entity{ entity } {}

void Sprite::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::DrawTexture(ctx, entity, false);
}

Sprite& Sprite::SetTextureKey(const TextureHandle& texture_key) {
	if (Has<TextureHandle>()) {
		Get<TextureHandle>() = texture_key;
	} else {
		Add<TextureHandle>(texture_key);
	}
	return *this;
}

const impl::Texture& Sprite::GetTexture() const {
	if (auto texture_handle{ TryGet<TextureHandle>() }; texture_handle) {
		return texture_handle->GetTexture(*this);
	} else if (auto frame_buffer{ TryGet<impl::FrameBuffer>() }; frame_buffer) {
		return frame_buffer->GetTexture();
	}
	PTGN_ERROR("Entity has no valid texture");
}

impl::Texture& Sprite::GetTexture() {
	return const_cast<impl::Texture&>(std::as_const(*this).GetTexture());
}

V2_int Sprite::GetTextureSize() const {
	return impl::GetTextureSize(*this);
}

V2_int Sprite::GetSize() const {
	return impl::GetCroppedSize(*this);
}

V2_float Sprite::GetDisplaySize() const {
	return impl::GetDisplaySize(*this);
}

std::array<V2_float, 4> Sprite::GetTextureCoordinates(bool flip_vertically) const {
	return impl::GetTextureCoordinates(*this, flip_vertically);
}

} // namespace ptgn