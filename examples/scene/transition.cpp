#include <chrono>
#include <string>

#include "app/application.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/util/string.h"
#include "platform/input/key.h"
#include "renderer/primitives/color.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_transition.h"

using namespace ptgn;

int reenter_count{ 1 };

constexpr V2_int game_size{ 800, 800 };

struct FadeInTransition : public SceneTransition {
	explicit FadeInTransition(milliseconds duration, milliseconds delay = milliseconds{ 0 }) :
		SceneTransition{ duration, delay } {}

	void OnDelayStart(Scene& scene) final {
		SetTint(scene.GetRenderTarget(), color::Transparent);
	}

	void OnStart(Scene& scene) final {
		FadeIn(scene.GetRenderTarget(), GetDuration(), {}, true, true);
	}

	void OnStop(Scene& scene) final {
		SetTint(scene.GetRenderTarget(), color::White);
	}
};

struct FadeOutTransition : public SceneTransition {
	explicit FadeOutTransition(milliseconds duration, milliseconds delay = milliseconds{ 0 }) :
		SceneTransition{ duration, delay } {}

	void OnDelayStart(Scene& scene) final {
		SetTint(scene.GetRenderTarget(), color::White);
	}

	void OnStart(Scene& scene) final {
		FadeOut(scene.GetRenderTarget(), GetDuration(), {}, true, true);
	}

	void OnStop(Scene& scene) final {
		SetTint(scene.GetRenderTarget(), color::Transparent);
	}
};

struct FadeOutInTransition : public SceneTransitionPair<FadeOutTransition, FadeInTransition> {
	explicit FadeOutInTransition(milliseconds duration) :
		SceneTransitionPair{ FadeOutTransition{ duration },
							 FadeInTransition{ duration, duration } } {}
};

struct CrossFadeTransition : public SceneTransitionPair<FadeOutTransition, FadeInTransition> {
	explicit CrossFadeTransition(milliseconds duration) :
		SceneTransitionPair{ FadeOutTransition{ duration },
							 FadeInTransition{ duration, milliseconds{ 0 } } } {}
};

class Scene3 : public Scene {
public:
	void OnUpdate() final;
};

class Scene2 : public Scene {
public:
	Scene2() = default;

	int local_reenter_count{ 0 };

	Scene2(int local_reenter_count) : local_reenter_count{ local_reenter_count } {}

	void OnEnter() {
		PTGN_LOG("Entered scene 2: ", local_reenter_count);
	}

	void OnUpdate() final {
		// PTGN_LOG("Scene 2 tint: ", GetTint(GetRenderTarget()));
		ctx().renderer.DrawTexture("bg2", {}, game_size * 0.5f, Origin::TopLeft);
		ctx().renderer.DrawText(
			"Scene 2: " + ToString(local_reenter_count), game_size * 0.25f + V2_int{ 0, 50 },
			color::Magenta, 30
		);
		if (ctx().input.KeyPressed(Key::A)) {
			if (ReEnter<Scene2>(FadeOutInTransition{ 3000ms }, reenter_count)) {
				++reenter_count;
			}
		}
		if (ctx().input.KeyPressed(Key::N)) {
			ctx().scene.Switch<Scene3>("scene3", FadeOutInTransition{ 3000ms });
		}
	}
};

class Scene1 : public Scene {
public:
	void OnUpdate() final {
		// PTGN_LOG("Scene 1 tint: ", GetTint(GetRenderTarget()));
		ctx().renderer.DrawTexture(
			"bg1", V2_float{ 0.0f, -game_size.y * 0.5f }, game_size * 0.5f, Origin::TopLeft
		);

		if (ctx().input.KeyPressed(Key::N)) {
			if (ctx().scene.Switch<Scene2>(
					"scene2", FadeOutInTransition{ 3000ms }, reenter_count
				)) {
				++reenter_count;
			}
		}
	}
};

void Scene3::OnUpdate() {
	// PTGN_LOG("Scene 3 tint: ", GetTint(GetRenderTarget()));
	ctx().renderer.DrawTexture("bg3", -game_size * 0.5f, game_size * 0.5f, Origin::TopLeft);
	if (ctx().input.KeyPressed(Key::N)) {
		ctx().scene.Switch<Scene1>("scene1", CrossFadeTransition{ 3000ms });
	}
}

class SceneTransitionExample : public Scene {
public:
	SceneTransitionExample() {}

	void OnEnter() override {
		ctx().asset.LoadMany({ { "bg1", "assets/scene1.png" },
							   { "bg2", "assets/scene2.png" },
							   { "bg3", "assets/scene3.png" } });

		ctx().scene.Enter<Scene1>("scene1");
	}

	void OnUpdate() override {}
};

int main(int, char**) {
	Application app{ "SceneTransitionExample: N: Transition to next scene", game_size };
	app.StartWith<SceneTransitionExample>("scene_transition_example");
}