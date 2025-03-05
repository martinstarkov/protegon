#include "vfx/tween_effects.h"

#include <functional>
#include <type_traits>

#include "components/draw.h"
#include "components/transform.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "utility/time.h"
#include "utility/tween.h"

namespace ptgn {

namespace impl {

Tween& DoEffect(
	const ecs::Entity& effect_entity, const TweenCallback& start, const TweenCallback& update,
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

PanEffect::PanEffect(ecs::Manager& manager) : GameObject{ manager } {
	Add<Tween>();
	Add<StartPosition>();
}

Tween& PanEffect::PanTo(
	ecs::Entity& entity, const V2_float& target_position, milliseconds duration, TweenEase ease,
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

RotateEffect::RotateEffect(ecs::Manager& manager) : GameObject{ manager } {
	Add<Tween>();
	Add<StartAngle>();
}

Tween& RotateEffect::RotateTo(
	ecs::Entity& entity, float target_angle, milliseconds duration, TweenEase ease, bool force
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

ScaleEffect::ScaleEffect(ecs::Manager& manager) : GameObject{ manager } {
	Add<Tween>();
	Add<StartScale>();
}

Tween& ScaleEffect::ScaleTo(
	ecs::Entity& entity, const V2_float& target_scale, milliseconds duration, TweenEase ease,
	bool force
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

TintEffect::TintEffect(ecs::Manager& manager) : GameObject{ manager } {
	Add<Tween>();
	Add<StartTint>();
}

Tween& TintEffect::TintTo(
	ecs::Entity& entity, const Color& target_tint, milliseconds duration, TweenEase ease, bool force
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

} // namespace impl

template <typename TEffect>
TEffect& AddEffect(ecs::Entity& e) {
	if (!e.Has<TEffect>()) {
		e.Add<TEffect>(e.GetManager());
	}
	return e.Get<TEffect>();
}

Tween& TintTo(
	ecs::Entity& e, const Color& target_tint, milliseconds duration, TweenEase ease, bool force
) {
	return AddEffect<impl::TintEffect>(e).TintTo(e, target_tint, duration, ease, force);
}

Tween& FadeIn(ecs::Entity& e, milliseconds duration, TweenEase ease, bool force) {
	return TintTo(e, color::White, duration, ease, force);
}

Tween& FadeOut(ecs::Entity& e, milliseconds duration, TweenEase ease, bool force) {
	return TintTo(e, color::Transparent, duration, ease, force);
}

Tween& After(ecs::Manager& manager, milliseconds duration, const std::function<void()>& callback) {
	auto entity{ manager.CreateEntity() };
	return entity.Add<Tween>()
		.During(duration)
		.OnComplete([entity, callback]() mutable {
			std::invoke(callback);
			entity.Destroy();
		})
		.Start();
}

Tween& ScaleTo(
	ecs::Entity& e, const V2_float& target_scale, milliseconds duration, TweenEase ease, bool force
) {
	return AddEffect<impl::ScaleEffect>(e).ScaleTo(e, target_scale, duration, ease, force);
}

Tween& PanTo(
	ecs::Entity& e, const V2_float& target_position, milliseconds duration, TweenEase ease,
	bool force
) {
	return AddEffect<impl::PanEffect>(e).PanTo(e, target_position, duration, ease, force);
}

Tween& RotateTo(
	ecs::Entity& e, float target_angle, milliseconds duration, TweenEase ease, bool force
) {
	return AddEffect<impl::RotateEffect>(e).RotateTo(e, target_angle, duration, ease, force);
}

} // namespace ptgn