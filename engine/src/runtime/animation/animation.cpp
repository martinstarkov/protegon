#include "runtime/animation/animation.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "core/util/timer.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/animation/animation_event.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event.h"

namespace ptgn {

Animation::Animation(Entity entity) : Entity{ entity } {}

Animation& Animation::SetConfig(AnimationConfig config) {
	auto texture_size{ GetTextureSize(*this) };

	if (auto anim_data{ TryGet<impl::AnimationData>() };
		anim_data && anim_data->config.IsIdentical(config, texture_size)) {
		return *this;
	}

	const auto& anim{ Add<impl::AnimationData>(std::move(config), texture_size) };

	if (Has<impl::TextureCrop>()) {
		auto& crop{ Get<impl::TextureCrop>() };
		crop.Update(anim);
	}

	return Reset();
}

Animation& Animation::Start(bool force) {
	if (!Has<impl::AnimationData, impl::TextureCrop>()) {
		return *this;
	}
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
	if (!Has<impl::AnimationData, impl::TextureCrop>()) {
		return *this;
	}
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
	if (!Has<impl::AnimationData>()) {
		return *this;
	}
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
	if (!Has<impl::AnimationData>()) {
		return *this;
	}
	auto& anim{ Get<impl::AnimationData>() };
	anim.frame_timer.Pause();
	PushEvent<event::AnimationPause>(*this, *this);
	return *this;
}

Animation& Animation::Resume() {
	if (!Has<impl::AnimationData>()) {
		return *this;
	}
	auto& anim{ Get<impl::AnimationData>() };
	anim.frame_timer.Resume();
	PushEvent<event::AnimationResume>(*this, *this);
	return *this;
}

bool Animation::IsPaused() const {
	if (!Has<impl::AnimationData>()) {
		return false;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.frame_timer.IsPaused();
}

bool Animation::IsPlaying() const {
	if (!Has<impl::AnimationData>()) {
		return false;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.frame_timer.IsRunning();
}

std::size_t Animation::GetPlayCount() const {
	if (!Has<impl::AnimationData>()) {
		return 0;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.GetPlayCount();
}

std::size_t Animation::GetFramePlayCount() const {
	if (!Has<impl::AnimationData>()) {
		return 0;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.frames_played;
}

milliseconds Animation::GetDuration() const {
	if (!Has<impl::AnimationData>()) {
		return 0ms;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.config.duration;
}

milliseconds Animation::GetFrameDuration() const {
	if (!Has<impl::AnimationData>()) {
		return 0ms;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	milliseconds frame_duration{ anim.config.duration / anim.config.frame_count };
	return frame_duration;
}

std::size_t Animation::GetFrameCount() const {
	if (!Has<impl::AnimationData>()) {
		return 0;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.config.frame_count;
}

Animation& Animation::SetCurrentFrame(std::size_t new_frame) {
	if (!Has<impl::AnimationData>()) {
		return *this;
	}
	auto& anim{ Get<impl::AnimationData>() };
	anim.SetCurrentFrame(new_frame);
	return *this;
}

Animation& Animation::SetTexture(TextureKey texture_key) {
	Sprite{ *this }.SetTexture(texture_key);
	return *this;
}

Animation& Animation::IncrementFrame() {
	if (!Has<impl::AnimationData>()) {
		return *this;
	}
	auto& anim{ Get<impl::AnimationData>() };
	anim.IncrementFrame();
	return *this;
}

Animation& Animation::SetResetOnComplete(bool reset_on_complete) {
	if (!Has<impl::AnimationData>()) {
		return *this;
	}
	auto& anim{ Get<impl::AnimationData>() };
	anim.config.reset_on_complete = reset_on_complete;
	return *this;
}

std::size_t Animation::GetCurrentFrame() const {
	if (!Has<impl::AnimationData>()) {
		return 0;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.current_frame;
}

V2_int Animation::GetCurrentFramePosition() const {
	if (!Has<impl::AnimationData>()) {
		return {};
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.GetCurrentFramePosition();
}

V2_int Animation::GetFrameSize() const {
	if (!Has<impl::AnimationData>()) {
		return {};
	}
	const auto& anim{ Get<impl::AnimationData>() };
	auto texture_size{ GetTextureSize(*this) };
	return anim.config.frame_size.value_or(
		impl::GetFrameSize(texture_size, anim.config.frame_count).value_or(V2_int{})
	);
}

namespace impl {

AnimationData::AnimationData(AnimationConfig&& anim_config, std::optional<V2_int> texture_size) :
	config{ std::move(anim_config) } {
	if (!config.frame_size.has_value()) {
		config.frame_size = impl::GetFrameSize(texture_size, config.frame_count);
	}
}

milliseconds AnimationData::GetFrameDuration() const {
	if (!config.frame_count) {
		return 0ms;
	}
	return config.duration / config.frame_count;
}

V2_int AnimationData::GetFrameSize(std::optional<V2_int> texture_size) const {
	return config.frame_size.value_or(
		impl::GetFrameSize(texture_size, config.frame_count).value_or(V2_int{})
	);
}

V2_int AnimationData::GetCurrentFramePosition() const {
	auto frame_size{ GetFrameSize(std::nullopt) };
	return { config.start_pixel.x + frame_size.x * static_cast<int>(current_frame),
			 config.start_pixel.y };
}

std::size_t AnimationData::GetPlayCount() const {
	if (!config.frame_count) {
		return 0;
	}
	return frames_played / config.frame_count;
}

void AnimationData::SetCurrentFrame(std::size_t new_frame) {
	current_frame = new_frame % config.frame_count;
	frame_dirty	  = true;
}

void AnimationData::IncrementFrame() {
	SetCurrentFrame(current_frame + 1);
}

void AnimationSystem::Update(Scene& scene, secondsf dt) {
	const auto frame_change = [](Animation anim_entity, auto& crop, const auto& anim) {
		PushEvent<event::AnimationFrameChange>(anim_entity, anim_entity);
		crop.Update(anim);
	};

	for (auto [entity, anim, crop] : scene.EntitiesWith<AnimationData, TextureCrop>()) {
		anim.frame_timer.Update(dt);

		Animation anim_entity{ entity };

		if (anim.frame_dirty) {
			crop.Update(anim);

			anim.frame_dirty = false;
		}

		if (anim.config.frame_count == 0 || anim.config.duration <= 0ms ||
			!anim.frame_timer.IsRunning() || anim.frame_timer.IsPaused()) {
			// Timer is not active or animation has no frames / duration.
			continue;
		}

		std::size_t next_frames_played{ anim.frames_played + 1 };

		// All animation plays have completed.
		if (anim.config.play_count.has_value()) {
			if (std::size_t total_frames{ anim.config.play_count.value() *
										  anim.config.frame_count };
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

Animation AnimationMap::Find(impl::AnimationMapKey key) const {
	if (!HasChildren(*this)) {
		return {};
	}

	auto hash{ Hash(key) };

	auto children{ GetChildren(*this) };
	auto it{ std::ranges::find_if(children, [hash](Entity child) {
		auto child_key{ child.TryGet<impl::AnimationMapKey>() };
		return child_key && Hash(*child_key) == hash;
	}) };

	return it != children.end() ? Animation{ *it } : Animation{};
}

Animation AnimationMap::Add(std::string_view animation_key, Animation animation, bool hide) {
	if (!Has<impl::AnimationMapData>()) {
		return animation;
	}

	impl::AnimationMapKey key{ animation_key };

	if (auto existing{ Find(key) }; existing && existing != animation) {
		existing.Destroy();
	}

	if (hide) {
		Hide(animation);
	}

	animation.Add<impl::AnimationMapKey>(key);
	SetParent(animation, *this);

	return animation;
}

void AnimationMap::Remove(std::string_view animation_key) {
	if (!Has<impl::AnimationMapData>()) {
		return;
	}

	impl::AnimationMapKey key{ animation_key };

	if (auto animation{ Find(key) }) {
		animation.Destroy();
	}

	auto& info{ Get<impl::AnimationMapData>() };

	if (info.active == key) {
		info.active = {};
	}
}

std::optional<Animation> AnimationMap::GetActive() const {
	if (!Has<impl::AnimationMapData>()) {
		return std::nullopt;
	}

	auto animation{ Find(Get<impl::AnimationMapData>().active) };

	return animation ? std::optional<Animation>{ animation } : std::nullopt;
}

bool AnimationMap::SetActive(std::string_view animation_key) {
	if (!Has<impl::AnimationMapData>()) {
		return false;
	}

	auto& info{ Get<impl::AnimationMapData>() };

	impl::AnimationMapKey key{ animation_key };

	if (info.active == key) {
		return false;
	}

	auto animation{ Find(key) };

	if (!animation) {
		PTGN_WARN("Attempting to set non-existent animation key to active: ", animation_key);
		return false;
	}

	if (auto previous{ Find(info.active) }) {
		Hide(previous);
		previous.Pause();
	}

	info.active = key;
	Show(animation);

	return true;
}

Animation CreateAnimation(
	Scene& scene, Transform transform, TextureKey texture_key, AnimationConfig config, Origin origin
) {
	Animation animation{ CreateSprite(scene, transform, std::move(texture_key), origin) };
	animation.SetConfig(std::move(config));
	animation.Add<Tag>("Animation");
	return animation;
}

Animation PlayTemporaryAnimation(
	Scene& scene, Transform transform, TextureKey texture_key, AnimationConfig config,
	milliseconds destroy_delay, Origin origin
) {
	Animation animation{
		CreateAnimation(scene, transform, std::move(texture_key), std::move(config), origin)
	};
	animation.Add<Tag>("Temporary Animation");

	animation.OnComplete([destroy_delay](auto& event) mutable {
		if (destroy_delay == 0ms) {
			event.animation.Destroy();
			return;
		}

		After(
			event.animation.GetScene(),
			destroy_delay,
			[animation = event.animation]() mutable {
				animation.Destroy();
			}
		);
	});

	animation.Start(true);
	return animation;
}

AnimationMap CreateAnimationMap(Scene& scene) {
	AnimationMap animation_map{ scene.CreateEntity() };

	animation_map.Entity::Add<Tag>("Animation Map");
	animation_map.Entity::Add<impl::AnimationMapData>();

	return animation_map;
}

} // namespace ptgn