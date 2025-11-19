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
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		Application::Get().render_.DrawTexture("bg3", {}, resolution, Origin::Center);
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
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		Application::Get().render_.DrawTexture("bg2", {}, resolution, Origin::Center);
		if (input.KeyDown(Key::A)) {
			// Application::Get().scene_.Enter("scene2");
			Application::Get().scene_.Enter<Scene2>("scene2", ++i);
			// Application::Get().scene_.Transition("scene2", "scene2");
			// Application::Get().scene_.Transition<Scene2>("scene2", "scene2");
			// Application::Get().scene_.Transition<Scene2>("scene2", "scene2", ++i);
		}
	}
};

class Scene1 : public Scene {
public:
	void Update() final {
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		Application::Get().render_.DrawTexture("bg1", {}, resolution, Origin::Center);
	}
};

class SceneExample : public Scene {
public:
	SceneExample() {
		LoadResource({ { "bg1", "resources/bg1.png" },
					   { "bg2", "resources/bg2.png" },
					   { "bg3", "resources/bg3.png" } });

		Application::Get().scene_.Load<Scene1>("scene1");
		Application::Get().scene_.Load<Scene2>("scene2");
		Application::Get().scene_.Load<Scene3>("scene3");
	}

	void Enter() override {
		Application::Get().scene_.Enter("scene1");
		Application::Get().scene_.Enter("scene2");
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("SceneExample", resolution);
	Application::Get().scene_.Enter<SceneExample>("scene_example");
	return 0;
}