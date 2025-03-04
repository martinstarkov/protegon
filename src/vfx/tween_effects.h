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

void DoEffect(
	const ecs::Entity& effect_entity, const TweenCallback& start, const TweenCallback& update,
	milliseconds duration, TweenEase ease, bool force
);

struct StartPosition : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct PanEffect : public GameObject {
	explicit PanEffect(ecs::Manager& manager);

	void PanTo(
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
	void RotateTo(
		ecs::Entity& entity, float target_angle, milliseconds duration, TweenEase ease, bool force
	);
};

struct StartScale : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct ScaleEffect : public GameObject {
	explicit ScaleEffect(ecs::Manager& manager);

	void ScaleTo(
		ecs::Entity& entity, const V2_float& target_scale, milliseconds duration, TweenEase ease,
		bool force
	);
};

struct StartTint : public Tint {
	using Tint::Tint;
};

struct TintEffect : public GameObject {
	explicit TintEffect(ecs::Manager& manager);

	void TintTo(
		ecs::Entity& entity, const Color& target_tint, milliseconds duration, TweenEase ease,
		bool force
	);
};

} // namespace impl

void PanTo(
	ecs::Entity& e, const V2_float& target_position, milliseconds duration,
	TweenEase ease = TweenEase::Linear, bool force = true
);

void RotateTo(
	ecs::Entity& e, float target_angle, milliseconds duration, TweenEase ease = TweenEase::Linear,
	bool force = true
);

void ScaleTo(
	ecs::Entity& e, const V2_float& target_scale, milliseconds duration,
	TweenEase ease = TweenEase::Linear, bool force = true
);

void TintTo(
	ecs::Entity& e, const Color& target_tint, milliseconds duration,
	TweenEase ease = TweenEase::Linear, bool force = true
);

void FadeIn(
	ecs::Entity& e, milliseconds duration, TweenEase ease = TweenEase::Linear, bool force = true
);

void FadeOut(
	ecs::Entity& e, milliseconds duration, TweenEase ease = TweenEase::Linear, bool force = true
);

} // namespace ptgn