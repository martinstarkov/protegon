#pragma once

#include <chrono>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/angle.h"
#include "core/math/easing.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/time.h"
#include "runtime/animation/follow_config.h"
#include "runtime/animation/shake_config.h"
#include "runtime/animation/tween.h"
#include "runtime/animation/tween_event.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/game_object.h"
#include "runtime/ecs/tag.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;

namespace impl {

struct Offsets;

template <typename T>
struct TweenEffect {
	TweenEffect() = default;

	explicit TweenEffect(const T& start) : start{ start } {}

	T start{};

	bool operator==(const TweenEffect&) const = default;

	PTGN_SERIALIZE(TweenEffect, start)
};

struct TranslateEffect : public TweenEffect<V2_float> {};

struct RotateEffect : public TweenEffect<Radians> {};

struct ScaleEffect : public TweenEffect<V2_float> {};

struct TintEffect : public TweenEffect<Color> {};

struct FollowEffect {
	FollowEffect() = default;

	std::size_t current_waypoint{ 0 };

	// Cache for comparing when a waypoint path changes.
	std::vector<V2_float> waypoints;

	bool operator==(const FollowEffect&) const = default;

	// TODO: Fix serialization.
	// PTGN_SERIALIZE(FollowEffect, current_waypoint, waypoints)
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

	// TODO: Fix serialization.
	// PTGN_SERIALIZE(ShakeEffect, trauma, previous_target)
};

template <typename TComponent>
struct EffectObject : public GameObject<> {
	using GameObject<>::GameObject;
};

} // namespace impl

template <typename TComponent>
Tween GetOrCreateTween(Entity entity) {
	Tween tween;

	if (!entity.Has<impl::EffectObject<TComponent>>()) {
		impl::EffectObject<TComponent> obj{ CreateTween(entity.GetScene()) };
		PTGN_DEFAULT_NAME(obj, "Tween Effect");
		SetParent(obj, entity);
		tween = Tween{ obj };
		entity.Add<impl::EffectObject<TComponent>>(std::move(obj));
	} else {
		tween = Tween{ entity.Get<impl::EffectObject<TComponent>>() };
	}

	PTGN_ASSERT(tween, "Failed to retrieve effect tween for entity: ", entity);

	return tween;
}

