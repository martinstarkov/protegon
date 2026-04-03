#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/math/angle.h"
#include "core/math/easing.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/util/concepts.h"
#include "renderer/primitives/color.h"
#include "runtime/animation/follow_config.h"
#include "runtime/animation/shake_config.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/text.h"
#include "serialization/json/serialize.h"

namespace ptgn {

class Scene;

namespace impl {

struct Offsets;

template <typename T>
struct Effect {
	Effect() = default;

	explicit Effect(const T& start) : start{ start } {}

	T start{};

	bool operator==(const Effect&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Effect, start)
};

struct TranslateEffect : public Effect<V2_float> {};

struct RotateEffect : public Effect<Radians> {};

struct ScaleEffect : public Effect<V2_float> {};

struct TintEffect : public Effect<Color> {};

struct FollowEffect {
	FollowEffect() = default;

	std::size_t current_waypoint{ 0 };

	// Cache for comparing when a waypoint path changes.
	std::vector<V2_float> waypoints;

	bool operator==(const FollowEffect&) const = default;

	// TODO: Fix serialization.
	// PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(FollowEffect, current_waypoint, waypoints)
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
	// PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(ShakeEffect, trauma, previous_target)
};

template <typename TComponent>
struct EffectObject : public GameObject<> {
	using GameObject<>::GameObject;
};

} // namespace impl

template <typename TComponent>
Tween GetTween(Entity entity) {
	Tween tween;

	if (!entity.Has<impl::EffectObject<TComponent>>()) {
		impl::EffectObject<TComponent> obj{ CreateTween(entity.GetScene()) };
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
	PTGN_ASSERT(duration > milliseconds{ 0 }, "Tween effect must have a positive duration");

	auto tween{ GetTween<TComponent>(entity) };

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

void ApplyShake(
	milliseconds time, Offsets& offsets, float trauma, const ShakeConfig& config, std::int32_t seed
);

V2_float GetFollowPosition(
	secondsf dt, const FollowConfig& config, V2_float position, V2_float target_position
);

void VelocityModeMoveImpl(const FollowConfig& config, Entity parent, V2_float dir);

void TargetFollowImpl(Entity target, const TargetFollowConfig& config, Entity tween_entity);

void PathFollowImpl(
	const std::vector<V2_float>& waypoints, const PathFollowConfig& config, Entity tween_entity
);

void EntityFollowStopImpl(Entity e);

Tween StartFollowImpl(
	Entity entity, bool force, const TweenCallback& start_func,
	const std::function<void(Entity, float)>& update_func
);

void EntityFollowStartImpl(Entity parent, const FollowConfig& config);

Tween StartFollowPathImpl(
	Entity entity, const std::vector<V2_float>& waypoints, const PathFollowConfig& config = {},
	bool force = true, bool reset_waypoint_index = false
);

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
	PTGN_ASSERT(duration > milliseconds{ 0 }, "Tween must have a positive duration");

	auto tween{ GetTween<TComponent>(entity) };

	if (force || tween.IsCompleted()) {
		tween.Clear();
	}

	// Store start value inside the tween
	auto start = std::make_shared<T>();

	auto update_start = [start, property](Entity e) mutable {
		Entity parent{ GetParent(e) };
		*start = property.get(parent);
	};

	tween.During(duration)
		.Ease(ease)
		.OnStart(update_start)
		.OnProgress([start, property, target](Entity e, float progress) mutable {
			auto result = Lerp(*start, target, progress);
			Entity parent{ GetParent(e) };
			property.set(parent, result);
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
	const std::vector<E>& entities, const std::variant<T, std::vector<T>>& target,
	milliseconds duration, Ease ease, TweenProperty<T> property, bool force = true
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());
	std::visit(
		[&]<typename TType>(const TType& target_value) {
			if constexpr (std::is_same_v<TType, T>) {
				for (const auto& entity : entities) {
					tweens.emplace_back(TweenTo<TComponent, T>(
						entity, target_value, duration, ease, property, force
					));
				}
			} else if constexpr (std::is_same_v<TType, std::vector<T>>) {
				PTGN_ASSERT(
					target_value.size() == entities.size(),
					"Target vector size must match entities size"
				);
				for (std::size_t i = 0; i < entities.size(); ++i) {
					tweens.emplace_back(TweenTo<TComponent, T>(
						entities[i], target_value[i], duration, ease, property, force
					));
				}
			} else {
				static_assert(false, "Unsupported target type for TweenTo");
			}
		},
		target
	);
	return tweens;
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

template <EntityType E>
std::vector<Tween> TranslateTo(
	const std::vector<E>& entities,
	const std::variant<V2_float, std::vector<V2_float>>& target_position, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());

	std::visit(
		[&]<typename TType>(const TType& target_value) {
			if constexpr (std::is_same_v<TType, V2_float>) {
				for (const auto& entity : entities) {
					tweens.emplace_back(TranslateTo(entity, target_value, duration, ease, force));
				}
			} else if constexpr (std::is_same_v<TType, std::vector<V2_float>>) {
				PTGN_ASSERT(
					target_value.size() == entities.size(),
					"Target vector size must match entities size"
				);
				for (std::size_t i = 0; i < entities.size(); ++i) {
					tweens.emplace_back(
						TranslateTo(entities[i], target_value[i], duration, ease, force)
					);
				}
			} else {
				static_assert(false, "Unsupported target type");
			}
		},
		target_position
	);

	return tweens;
}

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

template <EntityType E>
std::vector<Tween> RotateTo(
	const std::vector<E>& entities, const std::variant<Degrees, std::vector<Degrees>>& target_angle,
	milliseconds duration, Ease ease = Ease::Linear, bool force = true
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());

	std::visit(
		[&]<typename TType>(const TType& target_value) {
			if constexpr (std::is_same_v<TType, Degrees>) {
				for (const auto& entity : entities) {
					tweens.emplace_back(RotateTo(entity, target_value, duration, ease, force));
				}
			} else if constexpr (std::is_same_v<TType, std::vector<Degrees>>) {
				PTGN_ASSERT(
					target_value.size() == entities.size(),
					"Target vector size must match entities size"
				);
				for (std::size_t i = 0; i < entities.size(); ++i) {
					tweens.emplace_back(
						RotateTo(entities[i], target_value[i], duration, ease, force)
					);
				}
			} else {
				static_assert(false, "Unsupported target type");
			}
		},
		target_angle
	);

	return tweens;
}

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

template <EntityType E>
std::vector<Tween> ScaleTo(
	const std::vector<E>& entities,
	const std::variant<V2_float, std::vector<V2_float>>& target_scale, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());

	std::visit(
		[&]<typename TType>(const TType& target_value) {
			if constexpr (std::is_same_v<TType, V2_float>) {
				for (const auto& entity : entities) {
					tweens.emplace_back(ScaleTo(entity, target_value, duration, ease, force));
				}
			} else if constexpr (std::is_same_v<TType, std::vector<V2_float>>) {
				PTGN_ASSERT(
					target_value.size() == entities.size(),
					"Target vector size must match entities size"
				);
				for (std::size_t i = 0; i < entities.size(); ++i) {
					tweens.emplace_back(ScaleTo(entities[i], target_value[i], duration, ease, force)
					);
				}
			} else {
				static_assert(false, "Unsupported target type");
			}
		},
		target_scale
	);

	return tweens;
}

Tween ScaleTextSize(
	Text entity, float target_font_size, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true
);

std::vector<Tween> ScaleTextSize(
	const std::vector<Text>& entities,
	const std::variant<float, std::vector<float>>& target_font_size, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true
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
std::vector<Tween> TintTo(
	const std::vector<E>& entities, const std::variant<Color, std::vector<Color>>& target_tint,
	milliseconds duration, Ease ease = Ease::Linear, bool force = true
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());

	std::visit(
		[&]<typename TType>(const TType& target_value) {
			if constexpr (std::is_same_v<TType, Color>) {
				for (const auto& entity : entities) {
					tweens.emplace_back(TintTo(entity, target_value, duration, ease, force));
				}
			} else if constexpr (std::is_same_v<TType, std::vector<Color>>) {
				PTGN_ASSERT(
					target_value.size() == entities.size(),
					"Target vector size must match entities size"
				);
				for (std::size_t i = 0; i < entities.size(); ++i) {
					tweens.emplace_back(TintTo(entities[i], target_value[i], duration, ease, force)
					);
				}
			} else {
				static_assert(false, "Unsupported target type");
			}
		},
		target_tint
	);

	return tweens;
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

template <EntityType E>
std::vector<Tween> FadeIn(
	const std::vector<E>& entities, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true, bool start_transparent = false
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());
	for (const auto& entity : entities) {
		tweens.emplace_back(FadeIn(entity, duration, ease, force, start_transparent));
	}
	return tweens;
}

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

template <EntityType E>
std::vector<Tween> FadeOut(
	const std::vector<E>& entities, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true, bool start_opaque = false
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());
	for (const auto& entity : entities) {
		tweens.emplace_back(FadeOut(entity, duration, ease, force, start_opaque));
	}
	return tweens;
}

