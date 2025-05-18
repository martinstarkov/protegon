#include "tweening/tween_effects.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <variant>

#include "common/assert.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/timer.h"
#include "math/easing.h"
#include "math/math.h"
#include "math/noise.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "rendering/api/color.h"

namespace ptgn {

namespace impl {

ShakeEffectInfo::ShakeEffectInfo(
	float start_intensity, float target_intensity, milliseconds duration, const Ease& ease,
	const ShakeConfig& config, std::int32_t seed
) :
	EffectInfo<float>{ start_intensity, target_intensity, duration, ease } {
	PTGN_ASSERT(
		start_intensity >= 0.0f && start_intensity <= 1.0f,
		"Shake effect intensity must be in range [0.0, 1.0]"
	)
	PTGN_ASSERT(
		target_intensity >= 0.0f && target_intensity <= 1.0f,
		"Shake effect intensity must be in range [0.0, 1.0]"
	);
	this->config = config;
	this->seed	 = seed;
}

template <typename T>
auto GetTaskValue(T& task) {
	PTGN_ASSERT(task.timer.IsRunning());

	float t{ 1.0f };

	if (task.duration >= milliseconds{ 0 }) {
		t = task.timer.template ElapsedPercentage<milliseconds, float>(task.duration);
	}

	float eased_t{ ApplyEase(t, task.ease) };

	return Lerp(task.start_value, task.target_value, eased_t);
}

template <typename T, typename F>
void UpdateTask(T& effect, F& task) {
	if (!task.timer.Completed(task.duration)) {
		return;
	}

	// Task completed.
	effect.tasks.pop_front();

	if (effect.tasks.empty()) {
		return;
	}

	// Start next task.
	effect.tasks.front().timer.Start(true);
}

void TranslateEffectSystem::Update(Manager& manager) const {
	for (auto [entity, effect] : manager.EntitiesWith<TranslateEffect>()) {
		if (effect.tasks.empty()) {
			entity.template Remove<TranslateEffect>();
			return;
		}

		auto& task{ effect.tasks.front() };

		entity.SetPosition(GetTaskValue(task));

		UpdateTask(effect, task);
	}
}

void RotateEffectSystem::Update(Manager& manager) const {
	for (auto [entity, effect] : manager.EntitiesWith<RotateEffect>()) {
		if (effect.tasks.empty()) {
			entity.template Remove<RotateEffect>();
			return;
		}

		auto& task{ effect.tasks.front() };

		entity.SetRotation(GetTaskValue(task));

		UpdateTask(effect, task);
	}
}

void ScaleEffectSystem::Update(Manager& manager) const {
	for (auto [entity, effect] : manager.EntitiesWith<ScaleEffect>()) {
		if (effect.tasks.empty()) {
			entity.template Remove<ScaleEffect>();
			return;
		}

		auto& task{ effect.tasks.front() };

		entity.SetScale(GetTaskValue(task));

		UpdateTask(effect, task);
	}
}

void TintEffectSystem::Update(Manager& manager) const {
	for (auto [entity, effect] : manager.EntitiesWith<TintEffect>()) {
		if (effect.tasks.empty()) {
			entity.template Remove<TintEffect>();
			return;
		}

		auto& task{ effect.tasks.front() };

		entity.SetTint(GetTaskValue(task));

		UpdateTask(effect, task);
	}
}

BounceEffectInfo::BounceEffectInfo(
	const V2_float& amplitude, milliseconds duration, const Ease& ease,
	const V2_float& static_offset, std::int64_t total_periods, bool symmetrical
) :
	amplitude{ amplitude },
	duration{ duration },
	ease{ ease },
	static_offset{ static_offset },
	total_periods{ total_periods },
	symmetrical{ symmetrical } {
	PTGN_ASSERT(
		total_periods == -1 || total_periods > 0,
		"Invalid number of total periods for bounce effect"
	);
}

void BounceEffectSystem::Update(Manager& manager) const {
	for (auto [entity, effect, offsets] : manager.EntitiesWith<BounceEffect, Offsets>()) {
		if (effect.tasks.empty()) {
			offsets.bounce = {};
			entity.template Remove<BounceEffect>();
			continue;
		}

		auto& task{ effect.tasks.front() };

		PTGN_ASSERT(task.timer.IsRunning());

		float t{ task.timer.template ElapsedPercentage<milliseconds, float>(task.duration) };

		float eased_t{ ApplyEase(t, task.symmetrical, task.ease) };

		offsets.bounce.position = task.static_offset + task.amplitude * eased_t;

		if (!task.timer.Completed(task.duration)) {
			continue;
		}

		// Bounce timer completed.

		task.periods_completed++;

		if (task.total_periods == -1 || task.periods_completed < task.total_periods) {
			task.timer.Start(true);
			continue;
		}

		// Bounce repeats completed.

		effect.tasks.pop_front();

		if (effect.tasks.empty()) {
			continue;
		}

		// New bounce effect.

		effect.tasks.front().timer.Start(true);
	}
}

float BounceEffectSystem::ApplyEase(float t, bool symmetrical, const Ease& ease) {
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

void ShakeEffectSystem::Update(Manager& manager, float time, float dt) const {
	for (auto [entity, effect, offsets] : manager.EntitiesWith<ShakeEffect, Offsets>()) {
		if (effect.tasks.empty()) {
			offsets.shake = {};
			entity.template Remove<ShakeEffect>();
			continue;
		}

		auto& task{ effect.tasks.front() };

		bool completed{ task.timer.Completed(task.duration) };

		if (completed) {
			// Shake effect has finished but trauma needs to be decayed (organically).
			task.trauma = std::clamp(task.trauma - task.config.recovery_speed * dt, 0.0f, 1.0f);
		} else {
			// Shake effect is ongoing, update trauma value with intensity.

			float intensity{ GetTaskValue(task) };

			PTGN_ASSERT(intensity >= 0.0f && intensity <= 1.0f);

			task.trauma = intensity;
		}

		// Shake algorithm based on: https://roystan.net/articles/camera-shake/

		// Taking trauma to an exponent allows the ability to smoothen
		// out the transition from shaking to being static.
		float shake{ std::pow(task.trauma, task.config.trauma_exponent) };

		float x{ time * task.config.frequency };

		V2_float position_noise{ PerlinNoise::GetValue(x, 0.0f, task.seed + 0) * 2.0f - 1.0f,
								 PerlinNoise::GetValue(x, 0.0f, task.seed + 1) * 2.0f - 1.0f };

		float rotation_noise{ PerlinNoise::GetValue(x, 0.0f, task.seed + 3) * 2.0f - 1.0f };

		offsets.shake.position = shake * task.config.maximum_translation * position_noise;
		offsets.shake.rotation = shake * task.config.maximum_rotation * rotation_noise;

		if (!completed) {
			// Shake effect has not finished yet.
			continue;
		}

		if (task.duration == milliseconds{ -1 }) {
			// Shake effect is infinite, restart the timer.
			task.timer.Start(true);
			continue;
		}

		if (task.trauma >= 0.0f && effect.tasks.size() == 1) {
			// Shake effect has finished and is the only queued effect, but there is some trauma
			// left to decay.
			continue;
		}

		// Shake effect has finished and all trauma has been decayed (or another queued shake
		// starts).
		effect.tasks.pop_front();

		if (effect.tasks.empty()) {
			// No more shake effects.
			continue;
		}

		// Start next shake effect.
		effect.tasks.front().timer.Start(true);
	}
}

void BounceImpl(
	Entity& entity, const V2_float& amplitude, milliseconds duration, std::int64_t total_periods,
	const Ease& ease, const V2_float& static_offset, bool force, bool symmetrical
) {
	auto& bounce{ entity.GetOrAdd<impl::BounceEffect>() };
	entity.GetOrAdd<impl::Offsets>();

	bool first_task{ force || bounce.tasks.empty() };

	if (first_task) {
		bounce.tasks.clear();
	}

	auto& task{ bounce.tasks.emplace_back(
		amplitude, duration, ease, static_offset, total_periods, symmetrical
	) };

	if (first_task) {
		task.timer.Start(true);
	}
}

} // namespace impl

void TranslateTo(
	Entity& entity, const V2_float& target_position, milliseconds duration, const Ease& ease,
	bool force
) {
	impl::AddTweenEffect<impl::TranslateEffect>(
		entity, target_position, duration, ease, force, entity.GetPosition()
	);
}

void RotateTo(
	Entity& entity, float target_angle, milliseconds duration, const Ease& ease, bool force
) {
	impl::AddTweenEffect<impl::RotateEffect>(
		entity, target_angle, duration, ease, force, entity.GetRotation()
	);
}

void ScaleTo(
	Entity& entity, const V2_float& target_scale, milliseconds duration, const Ease& ease,
	bool force
) {
	impl::AddTweenEffect<impl::ScaleEffect>(
		entity, target_scale, duration, ease, force, entity.GetScale()
	);
}

void TintTo(
	Entity& entity, const Color& target_tint, milliseconds duration, const Ease& ease, bool force
) {
	impl::AddTweenEffect<impl::TintEffect>(
		entity, target_tint, duration, ease, force, entity.GetTint()
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
	if (!entity.Has<impl::BounceEffect>()) {
		return;
	}

	auto& bounce   = entity.Get<impl::BounceEffect>();
	auto& offsets  = entity.Get<impl::Offsets>();
	offsets.bounce = {}; // or reset to bounce.static_offset

	if (force) {
		bounce.tasks.clear();
	} else if (!bounce.tasks.empty()) {
		bounce.tasks.pop_front();
		if (!bounce.tasks.empty()) {
			bounce.tasks.front().timer.Start(true);
		}
	}
}

void Shake(
	Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config,
	const Ease& ease, bool force
) {
	PTGN_ASSERT(
		intensity >= -1.0f && intensity <= 1.0f, "Shake intensity must be in range [-1, 1]"
	);

	auto& comp = entity.GetOrAdd<impl::ShakeEffect>();
	entity.GetOrAdd<impl::Offsets>();

	float start_intensity{ 0.0f };

	bool first_task{ force || comp.tasks.empty() };

	if (first_task) {
		comp.tasks.clear();
	} else {
		start_intensity = comp.tasks.back().target_value;
	}

	RNG<std::int32_t> rng{ std::numeric_limits<std::int32_t>::min(),
						   std::numeric_limits<std::int32_t>::max() };
	std::int32_t seed{ rng() };

	auto& task = comp.tasks.emplace_back(
		start_intensity, std::clamp(start_intensity + intensity, 0.0f, 1.0f), duration, ease,
		config, seed
	);

	task.trauma = start_intensity;

	if (first_task) {
		task.timer.Start(true);
	}
}

void Shake(Entity& entity, float intensity, const ShakeConfig& config, bool force) {
	auto& comp = entity.GetOrAdd<impl::ShakeEffect>();
	entity.GetOrAdd<impl::Offsets>();
	PTGN_ASSERT(
		intensity >= -1.0f && intensity <= 1.0f, "Shake intensity must be in range [-1, 1]"
	);

	float start_intensity{ 0.0f };

	bool first_task{ force || comp.tasks.empty() };

	if (first_task) {
		comp.tasks.clear();
	} else {
		start_intensity = comp.tasks.back().target_value;
	}

	// If a previous instantenous shake exists with 0 duration, add to its trauma instead of
	// queueing a new shake effect.
	if (!comp.tasks.empty()) {
		if (auto& back_task{ comp.tasks.back() }; back_task.duration == milliseconds{ 0 }) {
			back_task.trauma = std::clamp(back_task.trauma + intensity, 0.0f, 1.0f);
			return;
		}
	}

	RNG<std::int32_t> rng{ std::numeric_limits<std::int32_t>::min(),
						   std::numeric_limits<std::int32_t>::max() };
	std::int32_t seed{ rng() };

	auto& task = comp.tasks.emplace_back(
		start_intensity, std::clamp(start_intensity + intensity, 0.0f, 1.0f), milliseconds{ 0 },
		SymmetricalEase::None, config, seed
	);

	task.trauma = intensity;

	if (first_task) {
		task.timer.Start(true);
	}
}

void StopShake(Entity& entity, bool force) {
	if (!entity.Has<impl::ShakeEffect>()) {
		return;
	}

	auto& shake	  = entity.Get<impl::ShakeEffect>();
	auto& offsets = entity.Get<impl::Offsets>();
	offsets.shake = {};

	if (force) {
		shake.tasks.clear();
	} else if (!shake.tasks.empty()) {
		shake.tasks.pop_front();
		if (!shake.tasks.empty()) {
			shake.tasks.front().timer.Start(true);
		}
	}
}

/*

Tween& After(Manager& manager, milliseconds duration, const std::function<void()>& callback) {
	auto entity{ manager.CreateEntity() };
	return entity.Add<Tween>()
		.During(duration)
		.OnComplete([entity, callback]() mutable {
			std::invoke(callback);
			entity.Destroy();
		})
		.Start();
}

Tween& During(Manager& manager, milliseconds duration, const std::function<void()>& callback) {
	auto entity{ manager.CreateEntity() };
	return entity.Add<Tween>().During(duration).OnUpdate([callback]() { std::invoke(callback); }
	).OnComplete([entity]() mutable {
		 entity.Destroy();
	 }).Start();
}

Tween& Every(
	Manager& manager, milliseconds duration, std::int64_t repeats,
	const std::function<void()>& callback, const std::function<bool()>& exit_condition_callback
) {
	auto entity{ manager.CreateEntity() };
	return entity.Add<Tween>()
		.During(duration)
		.Repeat(repeats)
		.OnUpdate([entity, exit_condition_callback]() mutable {
			// If callback returns true, stop repetitions.
			if (exit_condition_callback != nullptr && std::invoke(exit_condition_callback)) {
				entity.Get<Tween>().IncrementTweenPoint();
			}
		})
		.OnRepeat(callback)
		.OnComplete([entity]() mutable { entity.Destroy(); })
		.Start();
}
*/

} // namespace ptgn