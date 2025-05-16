#pragma once

#include <cstdint>
#include <deque>
#include <functional>

#include "components/offsets.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/timer.h"
#include "math/math.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "tweening/tween.h"

namespace ptgn {

/*
struct ShakeConfig {
	// Maximum translation distance during shaking.
	V2_float maximum_translation{ 30.0f, 30.0f };

	// Maximum rotation (in radians) during shaking.
	float maximum_rotation{ DegToRad(30.0f) };

	// Frequency of the Perlin noise function. Higher values will result in faster shaking.
	float frequency{ 10.0f };

	// Trauma is taken to this power before shaking is applied. Higher values will result in a
	// smoother falloff as trauma reduces.
	float trauma_exponent{ 2.0f };

	// Amount of trauma per second that is recovered.
	float recovery_speed{ 0.5f };
};
*/

namespace impl {

/*
struct BounceEffect : public Entity {
	explicit BounceEffect(Manager& manager);

	[[nodiscard]] Tween& Bounce(
		Entity& entity, const V2_float& bounce_amplitude, const V2_float& static_offset,
		milliseconds duration, TweenEase ease, std::int64_t repeats, bool force
	);
};

struct ContinuousShakeEffect : public Entity {
	explicit ContinuousShakeEffect(Manager& manager);

	void Reset(Entity& entity);

	[[nodiscard]] Tween& Shake(
		Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config,
		bool force
	);

	[[nodiscard]] Tween& Shake(
		Entity& entity, float intensity, const ShakeConfig& config, bool force
	);
};

struct ShakeEffect {
	ShakeEffect(const ShakeConfig& config = {});

	void SetConfig(const ShakeConfig& config);

	// Needs to be called once a frame to update the local translation and rotation of the camera
	// shake.
	void Update(Entity& entity, float dt, float time);

	// Resets camera shake back to 0.
	void Reset();

	void AddIntensity(float intensity);

	void SetIntensity(float intensity);

private:
	ShakeConfig config_;

	// Range [0, 1] defining the current amount of stress this entity is enduring.
	float trauma_{ 0.0f };

	// Perlin noise seed.
	std::int32_t seed_{ 0 };
};
*/

[[nodiscard]] float ApplyEasing(TweenEase ease, float t);

template <typename TComponent, typename T>
class EffectSystemBase {
public:
	virtual ~EffectSystemBase() = default;

	void Update(Manager& manager) {
		for (auto [entity, effect] : manager.EntitiesWith<TComponent>()) {
			if (effect.tasks.empty()) {
				entity.template Remove<TComponent>();
				continue;
			}

			auto& task = effect.tasks.front();

			PTGN_ASSERT(task.timer.IsRunning());

			float t = task.timer.template ElapsedPercentage<milliseconds, float>(task.duration);
			float eased_t = ApplyEasing(task.ease, t);
			T value{ Lerp(task.start_value, task.target_value, eased_t) };

			Apply(entity, value);

			if (task.timer.Completed(task.duration)) {
				effect.tasks.pop_front();
				if (!effect.tasks.empty()) {
					effect.tasks.front().timer.Start(true);
				}
			}
		}
	}

protected:
	virtual void Apply(Entity& entity, const T& value) = 0;
};

template <typename T>
struct EffectInfo {
	T start_value{};
	T target_value{};
	milliseconds duration{ 0 };
	TweenEase ease{ TweenEase::Linear };
	Timer timer;

	EffectInfo() = default;

