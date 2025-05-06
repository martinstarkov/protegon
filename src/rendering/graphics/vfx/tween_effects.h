#pragma once

#include <cstdint>
#include <functional>

#include "components/draw.h"
#include "components/generic.h"
#include "core/entity.h"
#include "core/game_object.h"
#include "core/manager.h"
#include "math/math.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "core/time.h"
#include "core/tween.h"

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
	const Entity& effect_entity, const TweenCallback& start, const TweenCallback& update,
	milliseconds duration, TweenEase ease, bool force
);

struct StartPosition : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct TranslateEffect : public GameObject {
	explicit TranslateEffect(Manager& manager);

	[[nodiscard]] Tween& TranslateTo(
		Entity& entity, const V2_float& target_position, milliseconds duration, TweenEase ease,
		bool force
	);
};

struct StartAngle : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;
};

struct RotateEffect : public GameObject {
	explicit RotateEffect(Manager& manager);

	// @param target_angle In radians.
	[[nodiscard]] Tween& RotateTo(
		Entity& entity, float target_angle, milliseconds duration, TweenEase ease, bool force
	);
};

struct StartScale : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct ScaleEffect : public GameObject {
	explicit ScaleEffect(Manager& manager);

	[[nodiscard]] Tween& ScaleTo(
		Entity& entity, const V2_float& target_scale, milliseconds duration, TweenEase ease,
		bool force
	);
};

struct StartTint : public Tint {
	using Tint::Tint;
};

struct TintEffect : public GameObject {
	explicit TintEffect(Manager& manager);

	[[nodiscard]] Tween& TintTo(
		Entity& entity, const Color& target_tint, milliseconds duration, TweenEase ease, bool force
	);
};

struct BounceEffect : public GameObject {
	explicit BounceEffect(Manager& manager);

	[[nodiscard]] Tween& Bounce(
		Entity& entity, const V2_float& bounce_amplitude, const V2_float& static_offset,
		milliseconds duration, TweenEase ease, std::int64_t repeats, bool force
	);
};

struct ContinuousShakeEffect : public GameObject {
	explicit ContinuousShakeEffect(Manager& manager);

	void Reset(Entity& entity);

	[[nodiscard]] Tween& Shake(
		Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config,
		bool force
	);

	[[nodiscard]] Tween& Shake(
		Entity& entity, float intensity, const ShakeConfig& config, bool force
	);
};

struct ShakeEffect {
	ShakeEffect(const ShakeConfig& config = {});

	void SetConfig(const ShakeConfig& config);

	// Needs to be called once a frame to update the local translation and rotation of the camera
	// shake.
	void Update(Entity& entity, float dt, float time);

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
	Entity& e, const V2_float& target_position, milliseconds duration,
	TweenEase ease = TweenEase::Linear, bool force = true
);

Tween& RotateTo(
	Entity& e, float target_angle, milliseconds duration, TweenEase ease = TweenEase::Linear,
	bool force = true
);

Tween& ScaleTo(
	Entity& e, const V2_float& target_scale, milliseconds duration,
	TweenEase ease = TweenEase::Linear, bool force = true
);

Tween& TintTo(
	Entity& e, const Color& target_tint, milliseconds duration, TweenEase ease = TweenEase::Linear,
	bool force = true
);

Tween& FadeIn(
	Entity& e, milliseconds duration, TweenEase ease = TweenEase::Linear, bool force = true
);

Tween& FadeOut(
	Entity& e, milliseconds duration, TweenEase ease = TweenEase::Linear, bool force = true
);

// Stops the current bounce tween and moves onto the next one in the queue.
// @param force If true, clears the entire bounce queue.
void StopBounce(Entity& e, bool force = true);

// Bounce starts with upward motion unless reversed.
// @param duration Duration of the upward motion.
// @param repeats If -1, bounce continues until StopBounce is called.
// @param static_offset A continuous offset from the entity position.
Tween& Bounce(
	Entity& e, const V2_float& bounce_amplitude, const V2_float& static_offset,
	milliseconds duration, TweenEase ease, std::int64_t repeats, bool force = true
);

// @param intensity Range: [0, 1].
Tween& Shake(
	Entity& e, float intensity, milliseconds duration, const ShakeConfig& config = {},
	bool force = true
);

// @param intensity Range: [0, 1].
Tween& Shake(Entity& e, float intensity, const ShakeConfig& config = {}, bool force = true);

void StopShake(Entity& e, bool force = true);

// Calls the callback after the given duration has elapsed.
Tween& After(Manager& manager, milliseconds duration, const std::function<void()>& callback);

// Calls the callback during the given duration.
Tween& During(Manager& manager, milliseconds duration, const std::function<void()>& callback);

// Calls the callback every duration for a certain number of repeats.
// @param repeats If -1, repeats indefinitely until exit_condition_callback returns true. Warning:
// if condition_callback is nullptr, the callback will repeat until the manager is cleared!
// @param exit_condition_callback Called every frame of the duration. If it ever returns true, the
// callback repetition is stopped.
Tween& Every(
	Manager& manager, milliseconds duration, std::int64_t repeats,
	const std::function<void()>& callback, const std::function<bool()>& exit_condition_callback
);

} // namespace ptgn