#include "runtime/animation/animation.h"

#include <chrono>
#include <cstdint>
#include <list>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

#include "app/context.h"
#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/time/timer.h"
#include "renderer/primitives/texture.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/scripts.h"

namespace ptgn {

Animation::Animation(Entity entity) : Entity{ entity } {}

void Animation::Draw(RenderContext& renderer, Entity entity) {
	Sprite::Draw(renderer, entity);
}

Animation& Animation::Start(bool force) {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	PTGN_ASSERT(Has<impl::TextureCrop>(), "Animation must have TextureCrop component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.current_frame = 0;
	anim.frames_played = 0;
	auto& crop		   = Get<impl::TextureCrop>();
	crop.position	   = anim.GetCurrentFramePosition();
	crop.size		   = anim.frame_size;
	if (bool started{ anim.frame_timer.Start(force) }; started) {
		if (auto scripts{ TryGet<impl::Scripts>() }) {
			AnimationStart event;
			scripts->Emit(event);
		}
	}
	return *this;
}

Animation& Animation::Reset() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	PTGN_ASSERT(Has<impl::TextureCrop>(), "Animation must have TextureCrop component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.current_frame = 0;
	anim.frames_played = 0;
	auto& crop		   = Get<impl::TextureCrop>();
	crop.position	   = anim.GetCurrentFramePosition();
	crop.size		   = anim.frame_size;
	anim.frame_timer.Reset();
	if (auto scripts{ TryGet<impl::Scripts>() }) {
		AnimationStop event;
		scripts->Emit(event);
	}
	return *this;
}

Animation& Animation::Stop() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.frame_timer.Stop();
	if (auto scripts{ TryGet<impl::Scripts>() }) {
		AnimationStop event;
		scripts->Emit(event);
	}
	return *this;
}

Animation& Animation::Toggle() {
	if (IsPlaying()) {
		Stop();
	} else {
		Start();
	}
	return *this;
}

Animation& Animation::Pause() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.frame_timer.Pause();
	if (auto scripts{ TryGet<impl::Scripts>() }) {
		AnimationPause event;
		scripts->Emit(event);
	}
	return *this;
}

Animation& Animation::Resume() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.frame_timer.Resume();
	if (auto scripts{ TryGet<impl::Scripts>() }) {
		AnimationResume event;
		scripts->Emit(event);
	}
	return *this;
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

Animation& Animation::SetCurrentFrame(std::size_t new_frame) {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.SetCurrentFrame(new_frame);
	return *this;
}

Animation& Animation::IncrementFrame() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.IncrementFrame();
	return *this;
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
	V2_float animation_frame_size, std::int64_t animation_play_count, V2_float animation_start_pixel
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

void AnimationSystem::Update(Scene& scene) {
	for (auto [entity, anim, crop] : scene.EntitiesWith<AnimationInfo, TextureCrop>()) {
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
				AnimationComplete event;
				scripts->Emit(event);
			}
			// Reset animation to start frame after it finishes.
			anim.SetCurrentFrame(0);
			if (auto scripts{ entity.TryGet<Scripts>() }) {
				AnimationFrameChange event;
				scripts->Emit(event);
			}
			crop.size	  = anim.frame_size;
			crop.position = anim.GetCurrentFramePosition();
			anim.frame_timer.Stop();
			if (auto scripts{ entity.TryGet<Scripts>() }) {
				AnimationStop event;
				scripts->Emit(event);
			}
			continue;
		}

		if (auto scripts{ entity.TryGet<Scripts>() }) {
			AnimationUpdate event;
			scripts->Emit(event);
		}

		if (auto frame_duration{ anim.GetFrameDuration() };
			!anim.frame_timer.Completed(frame_duration)) {
			continue;
		}

		// Frame completed.

		anim.frames_played++;

		anim.IncrementFrame();

		if (auto scripts{ entity.TryGet<Scripts>() }) {
			AnimationFrameChange event;
			scripts->Emit(event);
		}

		crop.size	  = anim.frame_size;
		crop.position = anim.GetCurrentFramePosition();

		if (anim.frames_played % anim.frame_count == 0) {
			if (auto scripts{ entity.TryGet<Scripts>() }) {
				AnimationRepeat event;
				scripts->Emit(event);
			}
		}

		anim.frame_timer.Start(true);
	}

	scene.Refresh();
}

} // namespace impl