	EffectInfo(const T& start, const T& target, milliseconds duration, TweenEase ease_type) :
		start_value{ start }, target_value{ target }, duration{ duration }, ease{ ease_type } {}
};

struct TranslateEffect {
	std::deque<EffectInfo<V2_float>> tasks;
};

struct RotateEffect {
	std::deque<EffectInfo<float>> tasks;
};

struct ScaleEffect {
	std::deque<EffectInfo<V2_float>> tasks;
};

struct TintEffect {
	std::deque<EffectInfo<Color>> tasks;
};

struct BounceEffect {
	std::deque<EffectInfo<V2_float>> tasks;
	V2_float static_offset;
	std::int64_t remaining_repeats{ -1 }; // -1 means infinite
};

struct ShakeConfig {
	float frequency = 20.0f; // shakes per second
	float damping	= 1.0f;	 // how quickly shake fades
};

struct ShakeEffect {
	bool indefinite = false;
	float intensity = 1.0f;
	ShakeConfig config;
	Timer timer;
	milliseconds duration{ 0 };
	V2_float original_position;
};

class TranslateEffectSystem : public EffectSystemBase<TranslateEffect, V2_float> {
protected:
	void Apply(Entity& entity, const V2_float& value) override {
		entity.SetPosition(value);
	}
};

class RotateEffectSystem : public EffectSystemBase<RotateEffect, float> {
protected:
	void Apply(Entity& entity, const float& value) override {
		entity.SetRotation(value);
	}
};

class ScaleEffectSystem : public EffectSystemBase<ScaleEffect, V2_float> {
protected:
	void Apply(Entity& entity, const V2_float& value) override {
		entity.SetScale(value);
	}
};

class TintEffectSystem : public EffectSystemBase<TintEffect, Color> {
protected:
	void Apply(Entity& entity, const Color& value) override {
		entity.SetTint(value);
	}
};

class BounceEffectSystem : public EffectSystemBase<BounceEffect, V2_float> {
protected:
	void Apply(Entity& entity, const V2_float& value) override {
		auto& offsets			= entity.Get<Offsets>();
		auto& bounce			= entity.Get<BounceEffect>();
		offsets.bounce.position = bounce.static_offset + value;
	}
};

class ShakeEffectSystem {
public:
	void Update(Manager& manager) {
		for (auto [entity, shake] : manager.EntitiesWith<ShakeEffect>()) {
			float t = shake.timer.Elapsed<duration<float, seconds::period>>().count();
			if (!shake.indefinite && shake.timer.Completed(shake.duration)) {
				entity.Get<Offsets>().shake = {};
				entity.Remove<ShakeEffect>();
				continue;
			}

			// TODO: Switch to different algorithm.

			float damp	  = std::exp(-shake.config.damping * t);
			float angle	  = two_pi<float> * shake.config.frequency * t;
			float offsetX = std::sin(angle) * shake.intensity * damp;
			float offsetY = std::cos(angle) * shake.intensity * damp;

			entity.Get<Offsets>().shake.position = V2_float{ offsetX, offsetY };
		}
	}
};

template <typename TComponent, typename T>
void AddTweenEffect(
	Entity& entity, const T& target, milliseconds duration, TweenEase ease, bool force,
	const T& current_value
) {
	auto& comp = entity.GetOrAdd<TComponent>();

	T start{};

	bool first_task{ force || comp.tasks.empty() };

	if (first_task) {
		comp.tasks.clear();
		start = current_value;
	} else {
		// Use previous task's target value as new starting point.
		start = comp.tasks.back().target_value;
	}

	auto& task = comp.tasks.emplace_back(start, target, duration, ease);

	if (first_task) {
		task.timer.Start(true);
	}
}

} // namespace impl

/**
 * @brief Translates an entity to a target position over a specified duration using a tweening
 * function.
 *
 * @param entity The entity to be moved.
 * @param target_position The position to move the entity to.
 * @param duration The duration over which the translation should occur.
 * @param ease The easing function to apply for the translation animation. Defaults to
 * TweenEase::Linear.
 * @param force If true, forcibly overrides any ongoing translation. Defaults to true.
 */
void TranslateTo(
	Entity& entity, const V2_float& target_position, milliseconds duration,
	TweenEase ease = TweenEase::Linear, bool force = true
);

/**
 * @brief Rotates an entity to a target angle over a specified duration using a tweening function.
 *
 * @param entity The entity to be rotated.
 * @param target_angle The angle (in radians) to rotate the entity to. Positive clockwise, negative
 * counter-clockwise.
 * @param duration The duration over which the rotation should occur.
 * @param ease The easing function to apply for the rotation animation. Defaults to
 * TweenEase::Linear.
 * @param force If true, forcibly overrides any ongoing rotation. Defaults to true.
 */
void RotateTo(
	Entity& entity, float target_angle, milliseconds duration, TweenEase ease = TweenEase::Linear,
	bool force = true
);

/**
 * @brief Scales an entity to a target size over a specified duration using a tweening function.
 *
 * @param entity The entity to be scaled.
 * @param target_scale The target scale (width, height) to apply to the entity.
 * @param duration The duration over which the scaling should occur.
 * @param ease The easing function to apply for the scale animation. Defaults to TweenEase::Linear.
 * @param force If true, forcibly overrides any ongoing scaling. Defaults to true.
 */
void ScaleTo(
	Entity& entity, const V2_float& target_scale, milliseconds duration,
	TweenEase ease = TweenEase::Linear, bool force = true
);

/**
 * @brief Tints an entity to a target color over a specified duration using a tweening function.
 *
 * @param entity The entity to be tinted.
 * @param target_tint The target color tint to apply to the entity.
 * @param duration The duration over which the tinting should occur.
 * @param ease The easing function to apply for the tint animation. Defaults to TweenEase::Linear.
 * @param force If true, forcibly overrides any ongoing tinting. Defaults to true.
 */
void TintTo(
	Entity& entity, const Color& target_tint, milliseconds duration,
	TweenEase ease = TweenEase::Linear, bool force = true
);

/**
 * @brief Fades in the specified entity over a given duration.
 *
 * @param entity The entity to apply the fade-in effect to.
 * @param duration The time span over which the fade-in will occur.
 * @param ease The easing function used to interpolate the fade (default is linear).
 * @param force If true, the fade-in will override any ongoing fade effect (default is true).
 */
void FadeIn(
	Entity& entity, milliseconds duration, TweenEase ease = TweenEase::Linear, bool force = true
);

/**
 * @brief Fades out the specified entity over a given duration.
 *
 * @param entity The entity to apply the fade-out effect to.
 * @param duration The time span over which the fade-out will occur.
 * @param ease The easing function used to interpolate the fade (default is linear).
 * @param force If true, the fade-out will override any ongoing fade effect (default is true).
 */
void FadeOut(
	Entity& entity, milliseconds duration, TweenEase ease = TweenEase::Linear, bool force = true
);

/**
 * @brief Stops the current bounce tween and proceeds to the next one in the queue.
 *
 * @param entity The entity whose bounce animation should be stopped.
 * @param force If true, clears the entire bounce queue instead of just the current tween.
 */
void StopBounce(Entity& entity, bool force = true);

/**
 * @brief Applies a bouncing motion to the specified entity.
 *
 * The bounce starts with an upward motion unless reversed. It uses a tweening function
 * for smooth animation and can repeat a specified number of times or indefinitely.
 *
 * @param entity The entity to apply the bounce effect to.
 * @param bounce_amplitude The peak offset applied during the bounce animation.
 * @param static_offset A constant offset added to the entity's position throughout the animation.
 * @param duration The duration of one bounce cycle (e.g., up and down).
 * @param ease The easing function to use for the animation.
 * @param repeats Number of bounce cycles. If -1, bounce continues until StopBounce is called.
 * @param force If true, overrides any existing bounce effect on the entity.
 */
void Bounce(
	Entity& entity, const V2_float& bounce_amplitude, const V2_float& static_offset,
	milliseconds duration, TweenEase ease, std::int64_t repeats, bool force = true
);

/**
 * @brief Applies a shaking effect to the specified entity.
 *
 * @param entity The entity to apply the shake effect to.
 * @param intensity The intensity of the shake, in the range [0, 1].
 * @param duration The total duration of the shake effect.
 * @param config Configuration parameters for the shake behavior.
 * @param force If true, overrides any existing shake effect.
 */
void Shake(
	Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config = {},
	bool force = true
);

/**
 * @brief Applies a continuous shake effect to the specified entity.
 *
 * This overload does not specify a duration and is used for indefinite shaking
 * until stopped with StopShake.
 *
 * @param entity The entity to apply the shake effect to.
 * @param intensity The intensity of the shake, in the range [0, 1].
 * @param config Configuration parameters for the shake behavior.
 * @param force If true, overrides any existing shake effect.
 */
void Shake(Entity& entity, float intensity, const ShakeConfig& config = {}, bool force = true);

/**
 * @brief Stops any ongoing shake effect on the specified entity.
 *
 * @param entity The entity whose shake effect should be stopped.
 * @param force If true, clears all queued or active shake effects.
 */
void StopShake(Entity& entity, bool force = true);

/**
 * @brief Executes a callback once after a specified delay.
 *
 * @param manager The animation or tween manager responsible for scheduling the callback.
 * @param duration The delay before the callback is triggered.
 * @param callback The function to call after the delay has elapsed.
 */
void After(Manager& manager, milliseconds duration, const std::function<void()>& callback);

/**
 * @brief Executes a callback continuously during a specified time window.
 *
 * @param manager The animation or tween manager responsible for scheduling the callback.
 * @param duration The duration during which the callback will be invoked repeatedly (e.g., once per
 * frame).
 * @param callback The function to invoke during the duration.
 */
void During(Manager& manager, milliseconds duration, const std::function<void()>& callback);

/**
 * @brief Executes a callback repeatedly at fixed intervals, with optional exit conditions.
 *
 * @param manager The animation or tween manager responsible for scheduling the callback.
 * @param duration The interval between each callback execution.
 * @param repeats Number of repetitions. If -1, repeats indefinitely until the exit condition is
 * met.
 * @param callback The function to call every interval.
 * @param exit_condition_callback A function called every frame during the interval.
 *                                If it returns true, repetition stops early.
 *                                WARNING: If this is nullptr and repeats == -1, the callback will
 * repeat indefinitely.
 */
void Every(
	Manager& manager, milliseconds duration, std::int64_t repeats,
	const std::function<void()>& callback, const std::function<bool()>& exit_condition_callback
);

} // namespace ptgn