#include "world/scene/scene.h"

#include "core/app/game.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "world/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

class Scene3 : public Scene {
public:
	void OnUpdate() final {
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		game.renderer.DrawTexture("bg3", {}, game_size, Origin::Center);
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
		game.renderer.DrawTexture("bg2", {}, game_size, Origin::Center);
		if (input.KeyDown(Key::A)) {
			// game.scene.Enter("scene2");
			game.scene.Enter<Scene2>("scene2", ++i);
			// game.scene.Transition("scene2", "scene2");
			// game.scene.Transition<Scene2>("scene2", "scene2");
			// game.scene.Transition<Scene2>("scene2", "scene2", ++i);
		}
	}
};

class Scene1 : public Scene {
public:
	void OnUpdate() final {
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		game.renderer.DrawTexture("bg1", {}, game_size, Origin::Center);
	}
};

class SceneExample : public Scene {
public:
	SceneExample() {
		LoadResource({ { "bg1", "resources/bg1.png" },
					   { "bg2", "resources/bg2.png" },
					   { "bg3", "resources/bg3.png" } });

		game.scene.Load<Scene1>("scene1");
		game.scene.Load<Scene2>("scene2");
		game.scene.Load<Scene3>("scene3");
	}

	void OnEnter() override {
		game.scene.Enter("scene1");
		game.scene.Enter("scene2");
	}

	void OnUpdate() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application app{ "SceneExample", game_size };
	app.StartWith<SceneExample>("scene_example");
}