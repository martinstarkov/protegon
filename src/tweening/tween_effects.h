#pragma once

#include <cstdint>
#include <deque>

#include "common/assert.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/timer.h"
#include "math/easing.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "serialization/serializable.h"
#include "tweening/follow_config.h"
#include "tweening/shake_config.h"

namespace ptgn {

namespace impl {

struct FollowEffectInfo {
	FollowEffectInfo() = default;

	FollowEffectInfo(Entity follow_target, const FollowConfig& follow_config);

	Entity target;
	FollowConfig config;

	std::size_t current_waypoint{ 0 };

	PTGN_SERIALIZER_REGISTER(FollowEffectInfo, target, config, current_waypoint)
};

struct FollowEffect {
	std::deque<FollowEffectInfo> tasks;

	PTGN_SERIALIZER_REGISTER(FollowEffect, tasks)
};

template <typename T>
struct EffectInfo {
	EffectInfo() = default;

	EffectInfo(
		const T& start, const T& target, milliseconds tween_duration, const Ease& tween_ease
	) :
		start_value{ start },
		target_value{ target },
		duration{ tween_duration },
		ease{ tween_ease } {}

	T start_value{};
	T target_value{};
	milliseconds duration{ 0 };
	Ease ease{ SymmetricalEase::Linear };
	Timer timer;

	PTGN_SERIALIZER_REGISTER(EffectInfo, start_value, target_value, duration, ease, timer)
};

struct TranslateEffect {
	std::deque<EffectInfo<V2_float>> tasks;

	PTGN_SERIALIZER_REGISTER(TranslateEffect, tasks)
};

struct RotateEffect {
	std::deque<EffectInfo<float>> tasks;

	PTGN_SERIALIZER_REGISTER(RotateEffect, tasks)
};

struct ScaleEffect {
	std::deque<EffectInfo<V2_float>> tasks;

	PTGN_SERIALIZER_REGISTER(ScaleEffect, tasks)
};

struct TintEffect {
	std::deque<EffectInfo<Color>> tasks;

	PTGN_SERIALIZER_REGISTER(TintEffect, tasks)
};

struct BounceEffectInfo {
	BounceEffectInfo() = default;

	BounceEffectInfo(
		const V2_float& amplitude, milliseconds duration, const Ease& ease,
		const V2_float& static_offset, std::int64_t total_periods, bool symmetrical
	);

	V2_float amplitude;
	milliseconds duration{ 0 };
	Ease ease{ SymmetricalEase::Linear };
	Timer timer;
	V2_float static_offset;
	std::int64_t total_periods{ -1 };	 // -1 means infinite
	std::int64_t periods_completed{ 0 }; // How many times the bounce has repeated so far.
	bool symmetrical{ false }; // If true, bounce origin is the middle point of the movement. If
							   // false, bounce origin is the bottom (or top) point of the movement.

	PTGN_SERIALIZER_REGISTER(
		BounceEffectInfo, amplitude, duration, ease, timer, static_offset, total_periods,
		periods_completed, symmetrical
	)
};

struct BounceEffect {
	std::deque<BounceEffectInfo> tasks;

	PTGN_SERIALIZER_REGISTER(BounceEffect, tasks)
};

struct ShakeEffectInfo : public EffectInfo<float> {
	ShakeEffectInfo() = default;

	ShakeEffectInfo(
		float start_intensity, float target_intensity, milliseconds duration, const Ease& ease,
		const ShakeConfig& config, std::int32_t seed
	);

	ShakeConfig config;

	// Perlin noise seed.
	std::int32_t seed{ 0 };

	// Range [0, 1] defining the current amount of stress this entity is enduring.
	float trauma{ 0.0f };

	PTGN_SERIALIZER_REGISTER(
		ShakeEffectInfo, start_value, target_value, duration, ease, timer, config, seed, trauma
	)
};

struct ShakeEffect {
	std::deque<ShakeEffectInfo> tasks;

