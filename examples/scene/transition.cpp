#include <chrono>
#include <string>

#include "app/application.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/string.h"
#include "platform/input/key.h"
#include "renderer/primitives/color.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_transitions.h"

using namespace ptgn;

int scene2_reenter_count{ 1 }; // NOSONAR

constexpr V2_int game_size{ 800, 800 };

class Scene3 : public Scene {
public:
	void OnUpdate() final;
};

class Scene2 : public Scene {
public:
	Scene2() = default;

	int local_reenter_count{ 0 };

	explicit Scene2(int local_reenter_count) : local_reenter_count{ local_reenter_count } {}

	void OnEnter() final {
		PTGN_LOG("Entered scene 2: ", local_reenter_count);
	}

	void OnUpdate() final {
		// PTGN_LOG("Scene 2 tint: ", GetTint(GetRenderTarget()));
		ctx().renderer.DrawTexture("bg2", {}, game_size * 0.5f, Origin::TopLeft);
		ctx().renderer.DrawText(
			"Scene 2: " + ToString(local_reenter_count), game_size * 0.25f + V2_int{ 0, 50 },
			color::Magenta, 30
		);
		if (ctx().input.KeyPressed(Key::A) &&
			ReEnter<Scene2>(FadeTransition{ 3000ms }, scene2_reenter_count)) {
			++scene2_reenter_count;
		}
		if (ctx().input.KeyPressed(Key::N)) {
			ctx().scene.Switch<Scene3>(
				"scene3", SlideTransition{ 3000ms, V2_float{ -1.0f, 1.0f } }
			);
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

		if (ctx().input.KeyPressed(Key::N) &&
			ctx().scene.Switch<Scene2>("scene2", FadeTransition{ 3000ms }, scene2_reenter_count)) {
			++scene2_reenter_count;
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
	void OnEnter() override {
		ctx().asset.LoadMany({ { "bg1", "assets/scene1.png" },
							   { "bg2", "assets/scene2.png" },
							   { "bg3", "assets/scene3.png" } });

		ctx().scene.Enter<Scene1>("scene1");
	}
};

int main(int, char**) {
	Application app{ "SceneTransitionExample: N: Transition to next scene", game_size };
	app.StartWith<SceneTransitionExample>("scene_transition_example");
}