/// @brief Applies a bouncing motion to the specified entity.
///
/// The bounce starts at the entity position (or previously queued bounce end point), approaches the
/// amplitude offset and then returns back to the origin point all within a single duration and can
/// repeat a specified number of times or indefinitely.
///
/// @param entity The entity to apply the bounce effect to.
/// @param bounce_amplitude The peak offset applied during the bounce.
/// @param duration The duration of one bounce cycle (e.g., up and down).
/// @param total_periods Number of up and down bounce cycles. If -1, bounce continues indefinitely
/// until StopBounce is called.
/// @param ease The easing function to use for the bounce.
/// @param static_offset A constant offset added to the entity's position throughout the bounce.
/// @param force If true, overrides any existing bounce effect on the entity.
Tween Bounce(
	Entity entity, V2_float bounce_amplitude, milliseconds duration,
	std::int64_t total_periods = -1, Ease ease = Ease::Linear, V2_float static_offset = {},
	bool force = true
);

template <EntityType E>
std::vector<Tween> Bounce(
	const std::vector<E>& entities, V2_float bounce_amplitude, milliseconds duration,
	std::int64_t total_periods = -1, Ease ease = Ease::Linear, V2_float static_offset = {},
	bool force = true
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());
	for (const auto& entity : entities) {
		tweens.emplace_back(
			Bounce(entity, bounce_amplitude, duration, total_periods, ease, static_offset, force)
		);
	}
	return tweens;
}