namespace impl {

template <typename TComponent, typename T>
Tween AddTweenEffect(
	Entity entity, const T& target, milliseconds duration, Ease ease, bool force,
	const std::function<T(Entity)>& get_current_value,
	const std::function<void(Entity, T)>& set_current_value
) {
	PTGN_ASSERT(duration > 0ms, "Tween effect must have a positive duration");

	auto tween{ GetOrCreateTween<TComponent>(entity) };

	tween.template TryAdd<TComponent>();

	if (force || tween.IsCompleted()) {
		tween.Clear();
	}

	auto update_start = [get_current_value](auto p) mutable {
		auto& value{ p.tween.template Get<TComponent>() };
		value.start = get_current_value(p.parent);
	};

	tween.During(duration)
		.Ease(ease)
		.OnStart(update_start)
		.OnProgress([target, set_current_value](auto p) mutable {
			auto& value{ p.tween.template Get<TComponent>() };
			auto result{ Lerp(value.start, target, p.progress) };
			set_current_value(p.parent, result);
		})
		.OnPointComplete(update_start)
		.OnComplete(update_start)
		.OnStop(update_start)
		.OnReset(update_start);
	tween.Start(force);

	return tween;
}

void ApplyShake(
	milliseconds time, Offsets& offsets, float trauma, const ShakeConfig& config, std::int32_t seed
);

V2_float GetFollowPosition(
	secondsf dt, const FollowConfig& config, V2_float position, V2_float target_position
);

void VelocityModeMoveImpl(const FollowConfig& config, Entity parent, V2_float dir);

void TargetFollowImpl(Entity target, const TargetFollowConfig& config, Tween tween);

void PathFollowImpl(
	std::span<const V2_float> waypoints, const PathFollowConfig& config, Tween tween
);

void EntityFollowStopImpl(Entity parent);

template <typename T>
void EntityFollowStopImpl(const T& event) {
	EntityFollowStopImpl(event.parent);
}

template <
	EventCallbackInvocable<event::TweenStart> F1, EventCallbackInvocable<event::TweenProgress> F2>
Tween StartFollowImpl(Entity entity, bool force, F1&& start_func, F2&& progress_func) {
	auto tween{ GetOrCreateTween<FollowEffect>(entity) };

	tween.TryAdd<FollowEffect>();

	if (force || tween.IsCompleted()) {
		tween.Clear();
	}

	tween.During(0ms)
		.Repeat()
		.OnStart(std::forward<F1>(start_func))
		.OnProgress(std::forward<F2>(progress_func))
		.OnPointComplete(&EntityFollowStopImpl<ptgn::event::TweenPointComplete>)
		.OnComplete(&EntityFollowStopImpl<ptgn::event::TweenComplete>)
		.OnStop(&EntityFollowStopImpl<ptgn::event::TweenStop>)
		.OnReset(&EntityFollowStopImpl<ptgn::event::TweenReset>);
	tween.Start(force);

	return tween;
}

void EntityFollowStartImpl(Entity parent, const FollowConfig& config);

Tween StartFollowPathImpl(
	Entity entity, std::span<const V2_float> waypoints, const PathFollowConfig& config = {},
	bool force = true, bool reset_waypoint_index = false
);

template <EntityType E, InvocableR<Tween, const E&> F>
[[nodiscard]] std::vector<Tween> CreateTweens(std::span<const E> entities, F create_tween) {
	return entities | std::views::transform([&](const E& entity) {
			   return std::invoke(create_tween, entity);
		   }) |
		   std::ranges::to<std::vector<Tween>>();
}

template <EntityType E, typename T, InvocableR<Tween, const E&, const T&> F>
[[nodiscard]] std::vector<Tween> CreateTweens(
	std::span<const E> entities, std::span<const T> targets, F create_tween
) {
	PTGN_ASSERT(targets.size() == entities.size(), "Target count must match entity count");

	return std::views::zip_transform(
			   [&](const E& entity, const T& target) {
				   return std::invoke(create_tween, entity, target);
			   },
			   entities, targets
		   ) |
		   std::ranges::to<std::vector<Tween>>();
}

} // namespace impl

template <typename T>
struct TweenProperty {
	TweenProperty() = default;

	template <InvocableR<T, Entity> GetFunc, InvocableR<void, Entity, const T&> SetFunc>
	TweenProperty(GetFunc&& get, SetFunc&& set) :
		get{ std::forward<GetFunc>(get) }, set{ std::forward<SetFunc>(set) } {}

	template <InvocableR<T, Entity> GetFunc>
	TweenProperty(GetFunc&& get) : get{ std::forward<GetFunc>(get) } {} // NOSONAR

	template <InvocableR<void, Entity, const T&> SetFunc>
	TweenProperty(SetFunc&& set) : set{ std::forward<SetFunc>(set) } {} // NOSONAR

	std::function<T(Entity)> get;
	std::function<void(Entity, const T&)> set;
};

/// @brief Tweens a property of an entity to a target value over a specified duration.
///
/// @tparam TComponent The component used to tag the target property.
/// @tparam T The type of the value being tweened (e.g., float, V2_float).
///
/// @param entity The entity whose property will be tweened.
/// @param target The target value.
/// @param duration The duration of the tween.
/// @param ease The easing function to apply.
/// @param property The property getter/setter pair.
/// @param force If true, overrides any ongoing tween.
template <typename TComponent, std::copy_constructible T>
Tween TweenTo(
	Entity entity, const T& target, milliseconds duration, Ease ease, TweenProperty<T> property,
	bool force = true
) {
	PTGN_ASSERT(duration > 0ms, "Tween must have a positive duration");

	auto tween{ GetOrCreateTween<TComponent>(entity) };

	if (force || tween.IsCompleted()) {
		tween.Clear();
	}

	// Store start value inside the tween
	auto start = std::make_shared<T>();

	auto update_start = [start, property](auto p) mutable {
		*start = property.get(p.parent);
	};

	tween.During(duration)
		.Ease(ease)
		.OnStart(update_start)
		.OnProgress([start, property, target](auto p) mutable {
			auto result = Lerp(*start, target, p.progress);
			property.set(p.parent, result);
		})
		.OnPointComplete(update_start)
		.OnComplete(update_start)
		.OnStop(update_start)
		.OnReset(update_start);

	tween.Start(force);

	return tween;
}

