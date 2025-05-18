#include "tweening/tween_effects.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <variant>

#include "common/assert.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/timer.h"
#include "math/easing.h"
#include "math/math.h"
#include "math/vector2.h"
#include "rendering/api/color.h"

namespace ptgn {

namespace impl {

/*

ShakeEffect::ShakeEffect(const ShakeConfig& config) : config_{ config } {
	using type = decltype(seed_);
	RNG<type> rng_float{ std::numeric_limits<type>::min(), std::numeric_limits<type>::max() };
	seed_ = rng_float();
}

void ShakeEffect::SetConfig(const ShakeConfig& config) {
	config_ = config;
}

void ShakeEffect::Update(Entity& entity, float dt, float time) {
	if (!entity.Has<impl::Offsets>()) {
		return;
	}

	auto& offsets{ entity.Get<impl::Offsets>() };

	if (trauma_ <= 0.0f) {
		offsets.shake = {};
		return;
	}

	// Shake algorithm based on: https://roystan.net/articles/camera-shake/

	// Taking trauma to an exponent allows the ability to smoothen
	// out the transition from shaking to being static.
	float shake{ std::pow(trauma_, config_.trauma_exponent) };

	offsets.shake.position =
		V2_float{ config_.maximum_translation.x *
					  (PerlinNoise::GetValue(time * config_.frequency, 0.0f, seed_ + 0) * 2 - 1),
				  config_.maximum_translation.y *
					  (PerlinNoise::GetValue(time * config_.frequency, 0.0f, seed_ + 1) * 2 - 1) } *
		shake;

	offsets.shake.rotation =
		config_.maximum_rotation *
		(PerlinNoise::GetValue(time * config_.frequency, 0.0f, seed_ + 3) * 2 - 1) * shake;

	trauma_ = std::clamp(trauma_ - config_.recovery_speed * dt, 0.0f, 1.0f);
}

void ShakeEffect::Reset() {
	trauma_ = 0.0f;
}

void ShakeEffect::AddIntensity(float intensity) {
	trauma_ = std::clamp(trauma_ + intensity, 0.0f, 1.0f);
}

void ShakeEffect::SetIntensity(float intensity) {
	trauma_ = std::clamp(intensity, 0.0f, 1.0f);
}

ContinuousShakeEffect::ContinuousShakeEffect(Manager& manager) : Entity{ manager } {
	Add<Tween>();
}

void ContinuousShakeEffect::Reset(Entity& entity) {
	if (entity.Has<Offsets>()) {
		entity.Get<Offsets>().shake = {};
	}
	entity.Remove<ShakeEffect>();
}

Tween& ContinuousShakeEffect::Shake(
	Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config, bool force
) {
	return impl::DoEffect(
		*this,
		[config, entity]() mutable {
			if (!entity.Has<ShakeEffect>()) {
				entity.Add<ShakeEffect>();
			}
			if (!entity.Has<Offsets>()) {
				entity.Add<Offsets>();
			}
			auto& offsets{ entity.Get<Offsets>() };
			offsets.shake = {};
			auto& shake{ entity.Get<ShakeEffect>() };
			shake.SetConfig(config);
		},
		[intensity, entity]([[maybe_unused]] float progress) mutable {
			if (entity.Has<ShakeEffect>()) {
				entity.Get<ShakeEffect>().SetIntensity(intensity);
			}
		},
		duration, SymmetricalEase::Linear, force
	);
}

Tween& ContinuousShakeEffect::Shake(
	Entity& entity, float intensity, const ShakeConfig& config, bool force
) {
	return impl::DoEffect(
		*this,
		[config, intensity, entity]() mutable {
			if (!entity.Has<ShakeEffect>()) {
				entity.Add<ShakeEffect>();
			}
			if (!entity.Has<Offsets>()) {
				entity.Add<Offsets>();
			}
			auto& shake{ entity.Get<ShakeEffect>() };
			shake.SetConfig(config);
			shake.AddIntensity(intensity);
		},
		[]() {}, milliseconds{ 0 }, SymmetricalEase::Linear, force
	);
}
*/

template <typename T>
auto GetTaskValue(T& task) {
	PTGN_ASSERT(task.timer.IsRunning());

	float t{ task.timer.template ElapsedPercentage<milliseconds, float>(task.duration) };

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

void ShakeEffectSystem::Update(Manager& manager) const {
	for (auto [entity, shake, offsets] : manager.EntitiesWith<ShakeEffect, Offsets>()) {
		float t = shake.timer.Elapsed<duration<float, seconds::period>>().count();
		if (!shake.indefinite && shake.timer.Completed(shake.duration)) {
			offsets.shake = {};
			entity.Remove<ShakeEffect>();
			continue;
		}

		// TODO: Switch to different algorithm.

		float damp	  = std::exp(-shake.config.damping * t);
		float angle	  = two_pi<float> * shake.config.frequency * t;
		float offsetX = std::sin(angle) * shake.intensity * damp;
		float offsetY = std::cos(angle) * shake.intensity * damp;

		offsets.shake.position = V2_float{ offsetX, offsetY };
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
	Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config, bool force
) {
	auto& shake = entity.GetOrAdd<impl::ShakeEffect>();
	if (force || !entity.Has<impl::ShakeEffect>()) {
		shake.timer.Start(true);
		shake.original_position = entity.GetPosition();
		shake.duration			= duration;
		shake.intensity			= intensity;
		shake.indefinite		= false;
		shake.config			= config;
	}
}

void Shake(Entity& entity, float intensity, const ShakeConfig& config, bool force) {
	auto& shake = entity.GetOrAdd<impl::ShakeEffect>();
	if (force || !entity.Has<impl::ShakeEffect>()) {
		shake.timer.Start(true);
		shake.original_position = entity.GetPosition();
		shake.duration			= milliseconds(0);
		shake.intensity			= intensity;
		shake.indefinite		= true;
		shake.config			= config;
	}
}

void StopShake(Entity& entity, bool force) {
	if (!entity.Has<impl::ShakeEffect>()) {
		return;
	}
	entity.Get<impl::Offsets>().shake = {};
	entity.Remove<impl::ShakeEffect>();
}

/*

void StopBounce(Entity& e, bool force) {
	if (!e.Has<impl::BounceEffect>()) {
		return;
	}
	auto& effect{ e.Get<impl::BounceEffect>() };
	auto& tween{ effect.Get<Tween>() };
	tween.IncrementTweenPoint();
	if (force || tween.IsCompleted()) {
		tween.Clear();
	}
}

Tween& Bounce(
	Entity& e, const V2_float& bounce_amplitude, const V2_float& static_offset,
	milliseconds duration, const Ease& ease, std::int64_t repeats, bool force
) {
	return AddEffect<impl::BounceEffect>(e).Bounce(
		e, bounce_amplitude, static_offset, duration, ease, repeats, force
	);
}

Tween& Shake(
	Entity& e, float intensity, milliseconds duration, const ShakeConfig& config, bool force
) {
	return AddEffect<impl::ContinuousShakeEffect>(e).Shake(e, intensity, duration, config, force);
}

Tween& Shake(Entity& e, float intensity, const ShakeConfig& config, bool force) {
	return AddEffect<impl::ContinuousShakeEffect>(e).Shake(e, intensity, config, force);
}

void StopShake(Entity& e, bool force) {
	if (!e.Has<impl::ContinuousShakeEffect>()) {
		return;
	}
	auto& effect{ e.Get<impl::ContinuousShakeEffect>() };
	auto& tween{ effect.Get<Tween>() };
	tween.IncrementTweenPoint();
	if (force || tween.IsCompleted()) {
		tween.Clear();
		effect.Reset(e);
	}
}

*/

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