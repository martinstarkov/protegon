#pragma once

#include <chrono>

#include "core/math/easing.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

class Scene;

struct TimedTransition : public SceneTransition {
	explicit TimedTransition(milliseconds duration);
};

struct FadeInTransition : public SceneTransition {
	explicit FadeInTransition(
		milliseconds duration, milliseconds delay = 0ms, Ease ease = Ease::Linear
	);

	void OnDelayStart(Scene& scene) override;
	void OnStart(Scene& scene) override;
	void OnStop(Scene& scene) override;
};

struct FadeOutTransition : public SceneTransition {
	explicit FadeOutTransition(
		milliseconds duration, milliseconds delay = 0ms, Ease ease = Ease::Linear
	);

	void OnDelayStart(Scene& scene) override;
	void OnStart(Scene& scene) override;
	void OnStop(Scene& scene) override;
};

struct FadeTransition : public SceneTransitionPair<FadeOutTransition, FadeInTransition> {
	explicit FadeTransition(
		milliseconds duration, milliseconds delay = 0ms, Ease ease = Ease::Linear
	);
};

struct CrossFadeTransition : public SceneTransitionPair<FadeOutTransition, FadeInTransition> {
	explicit CrossFadeTransition(
		milliseconds duration, milliseconds delay = 0ms, Ease ease = Ease::Linear
	);
};

struct SlideInTransition : public SceneTransition {
	explicit SlideInTransition(
		milliseconds duration, V2_float from_direction = { 1.0f, 0.0f }, milliseconds delay = 0ms,
		Ease ease = Ease::Linear
	);

	void OnDelayStart(Scene& scene) override;
	void OnStart(Scene& scene) override;
	void OnStop(Scene& scene) override;

	V2_float direction{ 1.0f, 0.0f };
};

struct SlideOutTransition : public SceneTransition {
	explicit SlideOutTransition(
		milliseconds duration, V2_float to_direction = { 1.0f, 0.0f }, milliseconds delay = 0ms,
		Ease ease = Ease::Linear
	);

	void OnDelayStart(Scene& scene) override;
	void OnStart(Scene& scene) override;
	void OnStop(Scene& scene) override;

	V2_float direction{ 1.0f, 0.0f };
};

struct SlideTransition : public SceneTransitionPair<SlideOutTransition, SlideInTransition> {
	explicit SlideTransition(
		milliseconds duration, V2_float to_direction = { 1.0f, 0.0f }, milliseconds delay = 0ms,
		Ease ease = Ease::Linear
	);
};

} // namespace ptgn