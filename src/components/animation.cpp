#include "components/animation.h"

#include <chrono>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <utility>

#include "common/assert.h"
#include "components/draw.h"
#include "components/sprite.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "core/time.h"
#include "core/timer.h"
#include "math/vector2.h"
#include "renderer/texture.h"
#include "resources/resource_manager.h"

namespace ptgn {

void Animation::Start(bool force) {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	PTGN_ASSERT(Has<TextureCrop>(), "Animation must have TextureCrop component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.current_frame = 0;
	anim.frames_played = 0;
	auto& crop		   = Get<TextureCrop>();
	crop.position	   = anim.GetCurrentFramePosition();
	crop.size		   = anim.frame_size;
	bool started{ anim.frame_timer.Start(force) };
	if (started) {
		if (auto scripts{ TryGet<Scripts>() }) {
			scripts->AddAction(&AnimationScript::OnAnimationStart);
		}
	}
}

void Animation::Reset() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	PTGN_ASSERT(Has<TextureCrop>(), "Animation must have TextureCrop component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.current_frame = 0;
	anim.frames_played = 0;
	auto& crop		   = Get<TextureCrop>();
	crop.position	   = anim.GetCurrentFramePosition();
	crop.size		   = anim.frame_size;
	if (auto scripts{ TryGet<Scripts>() }) {
		scripts->AddAction(&AnimationScript::OnAnimationStop);
	}
	anim.frame_timer.Reset();
}

void Animation::Stop() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	if (auto scripts{ TryGet<Scripts>() }) {
		scripts->AddAction(&AnimationScript::OnAnimationStop);
	}
	anim.frame_timer.Stop();
}

void Animation::Toggle() {
	if (IsPlaying()) {
		Stop();
	} else {
		Start();
	}
}

void Animation::Pause() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	if (auto scripts{ TryGet<Scripts>() }) {
		scripts->AddAction(&AnimationScript::OnAnimationPause);
	}
	anim.frame_timer.Pause();
}

void Animation::Resume() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	if (auto scripts{ TryGet<Scripts>() }) {
		scripts->AddAction(&AnimationScript::OnAnimationResume);
	}
	anim.frame_timer.Resume();
}

bool Animation::IsPaused() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.frame_timer.IsPaused();
}

bool Animation::IsPlaying() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.frame_timer.IsRunning();
}

std::size_t Animation::GetPlayCount() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.GetPlayCount();
}

std::size_t Animation::GetFramePlayCount() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.frames_played;
}

milliseconds Animation::GetDuration() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.duration;
}

milliseconds Animation::GetFrameDuration() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	milliseconds frame_duration{ anim.duration / anim.frame_count };
	return frame_duration;
}

std::size_t Animation::GetFrameCount() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.frame_count;
}

void Animation::SetCurrentFrame(std::size_t new_frame) {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.SetCurrentFrame(new_frame);
}

void Animation::IncrementFrame() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.IncrementFrame();
}

std::size_t Animation::GetCurrentFrame() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.current_frame;
}

V2_int Animation::GetCurrentFramePosition() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.GetCurrentFramePosition();
}

V2_int Animation::GetFrameSize() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.frame_size;
}