template <typename TComponent, std::copy_constructible T, EntityType E>
std::vector<Tween> TweenTo(
	std::span<const E> entities, const T& target, milliseconds duration, Ease ease,
	TweenProperty<T> property, bool force = true
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return TweenTo<TComponent, T>(entity, target, duration, ease, property, force);
	});
}

template <typename TComponent, std::copy_constructible T, EntityType E>
std::vector<Tween> TweenTo(
	std::span<const E> entities, std::span<const T> targets, milliseconds duration, Ease ease,
	TweenProperty<T> property, bool force = true
) {
	return impl::CreateTweens(entities, targets, [&](const E& entity, const T& target) {
		return TweenTo<TComponent, T>(entity, target, duration, ease, property, force);
	});
}

/// @brief Translates an entity to a target position over a specified duration using a tweening
/// function.
///
/// @param entity The entity to be moved.
/// @param target_position The position to move the entity to.
/// @param duration The duration over which the translation should occur.
/// @param ease The easing function to apply for the translation animation.
/// @param force If true, forcibly overrides any ongoing translation.
Tween TranslateTo(
	Entity entity, V2_float target_position, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true
);

/// @brief Rotates an entity to a target angle over a specified duration using a tweening function.
///
/// @param entity The entity to be rotated.
/// @param target_angle The angle to rotate the entity to. Positive clockwise.
/// @param duration The duration over which the rotation should occur.
/// @param ease The easing function to apply for the rotation animation.
/// @param force If true, forcibly overrides any ongoing rotation.
Tween RotateTo(
	Entity entity, Degrees target_angle, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true
);

/// @brief Scales an entity to a target size over a specified duration using a tweening function.
///
/// @param entity The entity to be scaled.
/// @param target_scale The target scale (width, height) to apply to the entity.
/// @param duration The duration over which the scaling should occur.
/// @param ease The easing function to apply for the scale animation.
/// @param force If true, forcibly overrides any ongoing scaling.
Tween ScaleTo(
	Entity entity, V2_float target_scale, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true
);

/// @brief Tints an entity to a target color over a specified duration using a tweening function.
///
/// @param entity The entity to be tinted.
/// @param target_tint The target color tint to apply to the entity.
/// @param duration The duration over which the tinting should occur.
/// @param ease The easing function to apply for the tint animation.
/// @param force If true, forcibly overrides any ongoing tinting.
Tween TintTo(
	Entity entity, Color target_tint, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true
);

template <EntityType E>
std::vector<Tween> TranslateTo(
	std::span<const E> entities, V2_float target_position, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return TranslateTo(entity, target_position, duration, ease, force);
	});
}

template <EntityType E>
std::vector<Tween> TranslateTo(
	std::span<const E> entities, std::span<const V2_float> target_positions, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true
) {
	return impl::CreateTweens(
		entities, target_positions, [&](const E& entity, const V2_float& target_position) {
			return TranslateTo(entity, target_position, duration, ease, force);
		}
	);
}

template <EntityType E>
std::vector<Tween> RotateTo(
	std::span<const E> entities, Degrees target_angle, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return RotateTo(entity, target_angle, duration, ease, force);
	});
}

template <EntityType E>
std::vector<Tween> RotateTo(
	std::span<const E> entities, std::span<const Degrees> target_angles, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true
) {
	return impl::CreateTweens(
		entities, target_angles, [&](const E& entity, const Degrees& target_angle) {
			return RotateTo(entity, target_angle, duration, ease, force);
		}
	);
}

