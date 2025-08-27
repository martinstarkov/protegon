#include "core/game.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

class Scene3 : public Scene {
public:
	void Update() final {
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		DrawDebugTexture("bg3", {}, window_size, Origin::TopLeft);
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
		DrawDebugTexture("bg2", {}, window_size, Origin::TopLeft);
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
	void Update() final {
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		DrawDebugTexture("bg1", {}, window_size, Origin::TopLeft);
	}
};

class SceneExample : public Scene {
public:
	SceneExample() {
		LoadResources({ { "bg1", "resources/bg1.png" },
						{ "bg2", "resources/bg2.png" },
						{ "bg3", "resources/bg3.png" } });

		game.scene.Load<Scene1>("scene1");
		game.scene.Load<Scene2>("scene2");
		game.scene.Load<Scene3>("scene3");
	}

	void Enter() override {
		game.scene.Enter("scene1");
		game.scene.Enter("scene2");
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SceneExample", window_size);
	game.scene.Enter<SceneExample>("scene_example");
	return 0;
}