AnimationMap::AnimationMap(Entity entity) : Entity{ entity } {}

Animation AnimationMap::Add(std::string_view animation_key, Animation animation, bool hide) {
	if (hide) {
		Hide(animation);
	}

	PTGN_ASSERT(Has<impl::AnimationMapData>());

	auto& info{ Get<impl::AnimationMapData>() };

	impl::AnimationMapKey key{ animation_key };

	animation.Add<impl::AnimationMapKey>(key);

	if (auto it{ info.animations.find(key) }; it == info.animations.end()) {
		auto [new_it, inserted] = info.animations.try_emplace(key, std::move(animation));
		PTGN_ASSERT(inserted, "Failed to insert toggle button");
		Animation btn{ new_it->second };
		return btn;
	} else {
		it->second = GameObject{ std::move(animation) };
		return Animation{ it->second };
	}
}

void AnimationMap::Remove(std::string_view animation_key) {
	PTGN_ASSERT(Has<impl::AnimationMapData>());

	impl::AnimationMapKey key{ animation_key };

	auto& info{ Get<impl::AnimationMapData>() };

	info.animations.erase(key);
}

std::optional<Animation> AnimationMap::GetActive() const {
	PTGN_ASSERT(Has<impl::AnimationMapData>());

	auto& info{ Get<impl::AnimationMapData>() };

	auto it{ info.animations.find(info.active) };

	if (it == info.animations.end()) {
		return {};
	}

	return Animation{ it->second };
}

bool AnimationMap::SetActive(std::string_view animation_key) {
	PTGN_ASSERT(Has<impl::AnimationMapData>());

	auto& info{ Get<impl::AnimationMapData>() };

	impl::AnimationMapKey key{ animation_key };

	if (info.active == key) {
		return false;
	}

	PTGN_ASSERT(
		info.animations.contains(key),
		"Cannot set non-existent toggle button key to active: ", animation_key
	);

	auto prev_active{ info.animations.find(info.active) };

	// Hide and pause old active animation.
	Hide(prev_active->second, false);
	Animation{ prev_active->second }.Pause();

	auto it{ info.animations.find(key) };

	info.active = key;
	Show(it->second);
	return true;
}

Animation CreateAnimation(
	Scene& scene, std::variant<Texture, std::string_view> texture, V2_float position,
	std::size_t frame_count, milliseconds animation_duration, std::optional<V2_int> frame_size,
	std::int64_t play_count, V2_int start_pixel
) {
	Texture resolved_texture;

	std::visit(
		[&](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, Texture>) {
				resolved_texture = arg;
			} else if constexpr (std::is_same_v<T, std::string_view>) {
				PTGN_ASSERT(
					scene.app().asset.HasTexture(arg),
					"Texture key must be loaded in the asset manager before creating animation"
				);

				resolved_texture = *scene.app().asset.GetTexture(arg);
			}
		},
		texture
	);

	PTGN_ASSERT(
		play_count == -1 || play_count >= 0,
		"Play count must be -1 (infinite) or otherwise non-negative"
	);

	PTGN_ASSERT(frame_count > 0, "Cannot create an animation with 0 frames");

	Animation animation{ CreateSprite(scene, resolved_texture, position) };

	auto texture_size{ resolved_texture.GetSize() };

	if (!frame_size.has_value()) {
		frame_size = { static_cast<std::size_t>(texture_size.x) / frame_count, texture_size.y };
	}

	const auto& anim = animation.Add<impl::AnimationInfo>(
		animation_duration, frame_count, *frame_size, play_count, start_pixel
	);
	auto& crop = animation.Add<impl::TextureCrop>();

	crop.position = anim.GetCurrentFramePosition();
	crop.size	  = anim.frame_size;

	return animation;
}

AnimationMap CreateAnimationMap(Scene& scene) {
	AnimationMap animation_map{ scene.CreateEntity() };

	animation_map.Entity::Add<impl::AnimationMapData>();

	return animation_map;
}

} // namespace ptgn