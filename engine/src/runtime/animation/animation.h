#pragma once

#include <chrono>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "core/event/event.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "core/util/time.h"
#include "core/util/timer.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/ecs/key_hash.h"
#include "runtime/scripting/script.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class DrawContext;

struct AnimationConfig {
	/// @brief Number of frames in the animation sequence.
	std::size_t frame_count{ 0 };

	/// @brief Duration of the full animation sequence.
	milliseconds animation_duration{ 0 };

	/// @brief Pixel size of an individual animation frame within the texture.
	/// If {}, automatically calculated as { texture_size.x / frame_count, texture_size.y }.
	V2_int frame_size;

	/// @brief Number of times that the animation plays for, nullopt for infinite replay.
	std::optional<std::size_t> play_count{ 1 };

	/// @brief Pixel within the texture which indicates the top left position of the
	/// animation sequence.
	V2_int start_pixel;

	/// @brief Reset animation to frame 0 when it completes.
	bool reset_on_complete{ false };

	PTGN_SERIALIZE(
		AnimationConfig, frame_count, animation_duration, frame_size, play_count, start_pixel,
		reset_on_complete
	)
};

namespace event {

struct AnimationStart;
struct AnimationStop;
struct AnimationPause;
struct AnimationResume;
struct AnimationFrameChange;
struct AnimationUpdate;
struct AnimationComplete;
struct AnimationLoopComplete;

}; // namespace event

struct Animation : public Entity {
	Animation() = default;
	explicit Animation(Entity entity);

	/// @brief Triggered when an animation is started.
	template <typename F>
	Animation& OnStart(F&& callback) {
		AddScript<impl::EventScript<event::AnimationStart>>(
			*this, impl::MakeEventCallback<event::AnimationStart>(std::forward<F>(callback))
		);
		return *this;
	}

	/// @brief Triggered when an animation is stopped, either by calling Stop() or Reset(), or when
	/// the animation completes.
	template <typename F>
	Animation& OnStop(F&& callback) {
		AddScript<impl::EventScript<event::AnimationStop>>(
			*this, impl::MakeEventCallback<event::AnimationStop>(std::forward<F>(callback))
		);
		return *this;
	}

	/// @brief Triggered when an animation is paused.
	template <typename F>
	Animation& OnPause(F&& callback) {
		AddScript<impl::EventScript<event::AnimationPause>>(
			*this, impl::MakeEventCallback<event::AnimationPause>(std::forward<F>(callback))
		);
		return *this;
	}

	/// @brief Triggered when an animation is resumed.
	template <typename F>
	Animation& OnResume(F&& callback) {
		AddScript<impl::EventScript<event::AnimationResume>>(
			*this, impl::MakeEventCallback<event::AnimationResume>(std::forward<F>(callback))
		);
		return *this;
	}

	/// @brief Triggered any time the animation frame changes, including when the animation starts.
	/// Does not trigger when the animation is manually reset or if it completes and
	/// reset_on_complete is true.
	template <typename F>
	Animation& OnFrameChange(F&& callback) {
		AddScript<impl::EventScript<event::AnimationFrameChange>>(
			*this, impl::MakeEventCallback<event::AnimationFrameChange>(std::forward<F>(callback))
		);
		return *this;
	}

	/// @brief Triggered every frame that an animation is playing.
	template <typename F>
	Animation& OnUpdate(F&& callback) {
		AddScript<impl::EventScript<event::AnimationUpdate>>(
			*this, impl::MakeEventCallback<event::AnimationUpdate>(std::forward<F>(callback))
		);
		return *this;
	}

	/// @brief Triggered when all animation plays have completed.
	template <typename F>
	Animation& OnComplete(F&& callback) {
		AddScript<impl::EventScript<event::AnimationComplete>>(
			*this, impl::MakeEventCallback<event::AnimationComplete>(std::forward<F>(callback))
		);
		return *this;
	}

	/// @brief Triggered every time an animation plays through all its frames.
	template <typename F>
	Animation& OnLoopComplete(F&& callback) {
		AddScript<impl::EventScript<event::AnimationLoopComplete>>(
			*this, impl::MakeEventCallback<event::AnimationLoopComplete>(std::forward<F>(callback))
		);
		return *this;
	}

	Animation& SetTexture(std::string_view texture_key);

	/// @brief Starts the animation. Can also be used to restart the animation.
	/// @param force If false, only starts the animation if it is not already playing.
	Animation& Start(bool force = true);

	/// @brief Stops and resets the animation.
	Animation& Reset();

	Animation& Stop(bool reset = false);

