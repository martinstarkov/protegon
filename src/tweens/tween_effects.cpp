#include "tweens/tween_effects.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/movement.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/entity_hierarchy.h"
#include "core/game.h"
#include "core/time.h"
#include "math/easing.h"
#include "math/math.h"
#include "math/noise.h"
#include "math/rng.h"
#include "math/tolerance.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "renderer/api/color.h"
#include "tweens/follow_config.h"
#include "tweens/shake_config.h"
#include "tweens/tween.h"

namespace ptgn {

namespace impl {

void ApplyShake(Offsets& offsets, float trauma, const ShakeConfig& config, std::int32_t seed) {
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

static EffectObject<BounceEffect>& BounceImpl(
	Entity& entity, const V2_float& amplitude, milliseconds duration, std::int64_t total_periods,
	const Ease& ease, const V2_float& static_offset, bool force, bool symmetrical
) {
	PTGN_ASSERT(duration > milliseconds{ 0 }, "Tween effect must have a positive duration");

	auto& tween{ GetTween<BounceEffect>(entity) };

	entity.TryAdd<Offsets>();

	if (force || tween.IsCompleted()) {
		tween.Clear();
	}

	auto reset_bounce = [](auto e) mutable {
		Entity parent{ GetParent(e) };
		auto& offsets{ parent.Get<Offsets>() };
		offsets.bounce = {};
	};

	tween.During(duration)
		.Ease(ease)
		.OnStart(reset_bounce)
		.Repeat(total_periods)
		.OnProgress([amplitude, static_offset, symmetrical](Entity e, float) mutable {
			Tween tween_entity{ e };

			float linear_progress{ tween_entity.GetLinearProgress() };
			auto current_ease{ tween_entity.GetEase() };

			float t{ ApplyBounceEase(linear_progress, symmetrical, current_ease) };

			Entity parent{ GetParent(e) };

			auto& offsets{ parent.Get<Offsets>() };

			offsets.bounce.SetPosition(static_offset + amplitude * t);
		})
		.OnPointComplete(reset_bounce)
		.OnComplete(reset_bounce)
		.OnStop(reset_bounce)
		.OnReset(reset_bounce);
	tween.Start(force);
	return tween;
}

V2_float GetFollowPosition(
	const FollowConfig& config, const V2_float& position, const V2_float& target_position
) {
	PTGN_ASSERT(config.lerp.x >= 0.0f && config.lerp.x <= 1.0f);
	PTGN_ASSERT(config.lerp.y >= 0.0f && config.lerp.y <= 1.0f);

	V2_float lerp{ 1.0f - std::pow(1.0f - config.lerp.x, game.dt()),
				   1.0f - std::pow(1.0f - config.lerp.y, game.dt()) };

	V2_float new_pos{ position };

	if (config.deadzone.IsZero()) {
		new_pos = Lerp(position, target_position, lerp);
	} else {
		// TODO: Consider adding a custom deadzone origin in the future.
		V2_float deadzone_half{ config.deadzone * 0.5f };

		V2_float min{ target_position - deadzone_half };
		V2_float max{ target_position + deadzone_half };

		if (position.x < min.x) {
			new_pos.x = Lerp(position.x, position.x - (min.x - target_position.x), lerp.x);
		} else if (position.x > max.x) {
			new_pos.x = Lerp(position.x, position.x + (target_position.x - max.x), lerp.x);
		}
		if (position.y < min.y) {
			new_pos.y = Lerp(position.y, position.y - (min.y - target_position.y), lerp.y);
		} else if (position.y > max.y) {
			new_pos.y = Lerp(position.y, position.y + (target_position.y - max.y), lerp.y);
		}
	}

	if (!config.follow_x) {
		new_pos.x = position.x;
	}
	if (!config.follow_y) {
		new_pos.y = position.y;
	}

	return new_pos;
}

void VelocityModeMoveImpl(const FollowConfig& config, Entity& parent, const V2_float& dir) {
	PTGN_ASSERT(
		parent.Has<TopDownMovement>(),
		"Entity with MoveMode::Velocity must have a TopDownMovement component"
	);
	auto& movement{ parent.Get<TopDownMovement>() };

	auto dist2{ dir.MagnitudeSquared() };

	if (config.stop_distance >= epsilon<float> &&
		dist2 < config.stop_distance * config.stop_distance) {
		return;
	}

	if (NearlyEqual(dist2, 0.0f)) {
		return;
	}

	auto norm_dir{ dir / std::sqrt(dist2) };
	if (!config.follow_x) {
		norm_dir = { 0.0f, Sign(norm_dir.y) };
	}
	if (!config.follow_y) {
		norm_dir = { Sign(norm_dir.x), 0.0f };
	}
	movement.Move(norm_dir);
}

void EntityFollowStopImpl(Entity e) {
	Entity parent{ GetParent(e) };
	parent.template Remove<TopDownMovement>();
	parent.template Remove<RigidBody>();
}

} // namespace impl

impl::EffectObject<impl::TintEffect>& TintTo(
	Entity& entity, const Color& target_tint, milliseconds duration, const Ease& ease, bool force
) {
	return impl::AddTweenEffect<impl::TintEffect, Color>(
		entity, target_tint, duration, ease, force, [](Entity e) { return GetTint(e); },
		[](Entity e, Color v) { SetTint(e, v); }
	);
}

impl::EffectObject<impl::TintEffect>& FadeIn(
	Entity& entity, milliseconds duration, const Ease& ease, bool force
) {
	return TintTo(entity, color::White, duration, ease, force);
}

impl::EffectObject<impl::TintEffect>& FadeOut(
	Entity& entity, milliseconds duration, const Ease& ease, bool force
) {
	return TintTo(entity, color::Transparent, duration, ease, force);
}

impl::EffectObject<impl::BounceEffect>& Bounce(
	Entity& entity, const V2_float& amplitude, milliseconds duration, std::int64_t total_periods,
	const Ease& ease, const V2_float& static_offset, bool force
) {
	return impl::BounceImpl(
		entity, amplitude, duration, total_periods, ease, static_offset, force, false
	);
}

impl::EffectObject<impl::BounceEffect>& SymmetricalBounce(
	Entity& entity, const V2_float& amplitude, milliseconds duration, std::int64_t total_periods,
	SymmetricalEase ease, const V2_float& static_offset, bool force
) {
	return impl::BounceImpl(
		entity, amplitude, duration, total_periods, ease, static_offset, force, true
	);
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

impl::EffectObject<impl::ShakeEffect>& Shake(
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
		auto& shake{ e.template Get<impl::ShakeEffect>() };
		shake.trauma = previous_target;
	};

	auto update_stop = [](auto e) {
		Entity parent{ GetParent(e) };
		auto& offsets{ parent.template Get<impl::Offsets>() };
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
			return tween;
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
	return tween;
}

impl::EffectObject<impl::ShakeEffect>& Shake(
	Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config, bool force,
	bool reset_trauma
) {
	return Shake(entity, intensity, duration, config, SymmetricalEase::None, force, reset_trauma);
}

impl::EffectObject<impl::ShakeEffect>& Shake(
	Entity& entity, float intensity, const ShakeConfig& config, bool force
) {
	return Shake(entity, intensity, milliseconds{ 0 }, config, SymmetricalEase::None, force, false);
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

void StopFollow(Entity& entity, bool force, bool reset_previous_waypoints) {
	if (!entity.Has<impl::EffectObject<impl::FollowEffect>>()) {
		return;
	}
	auto& tween{ entity.Get<impl::EffectObject<impl::FollowEffect>>() };

	if (force || tween.IsCompleted()) {
		tween.Clear();
		impl::EntityFollowStopImpl(tween);
		if (reset_previous_waypoints) {
			entity.Remove<impl::EffectObject<impl::FollowEffect>>();
		}
	} else {
		tween.IncrementPoint();
	}
}

} // namespace ptgn