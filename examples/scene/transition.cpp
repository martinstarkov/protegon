#include <chrono>
#include <string>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/string.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_transitions.h"

using namespace ptgn;

int scene2_reenter_count{ 1 }; // NOSONAR

constexpr V2_int logical_size{ 800, 800 };

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
		ctx().render_queue.DrawTexture(
			{}, "bg2", { .size = logical_size * 0.5f, .origin = Origin::TopLeft }
		);
		ctx().render_queue.DrawText(
			"Scene 2: " + ToString(local_reenter_count), logical_size * 0.25f + V2_int{ 0, 50 },
			color::Magenta, 30
		);
		if (ctx().input.KeyPressed(Key::A) &&
			ctx().scene.ReEnter<Scene2>(GetTag(), FadeTransition{ 3000ms }, scene2_reenter_count)) {
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
		ctx().render_queue.DrawTexture(
			{}, "bg1", { .size = logical_size * 0.5f, .origin = Origin::TopLeft }
		);

		if (ctx().input.KeyPressed(Key::N) &&
			ctx().scene.Switch<Scene2>("scene2", FadeTransition{ 3000ms }, scene2_reenter_count)) {
			++scene2_reenter_count;
		}
	}
};

void Scene3::OnUpdate() {
	// PTGN_LOG("Scene 3 tint: ", GetTint(GetRenderTarget()));
	ctx().render_queue.DrawTexture(
		{}, "bg3", { .size = logical_size * 0.5f, .origin = Origin::TopLeft }
	);
	if (ctx().input.KeyPressed(Key::N)) {
		ctx().scene.Switch<Scene1>("scene1", CrossFadeTransition{ 3000ms });
	}
}

class SceneTransitionExample : public Scene {
public:
	void OnEnter() override {
		ctx().asset.LoadMany(
			{ { "bg1", "assets/scene1.png" },
			  { "bg2", "assets/scene2.png" },
			  { "bg3", "assets/scene3.png" } }
		);

		ctx().scene.Enter<Scene1>("scene1");
	}
};

int main(int, char**) {
	Application app{ "SceneTransitionExample: N: Transition to next scene", logical_size };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<SceneTransitionExample>("scene_transition_example");
}