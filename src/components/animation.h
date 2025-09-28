#pragma once

#include <cstdint>

#include "components/sprite.h"
#include "core/time.h"
#include "core/timer.h"
#include "math/vector2.h"
#include "renderer/texture.h"
#include "resources/resource_manager.h"
#include "serialization/serializable.h"

namespace ptgn {

class Manager;

struct Animation : public Sprite {
	using Sprite::Sprite;

	Animation() = default;

	// Starts the animation. Can also be used to restart the animation.
	// @param force If false, only starts the animation if it is not already playing.
	void Start(bool force = true);

	// Stops and resets the animation.
	void Reset();

	void Stop();

	// Toggles the pause state of the animation.
	void Toggle();

	void Pause();

	void Resume();

	// @return True if the animation is currently paused, false otherwise.
	[[nodiscard]] bool IsPaused() const;

	// @return True if the animation is currently playing, false otherwise.
	[[nodiscard]] bool IsPlaying() const;

	// @return The number of plays of the full animation sequence so far.
	[[nodiscard]] std::size_t GetPlayCount() const;

	// @return The total number of plays of individual animation frames so far.
	[[nodiscard]] std::size_t GetFramePlayCount() const;

	// @return Duration of the full animation sequence.
	[[nodiscard]] milliseconds GetDuration() const;

	// @return Duration of a single animation frame (all frames currently have the same duration).
	[[nodiscard]] milliseconds GetFrameDuration() const;

	[[nodiscard]] std::size_t GetFrameCount() const;

	// Set the current animation frame.
	// new_frame is wrapped around frame_count using Mod().
	void SetCurrentFrame(std::size_t new_frame);

	void IncrementFrame();

	[[nodiscard]] std::size_t GetCurrentFrame() const;

	[[nodiscard]] V2_int GetCurrentFramePosition() const;

	[[nodiscard]] V2_int GetFrameSize() const;
};

struct AnimationMap : public ActiveMapManager<Animation> {
public:
	using ActiveMapManager::ActiveMapManager;

	AnimationMap()									 = default;
	AnimationMap(const AnimationMap&)				 = default;
	AnimationMap& operator=(const AnimationMap&)	 = default;
	AnimationMap(AnimationMap&&) noexcept			 = default;
	AnimationMap& operator=(AnimationMap&&) noexcept = default;
	~AnimationMap() override						 = default;

	/*
	 * The loaded animation is hidden be default.
	 * If key already exists, does nothing.
	 * @param key Unique id of the animation to be loaded.
	 * @return Reference to the loaded animation.
	 */
	Animation& Load(const ActiveMapManager::Key& key, Animation&& entity, bool hide = true);

	// If the provided key is a not currently active, this function pauses the previously active
	// animation. If the key is already active, does nothing.
	// @return True if active value changed, false otherwise.
	bool SetActive(const ActiveMapManager::Key& key);
};

namespace impl {

class AnimationInfo {
public:
	AnimationInfo() = default;

	AnimationInfo(
		milliseconds animation_duration, std::size_t animation_frame_count,
		const V2_float& animation_frame_size, std::int64_t animation_play_count,
		const V2_float& animation_start_pixel
	);

	[[nodiscard]] milliseconds GetFrameDuration() const;
	[[nodiscard]] V2_int GetCurrentFramePosition() const;

	// @return Total number of animation repeats.
	[[nodiscard]] std::size_t GetPlayCount() const;

	void SetCurrentFrame(std::size_t new_frame);
	void IncrementFrame();

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		AnimationInfo, duration, frame_timer, frame_count, frame_size, play_count, start_pixel,
		current_frame, frames_played
	)

	milliseconds duration{ 0 };

	Timer frame_timer;

	// Number of frames in the animation.
	std::size_t frame_count{ 0 };

	// Size of an individual animation frame.
	V2_int frame_size;

	// Number of times the full animation is played. -1 for infinite playback.
	std::int64_t play_count{ 1 };

	// Pixel within the texture which indicates the top left position of the animation sequence.
	V2_int start_pixel;

	// Current frame of the animation.
	std::size_t current_frame{ 0 };

	// Number of frames the animation has gone through. frames_played / frame_count gives the
	// number of repeats of the full animation sequence.
	std::size_t frames_played{ 0 };

	// If the current frame has been changed externally.
	bool frame_dirty{ false };
};

class AnimationSystem {
public:
	static void Update(Manager& manager);
};

} // namespace impl

// @param manager Which manager the entity is added to.
// @param animation_key Key of the animation texture loaded into the texture manager.
// @param position Where on the screen to place the animation object.
// @param frame_count Number of frames in the animation sequence.
// @param animation_duration Duration of the full animation sequence.
// @param frame_size Pixel size of an individual animation frame within the texture.
// If {}, frame_size = { texture_size.x / frame_count, texture_size.y }.
// @param play_count Number of times that the animation plays for, -1 for infinite replay.
// @param start_pixel Pixel within the texture which indicates the top left position of the
// animation sequence.
Animation CreateAnimation(
	Manager& manager, const TextureHandle& animation_key, const V2_float& position,
	std::size_t frame_count, milliseconds animation_duration = milliseconds{ 0 },
	V2_int frame_size = {}, std::int64_t play_count = -1, const V2_int& start_pixel = {}
);

} // namespace ptgn