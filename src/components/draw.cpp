#include "components/draw.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <list>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "common/assert.h"
#include "components/common.h"
#include "components/drawable.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/resource_manager.h"
#include "core/time.h"
#include "core/tween.h"
#include "math/geometry.h"
#include "math/math.h"
#include "math/vector2.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/api/flip.h"
#include "rendering/batching/render_data.h"
#include "rendering/resources/texture.h"

namespace ptgn {

Sprite::Sprite(const Entity& entity) : Entity{ entity } {}

Sprite::Sprite(Entity entity, const TextureHandle& texture_key) : Entity{ entity } {
	SetDraw<Sprite>();
	SetTextureKey(texture_key);
	Show();
}

Sprite::Sprite(Manager& manager, const TextureHandle& texture_key) :
	Sprite{ manager.CreateEntity(), texture_key } {}

void Sprite::Draw(impl::RenderData& ctx, const Entity& entity) {
	Sprite sprite{ entity };

	const auto& texture{ sprite.GetTexture() };

	if (!texture.IsValid()) {
		return;
	}

	auto transform{ sprite.GetAbsoluteTransform() };
	auto depth{ sprite.GetDepth() };
	auto blend_mode{ sprite.GetBlendMode() };
	auto tint{ sprite.GetTint().Normalized() };
	auto origin{ sprite.GetOrigin() };

	auto size{ sprite.GetTextureSize() };
	auto coords{ sprite.GetTextureCoordinates(false) };
	auto vertices{ impl::GetVertices(transform, size, origin) };

	ctx.AddTexturedQuad(vertices, coords, texture, depth, blend_mode, tint, false);
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
	PTGN_ASSERT(Has<TextureHandle>(), "Failed to retrieve texture handle associated with sprite");

	const auto& texture_handle{ Get<TextureHandle>() };
	return texture_handle.GetTexture(*this);
}

impl::Texture& Sprite::GetTexture() {
	return const_cast<impl::Texture&>(std::as_const(*this).GetTexture());
}

Sprite& Sprite::SetVisible(bool visible) {
	AddOrRemove<Visible>(visible);
	return *this;
}

Sprite& Sprite::Show() {
	return SetVisible(true);
}

Sprite& Sprite::Hide() {
	return SetVisible(false);
}

bool Sprite::IsVisible() const {
	return GetOrParentOrDefault<Visible>(false);
}

Sprite& Sprite::SetDepth(const Depth& depth) {
	if (Has<Depth>()) {
		Get<Depth>() = depth;
	} else {
		Add<Depth>(depth);
	}
	return *this;
}

Depth Sprite::GetDepth() const {
	return GetOrDefault<Depth>();
}

Sprite& Sprite::SetBlendMode(BlendMode blend_mode) {
	if (Has<BlendMode>()) {
		Get<BlendMode>() = blend_mode;
	} else {
		Add<BlendMode>(blend_mode);
	}
	return *this;
}

BlendMode Sprite::GetBlendMode() const {
	return GetOrDefault<BlendMode>(BlendMode::Blend);
}

Sprite& Sprite::SetTint(const Color& color) {
	AddOrRemove<Tint>(color != Tint{}, color);
	return *this;
}

Color Sprite::GetTint() const {
	return GetOrDefault<Tint>();
}

V2_int Sprite::GetTextureSize() const {
	return Has<TextureHandle>() ? Get<TextureHandle>().GetSize(*this) : V2_int{};
}

std::array<V2_float, 4> Sprite::GetTextureCoordinates(bool flip_vertically) const {
	auto tex_coords{ impl::GetDefaultTextureCoordinates() };

	auto check_vertical_flip = [flip_vertically, &tex_coords]() {
		if (flip_vertically) {
			impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
		}
	};

	if (!*this) {
		std::invoke(check_vertical_flip);
		return tex_coords;
	}

	V2_int texture_size{ GetTextureSize() };

	if (texture_size.IsZero()) {
		std::invoke(check_vertical_flip);
		return tex_coords;
	}

	if (Has<TextureCrop>()) {
		const auto& crop{ Get<TextureCrop>() };
		if (crop != TextureCrop{}) {
			tex_coords = impl::GetTextureCoordinates(crop.position, crop.size, texture_size);
		}
	}

	auto scale{ GetScale() };

	bool flip_x{ scale.x < 0.0f };
	bool flip_y{ scale.y < 0.0f };

	if (flip_x && flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Both);
	} else if (flip_x) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Horizontal);
	} else if (flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
	}

	// TODO: Consider if this is necessary given entity scale already flips a texture.
	if (Has<Flip>()) {
		impl::FlipTextureCoordinates(tex_coords, Get<Flip>());
	}

	std::invoke(check_vertical_flip);

	return tex_coords;
}