template <EntityType E>
std::vector<Tween> ScaleTo(
	std::span<const E> entities, V2_float target_scale, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return ScaleTo(entity, target_scale, duration, ease, force);
	});
}

template <EntityType E>
std::vector<Tween> ScaleTo(
	std::span<const E> entities, std::span<const V2_float> target_scales, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true
) {
	return impl::CreateTweens(
		entities, target_scales, [&](const E& entity, const V2_float& target_scale) {
			return ScaleTo(entity, target_scale, duration, ease, force);
		}
	);
}

template <EntityType E>
std::vector<Tween> TintTo(
	std::span<const E> entities, Color target_tint, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return TintTo(entity, target_tint, duration, ease, force);
	});
}

template <EntityType E>
std::vector<Tween> TintTo(
	std::span<const E> entities, std::span<const Color> target_tints, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true
) {
	return impl::CreateTweens(
		entities, target_tints, [&](const E& entity, const Color& target_tint) {
			return TintTo(entity, target_tint, duration, ease, force);
		}
	);
}

/// @brief Fades in the specified entity over a given duration. If the object already has a tint of
/// color::White, does nothing. Set tint to color::Transparent for a full fade in effect.
///
/// @param entity The entity to apply the fade-in effect to.
/// @param duration The time span over which the fade-in will occur.
/// @param ease The easing function used to interpolate the fade.
/// @param force If true, the fade-in will override any ongoing fade effect.
Tween FadeIn(
	Entity entity, milliseconds duration, Ease ease = Ease::Linear, bool force = true,
	bool start_transparent = false
);

/// @brief Fades out the specified entity over a given duration. If the object already has a tint of
/// color::Transparent, does nothing. Set tint to color::White for a full fade out effect.
///
/// @param entity The entity to apply the fade-out effect to.
/// @param duration The time span over which the fade-out will occur.
/// @param ease The easing function used to interpolate the fade.
/// @param force If true, the fade-out will override any ongoing fade effect.
Tween FadeOut(
	Entity entity, milliseconds duration, Ease ease = Ease::Linear, bool force = true,
	bool start_opaque = false
);

/// @brief Applies a bouncing motion to the specified entity.
///
/// The bounce starts at the entity position (or previously queued bounce end point), approaches the
/// amplitude offset and then returns back to the origin point all within a single duration and can
/// repeat a specified number of times or indefinitely.
///
/// @param entity The entity to apply the bounce effect to.
/// @param bounce_amplitude The peak offset applied during the bounce.
/// @param duration The duration of one bounce cycle (e.g., up and down).
/// @param total_periods Number of up and down bounce cycles. If nullopt, bounce continues
/// indefinitely until StopBounce is called.
/// @param ease The easing function to use for the bounce.
/// @param static_offset A constant offset added to the entity's position throughout the bounce.
/// @param force If true, overrides any existing bounce effect on the entity.
Tween Bounce(
	Entity entity, V2_float bounce_amplitude, milliseconds duration,
	std::optional<std::size_t> total_periods = std::nullopt, Ease ease = Ease::Linear,
	V2_float static_offset = {}, bool force = true
);

/// @brief Applies a symmetrical bouncing motion to the specified entity.
///
/// Similar to a regular bounce, the symmetrical bounce starts at the entity position (or previously
/// queued bounce end point), approaches the amplitude offset and then goes to a negative amplitude
/// offset before returning back to the origin point all within a single duration and can repeat a
/// specified number of times or indefinitely. As a result, a symmetrical bounce requires a
/// symmetrical easing function. Note: Symmetrical bounces occupy the same effect queue as regular
/// bounces, i.e. can not occur at the same time for the same entity.
///
/// @param entity The entity to apply the bounce effect to.
/// @param bounce_amplitude The peak offset applied during the bounce.
/// @param duration The duration of one bounce cycle (e.g., up and down).
/// @param total_periods Number of up and down bounce cycles. If nullopt, bounce continues
/// indefinitely until StopBounce is called.
/// @param ease The symmetrical easing function to use for the bounce.
/// @param static_offset A constant offset added to the entity's position throughout the bounce.
/// @param force If true, overrides any existing bounce effect on the entity.
Tween SymmetricalBounce(
	Entity entity, V2_float bounce_amplitude, milliseconds duration,
	std::optional<std::size_t> total_periods = std::nullopt, Ease ease = Ease::Linear,
	V2_float static_offset = {}, bool force = true
);

