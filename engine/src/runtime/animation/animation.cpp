#include "runtime/animation/animation.h"

#include <chrono>
#include <list>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "core/util/timer.h"
#include "renderer/resources/texture.h"
#include "runtime/animation/animation_event.h"
#include "runtime/asset/asset.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/script_sequence.h"

namespace ptgn {

Animation::Animation(Entity entity) : Entity{ entity } {}

Animation& Animation::Start(bool force) {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	PTGN_ASSERT(Has<impl::TextureCrop>(), "Animation must have TextureCrop component");
	auto& anim{ Get<impl::AnimationData>() };
	anim.current_frame = 0;
	anim.frames_played = 0;
	auto& crop{ Get<impl::TextureCrop>() };
	crop.Update(anim);
	if (bool started{ anim.frame_timer.Start(force) }; started) {
		PushEvent<event::AnimationStart>(*this, *this);
	}
	return *this;
}

Animation& Animation::Reset() {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	PTGN_ASSERT(Has<impl::TextureCrop>(), "Animation must have TextureCrop component");
	auto& anim{ Get<impl::AnimationData>() };
	anim.current_frame = 0;
	anim.frames_played = 0;
	auto& crop{ Get<impl::TextureCrop>() };
	crop.Update(anim);
	anim.frame_timer.Reset();
	PushEvent<event::AnimationStop>(*this, *this);
	return *this;
}

Animation& Animation::Stop(bool reset) {
	if (reset) {
		Reset();
		return *this;
	}
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	auto& anim{ Get<impl::AnimationData>() };
	anim.frame_timer.Stop();
	PushEvent<event::AnimationStop>(*this, *this);
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
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	auto& anim{ Get<impl::AnimationData>() };
	anim.frame_timer.Pause();
	PushEvent<event::AnimationPause>(*this, *this);
	return *this;
}

Animation& Animation::Resume() {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	auto& anim{ Get<impl::AnimationData>() };
	anim.frame_timer.Resume();
	PushEvent<event::AnimationResume>(*this, *this);
	return *this;
}

bool Animation::IsPaused() const {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.frame_timer.IsPaused();
}

bool Animation::IsPlaying() const {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.frame_timer.IsRunning();
}

std::size_t Animation::GetPlayCount() const {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.GetPlayCount();
}

std::size_t Animation::GetFramePlayCount() const {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.frames_played;
}

milliseconds Animation::GetDuration() const {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.config.animation_duration;
}

milliseconds Animation::GetFrameDuration() const {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	const auto& anim{ Get<impl::AnimationData>() };
	milliseconds frame_duration{ anim.config.animation_duration / anim.config.frame_count };
	return frame_duration;
}

std::size_t Animation::GetFrameCount() const {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.config.frame_count;
}

Animation& Animation::SetCurrentFrame(std::size_t new_frame) {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	auto& anim{ Get<impl::AnimationData>() };
	anim.SetCurrentFrame(new_frame);
	return *this;
}

Animation& Animation::SetTexture(TextureOrKey texture) {
	Sprite{ *this }.SetTexture(texture);
	return *this;
}

Animation& Animation::IncrementFrame() {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	auto& anim{ Get<impl::AnimationData>() };
	anim.IncrementFrame();
	return *this;
}

Animation& Animation::SetResetOnComplete(bool reset_on_complete) {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	auto& anim{ Get<impl::AnimationData>() };
	anim.config.reset_on_complete = reset_on_complete;
	return *this;
}

std::size_t Animation::GetCurrentFrame() const {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.current_frame;
}

V2_int Animation::GetCurrentFramePosition() const {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.GetCurrentFramePosition();
}

V2_int Animation::GetFrameSize() const {
	PTGN_ASSERT(Has<impl::AnimationData>(), "Animation must have AnimationData component");
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.config.frame_size;
}

namespace impl {

AnimationData::AnimationData(const AnimationConfig& anim_config, V2_int texture_size) :
	config{ anim_config } {
	PTGN_ASSERT(config.frame_count > 0, "Cannot create an animation with 0 frames");

	if (config.frame_size.IsZero()) {
		config.frame_size = { static_cast<std::size_t>(texture_size.x) / config.frame_count,
							  texture_size.y };
	}
}

milliseconds AnimationData::GetFrameDuration() const {
	return config.animation_duration / config.frame_count;
}

V2_int AnimationData::GetCurrentFramePosition() const {
	return { config.start_pixel.x + config.frame_size.x * static_cast<int>(current_frame),
			 config.start_pixel.y };
}

std::size_t AnimationData::GetPlayCount() const {
	return frames_played / config.frame_count;
}

void AnimationData::SetCurrentFrame(std::size_t new_frame) {
	current_frame = new_frame % config.frame_count;
	frame_dirty	  = true;
}

void AnimationData::IncrementFrame() {
	SetCurrentFrame(current_frame + 1);
}

void AnimationSystem::Update(Scene& scene) {
	const auto frame_change = [](Animation anim_entity, auto& crop, const auto& anim) {
		PushEvent<event::AnimationFrameChange>(anim_entity, anim_entity);
		crop.Update(anim);
	};

	for (auto [entity, anim, crop] : scene.EntitiesWith<AnimationData, TextureCrop>()) {
		Animation anim_entity{ entity };

		if (anim.frame_dirty) {
			crop.Update(anim);

			anim.frame_dirty = false;
		}

		if (anim.config.frame_count == 0 || anim.config.animation_duration <= 0ms ||
			!anim.frame_timer.IsRunning() || anim.frame_timer.IsPaused()) {
			// Timer is not active or animation has no frames / duration.
			continue;
		}

		std::size_t next_frames_played{ anim.frames_played + 1 };

		// All animation plays have completed.
		if (anim.config.play_count.has_value()) {
			if (std::size_t total_frames{ *anim.config.play_count * anim.config.frame_count };
				next_frames_played >= total_frames) {
				PushEvent<event::AnimationComplete>(anim_entity, anim_entity);

				if (anim.config.reset_on_complete) {
					// Reset animation to start frame after it finishes.
					anim.SetCurrentFrame(0);

					frame_change(anim_entity, crop, anim);
				}

				anim.frame_timer.Stop();

				PushEvent<event::AnimationStop>(anim_entity, anim_entity);
				continue;
			}
		}

		PushEvent<event::AnimationUpdate>(anim_entity, anim_entity);

		if (auto frame_duration{ anim.GetFrameDuration() };
			!anim.frame_timer.Completed(frame_duration)) {
			continue;
		}

		anim.frames_played = next_frames_played;

		anim.IncrementFrame();

		frame_change(anim_entity, crop, anim);

		// Loop completed.
		if (anim.frames_played % anim.config.frame_count == 0) {
			PushEvent<event::AnimationLoopComplete>(anim_entity, anim_entity);
		}

		anim.frame_timer.Start(true);
	}
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
	prev_active->second.Pause();

	auto it{ info.animations.find(key) };

	info.active = key;
	Show(it->second);
	return true;
}

Animation CreateAnimation(
	Scene& scene, TextureOrKey texture, V2_float position, const AnimationConfig& config,
	Origin draw_origin
) {
	const auto& assets{ scene.ctx().asset };

	Texture resolved_texture{ texture.Get(assets) };

	Animation animation{ CreateSprite(scene, resolved_texture, position, draw_origin) };

	auto texture_size{ resolved_texture.GetSize() };

	const auto& anim{ animation.Add<impl::AnimationData>(config, texture_size) };

	auto& crop{ animation.Add<impl::TextureCrop>() };
	crop.Update(anim);

	return animation;
}

Animation PlayTemporaryAnimation(
	Scene& scene, TextureOrKey texture, V2_float position, const AnimationConfig& config,
	milliseconds destroy_delay, Origin draw_origin
) {
	Animation anim{ CreateAnimation(scene, texture, position, config, draw_origin) };

	if (destroy_delay == 0ms) {
		anim.OnComplete([](auto& a) mutable { a.animation.Destroy(); });
	} else {
		auto script_sequence{ CreateScriptSequence(scene) };
		script_sequence.Wait(destroy_delay);
		script_sequence.Then([anim]() mutable { anim.Destroy(); });
		anim.OnComplete([script_sequence]() mutable { script_sequence.Start(); });
	}

	anim.Start(true);

	return anim;
}

AnimationMap CreateAnimationMap(Scene& scene) {
	AnimationMap animation_map{ scene.CreateEntity() };

	animation_map.Entity::Add<impl::AnimationMapData>();

	return animation_map;
}

} // namespace ptgn