/// @brief Applies a symmetrical bouncing motion to the specified entity.
///
/// Similar to a regular bounce, the symmetrical bounce starts at the entity position (or previously
/// queued bounce end point), approaches the amplitude offset and then goes to a negative amplitude
/// offset before returning back to the origin point all within a single duration and can repeat a
/// specified number of times or indefinitely. As a result, a symmetrical bounce requires a
/// symmetrical easing function. Note: Symmetrical bounces occupy the same effect queue as regular
/// bounces, i.e. they can not occur at the same time for the same entity.
///
/// @param entity The entity to apply the bounce effect to.
/// @param bounce_amplitude The peak offset applied during the bounce.
/// @param duration The duration of one bounce cycle (e.g., up and down).
/// @param total_periods Number of up and down bounce cycles. If -1, bounce continues indefinitely
/// until StopBounce is called.
/// @param ease The symmetrical easing function to use for the bounce.
/// @param static_offset A constant offset added to the entity's position throughout the bounce.
/// @param force If true, overrides any existing bounce effect on the entity.
Tween SymmetricalBounce(
	Entity entity, V2_float bounce_amplitude, milliseconds duration,
	std::int64_t total_periods = -1, Ease ease = Ease::Linear, V2_float static_offset = {},
	bool force = true
);

template <EntityType E>
std::vector<Tween> SymmetricalBounce(
	const std::vector<E>& entities, V2_float bounce_amplitude, milliseconds duration,
	std::int64_t total_periods = -1, Ease ease = Ease::Linear, V2_float static_offset = {},
	bool force = true
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());
	for (const auto& entity : entities) {
		tweens.emplace_back(SymmetricalBounce(
			entity, bounce_amplitude, duration, total_periods, ease, static_offset, force
		));
	}
	return tweens;
}

/// @brief Stops the current bounce tween and proceeds to the next one in the queue.
///
/// @param entity The entity whose bounce animation should be stopped.
/// @param force If true, clears the entire bounce queue instead of just the current tween.
void StopBounce(Entity entity, bool force = true);

