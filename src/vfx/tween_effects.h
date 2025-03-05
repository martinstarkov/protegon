#pragma once

#include "components/draw.h"
#include "components/generic.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "utility/time.h"
#include "utility/tween.h"

namespace ptgn {

namespace impl {

[[nodiscard]] Tween& DoEffect(
	const ecs::Entity& effect_entity, const TweenCallback& start, const TweenCallback& update,
	milliseconds duration, TweenEase ease, bool force
);

struct StartPosition : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct PanEffect : public GameObject {
	explicit PanEffect(ecs::Manager& manager);

	[[nodiscard]] Tween& PanTo(
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

} // namespace impl

Tween& PanTo(
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

// Calls the callback after the given duration has elapsed.
Tween& After(ecs::Manager& manager, milliseconds duration, const std::function<void()>& callback);

// Stops the current bounce tween and moves onto the next one in the queue.
// @param force If true, clears the entire bounce queue.
void StopBounce(ecs::Entity& e, bool force);

// Bounce starts with upward motion unless reversed.
// @param duration Duration of the upward motion.
// @param repeats If -1, bounce continues until StopBounce is called.
// @param static_offset A continuous offset from the entity position.
Tween& Bounce(
	ecs::Entity& e, const V2_float& bounce_amplitude, const V2_float& static_offset,
	milliseconds duration, TweenEase ease, std::int64_t repeats, bool force
);

// TODO: Add shake effect.

} // namespace ptgn