	/// @brief Toggles the pause state of the animation.
	Animation& Toggle();

	Animation& Pause();

	Animation& Resume();

	/// @return True if the animation is currently paused, false otherwise.
	[[nodiscard]] bool IsPaused() const;

	/// @return True if the animation is currently playing, false otherwise.
	[[nodiscard]] bool IsPlaying() const;

	/// @return The number of plays of the full animation sequence so far.
	std::size_t GetPlayCount() const;

	/// @return The total number of plays of individual animation frames so far.
	std::size_t GetFramePlayCount() const;

	/// @return Duration of the full animation sequence.
	milliseconds GetDuration() const;

	/// @return Duration of a single animation frame (all frames currently have the same duration).
	milliseconds GetFrameDuration() const;

	std::size_t GetFrameCount() const;

	/// @brief Set the current animation frame.
	/// new_frame is wrapped around frame_count using Mod().
	Animation& SetCurrentFrame(std::size_t new_frame);

	Animation& IncrementFrame();

	/// @brief If true, the animation will reset to frame 0 when it completes. Otherwise, it will
	/// stay on the last frame.
	Animation& SetResetOnComplete(bool reset_on_complete = true);

	std::size_t GetCurrentFrame() const;

	V2_int GetCurrentFramePosition() const;

	V2_int GetFrameSize() const;
};

namespace impl {

struct AnimationMapKey : public KeyHash {
	using KeyHash::KeyHash;
};

struct AnimationMapData {
public:
	AnimationMapData()										 = default;
	~AnimationMapData() noexcept							 = default;
	AnimationMapData(AnimationMapData&&) noexcept			 = default;
	AnimationMapData& operator=(AnimationMapData&&) noexcept = default;
	AnimationMapData(const AnimationMapData&)				 = delete;
	AnimationMapData& operator=(const AnimationMapData&)	 = delete;

	AnimationMapKey active;
	std::unordered_map<AnimationMapKey, GameObject<Animation>, KeyHasher> animations;
};

} // namespace impl

struct AnimationMap : public Entity {
public:
	AnimationMap() = default;
	explicit AnimationMap(Entity entity);

	/// @brief The loaded animation is hidden by default.
	Animation Add(std::string_view animation_key, Animation animation, bool hide = true);

	void Remove(std::string_view animation_key);

	/// @brief If the provided key is a not currently active, this function pauses the previously
	/// active animation. If the key is already active, does nothing.
	/// @return True if active value changed, false otherwise.
	bool SetActive(std::string_view animation_key);

	/// @return Active animation, or nullopt if no animation is active.
	std::optional<Animation> GetActive() const;
};

namespace impl {

class AnimationData {
public:
	AnimationData() = default;

	AnimationData(const AnimationConfig& config, V2_int texture_size);

	milliseconds GetFrameDuration() const;
	V2_int GetCurrentFramePosition() const;

	/// @return Total number of animation repeats.
	std::size_t GetPlayCount() const;

	void SetCurrentFrame(std::size_t new_frame);
	void IncrementFrame();

	AnimationConfig config;

	Timer frame_timer;

	/// @brief Current frame of the animation.
	std::size_t current_frame{ 0 };

	/// @brief Number of frames the animation has gone through. frames_played / frame_count gives
	/// the number of repeats of the full animation sequence.
	std::size_t frames_played{ 0 };

	/// @brief If the current frame has been changed externally.
	bool frame_dirty{ false };

	PTGN_SERIALIZE(AnimationData, config, frame_timer, current_frame, frames_played)
};

class AnimationSystem {
public:
	static void Update(Scene& scene);
};

} // namespace impl

/// @param manager Which manager the entity is added to.
/// @param texture Texture key to be used for the animation.
/// @param position Where on the screen to place the animation object.
Animation CreateAnimation(
	Scene& scene, std::string_view texture_key, V2_float position, const AnimationConfig& config,
	Origin draw_origin = Origin::Center
);

/// @brief Creates and starts an animation that will automatically destroy itself once it finishes.
/// @param texture Texture key to be used for the animation.
/// @param position Where on the screen to place the animation object.
/// @param destroy_delay If 0ms, the animation is destroyed immediately after finishing. Otherwise,
/// the animation is destroyed after the specified delay once it finishes.
Animation PlayTemporaryAnimation(
	Scene& scene, std::string_view texture_key, V2_float position, const AnimationConfig& config,
	milliseconds destroy_delay = 0ms, Origin draw_origin = Origin::Center
);

AnimationMap CreateAnimationMap(Scene& scene);

} // namespace ptgn