#pragma once

#include <cstdint>
#include <functional>

#include "components/draw.h"
#include "components/generic.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "utility/time.h"
#include "utility/tween.h"

namespace ptgn {

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

namespace impl {

[[nodiscard]] Tween& DoEffect(
	const ecs::Entity& effect_entity, const TweenCallback& start, const TweenCallback& update,
	milliseconds duration, TweenEase ease, bool force
);

struct StartPosition : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct TranslateEffect : public GameObject {
	explicit TranslateEffect(ecs::Manager& manager);

	[[nodiscard]] Tween& TranslateTo(
		ecs::Entity& entity, const V2_float& target_position, milliseconds duration, TweenEase ease,
		bool force
	);
};

struct StartAngle : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;
};

struct RotateEffect : public GameObject {
	explicit RotateEffect(ecs::Manager& manager);

	// @param target_angle In radians.
	[[nodiscard]] Tween& RotateTo(
		ecs::Entity& entity, float target_angle, milliseconds duration, TweenEase ease, bool force
	);
};

struct StartScale : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct ScaleEffect : public GameObject {
	explicit ScaleEffect(ecs::Manager& manager);

	[[nodiscard]] Tween& ScaleTo(
		ecs::Entity& entity, const V2_float& target_scale, milliseconds duration, TweenEase ease,
		bool force
	);
};

struct StartTint : public Tint {
	using Tint::Tint;
};

struct TintEffect : public GameObject {
	explicit TintEffect(ecs::Manager& manager);

	[[nodiscard]] Tween& TintTo(
		ecs::Entity& entity, const Color& target_tint, milliseconds duration, TweenEase ease,
		bool force
	);
};

struct BounceEffect : public GameObject {
	explicit BounceEffect(ecs::Manager& manager);

	[[nodiscard]] Tween& Bounce(
		ecs::Entity& entity, const V2_float& bounce_amplitude, const V2_float& static_offset,
		milliseconds duration, TweenEase ease, std::int64_t repeats, bool force
	);
};

struct ContinuousShakeEffect : public GameObject {
	explicit ContinuousShakeEffect(ecs::Manager& manager);

	void Reset(ecs::Entity& entity);

	[[nodiscard]] Tween& Shake(
		ecs::Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config,
		bool force
	);

	[[nodiscard]] Tween& Shake(
		ecs::Entity& entity, float intensity, const ShakeConfig& config, bool force
	);
};

struct ShakeEffect {
	ShakeEffect(const ShakeConfig& config = {});

	void SetConfig(const ShakeConfig& config);

	// Needs to be called once a frame to update the local translation and rotation of the camera
	// shake.
	void Update(ecs::Entity& entity, float dt, float time);

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

} // namespace impl

Tween& TranslateTo(
	ecs::Entity& e, const V2_float& target_position, milliseconds duration,
	TweenEase ease = TweenEase::Linear, bool force = true
);

Tween& RotateTo(
	ecs::Entity& e, float target_angle, milliseconds duration, TweenEase ease = TweenEase::Linear,
	bool force = true
);

Tween& ScaleTo(
	ecs::Entity& e, const V2_float& target_scale, milliseconds duration,
	TweenEase ease = TweenEase::Linear, bool force = true
);

Tween& TintTo(
	ecs::Entity& e, const Color& target_tint, milliseconds duration,
	TweenEase ease = TweenEase::Linear, bool force = true
);

Tween& FadeIn(
	ecs::Entity& e, milliseconds duration, TweenEase ease = TweenEase::Linear, bool force = true
);

Tween& FadeOut(
	ecs::Entity& e, milliseconds duration, TweenEase ease = TweenEase::Linear, bool force = true
);

// Stops the current bounce tween and moves onto the next one in the queue.
// @param force If true, clears the entire bounce queue.
void StopBounce(ecs::Entity& e, bool force = true);

// Bounce starts with upward motion unless reversed.
// @param duration Duration of the upward motion.
// @param repeats If -1, bounce continues until StopBounce is called.
// @param static_offset A continuous offset from the entity position.
Tween& Bounce(
	ecs::Entity& e, const V2_float& bounce_amplitude, const V2_float& static_offset,
	milliseconds duration, TweenEase ease, std::int64_t repeats, bool force = true
);

// @param intensity Range: [0, 1].
Tween& Shake(
	ecs::Entity& e, float intensity, milliseconds duration, const ShakeConfig& config = {},
	bool force = true
);

// @param intensity Range: [0, 1].
Tween& Shake(ecs::Entity& e, float intensity, const ShakeConfig& config = {}, bool force = true);

void StopShake(ecs::Entity& e, bool force = true);

// Calls the callback after the given duration has elapsed.
Tween& After(ecs::Manager& manager, milliseconds duration, const std::function<void()>& callback);

// Calls the callback during the given duration.
Tween& During(ecs::Manager& manager, milliseconds duration, const std::function<void()>& callback);

// Calls the callback every duration for a certain number of repeats.
// @param repeats If -1, repeats indefinitely until exit_condition_callback returns true. Warning:
// if condition_callback is nullptr, the callback will repeat until the manager is cleared!
// @param exit_condition_callback Called every frame of the duration. If it ever returns true, the
// callback repetition is stopped.
Tween& Every(
	ecs::Manager& manager, milliseconds duration, std::int64_t repeats,
	const std::function<void()>& callback, const std::function<bool()>& exit_condition_callback
);

} // namespace ptgn