#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

#include "core/assert.h"
#include "core/ecs/components/movement.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/ecs/entity_hierarchy.h"
#include "core/ecs/game_object.h"
#include "core/util/time.h"
#include "math/easing.h"
#include "math/tolerance.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "renderer/api/color.h"
#include "serialization/json/serializable.h"
#include "tween/follow_config.h"
#include "tween/shake_config.h"
#include "tween/tween.h"

namespace ptgn {

class Manager;

namespace impl {

struct Offsets;

template <typename T>
struct Effect {
	Effect() = default;

	Effect(const T& start) : start{ start } {}

	T start{};

	bool operator==(const Effect&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Effect, start)
};

struct TranslateEffect : public Effect<V2_float> {};

struct RotateEffect : public Effect<float> {};

struct ScaleEffect : public Effect<V2_float> {};

struct TintEffect : public Effect<Color> {};

struct FollowEffect {
	FollowEffect() = default;

	std::size_t current_waypoint{ 0 };

	// Cache for comparing when a waypoint path changes.
	std::vector<V2_float> waypoints;

	bool operator==(const FollowEffect&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(FollowEffect, current_waypoint, waypoints)
};

struct BounceEffect {
	BounceEffect() = default;
};

struct ShakeEffect {
	ShakeEffect() = default;

	// Range [0, 1] defining the current amount of stress this entity is enduring.
	float trauma{ 0.0f };

	float previous_target{ 0.0f };

