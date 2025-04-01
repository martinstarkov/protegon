#include "vfx/tween_effects.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>

#include "components/draw.h"
#include "components/offsets.h"
#include "core/entity.h"
#include "core/game_object.h"
#include "core/manager.h"
#include "core/transform.h"
#include "math/math.h"
#include "math/noise.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "utility/time.h"
#include "utility/tween.h"

namespace ptgn {

namespace impl {

Tween& DoEffect(
	const Entity& effect_entity, const TweenCallback& start, const TweenCallback& update,
	milliseconds duration, TweenEase ease, bool force
) {
	auto& tween{ effect_entity.Get<Tween>() };
	if (force || tween.IsCompleted()) {
		tween.Clear();
	}
	tween.During(duration)
		.Ease(ease)
		.OnStart(start)
		.OnUpdate(update)
		.OnComplete(start)
		.OnStop(start)
		.OnReset(start);
	tween.Start(force);
	return effect_entity.Get<Tween>();
}

TranslateEffect::TranslateEffect(Manager& manager) : GameObject{ manager } {
	Add<Tween>();
	Add<StartPosition>();
}

Tween& TranslateEffect::TranslateTo(
	Entity& entity, const V2_float& target_position, milliseconds duration, TweenEase ease,
	bool force
) {
	if (!entity.Has<Transform>()) {
		entity.Add<Transform>();
	}
	return impl::DoEffect(
		GetEntity(),
		[entity, e = GetEntity()]() mutable {
			e.Add<StartPosition>(entity.Get<Transform>().position);
		},
		[target_position, entity, e = GetEntity()](float progress) mutable {
			if (entity.Has<Transform>()) {
				auto& transform{ entity.Get<Transform>() };
				transform.position =
					Lerp(V2_float{ e.Get<StartPosition>() }, target_position, progress);
			}
		},
		duration, ease, force
	);
}

RotateEffect::RotateEffect(Manager& manager) : GameObject{ manager } {
	Add<Tween>();
	Add<StartAngle>();
}

Tween& RotateEffect::RotateTo(
	Entity& entity, float target_angle, milliseconds duration, TweenEase ease, bool force
) {
	if (!entity.Has<Transform>()) {
		entity.Add<Transform>();
	}
	return impl::DoEffect(
		GetEntity(),
		[entity, e = GetEntity()]() mutable {
			e.Add<StartAngle>(entity.Get<Transform>().rotation);
		},
		[target_angle, entity, e = GetEntity()](float progress) mutable {
			if (entity.Has<Transform>()) {
				auto& transform{ entity.Get<Transform>() };
				transform.rotation = Lerp(float{ e.Get<StartAngle>() }, target_angle, progress);
			}
		},
		duration, ease, force
	);
}

ScaleEffect::ScaleEffect(Manager& manager) : GameObject{ manager } {
	Add<Tween>();
	Add<StartScale>();
}

Tween& ScaleEffect::ScaleTo(
	Entity& entity, const V2_float& target_scale, milliseconds duration, TweenEase ease, bool force
) {
	if (!entity.Has<Transform>()) {
		entity.Add<Transform>();
	}
	return impl::DoEffect(
		GetEntity(),
		[entity, e = GetEntity()]() mutable { e.Add<StartScale>(entity.Get<Transform>().scale); },
		[target_scale, entity, e = GetEntity()](float progress) mutable {
			if (entity.Has<Transform>()) {
				auto& transform{ entity.Get<Transform>() };
				transform.scale = Lerp(V2_float{ e.Get<StartScale>() }, target_scale, progress);
			}
		},
		duration, ease, force
	);
}

TintEffect::TintEffect(Manager& manager) : GameObject{ manager } {
	Add<Tween>();
	Add<StartTint>();
}

Tween& TintEffect::TintTo(
	Entity& entity, const Color& target_tint, milliseconds duration, TweenEase ease, bool force
) {
	if (!entity.Has<Tint>()) {
		entity.Add<Tint>();
	}
	return impl::DoEffect(
		GetEntity(), [entity, e = GetEntity()]() mutable { e.Add<StartTint>(entity.Get<Tint>()); },
		[target_tint, entity, e = GetEntity()](float progress) mutable {
			if (entity.Has<Tint>()) {
				auto& fade{ entity.Get<Tint>() };
				fade = Lerp(e.Get<StartTint>(), target_tint, progress);
			}
		},
		duration, ease, force
	);
}

BounceEffect::BounceEffect(Manager& manager) : GameObject{ manager } {
	Add<Tween>();
}

Tween& BounceEffect::Bounce(
	Entity& entity, const V2_float& bounce_amplitude, const V2_float& static_offset,
	milliseconds duration, TweenEase ease, std::int64_t repeats, bool force
) {
	auto& tween{ impl::DoEffect(
		GetEntity(),
		[entity]() mutable {
			if (!entity.Has<Offsets>()) {
				entity.Add<Offsets>();
			}
			entity.Get<Offsets>().bounce = {};
		},
		[bounce_amplitude, static_offset, entity](float progress) mutable {
			if (entity.Has<Offsets>()) {
				auto& offsets{ entity.Get<Offsets>() };
				offsets.bounce.position = static_offset + bounce_amplitude * progress;
			}
		},
		duration, ease, force
	) };
	tween.Yoyo();
	tween.Repeat(repeats);
	return tween;
}

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

ContinuousShakeEffect::ContinuousShakeEffect(Manager& manager) : GameObject{ manager } {
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
		GetEntity(),
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
		[intensity, entity](float progress) mutable {
			if (entity.Has<ShakeEffect>()) {
				entity.Get<ShakeEffect>().SetIntensity(intensity);
			}
		},
		duration, TweenEase::Linear, force
	);
}

Tween& ContinuousShakeEffect::Shake(
	Entity& entity, float intensity, const ShakeConfig& config, bool force
) {
	return impl::DoEffect(
		GetEntity(),
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
		[]() {}, milliseconds{ 0 }, TweenEase::Linear, force
	);
}

} // namespace impl

template <typename TEffect>
TEffect& AddEffect(Entity& e) {
	if (!e.Has<TEffect>()) {
		e.Add<TEffect>(e.GetManager());
	}
	return e.Get<TEffect>();
}

Tween& TintTo(
	Entity& e, const Color& target_tint, milliseconds duration, TweenEase ease, bool force
) {
	return AddEffect<impl::TintEffect>(e).TintTo(e, target_tint, duration, ease, force);
}

Tween& FadeIn(Entity& e, milliseconds duration, TweenEase ease, bool force) {
	return TintTo(e, color::White, duration, ease, force);
}

Tween& FadeOut(Entity& e, milliseconds duration, TweenEase ease, bool force) {
	return TintTo(e, color::Transparent, duration, ease, force);
}

Tween& ScaleTo(
	Entity& e, const V2_float& target_scale, milliseconds duration, TweenEase ease, bool force
) {
	return AddEffect<impl::ScaleEffect>(e).ScaleTo(e, target_scale, duration, ease, force);
}

Tween& TranslateTo(
	Entity& e, const V2_float& target_position, milliseconds duration, TweenEase ease, bool force
) {
	return AddEffect<impl::TranslateEffect>(e).TranslateTo(
		e, target_position, duration, ease, force
	);
}

Tween& RotateTo(Entity& e, float target_angle, milliseconds duration, TweenEase ease, bool force) {
	return AddEffect<impl::RotateEffect>(e).RotateTo(e, target_angle, duration, ease, force);
}

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
	milliseconds duration, TweenEase ease, std::int64_t repeats, bool force
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

} // namespace ptgn