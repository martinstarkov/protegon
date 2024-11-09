#include <memory>
#include <string_view>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "event/key.h"
#include "renderer/color.h"
#include "renderer/texture.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "utility/time.h"

static void TransitionScene(
	std::string_view from, std::string_view to, milliseconds duration = milliseconds{ 250 }
) {
	if (game.input.KeyDown(Key::W)) {
		game.scene.TransitionActive(from, to, { TransitionType::CoverDown, duration });
	} else if (game.input.KeyDown(Key::S)) {
		game.scene.TransitionActive(from, to, { TransitionType::CoverUp, duration });
	} else if (game.input.KeyDown(Key::D)) {
		game.scene.TransitionActive(from, to, { TransitionType::CoverLeft, duration });
	} else if (game.input.KeyDown(Key::A)) {
		game.scene.TransitionActive(from, to, { TransitionType::CoverRight, duration });
	}
	if (game.input.KeyDown(Key::T)) {
		game.scene.TransitionActive(from, to, { TransitionType::UncoverDown, duration });
	} else if (game.input.KeyDown(Key::G)) {
		game.scene.TransitionActive(from, to, { TransitionType::UncoverUp, duration });
	} else if (game.input.KeyDown(Key::F)) {
		game.scene.TransitionActive(from, to, { TransitionType::UncoverLeft, duration });
	} else if (game.input.KeyDown(Key::H)) {
		game.scene.TransitionActive(from, to, { TransitionType::UncoverRight, duration });
	} else if (game.input.KeyDown(Key::DOWN)) {
		game.scene.TransitionActive(from, to, { TransitionType::PushDown, duration });
	} else if (game.input.KeyDown(Key::UP)) {
		game.scene.TransitionActive(from, to, { TransitionType::PushUp, duration });
	} else if (game.input.KeyDown(Key::LEFT)) {
		game.scene.TransitionActive(from, to, { TransitionType::PushLeft, duration });
	} else if (game.input.KeyDown(Key::RIGHT)) {
		game.scene.TransitionActive(from, to, { TransitionType::PushRight, duration });
	} else if (game.input.KeyDown(Key::Q)) {
		game.scene.TransitionActive(from, to, { TransitionType::Fade, milliseconds{ 1000 } });
	} else if (game.input.KeyDown(Key::E)) {
		SceneTransition t{ TransitionType::FadeThroughColor, milliseconds{ 1000 } };
		t.SetFadeThroughColor(color::Black);
		game.scene.TransitionActive(from, to, t);
	}
}

class Scene2 : public Scene {
public:
	Scene2() = default;
	Texture test{ "resources/sprites/bg2.png" };

	void Update() final {
		game.draw.Texture(test);
		TransitionScene("scene2", "scene1");
	}
};

class Scene1 : public Scene {
public:
	Scene1() = default;
	Texture test{ "resources/sprites/bg1.png" };

	void Update() final {
		game.draw.Texture(test);
		TransitionScene("scene1", "scene2");
	}
};

class SceneTransitionTest : public Test {
public:
	SceneTransitionTest() {
		game.scene.Load<Scene1>("scene1");
		game.scene.Load<Scene2>("scene2");
	}

	void Shutdown() final {
		game.scene.RemoveActive("scene1");
		game.scene.RemoveActive("scene2");
		game.draw.SetTarget();
	}

	void Init() final {
		game.window.SetSize({ 800, 800 });
		game.scene.AddActive("scene1");
	}

	void Update() final {
		game.scene.Update();
	}
};

void TestScenes() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new SceneTransitionTest());

	AddTests(tests);
}