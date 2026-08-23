#pragma once

#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "core/util/strong_string.h"
#include "core/util/timer.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/key_hash.h"
#include "runtime/scripting/script.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class DrawContext;
class AssetManager;

namespace impl {

constexpr std::optional<V2_int> GetFrameSize(
	std::optional<V2_int> texture_size, std::size_t frame_count
) {
	if (!frame_count || !texture_size.has_value()) {
		return std::nullopt;
	}
	PTGN_ASSERT(texture_size.value().IsPositive(), "Texture size must be positive");
	return V2_int{ static_cast<std::size_t>(texture_size.value().x) / frame_count,
				   texture_size.value().y };
}

} // namespace impl

struct AnimationConfig {
	/// @brief Number of frames in the animation sequence.
	std::size_t frame_count{ 0 };

	/// @brief Duration of the full animation sequence.
	milliseconds duration{ 0 };

	/// @brief Pixel size of an individual animation frame within the texture.
	/// If nullopt, frame size is automatically calculated using impl::GetFrameSize(texture_size,
	/// frame_count).
	std::optional<V2_int> frame_size{};

	/// @brief Number of times that the animation plays for, nullopt for infinite replay.
	std::optional<std::size_t> play_count{ 1 };

	/// @brief Pixel within the texture which indicates the top left position of the
	/// animation sequence.
	V2_int start_pixel{};

	/// @brief Reset animation to frame 0 when it completes.
	bool reset_on_complete{ false };

	constexpr bool IsIdentical(const AnimationConfig& o, std::optional<V2_int> texture_size) const {
		auto zero_frame_size = [&](const auto& a, const auto& b) {
			return !a.frame_size.has_value() &&
				   impl::GetFrameSize(texture_size, b.frame_count) == b.frame_size;
		};

		return frame_count == o.frame_count && duration == o.duration &&
			   (frame_size == o.frame_size || zero_frame_size(*this, o) ||
				zero_frame_size(o, *this)) &&
			   play_count == o.play_count && start_pixel == o.start_pixel &&
			   reset_on_complete == o.reset_on_complete;
	}

	PTGN_REFLECT(
		AnimationConfig, frame_count, duration, frame_size, play_count, start_pixel,
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
struct AnimationFinalFrame;
struct AnimationComplete;
struct AnimationLoopComplete;

}; // namespace event

struct Animation : public Entity {
	Animation() = default;
	explicit Animation(Entity entity);

	/// @brief Sets the animation configuration. Animation will be reset if the configuration is
	/// new. If animation has a different texture key, the texture key must be set before calling
	/// this function.
	Animation& SetConfig(AnimationConfig config);

	/// @brief Triggered when an animation is started.
	template <EventCallbackInvocable<event::AnimationStart> F>
	Animation& OnStart(F&& callback) {
		return OnEvent<event::AnimationStart>(std::forward<F>(callback));
	}

	/// @brief Triggered when an animation is stopped, either by calling Stop() or Reset(), or when
	/// the animation completes.
	template <EventCallbackInvocable<event::AnimationStop> F>
	Animation& OnStop(F&& callback) {
		return OnEvent<event::AnimationStop>(std::forward<F>(callback));
	}

	/// @brief Triggered when an animation is paused.
	template <EventCallbackInvocable<event::AnimationPause> F>
	Animation& OnPause(F&& callback) {
		return OnEvent<event::AnimationPause>(std::forward<F>(callback));
	}

	/// @brief Triggered when an animation is resumed.
	template <EventCallbackInvocable<event::AnimationResume> F>
	Animation& OnResume(F&& callback) {
		return OnEvent<event::AnimationResume>(std::forward<F>(callback));
	}

	/// @brief Triggered any time the animation frame changes, including when the animation starts.
	/// Does not trigger when the animation is manually reset or if it completes and
	/// reset_on_complete is true.
	template <EventCallbackInvocable<event::AnimationFrameChange> F>
	Animation& OnFrameChange(F&& callback) {
		return OnEvent<event::AnimationFrameChange>(std::forward<F>(callback));
	}

	/// @brief Triggered every frame that an animation is playing.
	template <EventCallbackInvocable<event::AnimationUpdate> F>
	Animation& OnUpdate(F&& callback) {
		return OnEvent<event::AnimationUpdate>(std::forward<F>(callback));
	}