template <EntityType E>
void StopBounce(const std::vector<E>& entities, bool force = true) {
	for (const auto& entity : entities) {
		StopBounce(entity, force);
	}
}

/// @brief Applies a continuous shake effect to the specified entity.
///
/// @param entity The entity to apply the shake effect to.
/// @param intensity The intensity of the shake, in the range [-1, 1] (negative values reduce any
/// existing shake trauma).
/// @param duration The total duration of the shake effect. If -1, the shake continues until
/// StopShake is called.
/// @param config Configuration parameters for the shake behavior.
/// @param ease The easing function to use for the shake. If Ease::None, shake remains at
/// full intensity for the entire time.
/// @param force If true, overrides any existing shake effect.
/// @param reset_trauma If true, resets the trauma immediately upon completing the final queued
/// shake effect.
Tween Shake(
	Entity entity, float intensity, milliseconds duration, const ShakeConfig& config = {},
	Ease ease = Ease::None, bool force = true, bool reset_trauma = false
);

template <EntityType E>
std::vector<Tween> Shake(
	const std::vector<E>& entities, float intensity, milliseconds duration,
	const ShakeConfig& config = {}, Ease ease = Ease::None, bool force = true,
	bool reset_trauma = false
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());
	for (const auto& entity : entities) {
		tweens.emplace_back(Shake(entity, intensity, duration, config, ease, force, reset_trauma));
	}
	return tweens;
}

/// @brief Applies a continuous constant shake of a given intensity to the specified entity.
///
/// @param entity The entity to apply the shake effect to.
/// @param intensity The intensity of the shake, in the range [-1, 1] (negative values reduce any
/// existing shake trauma).
/// @param duration The total duration of the shake effect. If -1, the shake continues until
/// StopShake is called.
/// @param config Configuration parameters for the shake behavior.
/// @param force If true, overrides any existing shake effect.
/// @param reset_trauma If true, resets the trauma immediately upon completing the final queued
/// shake effect.
Tween Shake(
	Entity entity, float intensity, milliseconds duration, const ShakeConfig& config = {},
	bool force = true, bool reset_trauma = false
);

template <EntityType E>
std::vector<Tween> Shake(
	const std::vector<E>& entities, float intensity, milliseconds duration,
	const ShakeConfig& config = {}, bool force = true, bool reset_trauma = false
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());
	for (const auto& entity : entities) {
		tweens.emplace_back(
			Shake(entity, intensity, duration, config, Ease::None, force, reset_trauma)
		);
	}
	return tweens;
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
	const std::vector<E>& entities, float intensity, const ShakeConfig& config = {},
	bool force = true
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());
	for (const auto& entity : entities) {
		tweens.emplace_back(
			Shake(entity, intensity, milliseconds{ 0 }, config, Ease::None, force, false)
		);
	}
	return tweens;
}

/// @brief Stops any ongoing shake effect on the specified entity.
///
/// @param entity The entity whose shake effect should be stopped.
/// @param force If true, clears all queued or active shake effects.
void StopShake(Entity entity, bool force = true);

template <EntityType E>
void StopShake(const std::vector<E>& entities, bool force = true) {
	for (const auto& entity : entities) {
		StopShake(entity, force);
	}
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
	const std::vector<E>& entities, Entity target, const TargetFollowConfig& config = {},
	bool force = true
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());
	for (const auto& entity : entities) {
		tweens.emplace_back(StartFollow(entity, target, config, force));
	}
	return tweens;
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
	Entity entity, const std::vector<V2_float>& waypoints, const PathFollowConfig& config = {},
	bool force = true, bool reset_waypoint_index = false
);

template <EntityType E>
std::vector<Tween> StartFollow(
	const std::vector<E>& entities, const std::vector<V2_float>& waypoints,
	const PathFollowConfig& config = {}, bool force = true, bool reset_waypoint_index = false
) {
	std::vector<Tween> tweens;
	tweens.reserve(entities.size());
	for (const auto& entity : entities) {
		tweens.emplace_back(StartFollow(entity, waypoints, config, force, reset_waypoint_index));
	}
	return tweens;
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
	const std::vector<E>& entities, bool force = true, bool reset_previous_waypoints = false
) {
	for (const auto& entity : entities) {
		StopFollow(entity, force, reset_previous_waypoints);
	}
}

} // namespace ptgn