	bool operator==(const ShakeEffect&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(ShakeEffect, trauma, previous_target)
};

template <typename TComponent>
struct EffectObject : public GameObject<Tween> {
	using GameObject::GameObject;
};

template <typename TComponent>
EffectObject<TComponent>& GetTween(Entity& entity) {
	EffectObject<TComponent>* tween{ nullptr };

	if (!entity.Has<EffectObject<TComponent>>()) {
		tween = &entity.Add<EffectObject<TComponent>>(CreateTween(entity.GetManager()));
		SetParent(*tween, entity);
	} else {
		tween = &entity.Get<EffectObject<TComponent>>();
	}

	PTGN_ASSERT(tween);

	return *tween;
}

template <typename TComponent, typename T>
EffectObject<TComponent>& AddTweenEffect(
	Entity& entity, const T& target, milliseconds duration, const Ease& ease, bool force,
	const std::function<T(Entity)>& get_current_value,
	const std::function<void(Entity, T)>& set_current_value
) {
	PTGN_ASSERT(duration > milliseconds{ 0 }, "Tween effect must have a positive duration");

	EffectObject<TComponent>& tween{ GetTween<TComponent>(entity) };

	tween.template TryAdd<TComponent>();

	if (force || tween.IsCompleted()) {
		tween.Clear();
	}

	auto update_start = [get_current_value](auto e) mutable {
		auto& value{ e.template Get<TComponent>() };
		Entity parent{ GetParent(e) };
		value.start = get_current_value(parent);
	};

	tween.During(duration)
		.Ease(ease)
		.OnStart(update_start)
		.OnProgress([target, set_current_value](Entity e, float progress) mutable {
			auto& value{ e.template Get<TComponent>() };
			auto result{ Lerp(value.start, target, progress) };
			Entity parent{ GetParent(e) };
			set_current_value(parent, result);
		})
		.OnPointComplete(update_start)
		.OnComplete(update_start)
		.OnStop(update_start)
		.OnReset(update_start);
	tween.Start(force);

	return tween;
}

void ApplyShake(Offsets& offsets, float trauma, const ShakeConfig& config, std::int32_t seed);

V2_float GetFollowPosition(
	const FollowConfig& config, const V2_float& position, const V2_float& target_position
);

void VelocityModeMoveImpl(const FollowConfig& config, Entity& parent, const V2_float& dir);

template <EntityBase T>
void TargetFollowImpl(Entity target, const TargetFollowConfig& config, Entity tween_entity) {
	if (!config.follow_x && !config.follow_y) {
		return;
	}

	Tween tween{ tween_entity };

	if (!target || !target.IsAlive()) {
		tween.IncrementPoint();
		return;
	}

	T parent{ GetParent(tween_entity) };
	auto current_position{ GetAbsolutePosition(parent) };
	V2_float target_pos{ GetAbsolutePosition(target) + config.offset };

	auto dir{ target_pos - current_position };

	if (config.move_mode == MoveMode::Velocity) {
		VelocityModeMoveImpl(config, parent, dir);
	} else {
		auto new_pos{ GetFollowPosition(config, current_position, target_pos) };
		dir = target_pos - new_pos;

		SetPosition(parent, new_pos);
	}
	if (config.stop_distance < epsilon<float>) {
		return;
	}
	if (auto dist2{ dir.MagnitudeSquared() };
		dist2 >= config.stop_distance * config.stop_distance) {
		return;
	}
	tween.IncrementPoint();
}

template <EntityBase T>
void PathFollowImpl(
	const std::vector<V2_float>& waypoints, const PathFollowConfig& config, Entity tween_entity
) {
	if (!config.follow_x && !config.follow_y) {
		return;
	}

	Tween tween{ tween_entity };
	T parent{ GetParent(tween_entity) };

	auto current_pos{ GetAbsolutePosition(parent) };

	auto& follow{ tween_entity.Get<FollowEffect>() };

	PTGN_ASSERT(follow.current_waypoint < waypoints.size());

	V2_float target_pos{ waypoints[follow.current_waypoint] + config.offset };

	auto dir{ target_pos - current_pos };

	if (dir.MagnitudeSquared() < config.stop_distance * config.stop_distance) {
		if (follow.current_waypoint + 1 < waypoints.size()) {
			follow.current_waypoint++;
		} else if (config.loop_path) {
			follow.current_waypoint = 0;
		} else {
			tween.IncrementPoint();
			return;
		}
	}

	if (config.move_mode == MoveMode::Velocity) {
		VelocityModeMoveImpl(config, parent, dir);
		return;
	}

	auto new_pos{ GetFollowPosition(config, current_pos, target_pos) };
	SetPosition(parent, new_pos);
}

void EntityFollowStopImpl(Entity e);

EffectObject<FollowEffect>& StartFollowImpl(
	const FollowConfig& config, Entity& entity, bool force, auto start_func, auto update_func
) {
	PTGN_ASSERT(config.lerp.x >= 0.0f && config.lerp.x <= 1.0f);
	PTGN_ASSERT(config.lerp.y >= 0.0f && config.lerp.y <= 1.0f);

	EffectObject<FollowEffect>& tween{ GetTween<FollowEffect>(entity) };

	tween.TryAdd<FollowEffect>();

	if (force || tween.IsCompleted()) {
		tween.Clear();
	}

	tween.During(milliseconds{ 0 })
		.Repeat(-1)
		.OnStart(start_func)
		.OnProgress(update_func)
		.OnPointComplete(&EntityFollowStopImpl)
		.OnComplete(&EntityFollowStopImpl)
		.OnStop(&EntityFollowStopImpl)
		.OnReset(&EntityFollowStopImpl);
	tween.Start(force);
	return tween;
}

template <EntityBase T>
void EntityFollowStartImpl(T parent, const FollowConfig& config) {
	if (config.move_mode != MoveMode::Velocity) {
		parent.template Remove<TopDownMovement>();
		parent.template Remove<RigidBody>();
		return;
	}
	parent.template TryAdd<RigidBody>();
	if (!parent.template Has<Transform>()) {
		SetPosition(parent, {});
	}
	auto& movement{ parent.template TryAdd<TopDownMovement>() };
	movement.max_acceleration		  = config.max_acceleration;
	movement.max_deceleration		  = config.max_acceleration;
	movement.max_speed				  = config.max_speed;
	movement.keys_enabled			  = false;
	movement.only_orthogonal_movement = false;
}

template <EntityBase T>
impl::EffectObject<impl::FollowEffect>& StartFollowPathImpl(
	T& entity, const std::vector<V2_float>& waypoints, const PathFollowConfig& config = {},
	bool force = true, bool reset_waypoint_index = false
) {
	PTGN_ASSERT(!waypoints.empty(), "Cannot follow an empty set of waypoints");
	PTGN_ASSERT(
		config.stop_distance >= epsilon<float>,
		"Stopping distance cannot be negative or 0 when following waypoints"
	);

	PTGN_ASSERT(config.lerp.x >= 0.0f && config.lerp.x <= 1.0f);
	PTGN_ASSERT(config.lerp.y >= 0.0f && config.lerp.y <= 1.0f);

	impl::EffectObject<impl::FollowEffect>& tween{ impl::GetTween<impl::FollowEffect>(entity) };

	auto& follow_comp{ tween.TryAdd<impl::FollowEffect>() };

	if (force || tween.IsCompleted()) {
		tween.Clear();
	}

	std::vector<V2_float> prev_waypoints{ follow_comp.waypoints };
	follow_comp.waypoints = waypoints;

	const auto start_func = [reset_waypoint_index, config, waypoints, prev_waypoints](auto e) {
		T parent{ GetParent(e) };
		if (config.teleport_on_start && !waypoints.empty()) {
			V2_float target_position{ waypoints.back() };
			SetPosition(parent, target_position);
		}

		// Reasons to reset waypoint index:
		// 1. User requested it.
		// 2. Current waypoint is beyond the waypoints array size.
		// 3. Waypoints have changed.
		if (auto& follow{ e.template Get<impl::FollowEffect>() };
			reset_waypoint_index || follow.current_waypoint >= waypoints.size() ||
			waypoints != prev_waypoints) {
			follow.current_waypoint = 0;
		}

		impl::EntityFollowStartImpl<T>(parent, config);
	};

	const auto update_func = [config, waypoints](Entity e, float) {
		impl::PathFollowImpl<T>(waypoints, config, e);
	};

	tween.During(milliseconds{ 0 })
		.Repeat(-1)
		.OnStart(start_func)
		.OnProgress(update_func)
		.OnPointComplete(&impl::EntityFollowStopImpl)
		.OnComplete(&impl::EntityFollowStopImpl)
		.OnStop(&impl::EntityFollowStopImpl)
		.OnReset(&impl::EntityFollowStopImpl);
	tween.Start(force);
	return tween;
}

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
template <EntityBase T = Entity>
impl::EffectObject<impl::TranslateEffect>& TranslateTo(
	T& entity, const V2_float& target_position, milliseconds duration,
	const Ease& ease = SymmetricalEase::Linear, bool force = true
) {
	return impl::AddTweenEffect<impl::TranslateEffect, V2_float>(
		entity, target_position, duration, ease, force,
		[](Entity e) {
			T derived{ e };
			return GetPosition(derived);
		},
		[](Entity e, V2_float v) {
			T derived{ e };
			SetPosition(derived, v);
		}
	);
}

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
template <EntityBase T = Entity>
impl::EffectObject<impl::RotateEffect>& RotateTo(
	T& entity, float target_angle, milliseconds duration,
	const Ease& ease = SymmetricalEase::Linear, bool force = true
) {
	return impl::AddTweenEffect<impl::RotateEffect, float>(
		entity, target_angle, duration, ease, force,
		[](Entity e) {
			T derived{ e };
			return GetRotation(derived);
		},
		[](Entity e, float v) {
			T derived{ e };
			SetRotation(derived, v);
		}
	);
}

/**
 * @brief Scales an entity to a target size over a specified duration using a tweening function.
 *
 * @param entity The entity to be scaled.
 * @param target_scale The target scale (width, height) to apply to the entity.
 * @param duration The duration over which the scaling should occur.
 * @param ease The easing function to apply for the scale animation.
 * @param force If true, forcibly overrides any ongoing scaling.
 */
template <EntityBase T = Entity>
impl::EffectObject<impl::ScaleEffect>& ScaleTo(
	T& entity, const V2_float& target_scale, milliseconds duration,
	const Ease& ease = SymmetricalEase::Linear, bool force = true
) {
	return impl::AddTweenEffect<impl::ScaleEffect, V2_float>(
		entity, target_scale, duration, ease, force,
		[](Entity e) {
			T derived{ e };
			return GetScale(derived);
		},
		[](Entity e, V2_float v) {
			T derived{ e };
			SetScale(derived, v);
		}
	);
}

/**
 * @brief Tints an entity to a target color over a specified duration using a tweening function.
 *
 * @param entity The entity to be tinted.
 * @param target_tint The target color tint to apply to the entity.
 * @param duration The duration over which the tinting should occur.
 * @param ease The easing function to apply for the tint animation.
 * @param force If true, forcibly overrides any ongoing tinting.
 */
impl::EffectObject<impl::TintEffect>& TintTo(
	Entity& entity, const Color& target_tint, milliseconds duration,
	const Ease& ease = SymmetricalEase::Linear, bool force = true
);

/**
 * @brief Fades in the specified entity over a given duration. If the object already has a tint of
 * color::White, does nothing. Set tint to color::Transparent for a full fade in effect.
 *
 * @param entity The entity to apply the fade-in effect to.
 * @param duration The time span over which the fade-in will occur.
 * @param ease The easing function used to interpolate the fade.
 * @param force If true, the fade-in will override any ongoing fade effect.
 */
impl::EffectObject<impl::TintEffect>& FadeIn(
	Entity& entity, milliseconds duration, const Ease& ease = SymmetricalEase::Linear,
	bool force = true
);

/**
 * @brief Fades out the specified entity over a given duration. If the object already has a tint of
 * color::Transparent, does nothing. Set tint to color::White for a full fade out effect.
 *
 * @param entity The entity to apply the fade-out effect to.
 * @param duration The time span over which the fade-out will occur.
 * @param ease The easing function used to interpolate the fade.
 * @param force If true, the fade-out will override any ongoing fade effect.
 */
impl::EffectObject<impl::TintEffect>& FadeOut(
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
impl::EffectObject<impl::BounceEffect>& Bounce(
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
impl::EffectObject<impl::BounceEffect>& SymmetricalBounce(
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
 * @param intensity The intensity of the shake, in the range [-1, 1] (negative values reduce any
 * existing shake trauma).
 * @param duration The total duration of the shake effect. If -1, the shake continues until
 * StopShake is called.
 * @param config Configuration parameters for the shake behavior.
 * @param ease The easing function to use for the shake. If SymmetricalEase::None, shake remains at
 * full intensity for the entire time.
 * @param force If true, overrides any existing shake effect.
 * @param reset_trauma If true, resets the trauma immediately upon completing the final queued shake
 * effect.
 */
impl::EffectObject<impl::ShakeEffect>& Shake(
	Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config = {},
	const Ease& ease = SymmetricalEase::None, bool force = true, bool reset_trauma = false
);

/**
 * @brief Applies a continuous constant shake of a given intensity to the specified entity.
 *
 * @param entity The entity to apply the shake effect to.
 * @param intensity The intensity of the shake, in the range [-1, 1] (negative values reduce any
 * existing shake trauma).
 * @param duration The total duration of the shake effect. If -1, the shake continues until
 * StopShake is called.
 * @param config Configuration parameters for the shake behavior.
 * @param force If true, overrides any existing shake effect.
 * @param reset_trauma If true, resets the trauma immediately upon completing the final queued shake
 * effect.
 */
impl::EffectObject<impl::ShakeEffect>& Shake(
	Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config = {},
	bool force = true, bool reset_trauma = false
);

/**
 * @brief Applies an instantenous shake effect to the specified entity.
 *
 * @param entity The entity to apply the shake effect to.
 * @param intensity The intensity of the shake, in the range [-1, 1] (negative values reduce any
 * existing shake trauma).
 * @param config Configuration parameters for the shake behavior.
 * @param force If true, overrides any existing shake effect.
 */
impl::EffectObject<impl::ShakeEffect>& Shake(
	Entity& entity, float intensity, const ShakeConfig& config = {}, bool force = true
);

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
template <EntityBase T = Entity>
impl::EffectObject<impl::FollowEffect>& StartFollow(
	T& entity, Entity target, const TargetFollowConfig& config = {}, bool force = true
) {
	Entity base{ entity };

	return impl::StartFollowImpl(
		config, base, force,
		[config, target](Entity e) {
			T parent{ GetParent(e) };
			if (config.teleport_on_start) {
				SetPosition(parent, GetPosition(target));
			}
			impl::EntityFollowStartImpl<T>(parent, config);
		},
		[config, target](Entity e, float) { impl::TargetFollowImpl<T>(target, config, e); }
	);
}

/**
 * @brief Starts a follow behavior where the entity follows a path of waypoints based on the
 * specified configuration.
 *
 * @param entity The entity that will follow the target.
 * @param waypoints The set of waypoints the entity will visit during the follow.
 * @param config The configuration parameters that define how the follow behavior should operate.
 * @param force If true, forces the replacement of any existing follow behavior on the entity.
 * @param reset_waypoint_index If true, resets the waypoint index to 0. If false, continues where it
 * started as long as waypoints have not changed or the end has not been reached (if
 * config.loop_path is false).
 */
template <EntityBase T = Entity>
impl::EffectObject<impl::FollowEffect>& StartFollow(
	T& entity, const std::vector<V2_float>& waypoints, const PathFollowConfig& config = {},
	bool force = true, bool reset_waypoint_index = false
) {
	return impl::StartFollowPathImpl<T>(entity, waypoints, config, force, reset_waypoint_index);
}

/**
 * @brief Stops any active follow behavior on the specified entity.
 *
 * @param entity The entity whose follow behavior should be stopped.
 * @param force If true, clears all queued follows effects.
 * @param reset_previous_waypoints If true, resets the previously set waypoints. If false, a new
 * follow will continue where it started as long as waypoints have not changed or the end has not
 * been reached (if config.loop_path is false).
 */
void StopFollow(Entity& entity, bool force = true, bool reset_previous_waypoints = false);

} // namespace ptgn