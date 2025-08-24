#include "tweens/tween_effects.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <variant>

#include "common/assert.h"
#include "components/draw.h"
#include "components/movement.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/entity_hierarchy.h"
#include "core/game.h"
#include "core/time.h"
#include "debug/log.h"
#include "math/easing.h"
#include "math/math.h"
#include "math/noise.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "renderer/api/color.h"
#include "tweens/shake_config.h"
#include "tweens/tween.h"

namespace ptgn {

namespace impl {

static float ApplyBounceEase(float t, bool symmetrical, const Ease& ease) {
	if (!symmetrical) {
		// Standard up-down bounce.

		// Triangle wave with y=1.0 peak at t=0.5.
		float triangle_t{ TriangleWave(t, 2.0f, 0.25f) };
		float eased_t{ ptgn::ApplyEase(triangle_t, ease) };
		return eased_t;
	}

	// Symmetrical bounce.

	PTGN_ASSERT(
		std::holds_alternative<SymmetricalEase>(ease),
		"Symmetrical bounces only support symmetrical easing functions"
	);

	// In essence this is a piece wise triangle wave function which rises from 0.5 to 1.0 in the
	// domain [0, 0.25], falls from 1.0 to 0.0 in the domain [0.25, 0.75] and rises again from 0.0
	// to 0.5 in the domain [0.75, 1.0].
	float triangle_t{ 0.0f };
	if (t < 0.25f) {
		triangle_t = 1.0f + (2.0f * t - 0.5f);
	} else if (t > 0.75f) {
		triangle_t = -1.0f + (2.0f * t - 0.5f);
	} else {
		triangle_t = 1.0f - (2.0f * t - 0.5f);
	}

	float eased_t{ ptgn::ApplyEase(triangle_t, ease) };
	// Transform to -1 to 1 range for symmetrical amplitudes.
	return 2.0f * eased_t - 1.0f;
}

void BounceImpl(
	Entity& entity, const V2_float& amplitude, milliseconds duration, std::int64_t total_periods,
	const Ease& ease, const V2_float& static_offset, bool force, bool symmetrical
) {
	PTGN_ASSERT(duration > milliseconds{ 0 }, "Tween effect must have a positive duration");

	auto& tween{ GetTween<impl::BounceEffect>(entity) };

	entity.TryAdd<impl::Offsets>();

	if (force || tween.IsCompleted()) {
		tween.Clear();
	}

	auto reset_bounce = [](auto e) mutable {
		Entity parent{ GetParent(e) };
		auto& offsets{ parent.Get<impl::Offsets>() };
		offsets.bounce = {};
	};

	tween.During(duration)
		.Ease(ease)
		.OnStart(reset_bounce)
		.Repeat(total_periods)
		.OnProgress([amplitude, static_offset, symmetrical](Entity e, float) mutable {
			Tween tween{ e };
			float linear_progress{ tween.GetLinearProgress() };
			auto ease{ tween.GetEase() };
			float t{ ApplyBounceEase(linear_progress, symmetrical, ease) };
			Entity parent{ GetParent(e) };
			auto& offsets{ parent.Get<impl::Offsets>() };
			offsets.bounce.SetPosition(static_offset + amplitude * t);
		})
		.OnPointComplete(reset_bounce)
		.OnComplete(reset_bounce)
		.OnStop(reset_bounce)
		.OnReset(reset_bounce);
	tween.Start(force);
}

} // namespace impl

void TranslateTo(
	Entity& entity, const V2_float& target_position, milliseconds duration, const Ease& ease,
	bool force
) {
	impl::AddTweenEffect<impl::TranslateEffect, V2_float>(
		entity, target_position, duration, ease, force, [](Entity e) { return GetPosition(e); },
		[](Entity e, V2_float v) { SetPosition(e, v); }
	);
}

void RotateTo(
	Entity& entity, float target_angle, milliseconds duration, const Ease& ease, bool force
) {
	impl::AddTweenEffect<impl::RotateEffect, float>(
		entity, target_angle, duration, ease, force, [](Entity e) { return GetRotation(e); },
		[](Entity e, float v) { SetRotation(e, v); }
	);
}

void ScaleTo(
	Entity& entity, const V2_float& target_scale, milliseconds duration, const Ease& ease,
	bool force
) {
	impl::AddTweenEffect<impl::ScaleEffect, V2_float>(
		entity, target_scale, duration, ease, force, [](Entity e) { return GetScale(e); },
		[](Entity e, V2_float v) { SetScale(e, v); }
	);
}

void TintTo(
	Entity& entity, const Color& target_tint, milliseconds duration, const Ease& ease, bool force
) {
	impl::AddTweenEffect<impl::TintEffect, Color>(
		entity, target_tint, duration, ease, force, [](Entity e) { return GetTint(e); },
		[](Entity e, Color v) { SetTint(e, v); }
	);
}

void FadeIn(Entity& entity, milliseconds duration, const Ease& ease, bool force) {
	return TintTo(entity, color::White, duration, ease, force);
}

void FadeOut(Entity& entity, milliseconds duration, const Ease& ease, bool force) {
	return TintTo(entity, color::Transparent, duration, ease, force);
}

void Bounce(
	Entity& entity, const V2_float& amplitude, milliseconds duration, std::int64_t total_periods,
	const Ease& ease, const V2_float& static_offset, bool force
) {
	impl::BounceImpl(entity, amplitude, duration, total_periods, ease, static_offset, force, false);
}

void SymmetricalBounce(
	Entity& entity, const V2_float& amplitude, milliseconds duration, std::int64_t total_periods,
	SymmetricalEase ease, const V2_float& static_offset, bool force
) {
	impl::BounceImpl(entity, amplitude, duration, total_periods, ease, static_offset, force, true);
}

void StopBounce(Entity& entity, bool force) {
	if (!entity.Has<impl::EffectObject<impl::BounceEffect>>()) {
		return;
	}
	auto& tween{ entity.Get<impl::EffectObject<impl::BounceEffect>>() };

	auto& offsets{ entity.Get<impl::Offsets>() };
	offsets.bounce = {}; // or reset to bounce.static_offset

	if (force || tween.IsCompleted()) {
		tween.Clear();
		entity.Remove<impl::EffectObject<impl::BounceEffect>>();
	} else {
		tween.IncrementPoint();
	}
}

static void ApplyShake(
	impl::Offsets& offsets, float trauma, const ShakeConfig& config, std::int32_t seed
) {
	// Shake algorithm based on: https://roystan.net/articles/camera-shake/

	// Taking trauma to an exponent allows the ability to smoothen
	// out the transition from shaking to being static.
	float shake_value{ std::pow(trauma, config.trauma_exponent) };

	float x{ game.time() * config.frequency };

	V2_float position_noise{ PerlinNoise::GetValue(x, 0.0f, seed + 0) * 2.0f - 1.0f,
							 PerlinNoise::GetValue(x, 0.0f, seed + 1) * 2.0f - 1.0f };

	float rotation_noise{ PerlinNoise::GetValue(x, 0.0f, seed + 3) * 2.0f - 1.0f };

	offsets.shake.SetPosition(shake_value * config.maximum_translation * position_noise);
	offsets.shake.SetRotation(shake_value * config.maximum_rotation * rotation_noise);
}

void Shake(
	Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config,
	const Ease& ease, bool force, bool reset_trauma
) {
	PTGN_ASSERT(
		intensity >= -1.0f && intensity <= 1.0f, "Shake intensity must be in range [-1, 1]"
	);

	bool infinite_shake{ duration == milliseconds{ -1 } };

	PTGN_ASSERT(
		duration >= milliseconds{ 0 } || infinite_shake,
		"Shake effect must have a positive duration or be -1 (infinite shake)"
	);

	auto& tween{ impl::GetTween<impl::ShakeEffect>(entity) };
	auto& shake_effect{ tween.TryAdd<impl::ShakeEffect>() };

	float previous_target{ shake_effect.previous_target };

	float target_intensity{ std::clamp(previous_target + intensity, 0.0f, 1.0f) };
	shake_effect.previous_target = target_intensity;

	auto update_start = [previous_target](auto e) {
		auto& shake{ e.Get<impl::ShakeEffect>() };
		shake.trauma = previous_target;
	};

	auto update_stop = [](auto e) {
		Entity parent{ GetParent(e) };
		auto& offsets{ parent.Get<impl::Offsets>() };
		offsets.shake = {};
	};

	entity.TryAdd<impl::Offsets>();

	if (force || tween.IsCompleted()) {
		tween.Clear();
	}

	if (force && tween.GetTweenPointCount()) {
		const auto& last_point{ tween.GetLastTweenPoint() };

		bool instant_tween{ last_point.duration_ == milliseconds{ 0 } };
		bool infinite_tween{ last_point.total_repeats_ == -1 };

		if (instant_tween && infinite_tween) {
			// Skips the previous infinite tween point that reduces trauma.
			tween.RemoveLastTweenPoint();
		}
	}

	if (tween.GetTweenPointCount()) {
		const auto& last_point{ tween.GetLastTweenPoint() };

		bool instant_tween{ last_point.duration_ == milliseconds{ 0 } };
		bool infinite_tween{ last_point.total_repeats_ == -1 };

		if (instant_tween && !infinite_tween) {
			// If a previous instantenous shake exists with 0 duration, add to its trauma instead of
			// queueing a new shake effect.
			shake_effect.trauma = std::clamp(shake_effect.trauma + intensity, 0.0f, 1.0f);
			return;
		}
	}

	auto seed{ RandomNumber<std::int32_t>() };

	const auto shake_func = [seed, config, previous_target,
							 target_intensity](Entity e, float progress) mutable {
		auto& shake{ e.Get<impl::ShakeEffect>() };

		float current_intensity{ Lerp(previous_target, target_intensity, progress) };
		PTGN_ASSERT(current_intensity >= 0.0f && current_intensity <= 1.0f);

		shake.trauma = current_intensity;

		Entity parent{ GetParent(e) };
		auto& offsets{ parent.Get<impl::Offsets>() };

		ApplyShake(offsets, shake.trauma, config, seed);
	};

	if (!infinite_shake) {
		tween.During(duration)
			.Ease(ease)
			.OnStart(update_start)
			.OnProgress(shake_func)
			.OnPointComplete(update_stop)
			.OnComplete(update_stop)
			.OnStop(update_stop)
			.OnReset(update_stop);
	} else {
		tween.During(milliseconds{ 0 })
			.Ease(ease)
			.Repeat(-1)
			.OnStart(update_start)
			.OnProgress(shake_func)
			.OnPointComplete(update_stop)
			.OnComplete(update_stop)
			.OnStop(update_stop)
			.OnReset(update_stop);
	}

	if (!reset_trauma) {
		// Add a infinite tween point that reduces trauma organically.
		tween.During(milliseconds{ 0 }).Repeat(-1).OnProgress([config, seed](Entity e, float) {
			if (!e.Has<impl::ShakeEffect>()) {
				Tween{ e }.IncrementPoint();
				return;
			}
			auto& shake{ e.Get<impl::ShakeEffect>() };
			Entity parent{ GetParent(e) };
			auto& offsets{ parent.Get<impl::Offsets>() };

			shake.trauma = std::clamp(shake.trauma - config.recovery_speed * game.dt(), 0.0f, 1.0f);
			ApplyShake(offsets, shake.trauma, config, seed);

			if (shake.trauma <= 0.0f) {
				Tween{ e }.IncrementPoint();
			}
		});
	}

	tween.Start(force);
}

void Shake(
	Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config, bool force,
	bool reset_trauma
) {
	Shake(entity, intensity, duration, config, SymmetricalEase::None, force, reset_trauma);
}

void Shake(Entity& entity, float intensity, const ShakeConfig& config, bool force) {
	Shake(entity, intensity, milliseconds{ 0 }, config, SymmetricalEase::None, force, false);
}

void StopShake(Entity& entity, bool force) {
	if (!entity.Has<impl::EffectObject<impl::ShakeEffect>>()) {
		return;
	}

	auto& tween{ entity.Get<impl::EffectObject<impl::ShakeEffect>>() };

	auto& shake{ tween.Get<impl::ShakeEffect>() };
	shake.trauma = 0.0f;
	auto& offsets{ entity.Get<impl::Offsets>() };
	offsets.shake = {};

	if (force || tween.IsCompleted()) {
		tween.Clear();
		entity.Remove<impl::EffectObject<impl::ShakeEffect>>();
	} else {
		tween.IncrementPoint();
	}
}

void StartFollow(Entity entity, Entity target, TargetFollowConfig config, bool force) {
	PTGN_ASSERT(config.lerp_factor.x >= 0.0f && config.lerp_factor.x <= 1.0f);
	PTGN_ASSERT(config.lerp_factor.y >= 0.0f && config.lerp_factor.y <= 1.0f);

	impl::EffectObject<impl::FollowEffect>& tween{ impl::GetTween<impl::FollowEffect>(entity) };

	tween.TryAdd<impl::FollowEffect>();

	if (force || tween.IsCompleted()) {
		tween.Clear();
	}

	auto update_func = [config, target](Entity e, float progress) {
		if (!config.follow_x && !config.follow_y) {
			return;
		}

		Tween tween_entity{ e };
		Entity parent{ GetParent(e) };

		auto pos{ GetAbsolutePosition(parent) };

		V2_float target_pos;

		if (!target || !target.IsAlive()) {
			tween_entity.IncrementPoint();
			return;
		}
		target_pos = GetAbsolutePosition(target) + config.offset;

		if (config.move_mode == MoveMode::Velocity) {
			PTGN_ASSERT(
				parent.Has<TopDownMovement>(),
				"Entity with MoveMode::Velocity must have a TopDownMovement component"
			);
			auto& movement{ parent.Get<TopDownMovement>() };
			auto dir{ target_pos - pos };

			auto dist2{ dir.MagnitudeSquared() };

			if (config.stop_distance >= epsilon<float> &&
				dist2 < config.stop_distance * config.stop_distance) {
				tween_entity.IncrementPoint();
				return;
			} else if (!NearlyEqual(dist2, 0.0f)) {
				auto norm_dir{ dir / std::sqrt(dist2) };
				if (!config.follow_x) {
					norm_dir = { 0.0f, Sign(norm_dir.y) };
				}
				if (!config.follow_y) {
					norm_dir = { Sign(norm_dir.x), 0.0f };
				}
				movement.Move(norm_dir);
			}
		} else if (config.move_mode == MoveMode::Lerp) {
			PTGN_ASSERT(config.lerp_factor.x >= 0.0f && config.lerp_factor.x <= 1.0f);
			PTGN_ASSERT(config.lerp_factor.y >= 0.0f && config.lerp_factor.y <= 1.0f);

			V2_float lerp_dt{ 1.0f - std::pow(1.0f - config.lerp_factor.x, game.dt()),
							  1.0f - std::pow(1.0f - config.lerp_factor.y, game.dt()) };

			V2_float new_pos{ pos };

			if (config.deadzone.IsZero()) {
				new_pos = Lerp(pos, target_pos, lerp_dt);
			} else {
				// TODO: Consider adding a custom deadzone origin in the future.
				V2_float deadzone_half{ config.deadzone * 0.5f };

				V2_float min{ target_pos - deadzone_half };
				V2_float max{ target_pos + deadzone_half };

				if (pos.x < min.x) {
					new_pos.x = Lerp(pos.x, pos.x - (min.x - target_pos.x), lerp_dt.x);
				} else if (pos.x > max.x) {
					new_pos.x = Lerp(pos.x, pos.x + (target_pos.x - max.x), lerp_dt.x);
				}
				if (pos.y < min.y) {
					new_pos.y = Lerp(pos.y, pos.y - (min.y - target_pos.y), lerp_dt.y);
				} else if (pos.y > max.y) {
					new_pos.y = Lerp(pos.y, pos.y + (target_pos.y - max.y), lerp_dt.y);
				}
			}

			if (!config.follow_x) {
				new_pos.x = pos.x;
			}
			if (!config.follow_y) {
				new_pos.y = pos.y;
			}

			SetPosition(parent, new_pos);

			if (config.stop_distance < epsilon<float>) {
				return;
			}
			auto dir{ target_pos - new_pos };
			if (auto dist2{ dir.MagnitudeSquared() };
				dist2 >= config.stop_distance * config.stop_distance) {
				return;
			}
			tween_entity.IncrementPoint();
		} else {
			PTGN_ERROR("Unrecognized move mode")
		}
	};

	auto update_start = [config, target](auto e) {
		auto& value{ e.Get<impl::FollowEffect>() };
		value.current_waypoint = 0;
		Entity parent{ GetParent(e) };
		if (config.teleport_on_start) {
			SetPosition(parent, GetPosition(target));
		}
		if (config.move_mode != MoveMode::Velocity) {
			parent.template Remove<TopDownMovement>();
			parent.template Remove<RigidBody>();
			return;
		}
		parent.TryAdd<RigidBody>();
		if (!parent.Has<Transform>()) {
			SetPosition(parent, {});
		}
		auto& movement{ parent.TryAdd<TopDownMovement>() };
		movement.max_acceleration		  = config.max_acceleration;
		movement.max_deceleration		  = config.max_acceleration;
		movement.max_speed				  = config.max_speed;
		movement.keys_enabled			  = false;
		movement.only_orthogonal_movement = false;
	};

	auto update_stop = [](auto e) {
		Entity parent{ GetParent(e) };
		auto& value{ e.Get<impl::FollowEffect>() };
		value.current_waypoint = 0;
		parent.template Remove<TopDownMovement>();
		parent.template Remove<RigidBody>();
	};

	tween.During(milliseconds{ 0 })
		.Repeat(-1)
		.OnStart(update_start)
		.OnProgress(update_func)
		.OnPointComplete(update_stop)
		.OnComplete(update_stop)
		.OnStop(update_stop)
		.OnReset(update_stop);
	tween.Start(force);
}

void StartFollow(
	Entity entity, const std::vector<V2_float>& waypoints, PathFollowConfig config, bool force
) {
	PTGN_ASSERT(!waypoints.empty(), "Cannot follow an empty set of waypoints");

	PTGN_ASSERT(config.lerp_factor.x >= 0.0f && config.lerp_factor.x <= 1.0f);
	PTGN_ASSERT(config.lerp_factor.y >= 0.0f && config.lerp_factor.y <= 1.0f);

	impl::EffectObject<impl::FollowEffect>& tween{ impl::GetTween<impl::FollowEffect>(entity) };

	tween.TryAdd<impl::FollowEffect>();

	if (force || tween.IsCompleted()) {
		tween.Clear();
	}

	auto update_func = [config, waypoints](Entity e, float progress) {
		if (!config.follow_x && !config.follow_y) {
			return;
		}

		Tween tween_entity{ e };
		Entity parent{ GetParent(e) };

		auto pos{ GetAbsolutePosition(parent) };

		V2_float target_pos;

		auto& follow{ e.Get<impl::FollowEffect>() };
		PTGN_ASSERT(
			!waypoints.empty(),
			"Cannot set FollowMode::Path without providing at least one waypoint"
		);
		PTGN_ASSERT(
			config.stop_distance >= epsilon<float>,
			"Stopping distance cannot be negative or 0 when following waypoints"
		);

		PTGN_ASSERT(follow.current_waypoint < waypoints.size());

		target_pos = waypoints[follow.current_waypoint] + config.offset;

		auto dir{ target_pos - pos };

		if (dir.MagnitudeSquared() < config.stop_distance * config.stop_distance) {
			if (follow.current_waypoint + 1 < waypoints.size()) {
				follow.current_waypoint++;
			} else if (config.loop_path) {
				follow.current_waypoint = 0;
			} else {
				tween_entity.IncrementPoint();
				return;
			}
		}

		if (config.move_mode == MoveMode::Velocity) {
			PTGN_ASSERT(
				parent.Has<TopDownMovement>(),
				"Entity with MoveMode::Velocity must have a TopDownMovement component"
			);
			auto& movement{ parent.Get<TopDownMovement>() };
			auto dir{ target_pos - pos };

			auto dist2{ dir.MagnitudeSquared() };

			if (config.stop_distance >= epsilon<float> &&
				dist2 < config.stop_distance * config.stop_distance) {
				tween_entity.IncrementPoint();
				return;
			} else if (!NearlyEqual(dist2, 0.0f)) {
				auto norm_dir{ dir / std::sqrt(dist2) };
				if (!config.follow_x) {
					norm_dir = { 0.0f, Sign(norm_dir.y) };
				}
				if (!config.follow_y) {
					norm_dir = { Sign(norm_dir.x), 0.0f };
				}
				movement.Move(norm_dir);
			}
		} else if (config.move_mode == MoveMode::Lerp) {
			PTGN_ASSERT(config.lerp_factor.x >= 0.0f && config.lerp_factor.x <= 1.0f);
			PTGN_ASSERT(config.lerp_factor.y >= 0.0f && config.lerp_factor.y <= 1.0f);

			V2_float lerp_dt{ 1.0f - std::pow(1.0f - config.lerp_factor.x, game.dt()),
							  1.0f - std::pow(1.0f - config.lerp_factor.y, game.dt()) };

			V2_float new_pos{ pos };

			if (config.deadzone.IsZero()) {
				new_pos = Lerp(pos, target_pos, lerp_dt);
			} else {
				// TODO: Consider adding a custom deadzone origin in the future.
				V2_float deadzone_half{ config.deadzone * 0.5f };

				V2_float min{ target_pos - deadzone_half };
				V2_float max{ target_pos + deadzone_half };

				if (pos.x < min.x) {
					new_pos.x = Lerp(pos.x, pos.x - (min.x - target_pos.x), lerp_dt.x);
				} else if (pos.x > max.x) {
					new_pos.x = Lerp(pos.x, pos.x + (target_pos.x - max.x), lerp_dt.x);
				}
				if (pos.y < min.y) {
					new_pos.y = Lerp(pos.y, pos.y - (min.y - target_pos.y), lerp_dt.y);
				} else if (pos.y > max.y) {
					new_pos.y = Lerp(pos.y, pos.y + (target_pos.y - max.y), lerp_dt.y);
				}
			}

			if (!config.follow_x) {
				new_pos.x = pos.x;
			}
			if (!config.follow_y) {
				new_pos.y = pos.y;
			}

			SetPosition(parent, new_pos);
		} else {
			PTGN_ERROR("Unrecognized move mode")
		}
	};

	auto update_start = [config, waypoints](auto e) {
		auto& value{ e.Get<impl::FollowEffect>() };
		value.current_waypoint = 0;
		Entity parent{ GetParent(e) };
		if (config.teleport_on_start && !waypoints.empty()) {
			V2_float target{ waypoints.back() };
			SetPosition(parent, target);
		}
		if (config.move_mode != MoveMode::Velocity) {
			parent.template Remove<TopDownMovement>();
			parent.template Remove<RigidBody>();
			return;
		}
		parent.TryAdd<RigidBody>();
		if (!parent.Has<Transform>()) {
			SetPosition(parent, {});
		}
		auto& movement{ parent.TryAdd<TopDownMovement>() };
		movement.max_acceleration		  = config.max_acceleration;
		movement.max_deceleration		  = config.max_acceleration;
		movement.max_speed				  = config.max_speed;
		movement.keys_enabled			  = false;
		movement.only_orthogonal_movement = false;
	};

	auto update_stop = [](auto e) {
		Entity parent{ GetParent(e) };
		auto& value{ e.Get<impl::FollowEffect>() };
		value.current_waypoint = 0;
		parent.template Remove<TopDownMovement>();
		parent.template Remove<RigidBody>();
	};

	tween.During(milliseconds{ 0 })
		.Repeat(-1)
		.OnStart(update_start)
		.OnProgress(update_func)
		.OnPointComplete(update_stop)
		.OnComplete(update_stop)
		.OnStop(update_stop)
		.OnReset(update_stop);
	tween.Start(force);
}

void StopFollow(Entity entity, bool force) {
	if (!entity.Has<impl::EffectObject<impl::FollowEffect>>()) {
		return;
	}
	auto& tween{ entity.Get<impl::EffectObject<impl::FollowEffect>>() };

	if (force || tween.IsCompleted()) {
		tween.Clear();
		entity.Remove<impl::EffectObject<impl::FollowEffect>>();
		entity.template Remove<TopDownMovement>();
		entity.template Remove<RigidBody>();
	} else {
		tween.IncrementPoint();
	}
}

} // namespace ptgn