	PTGN_SERIALIZER_REGISTER(ShakeEffect, tasks)
};

class TranslateEffectSystem {
public:
	void Update(Manager& manager) const;
};

class RotateEffectSystem {
public:
	void Update(Manager& manager) const;
};

class ScaleEffectSystem {
public:
	void Update(Manager& manager) const;
};

class TintEffectSystem {
public:
	void Update(Manager& manager) const;
};

class BounceEffectSystem {
public:
	void Update(Manager& manager) const;

private:
	[[nodiscard]] static float ApplyEase(float t, bool symmetrical, const Ease& ease);
};

class ShakeEffectSystem {
public:
	void Update(Manager& manager, float time, float dt) const;
};

class FollowEffectSystem {
public:
	void Update(Manager& manager) const;
};

template <typename TComponent, typename T>
void AddTweenEffect(
	Entity& entity, const T& target, milliseconds duration, const Ease& ease, bool force,
	const T& current_value
) {
	PTGN_ASSERT(duration >= milliseconds{ 0 }, "Tween effect must have a positive duration");

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

void BounceImpl(
	Entity& entity, const V2_float& amplitude, milliseconds duration, std::int64_t total_periods,
	const Ease& ease, const V2_float& static_offset, bool force, bool symmetrical
);

} // namespace impl

/**
 * @brief Translates an entity to a target position over a specified duration using a tweening
 * function.
 *
 * @param entity The entity to be moved.
 * @param target_position The position to move the entity to.
 * @param duration The duration over which the translation should occur.
 * @param ease The easing function to apply for the translation animation.
 * @param force If true, forcibly overrides any ongoing translation.
 */
void TranslateTo(
	Entity& entity, const V2_float& target_position, milliseconds duration,
	const Ease& ease = SymmetricalEase::Linear, bool force = true
);

/**
 * @brief Rotates an entity to a target angle over a specified duration using a tweening function.
 *
 * @param entity The entity to be rotated.
 * @param target_angle The angle (in radians) to rotate the entity to. Positive clockwise, negative
 * counter-clockwise.
 * @param duration The duration over which the rotation should occur.
 * @param ease The easing function to apply for the rotation animation.
 * @param force If true, forcibly overrides any ongoing rotation.
 */
void RotateTo(
	Entity& entity, float target_angle, milliseconds duration,
	const Ease& ease = SymmetricalEase::Linear, bool force = true
);

/**
 * @brief Scales an entity to a target size over a specified duration using a tweening function.
 *
 * @param entity The entity to be scaled.
 * @param target_scale The target scale (width, height) to apply to the entity.
 * @param duration The duration over which the scaling should occur.
 * @param ease The easing function to apply for the scale animation.
 * @param force If true, forcibly overrides any ongoing scaling.
 */
void ScaleTo(
	Entity& entity, const V2_float& target_scale, milliseconds duration,
	const Ease& ease = SymmetricalEase::Linear, bool force = true
);

/**
 * @brief Tints an entity to a target color over a specified duration using a tweening function.
 *
 * @param entity The entity to be tinted.
 * @param target_tint The target color tint to apply to the entity.
 * @param duration The duration over which the tinting should occur.
 * @param ease The easing function to apply for the tint animation.
 * @param force If true, forcibly overrides any ongoing tinting.
 */
void TintTo(
	Entity& entity, const Color& target_tint, milliseconds duration,
	const Ease& ease = SymmetricalEase::Linear, bool force = true
);

/**
 * @brief Fades in the specified entity over a given duration.
 *
 * @param entity The entity to apply the fade-in effect to.
 * @param duration The time span over which the fade-in will occur.
 * @param ease The easing function used to interpolate the fade.
 * @param force If true, the fade-in will override any ongoing fade effect.
 */
void FadeIn(
	Entity& entity, milliseconds duration, const Ease& ease = SymmetricalEase::Linear,
	bool force = true
);

/**
 * @brief Fades out the specified entity over a given duration.
 *
 * @param entity The entity to apply the fade-out effect to.
 * @param duration The time span over which the fade-out will occur.
 * @param ease The easing function used to interpolate the fade.
 * @param force If true, the fade-out will override any ongoing fade effect.
 */
void FadeOut(
	Entity& entity, milliseconds duration, const Ease& ease = SymmetricalEase::Linear,
	bool force = true
);

/**
 * @brief Applies a bouncing motion to the specified entity.
 *
 * The bounce starts at the entity position (or previously queued bounce end point), approaches the
 * amplitude offset and then returns back to the origin point all within a single duration and can
 * repeat a specified number of times or indefinitely.
 *
 * @param entity The entity to apply the bounce effect to.
 * @param bounce_amplitude The peak offset applied during the bounce.
 * @param duration The duration of one bounce cycle (e.g., up and down).
 * @param total_periods Number of up and down bounce cycles. If -1, bounce continues indefinitely
 * until StopBounce is called.
 * @param ease The easing function to use for the bounce.
 * @param static_offset A constant offset added to the entity's position throughout the bounce.
 * @param force If true, overrides any existing bounce effect on the entity.
 */
void Bounce(
	Entity& entity, const V2_float& bounce_amplitude, milliseconds duration,
	std::int64_t total_periods = -1, const Ease& ease = SymmetricalEase::Linear,
	const V2_float& static_offset = {}, bool force = true
);

/**
 * @brief Applies a symmetrical bouncing motion to the specified entity.
 *
 * Similar to a regular bounce, the symmetrical bounce starts at the entity position (or previously
 * queued bounce end point), approaches the amplitude offset and then goes to a negative amplitude
 * offset before returning back to the origin point all within a single duration and can repeat a
 * specified number of times or indefinitely. As a result, a symmetrical bounce requires a
 * symmetrical easing function. Note: Symmetrical bounces occupy the same effect queue as regular
 * bounces, i.e. they can not occur at the same time for the same entity.
 *
 * @param entity The entity to apply the bounce effect to.
 * @param bounce_amplitude The peak offset applied during the bounce.
 * @param duration The duration of one bounce cycle (e.g., up and down).
 * @param total_periods Number of up and down bounce cycles. If -1, bounce continues indefinitely
 * until StopBounce is called.
 * @param ease The symmetrical easing function to use for the bounce.
 * @param static_offset A constant offset added to the entity's position throughout the bounce.
 * @param force If true, overrides any existing bounce effect on the entity.
 */
void SymmetricalBounce(
	Entity& entity, const V2_float& bounce_amplitude, milliseconds duration,
	std::int64_t total_periods = -1, SymmetricalEase ease = SymmetricalEase::Linear,
	const V2_float& static_offset = {}, bool force = true
);

/**
 * @brief Stops the current bounce tween and proceeds to the next one in the queue.
 *
 * @param entity The entity whose bounce animation should be stopped.
 * @param force If true, clears the entire bounce queue instead of just the current tween.
 */
void StopBounce(Entity& entity, bool force = true);

/**
 * @brief Applies a continuous shake effect to the specified entity.
 *
 * @param entity The entity to apply the shake effect to.
 * @param intensity The intensity of the shake, in the range [0, 1].
 * @param duration The total duration of the shake effect. If -1, the shake continues until
 * StopShake is called.
 * @param config Configuration parameters for the shake behavior.
 * @param ease The easing function to use for the shake. If SymmetricalEase::None, shake remains at
 * full intensity for the entire time.
 * @param force If true, overrides any existing shake effect.
 */
void Shake(
	Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config = {},
	const Ease& ease = SymmetricalEase::None, bool force = true
);

/**
 * @brief Applies an instantenous shake effect to the specified entity.
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
 * @brief Starts a follow behavior where one entity follows another based on the specified
 * configuration.
 *
 * @param entity The entity that will follow the target.
 * @param target The entity to be followed.
 * @param config The configuration parameters that define how the follow behavior should operate.
 * @param force If true, forces the replacement of any existing follow behavior on the entity.
 */
void StartFollow(Entity entity, Entity target, FollowConfig config = {}, bool force = true);

/**
 * @brief Stops any active follow behavior on the specified entity.
 *
 * @param entity The entity whose follow behavior should be stopped.
 * @param force If true, clears all queued follows effects.
 */
void StopFollow(Entity entity, bool force = true);

} // namespace ptgn