namespace impl {

AnimationInfo::AnimationInfo(
	std::size_t frame_count, const V2_float& frame_size, const V2_float& start_pixel,
	std::size_t start_frame
) :
	frame_count_{ frame_count },
	frame_size_{ frame_size },
	start_pixel_{ start_pixel },
	start_frame_{ start_frame } {}

std::size_t AnimationInfo::GetSequenceRepeats() const {
	return frame_repeats_ / frame_count_;
}

std::size_t AnimationInfo::GetFrameRepeats() const {
	return frame_repeats_;
}

std::size_t AnimationInfo::GetFrameCount() const {
	return frame_count_;
}

void AnimationInfo::SetCurrentFrame(std::size_t new_frame) {
	current_frame_ = Mod(new_frame, frame_count_);
}

void AnimationInfo::IncrementFrame() {
	SetCurrentFrame(++current_frame_);
}

void AnimationInfo::ResetToStartFrame() {
	current_frame_ = start_frame_;
}

std::size_t AnimationInfo::GetCurrentFrame() const {
	return current_frame_;
}

V2_float AnimationInfo::GetCurrentFramePosition() const {
	V2_float frame_pos{ start_pixel_.x + frame_size_.x * current_frame_, start_pixel_.y };
	return frame_pos;
}

V2_float AnimationInfo::GetFrameSize() const {
	return frame_size_;
}

std::size_t AnimationInfo::GetStartFrame() const {
	return start_frame_;
}

} // namespace impl

Animation& AnimationMap::Load(const ActiveMapManager::Key& key, Animation&& entity, bool hide) {
	auto [it, inserted] = GetMap().try_emplace(GetInternalKey(key), std::move(entity));
	if (hide) {
		it->second.Hide();
	}
	return it->second;
}

bool AnimationMap::SetActive(const ActiveMapManager::Key& key) {
	if (auto internal_key{ GetInternalKey(key) }; internal_key == active_key_) {
		return false;
	}
	auto& active{ GetActive() };
	active.Hide();
	active.Get<Tween>().Pause();
	ActiveMapManager::SetActive(key);
	auto& new_active{ GetActive() };
	new_active.Show();
	return true;
}

Animation::Animation(
	Manager& manager, const TextureHandle& texture_key, std::size_t frame_count,
	const V2_float& frame_size, milliseconds animation_duration, std::int64_t repeats,
	const V2_float& start_pixel, std::size_t start_frame
) :
	Sprite{ manager, texture_key } {
	PTGN_ASSERT(start_frame < frame_count, "Start frame must be within animation frame count");

	auto& crop = Add<TextureCrop>();
	auto& anim = Add<impl::AnimationInfo>(frame_count, frame_size, start_pixel, start_frame);

	crop.position = anim.GetCurrentFramePosition();
	crop.size	  = anim.GetFrameSize();

	milliseconds frame_duration{ animation_duration / frame_count };

	std::int64_t frame_repeats{ repeats == -1 ? -1
											  : repeats * static_cast<std::int64_t>(frame_count) };

	// TODO: Consider breaking this up into individual tween points using a for loop.
	// TODO: Switch to using a system.
	Add<Tween>()
		.During(frame_duration)
		.Repeat(frame_repeats)
		.OnStart([entity = *this]() mutable {
			auto [a, c] = entity.Get<impl::AnimationInfo, TextureCrop>();
			a.ResetToStartFrame();
			c.position = a.GetCurrentFramePosition();
			c.size	   = a.GetFrameSize();
			// TODO: Fix.
			// Invoke<callback::AnimationStart>(entity, entity);
		})
		.OnRepeat([entity = *this](Tween& tween) mutable {
			auto [a, c] = entity.Get<impl::AnimationInfo, TextureCrop>();
			a.IncrementFrame();
			c.position = a.GetCurrentFramePosition();
			c.size	   = a.GetFrameSize();
			auto anim_frame_count{ a.GetFrameCount() };
			auto tween_repeats{ tween.GetRepeats() };
			if (tween_repeats != -1 &&
				static_cast<std::size_t>(tween_repeats) == anim_frame_count) {
				// TODO: Fix.
				// Invoke<callback::AnimationComplete>(entity, entity);
			} else if (a.GetFrameRepeats() % anim_frame_count == 0) {
				// TODO: Fix.
				// Invoke<callback::AnimationRepeat>(entity, entity);
			}
		})
		.OnReset([entity = *this]() mutable {
			auto [a, c] = entity.Get<impl::AnimationInfo, TextureCrop>();
			a.ResetToStartFrame();
			c.position = a.GetCurrentFramePosition();
			c.size	   = a.GetFrameSize();
		});
}

} // namespace ptgn