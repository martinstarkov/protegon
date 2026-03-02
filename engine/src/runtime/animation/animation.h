#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "core/event/event.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/time/timer.h"
#include "renderer/primitives/texture.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "serialization/json/serialize.h"

namespace ptgn {

class Scene;
class Renderer;

struct AnimationStart : Event<AnimationStart> {};

struct AnimationStop : Event<AnimationStop> {};

struct AnimationPause : Event<AnimationPause> {};

struct AnimationResume : Event<AnimationResume> {};

struct AnimationRepeat : Event<AnimationRepeat> {};

struct AnimationFrameChange : Event<AnimationFrameChange> {};

struct AnimationUpdate : Event<AnimationUpdate> {};

struct AnimationComplete : Event<AnimationComplete> {};

struct Animation : public Entity {
	Animation() = default;
	explicit Animation(Entity entity);

	static void Draw(Renderer& renderer, Entity entity);

	Animation& SetTexture(Texture texture);

	/// @brief Starts the animation. Can also be used to restart the animation.
	/// @param force If false, only starts the animation if it is not already playing.
	Animation& Start(bool force = true);

	/// @brief Stops and resets the animation.
	Animation& Reset();

	Animation& Stop();

	/// @brief Toggles the pause state of the animation.
	Animation& Toggle();

	Animation& Pause();

	Animation& Resume();

	/// @return True if the animation is currently paused, false otherwise.
	[[nodiscard]] bool IsPaused() const;

	/// @return True if the animation is currently playing, false otherwise.
	[[nodiscard]] bool IsPlaying() const;

	/// @return The number of plays of the full animation sequence so far.
	[[nodiscard]] std::size_t GetPlayCount() const;

	/// @return The total number of plays of individual animation frames so far.
	[[nodiscard]] std::size_t GetFramePlayCount() const;

	/// @return Duration of the full animation sequence.
	[[nodiscard]] milliseconds GetDuration() const;

	/// @return Duration of a single animation frame (all frames currently have the same duration).
	[[nodiscard]] milliseconds GetFrameDuration() const;

	[[nodiscard]] std::size_t GetFrameCount() const;

	/// @brief Set the current animation frame.
	/// new_frame is wrapped around frame_count using Mod().
	Animation& SetCurrentFrame(std::size_t new_frame);

	Animation& IncrementFrame();

	[[nodiscard]] std::size_t GetCurrentFrame() const;

	[[nodiscard]] V2_int GetCurrentFramePosition() const;

	[[nodiscard]] V2_int GetFrameSize() const;
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
	std::unordered_map<AnimationMapKey, GameObject> animations;
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
	[[nodiscard]] std::optional<Animation> GetActive() const;
};

namespace impl {

class AnimationInfo {
public:
	AnimationInfo() = default;

	AnimationInfo(
		milliseconds animation_duration, std::size_t animation_frame_count,
		V2_float animation_frame_size, std::int64_t animation_play_count,
		V2_float animation_start_pixel
	);

	[[nodiscard]] milliseconds GetFrameDuration() const;
	[[nodiscard]] V2_int GetCurrentFramePosition() const;

	/// @return Total number of animation repeats.
	[[nodiscard]] std::size_t GetPlayCount() const;

	void SetCurrentFrame(std::size_t new_frame);
	void IncrementFrame();

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		AnimationInfo, duration, frame_timer, frame_count, frame_size, play_count, start_pixel,
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
/// @param frame_count Number of frames in the animation sequence.
/// @param animation_duration Duration of the full animation sequence.
/// @param frame_size Pixel size of an individual animation frame within the texture.
/// If {}, frame_size = { texture_size.x / frame_count, texture_size.y }.
/// @param play_count Number of times that the animation plays for, -1 for infinite replay.
/// @param start_pixel Pixel within the texture which indicates the top left position of the
/// animation sequence.
Animation CreateAnimation(
	Scene& scene, std::variant<Texture, std::string_view> texture, V2_float position,
	std::size_t frame_count, milliseconds animation_duration = milliseconds{ 0 },
	std::optional<V2_int> frame_size = {}, std::int64_t play_count = -1, V2_int start_pixel = {}
);

AnimationMap CreateAnimationMap(Scene& scene);

} // namespace ptgn