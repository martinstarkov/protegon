#include "components/draw.h"

#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "common/assert.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/game_object.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/tween.h"
#include "math/geometry.h"
#include "math/math.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/batching/render_data.h"
#include "rendering/resources/texture.h"

namespace ptgn {

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

Sprite::Sprite(Manager& manager, std::string_view texture_key) : GameObject{ manager } {
	SetDraw<Sprite>();

	PTGN_ASSERT(
		game.texture.Has(texture_key), "Sprite texture key must be loaded in the texture manager"
	);

	Add<TextureKey>(texture_key);
	Add<Visible>();
}

void Sprite::Draw(impl::RenderData& ctx, const Entity& entity) {
	auto transform{ entity.GetAbsoluteTransform() };
	auto depth{ entity.GetDepth() };
	auto blend_mode{ entity.GetBlendMode() };
	auto tint{ entity.GetTint().Normalized() };
	auto origin{ entity.GetOrigin() };

	const auto& texture_key{ entity.Get<TextureKey>() };
	const auto& texture{ game.texture.Get(texture_key) };
	auto size{ entity.GetSize() };
	auto coords{ entity.GetTextureCoordinates(false) };
	auto vertices{ impl::GetVertices(transform, size, origin) };

	ctx.AddTexturedQuad(vertices, coords, texture, depth, blend_mode, tint, false);
}

Animation::Animation(
	Manager& manager, std::string_view texture_key, std::size_t frame_count,
	const V2_float& frame_size, milliseconds animation_duration, const V2_float& start_pixel,
	std::size_t start_frame
) :
	Sprite{ manager, texture_key } {
	PTGN_ASSERT(start_frame < frame_count, "Start frame must be within animation frame count");

	auto& crop = Add<TextureCrop>();
	auto& anim = Add<impl::AnimationInfo>(frame_count, frame_size, start_pixel, start_frame);

	crop.position = anim.GetCurrentFramePosition();
	crop.size	  = anim.GetFrameSize();

	milliseconds frame_duration{ animation_duration / frame_count };

	// TODO: Consider breaking this up into individual tween points using a for loop.
	// TODO: Switch to using a system.
	Add<Tween>()
		.During(frame_duration)
		.Repeat(-1)
		.OnStart([entity = GetEntity()]() mutable {
			auto [a, c] = entity.Get<impl::AnimationInfo, TextureCrop>();
			a.ResetToStartFrame();
			c.position = a.GetCurrentFramePosition();
			c.size	   = a.GetFrameSize();
			Invoke<callback::AnimationStart>(entity);
		})
		.OnRepeat([entity = GetEntity()]() mutable {
			auto [a, c] = entity.Get<impl::AnimationInfo, TextureCrop>();
			a.IncrementFrame();
			c.position = a.GetCurrentFramePosition();
			c.size	   = a.GetFrameSize();
			if (a.GetFrameRepeats() % a.GetFrameCount() == 0) {
				Invoke<callback::AnimationRepeat>(entity);
			}
		})
		.OnReset([entity = GetEntity()]() mutable {
			auto [a, c] = entity.Get<impl::AnimationInfo, TextureCrop>();
			a.ResetToStartFrame();
			c.position = a.GetCurrentFramePosition();
			c.size	   = a.GetFrameSize();
		});
}

void Animation::Draw(impl::RenderData& ctx, const Entity& entity) {
	Sprite::Draw(ctx, entity);
}

} // namespace ptgn