template <EntityType E>
std::vector<Tween> FadeIn(
	std::span<const E> entities, milliseconds duration, Ease ease = Ease::Linear, bool force = true,
	bool start_transparent = false
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return FadeIn(entity, duration, ease, force, start_transparent);
	});
}

template <EntityType E>
std::vector<Tween> FadeOut(
	std::span<const E> entities, milliseconds duration, Ease ease = Ease::Linear, bool force = true,
	bool start_opaque = false
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return FadeOut(entity, duration, ease, force, start_opaque);
	});
}

template <EntityType E>
std::vector<Tween> Bounce(
	std::span<const E> entities, V2_float bounce_amplitude, milliseconds duration,
	std::optional<std::size_t> total_periods = std::nullopt, Ease ease = Ease::Linear,
	V2_float static_offset = {}, bool force = true
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return Bounce(
			entity, bounce_amplitude, duration, total_periods, ease, static_offset, force
		);
	});
}

template <EntityType E>
std::vector<Tween> SymmetricalBounce(
	std::span<const E> entities, V2_float bounce_amplitude, milliseconds duration,
	std::optional<std::size_t> total_periods = std::nullopt, Ease ease = Ease::Linear,
	V2_float static_offset = {}, bool force = true
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return SymmetricalBounce(
			entity, bounce_amplitude, duration, total_periods, ease, static_offset, force
		);
	});
}

/// @brief Stops the current bounce tween and proceeds to the next one in the queue.
///
/// @param entity The entity whose bounce animation should be stopped.
/// @param force If true, clears the entire bounce queue instead of just the current tween.
void StopBounce(Entity entity, bool force = true);

template <EntityType E>
void StopBounce(std::span<const E> entities, bool force = true) {
	std::ranges::for_each(entities, [&](const E& entity) { StopBounce(entity, force); });
}

/// @brief Applies a continuous shake effect to the specified entity.
///
/// @param entity The entity to apply the shake effect to.
/// @param intensity The intensity of the shake, in the range [-1, 1] (negative values reduce any
/// existing shake trauma).
/// @param duration The total duration of the shake effect. If nullopt, the shake continues until
/// StopShake is called.
/// @param config Configuration parameters for the shake behavior.
/// @param ease The easing function to use for the shake. If Ease::None, shake remains at
/// full intensity for the entire time.
/// @param force If true, overrides any existing shake effect.
/// @param reset_trauma If true, resets the trauma immediately upon completing the final queued
/// shake effect.
Tween Shake(
	Entity entity, float intensity, std::optional<milliseconds> duration,
	const ShakeConfig& config = {}, Ease ease = Ease::None, bool force = true,
	bool reset_trauma = false
);

template <EntityType E>
std::vector<Tween> Shake(
	std::span<const E> entities, float intensity, milliseconds duration,
	const ShakeConfig& config = {}, Ease ease = Ease::None, bool force = true,
	bool reset_trauma = false
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return Shake(entity, intensity, duration, config, ease, force, reset_trauma);
	});
}

/// @brief Applies a continuous constant shake of a given intensity to the specified entity.
///
/// @param entity The entity to apply the shake effect to.
/// @param intensity The intensity of the shake, in the range [-1, 1] (negative values reduce any
/// existing shake trauma).
/// @param duration The total duration of the shake effect. If nullopt, the shake continues until
/// StopShake is called.
/// @param config Configuration parameters for the shake behavior.
/// @param force If true, overrides any existing shake effect.
/// @param reset_trauma If true, resets the trauma immediately upon completing the final queued
/// shake effect.
Tween Shake(
	Entity entity, float intensity, std::optional<milliseconds> duration,
	const ShakeConfig& config = {}, bool force = true, bool reset_trauma = false
);

