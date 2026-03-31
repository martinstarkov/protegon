#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "core/event/dispatcher.h"
#include "core/event/event.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/time/timer.h"
#include "runtime/asset/asset.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
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
	/// If {}, frame_size = { texture_size.x / frame_count, texture_size.y }.
	std::optional<V2_int> frame_size;

	/// @brief Number of times that the animation plays for, -1 for infinite replay.
	std::int64_t play_count = -1;

	/// @brief Pixel within the texture which indicates the top left position of the
	/// animation sequence.
	V2_int start_pixel;
};

struct AnimationStart : public Event<AnimationStart> {};

struct AnimationStop : public Event<AnimationStop> {};

struct AnimationPause : public Event<AnimationPause> {};

struct AnimationResume : public Event<AnimationResume> {};

struct AnimationRepeat : public Event<AnimationRepeat> {};

struct AnimationFrameChange : public Event<AnimationFrameChange> {};

struct AnimationUpdate : public Event<AnimationUpdate> {};

struct AnimationComplete : public Event<AnimationComplete> {};

namespace impl {

template <EventType T>
struct AnimationScript : public Script {
	AnimationScript() = default;

	explicit AnimationScript(const std::function<void()>& callback) : callback_{ callback } {}

	void OnEvent(EventDispatcher d) override {
		d.Dispatch<T>([this](T&) { callback_(); });
	}

private:
	std::function<void()> callback_;
};

using AnimationStartScript		 = AnimationScript<AnimationStart>;
using AnimationStopScript		 = AnimationScript<AnimationStop>;
using AnimationPauseScript		 = AnimationScript<AnimationPause>;
using AnimationResumeScript		 = AnimationScript<AnimationResume>;
using AnimationRepeatScript		 = AnimationScript<AnimationRepeat>;
using AnimationFrameChangeScript = AnimationScript<AnimationFrameChange>;
using AnimationUpdateScript		 = AnimationScript<AnimationUpdate>;
using AnimationCompleteScript	 = AnimationScript<AnimationComplete>;

} // namespace impl

struct Animation : public Entity {
	Animation() = default;
	explicit Animation(Entity entity);

	Animation& OnStart(const std::function<void()>& callback);
	Animation& OnStop(const std::function<void()>& callback);
	Animation& OnPause(const std::function<void()>& callback);
	Animation& OnResume(const std::function<void()>& callback);
	Animation& OnFrameChange(const std::function<void()>& callback);
	Animation& OnUpdate(const std::function<void()>& callback);
	Animation& OnComplete(const std::function<void()>& callback);

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

	std::size_t GetCurrentFrame() const;

	V2_int GetCurrentFramePosition() const;

	V2_int GetFrameSize() const;
};

namespace impl {

struct AnimationMapKey : public HashComponent {
	using HashComponent::HashComponent;
};

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

	AnimationData(
		milliseconds animation_duration, std::size_t animation_frame_count,
		V2_float animation_frame_size, std::int64_t animation_play_count,
		V2_float animation_start_pixel
	);

	milliseconds GetFrameDuration() const;
	V2_int GetCurrentFramePosition() const;

	/// @return Total number of animation repeats.
	std::size_t GetPlayCount() const;

	void SetCurrentFrame(std::size_t new_frame);
	void IncrementFrame();

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		AnimationData, duration, frame_timer, frame_count, frame_size, play_count, start_pixel,
		current_frame, frames_played
	)

	milliseconds duration{ 0 };

	Timer frame_timer;

	/// @brief Number of frames in the animation.
	std::size_t frame_count{ 0 };

	/// @brief Size of an individual animation frame.
	V2_int frame_size;

	/// @brief Number of times the full animation is played. -1 for infinite playback.
	std::int64_t play_count{ 1 };

	/// @brief Pixel within the texture which indicates the top left position of the animation
	/// sequence.
	V2_int start_pixel;

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

AnimationMap CreateAnimationMap(Scene& scene);

} // namespace ptgn