namespace impl {

AnimationInfo::AnimationInfo(
	milliseconds animation_duration, std::size_t animation_frame_count,
	const V2_float& animation_frame_size, std::int64_t animation_play_count,
	const V2_float& animation_start_pixel
) :
	duration{ animation_duration },
	frame_count{ animation_frame_count },
	frame_size{ animation_frame_size },
	play_count{ animation_play_count },
	start_pixel{ animation_start_pixel } {}

milliseconds AnimationInfo::GetFrameDuration() const {
	return duration / frame_count;
}

V2_int AnimationInfo::GetCurrentFramePosition() const {
	return { start_pixel.x + frame_size.x * static_cast<int>(current_frame), start_pixel.y };
}

std::size_t AnimationInfo::GetPlayCount() const {
	return frames_played / frame_count;
}

void AnimationInfo::SetCurrentFrame(std::size_t new_frame) {
	current_frame = new_frame % frame_count;
	frame_dirty	  = true;
}

void AnimationInfo::IncrementFrame() {
	SetCurrentFrame(current_frame + 1);
}

void AnimationSystem::Update(Manager& manager) {
	for (auto [entity, anim, crop] : manager.EntitiesWith<AnimationInfo, TextureCrop>()) {
		if (anim.frame_dirty) {
			crop.size	  = anim.frame_size;
			crop.position = anim.GetCurrentFramePosition();

			anim.frame_dirty = false;
		}

		if (anim.frame_count == 0 || anim.duration <= milliseconds{ 0 } ||
			!anim.frame_timer.IsRunning() || anim.frame_timer.IsPaused()) {
			// Timer is not active or animation has no frames / duration.
			continue;
		}

		if (bool infinite_playback{ anim.play_count == -1 };
			!infinite_playback &&
			anim.frames_played >= static_cast<std::size_t>(anim.play_count) * anim.frame_count) {
			if (auto scripts{ entity.TryGet<Scripts>() }) {
				scripts->AddAction(&AnimationScript::OnAnimationComplete);
			}
			// Reset animation to start frame after it finishes.
			anim.SetCurrentFrame(0);
			if (auto scripts{ entity.TryGet<Scripts>() }) {
				scripts->AddAction(&AnimationScript::OnAnimationFrameChange);
			}
			crop.size	  = anim.frame_size;
			crop.position = anim.GetCurrentFramePosition();
			anim.frame_timer.Stop();
			if (auto scripts{ entity.TryGet<Scripts>() }) {
				scripts->AddAction(&AnimationScript::OnAnimationStop);
			}
			continue;
		}

		if (auto scripts{ entity.TryGet<Scripts>() }) {
			scripts->AddAction(&AnimationScript::OnAnimationUpdate);
		}

		if (auto frame_duration{ anim.GetFrameDuration() };
			!anim.frame_timer.Completed(frame_duration)) {
			continue;
		}

		// Frame completed.

		anim.frames_played++;

		anim.IncrementFrame();

		if (auto scripts{ entity.TryGet<Scripts>() }) {
			scripts->AddAction(&AnimationScript::OnAnimationFrameChange);
		}

		crop.size	  = anim.frame_size;
		crop.position = anim.GetCurrentFramePosition();

		if (anim.frames_played % anim.frame_count == 0) {
			if (auto scripts{ entity.TryGet<Scripts>() }) {
				scripts->AddAction(&AnimationScript::OnAnimationRepeat);
			}
		}

		anim.frame_timer.Start(true);
	}

	for (auto [e, anim, scripts] : manager.EntitiesWith<AnimationInfo, Scripts>()) {
		scripts.InvokeActions();
	}

	manager.Refresh();
}

} // namespace impl

Animation& AnimationMap::Load(const ActiveMapManager::Key& key, Animation&& entity, bool hide) {
	auto [it, inserted] = GetMap().try_emplace(GetInternalKey(key), std::move(entity));
	if (hide) {
		Hide(it->second);
	}
	return it->second;
}

bool AnimationMap::SetActive(const ActiveMapManager::Key& key) {
	if (auto internal_key{ GetInternalKey(key) }; internal_key == active_key_) {
		return false;
	}
	auto& active{ GetActive() };
	Hide(active);
	active.Pause();
	ActiveMapManager::SetActive(key);
	auto& new_active{ GetActive() };
	Show(new_active);
	return true;
}

Animation CreateAnimation(
	Manager& manager, const TextureHandle& texture_key, const V2_float& position,
	std::size_t frame_count, milliseconds animation_duration, V2_int frame_size,
	std::int64_t play_count, const V2_int& start_pixel
) {
	PTGN_ASSERT(
		play_count == -1 || play_count >= 0,
		"Play count must be -1 (infinite) or otherwise non-negative"
	);

	PTGN_ASSERT(frame_count > 0, "Cannot create an animation with 0 frames");

	Animation animation{ CreateSprite(manager, texture_key, position) };

	auto texture_size{ texture_key.GetSize() };

	if (frame_size.IsZero()) {
		frame_size = { texture_size.x / frame_count, texture_size.y };
	}

	const auto& anim = animation.Add<impl::AnimationInfo>(
		animation_duration, frame_count, frame_size, play_count, start_pixel
	);
	auto& crop = animation.Add<TextureCrop>();

	crop.position = anim.GetCurrentFramePosition();
	crop.size	  = anim.frame_size;

	return animation;
}

} // namespace ptgn