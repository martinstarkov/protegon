#include "runtime/scene/scene.h"

#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

class Scene3 : public Scene {
public:
	void OnUpdate() final {
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		renderer.DrawTexture("bg3", {}, game_size, Origin::Center);
	}
};

class Scene2 : public Scene {
public:
	Scene2() = default;

	Scene2(int i) : i{ i } {}

	int i{ 0 };

	void OnEnter() {
		PTGN_LOG("Entered scene 2: ", i);
	}

	void OnUpdate() final {
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		renderer.DrawTexture("bg2", {}, game_size, Origin::Center);
		if (input.KeyPressed(Key::A)) {
			// app().scene.Enter("scene2");
			app().scene.Enter<Scene2>("scene2", ++i);
			// app().scene.Transition("scene2", "scene2");
			// app().scene.Transition<Scene2>("scene2", "scene2");
			// app().scene.Transition<Scene2>("scene2", "scene2", ++i);
		}
	}
};

class Scene1 : public Scene {
public:
	void OnUpdate() final {
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		renderer.DrawTexture("bg1", {}, game_size, Origin::Center);
	}
};

class SceneExample : public Scene {
public:
	SceneExample() {
		app().asset.LoadMany({ { "bg1", "assets/scene1.png" },
							   { "bg2", "assets/scene2.png" },
							   { "bg3", "assets/scene3.png" } });

		app().scene.Load<Scene1>("scene1");
		app().scene.Load<Scene2>("scene2");
		app().scene.Load<Scene3>("scene3");
	}

	void OnEnter() override {
		app().scene.Enter("scene1");
		app().scene.Enter("scene2");
	}

	void OnUpdate() override {}
};

int main(int, char**) {
	Application app{ "SceneExample", game_size };
	app.StartWith<SceneExample>("scene_example");
}