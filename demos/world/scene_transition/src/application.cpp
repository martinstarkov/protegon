#include "core/app/application.h"
#include "math/vector2.h"
#include "ecs/components/origin.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

class Scene3 : public Scene {
public:
	void Update() final {
		Application::Get().render_.DrawTexture("bg3", -resolution * 0.5f, resolution * 0.5f, Origin::TopLeft);
		if (input.KeyDown(Key::N)) {
			Application::Get().scene_.Transition(
				"scene3", "scene1", FadeInTransition{ milliseconds{ 3000 } },
				FadeOutTransition{ milliseconds{ 3000 }, milliseconds{ 3000 } }
			);
		}
	}
};

class Scene2 : public Scene {
public:
	Scene2() = default;

	Scene2(int i) : i{ i } {}

	int i{ 0 };

	void Enter() {
		PTGN_LOG("Entered scene 2: ", i);
	}

	void Update() final {
		Application::Get().render_.DrawTexture("bg2", {}, resolution * 0.5f, Origin::TopLeft);
		if (input.KeyDown(Key::A)) {
			// Application::Get().scene_.Enter("scene2");
			Application::Get().scene_.Enter<Scene2>("scene2", ++i);
			// Application::Get().scene_.Transition("scene2", "scene2");
			// Application::Get().scene_.Transition<Scene2>("scene2", "scene2");
			// Application::Get().scene_.Transition<Scene2>("scene2", "scene2", ++i);
		}
		if (input.KeyDown(Key::N)) {
			Application::Get().scene_.Transition(
				"scene2", "scene3", FadeInTransition{ milliseconds{ 3000 }, milliseconds{ 3000 } },
				FadeOutTransition{ milliseconds{ 3000 } }
			);
		}
	}
};

class Scene1 : public Scene {
public:
	void Update() final {
		// SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		Application::Get().render_.DrawTexture(
			"bg1", V2_float{ 0.0f, -resolution.y * 0.5f }, resolution * 0.5f, Origin::TopLeft
		);

		if (input.KeyDown(Key::N)) {
			Application::Get().scene_.Transition(
				"scene1", "scene2", FadeInTransition{ milliseconds{ 3000 } },
				FadeOutTransition{ milliseconds{ 3000 } }
			);
		}
	}
};

class SceneTransitionExample : public Scene {
public:
	SceneTransitionExample() {
		LoadResource({ { "bg1", "resources/bg1.png" },
					   { "bg2", "resources/bg2.png" },
					   { "bg3", "resources/bg3.png" } });

		Application::Get().scene_.Load<Scene1>("scene1");
		Application::Get().scene_.Load<Scene2>("scene2");
		Application::Get().scene_.Load<Scene3>("scene3");
	}

	void Enter() override {
		Application::Get().scene_.Enter("scene1");
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("SceneTransitionExample: N: Transition to next scene", resolution);
	Application::Get().scene_.Enter<SceneTransitionExample>("scene_transition_example");
	return 0;
}