template <EntityType E>
std::vector<Tween> Shake(
	std::span<const E> entities, float intensity, milliseconds duration,
	const ShakeConfig& config = {}, bool force = true, bool reset_trauma = false
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return Shake(entity, intensity, duration, config, Ease::None, force, reset_trauma);
	});
}

/// @brief Applies an instantenous shake effect to the specified entity.
///
/// @param entity The entity to apply the shake effect to.
/// @param intensity The intensity of the shake, in the range [-1, 1] (negative values reduce any
/// existing shake trauma).
/// @param config Configuration parameters for the shake behavior.
/// @param force If true, overrides any existing shake effect.
Tween Shake(Entity entity, float intensity, const ShakeConfig& config = {}, bool force = true);

template <EntityType E>
std::vector<Tween> Shake(
	std::span<const E> entities, float intensity, const ShakeConfig& config = {}, bool force = true
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return Shake(entity, intensity, 0ms, config, Ease::None, force, false);
	});
}

/// @brief Stops any ongoing shake effect on the specified entity.
///
/// @param entity The entity whose shake effect should be stopped.
/// @param force If true, clears all queued or active shake effects.
void StopShake(Entity entity, bool force = true);

template <EntityType E>
void StopShake(std::span<const E> entities, bool force = true) {
	std::ranges::for_each(entities, [&](const E& entity) { StopShake(entity, force); });
}

/// @brief Starts a follow behavior where one entity follows another based on the specified
/// configuration.
///
/// @param entity The entity that will follow the target.
/// @param target The entity to be followed.
/// @param config The configuration parameters that define how the follow behavior should operate.
/// @param force If true, forces the replacement of any existing follow behavior on the entity.
Tween StartFollow(
	Entity entity, Entity target, const TargetFollowConfig& config = {}, bool force = true
);

template <EntityType E>
std::vector<Tween> StartFollow(
	std::span<const E> entities, Entity target, const TargetFollowConfig& config = {},
	bool force = true
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return StartFollow(entity, target, config, force);
	});
}

/// @brief Starts a follow behavior where the entity follows a path of waypoints based on the
/// specified configuration.
///
/// @param entity The entity that will follow the target.
/// @param waypoints The set of waypoints the entity will visit during the follow.
/// @param config The configuration parameters that define how the follow behavior should operate.
/// @param force If true, forces the replacement of any existing follow behavior on the entity.
/// @param reset_waypoint_index If true, resets the waypoint index to 0. If false, continues where
/// it started as long as waypoints have not changed or the end has not been reached (if
/// config.loop_path is false).
Tween StartFollow(
	Entity entity, std::span<const V2_float> waypoints, const PathFollowConfig& config = {},
	bool force = true, bool reset_waypoint_index = false
);

template <EntityType E>
std::vector<Tween> StartFollow(
	std::span<const E> entities, std::span<const V2_float> waypoints,
	const PathFollowConfig& config = {}, bool force = true, bool reset_waypoint_index = false
) {
	return impl::CreateTweens(entities, [&](const E& entity) {
		return StartFollow(entity, waypoints, config, force, reset_waypoint_index);
	});
}

/// @brief Stops any active follow behavior on the specified entity.
///
/// @param entity The entity whose follow behavior should be stopped.
/// @param force If true, clears all queued follows effects.
/// @param reset_previous_waypoints If true, resets the previously set waypoints. If false, a new
/// follow will continue where it started as long as waypoints have not changed or the end has not
/// been reached (if config.loop_path is false).
void StopFollow(Entity entity, bool force = true, bool reset_previous_waypoints = false);

template <EntityType E>
void StopFollow(
	std::span<const E> entities, bool force = true, bool reset_previous_waypoints = false
) {
	std::ranges::for_each(entities, [&](const E& entity) {
		StopFollow(entity, force, reset_previous_waypoints);
	});
}

} // namespace ptgn