	/// @brief Triggered when an animation is paused.
	template <EventCallbackInvocable<event::AnimationFinalFrame> F>
	Animation& OnFinalFrame(F&& callback) {
		return OnEvent<event::AnimationFinalFrame>(std::forward<F>(callback));
	}

	/// @brief Triggered when all animation plays have completed.
	template <EventCallbackInvocable<event::AnimationComplete> F>
	Animation& OnComplete(F&& callback) {
		return OnEvent<event::AnimationComplete>(std::forward<F>(callback));
	}

	/// @brief Triggered every time an animation plays through all its frames.
	template <EventCallbackInvocable<event::AnimationLoopComplete> F>
	Animation& OnLoopComplete(F&& callback) {
		return OnEvent<event::AnimationLoopComplete>(std::forward<F>(callback));
	}

	Animation& SetTexture(TextureKey texture_key);

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

private:
	/// @brief Adds a callback for a specific event.
	template <typename E, EventCallbackInvocable<E> F>
	Animation& OnEvent(F&& callback) {
		AddScript<impl::EventScript<E>>(
			*this, impl::MakeEventCallback<E>(std::forward<F>(callback))
		);
		return *this;
	}
};

namespace impl {

struct AnimationMapKey : public StrongString<AnimationMapKey> {
	using StrongString::StrongString;

	constexpr AnimationMapKey() = default;

	friend std::ostream& operator<<(std::ostream& os, const AnimationMapKey& key) {
		os << key.value;
		return os;
	}

	PTGN_REFLECT_VALUE(AnimationMapKey, value)
};

struct AnimationMapData {
	AnimationMapKey active{};
};

std::optional<std::size_t> DetectAnimationFrameCount(AssetManager& assets, const TextureKey& texture_key);

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

	/// @return Animation with the provided map key, or a null animation if it does not exist.
	[[nodiscard]] Animation GetAnimation(std::string_view animation_key) const;

private:
	/// @return May return a null animation if no child has the given key.
	[[nodiscard]] Animation Find(impl::AnimationMapKey key) const;
};

namespace impl {

class AnimationData {
public:
	AnimationData() = default;

	AnimationData(AnimationConfig&& config, std::optional<V2_int> texture_size);

	milliseconds GetFrameDuration() const;
	V2_int GetFrameSize(std::optional<V2_int> texture_size) const;
	V2_int GetCurrentFramePosition(std::optional<V2_int> texture_size) const;

	/// @return Total number of animation repeats.
	std::size_t GetPlayCount() const;

	void SetCurrentFrame(std::size_t new_frame);
	void IncrementFrame();

	AnimationConfig config;

	ManualTimer frame_timer;

	/// @brief Current frame of the animation.
	std::size_t current_frame{ 0 };

	/// @brief Number of frames the animation has gone through. frames_played / frame_count gives
	/// the number of repeats of the full animation sequence.
	std::size_t frames_played{ 0 };

	/// @brief If the current frame has been changed externally.
	bool frame_dirty{ false };

	PTGN_REFLECT(AnimationData, config, current_frame)
	PTGN_REFLECT_READONLY(AnimationData, frame_timer, frames_played)
};

class AnimationSystem {
public:
	static void Prepare(Scene& scene);
	static void Update(Scene& scene, secondsf dt);
};

} // namespace impl

/// @param manager Which manager the entity is added to.
/// @param texture Texture key to be used for the animation.
Animation CreateAnimation(
	Scene& scene, Transform transform = {}, TextureKey texture_key = {},
	AnimationConfig config = {}, Origin origin = Origin::Center
);

/// @brief Creates and starts an animation that will automatically destroy itself once it finishes.
/// @param texture Texture key to be used for the animation.
/// @param destroy_delay If 0ms, the animation is destroyed immediately after finishing. Otherwise,
/// the animation is destroyed after the specified delay once it finishes.
Animation PlayTemporaryAnimation(
	Scene& scene, Transform transform = {}, TextureKey texture_key = {},
	AnimationConfig config = {}, milliseconds destroy_delay = 0ms, Origin origin = Origin::Center
);

AnimationMap CreateAnimationMap(Scene& scene);

} // namespace ptgn