#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include "core/event/event.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/time/timer.h"
#include "runtime/asset/asset.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/event/event_dispatcher.h"
#include "runtime/scripting/script.h"
#include "serialization/json/serialize.h"

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

	// TODO: Fix play count serialization.
	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		AnimationConfig, frame_count, animation_duration, frame_size, start_pixel, reset_on_complete
	)
};

namespace event {

/// @brief Triggered when an animation is started.
struct AnimationStart : public Event<AnimationStart> {
	AnimationStart() = default;
};

/// @brief Triggered when an animation is stopped, either by calling Stop() or Reset(), or when the
/// animation completes.
struct AnimationStop : public Event<AnimationStop> {
	AnimationStop() = default;
};

/// @brief Triggered when an animation is paused.
struct AnimationPause : public Event<AnimationPause> {
	AnimationPause() = default;
};

/// @brief Triggered when an animation is resumed.
struct AnimationResume : public Event<AnimationResume> {
	AnimationResume() = default;
};

/// @brief Triggered any time the animation frame changes, including when the animation starts. Does
/// not trigger when the animation is manually reset or if it completes and reset_on_complete is
/// true.
struct AnimationFrameChange : public Event<AnimationFrameChange> {
	AnimationFrameChange() = default;
};

/// @brief Triggered every frame that an animation is playing.
struct AnimationUpdate : public Event<AnimationUpdate> {
	AnimationUpdate() = default;
};

/// @brief Triggered when all animation plays have completed.
struct AnimationComplete : public Event<AnimationComplete> {
	AnimationComplete() = default;
};

/// @brief Triggered every time an animation plays through all its frames.
struct AnimationLoopComplete : public Event<AnimationLoopComplete> {
	AnimationLoopComplete() = default;
};

} // namespace event

struct Animation : public Entity {
	Animation() = default;
	explicit Animation(Entity entity);

	using Callback = std::variant<std::function<void()>, std::function<void(Animation)>>;

	/// @brief Triggered when an animation is started.
	Animation& OnStart(const Callback& callback);

	/// @brief Triggered when an animation is stopped, either by calling Stop() or Reset(), or when
	/// the animation completes.
	Animation& OnStop(const Callback& callback);

	/// @brief Triggered when an animation is paused.
	Animation& OnPause(const Callback& callback);

	/// @brief Triggered when an animation is resumed.
	Animation& OnResume(const Callback& callback);

	/// @brief Triggered any time the animation frame changes, including when the animation starts.
	/// Does not trigger when the animation is manually reset or if it completes and
	/// reset_on_complete is true.
	Animation& OnFrameChange(const Callback& callback);

	/// @brief Triggered every frame that an animation is playing.
	Animation& OnUpdate(const Callback& callback);

	/// @brief Triggered when all animation plays have completed.
	Animation& OnComplete(const Callback& callback);

	/// @brief Triggered every time an animation plays through all its frames.
	Animation& OnLoopComplete(const Callback& callback);

	Animation& SetTexture(TextureOrKey texture);

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

struct AnimationMapKey : public HashComponent {
	using HashComponent::HashComponent;
};

template <EventType T>
struct AnimationScript : public Script {
	AnimationScript() = default;

	explicit AnimationScript(const Animation::Callback& callback) : callback_{ callback } {}

	void OnEvent(EventDispatcher dispatcher) override {
		dispatcher.DispatchVariantBound<T>(callback_, Animation{ entity });
	}

private:
	Animation::Callback callback_;
};

using AnimationStartScript		  = AnimationScript<ptgn::event::AnimationStart>;
using AnimationStopScript		  = AnimationScript<ptgn::event::AnimationStop>;
using AnimationPauseScript		  = AnimationScript<ptgn::event::AnimationPause>;
using AnimationResumeScript		  = AnimationScript<ptgn::event::AnimationResume>;
using AnimationFrameChangeScript  = AnimationScript<ptgn::event::AnimationFrameChange>;
using AnimationUpdateScript		  = AnimationScript<ptgn::event::AnimationUpdate>;
using AnimationCompleteScript	  = AnimationScript<ptgn::event::AnimationComplete>;
using AnimationLoopCompleteScript = AnimationScript<ptgn::event::AnimationLoopComplete>;

} // namespace impl

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::impl::AnimationMapKey> {
	std::size_t operator()(const ptgn::impl::AnimationMapKey& key) const {
		return key.GetHash();
	}
};

} // namespace std

namespace ptgn {

namespace impl {

struct AnimationMapData {
public:
	AnimationMapData()										 = default;
	~AnimationMapData() noexcept							 = default;
	AnimationMapData(AnimationMapData&&) noexcept			 = default;
	AnimationMapData& operator=(AnimationMapData&&) noexcept = default;
	AnimationMapData(const AnimationMapData&)				 = delete;
	AnimationMapData& operator=(const AnimationMapData&)	 = delete;

	AnimationMapKey active;
	std::unordered_map<AnimationMapKey, GameObject<Animation>> animations;
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

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		AnimationData, config, frame_timer, current_frame, frames_played
	)

	AnimationConfig config;

	Timer frame_timer;

	/// @brief Current frame of the animation.
	std::size_t current_frame{ 0 };

	/// @brief Number of frames the animation has gone through. frames_played / frame_count gives
	/// the number of repeats of the full animation sequence.
	std::size_t frames_played{ 0 };

	/// @brief If the current frame has been changed externally.
	bool frame_dirty{ false };
};

class AnimationSystem {
public:
	static void Update(Scene& scene);
};

} // namespace impl

/// @param manager Which manager the entity is added to.
/// @param texture Texture or texture key to be used for the animation.
/// @param position Where on the screen to place the animation object.
Animation CreateAnimation(
	Scene& scene, TextureOrKey texture, V2_float position, const AnimationConfig& config,
	Origin draw_origin = Origin::Center
);

/// @brief Creates and starts an animation that will automatically destroy itself once it finishes.
/// @param texture Texture or texture key to be used for the animation.
/// @param position Where on the screen to place the animation object.
/// @param destroy_delay If 0ms, the animation is destroyed immediately after finishing. Otherwise,
/// the animation is destroyed after the specified delay once it finishes.
Animation PlayTemporaryAnimation(
	Scene& scene, TextureOrKey texture, V2_float position, const AnimationConfig& config,
	milliseconds destroy_delay = 0ms, Origin draw_origin = Origin::Center
);

AnimationMap CreateAnimationMap(Scene& scene);

} // namespace ptgn