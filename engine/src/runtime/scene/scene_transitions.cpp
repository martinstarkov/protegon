#include "runtime/scene/scene_transitions.h"

#include <chrono>

#include "core/math/easing.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "renderer/primitives/color.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/render_target.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

TimedTransition::TimedTransition(milliseconds duration) : SceneTransition{ duration } {}

FadeInTransition::FadeInTransition(milliseconds duration, milliseconds delay, Ease ease) :
	SceneTransition{ duration, delay, ease } {}

void FadeInTransition::OnDelayStart(Scene& scene) {
	SetTint(scene.GetRenderTarget(), color::Transparent);
}

void FadeInTransition::OnStart(Scene& scene) {
	FadeIn(scene.GetRenderTarget(), GetDuration(), GetEase(), true, true);
}

void FadeInTransition::OnStop(Scene& scene) {
	SetTint(scene.GetRenderTarget(), color::White);
}

FadeOutTransition::FadeOutTransition(milliseconds duration, milliseconds delay, Ease ease) :
	SceneTransition{ duration, delay, ease } {}

void FadeOutTransition::OnDelayStart(Scene& scene) {
	SetTint(scene.GetRenderTarget(), color::White);
}

void FadeOutTransition::OnStart(Scene& scene) {
	FadeOut(scene.GetRenderTarget(), GetDuration(), GetEase(), true, true);
}

void FadeOutTransition::OnStop(Scene& scene) {
	SetTint(scene.GetRenderTarget(), color::Transparent);
}

FadeTransition::FadeTransition(milliseconds duration, milliseconds delay, Ease ease) :
	SceneTransitionPair{ FadeOutTransition{ duration, delay, ease },
						 FadeInTransition{ duration, delay + duration, ease } } {}

CrossFadeTransition::CrossFadeTransition(milliseconds duration, milliseconds delay, Ease ease) :
	SceneTransitionPair{ FadeOutTransition{ duration, delay, ease },
						 FadeInTransition{ duration, delay, ease } } {}

SlideInTransition::SlideInTransition(
	milliseconds duration, V2_float from_direction, milliseconds delay, Ease ease
) :
	SceneTransition{ duration, delay, ease }, direction{ from_direction.Normalized() } {}

void SlideInTransition::OnDelayStart(Scene& scene) {
	auto size{ scene.ctx().renderer.GetGameSize() };
	auto start_position{ size * direction };
	SetPosition(scene.GetRenderTarget(), start_position);
}

void SlideInTransition::OnStart(Scene& scene) {
	constexpr V2_float destination{ 0, 0 };
	TranslateTo(scene.GetRenderTarget(), destination, GetDuration(), GetEase(), true);
}

void SlideInTransition::OnStop(Scene& scene) {
	constexpr V2_float destination{ 0, 0 };
	SetPosition(scene.GetRenderTarget(), destination);
}

SlideOutTransition::SlideOutTransition(
	milliseconds duration, V2_float to_direction, milliseconds delay, Ease ease
) :
	SceneTransition{ duration, delay, ease }, direction{ to_direction.Normalized() } {}

void SlideOutTransition::OnDelayStart(Scene& scene) {
	constexpr V2_float start_position{ 0, 0 };
	SetPosition(scene.GetRenderTarget(), start_position);
}

void SlideOutTransition::OnStart(Scene& scene) {
	auto size{ scene.ctx().renderer.GetGameSize() };
	auto destination{ size * direction };
	TranslateTo(scene.GetRenderTarget(), destination, GetDuration(), GetEase(), true);
}

void SlideOutTransition::OnStop(Scene& scene) {
	auto size{ scene.ctx().renderer.GetGameSize() };
	auto destination{ size * direction };
	SetPosition(scene.GetRenderTarget(), destination);
}

SlideTransition::SlideTransition(
	milliseconds duration, V2_float to_direction, milliseconds delay, Ease ease
) :
	SceneTransitionPair{ SlideOutTransition{ duration, to_direction, delay, ease },
						 SlideInTransition{ duration, -to_direction, delay, ease } } {}

} // namespace ptgn