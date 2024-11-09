#define SDL_MAIN_HANDLED

#include "../tests/test_ecs.h"
#include "test_animation.h"
#include "test_camera.h"
#include "test_collision.h"
#include "test_events.h"
#include "test_lighting.h"
#include "test_math.h"
#include "test_matrix4.h"
#include "test_pathfinding.h"
#include "test_renderer.h"
#include "test_rng.h"
#include "test_text.h"
#include "test_tween.h"
#include "test_vector.h"
#include "test_window.h"

using namespace ptgn;

class Tests : public Scene {
public:
	Tests() = default;

	void Preload() final {
		PTGN_INFO("Preloaded test scene");
	}

	void Init() final {
		game.draw.SetClearColor(color::White);
		V2_int window_size{ 800, 800 };
		game.window.SetSize(window_size);

		// Non-visual tests.

		TestECS();
		TestMatrix4();
		TestMath();
		TestVector2();

		// Visual tests.

		TestCollisions();
		TestLighting();
		TestRenderer();
		TestCamera();
		TestRNG();
		TestAnimations();
		TestPathfinding();
		TestTween();
		TestText();
		TestWindow();
		TestEvents();

		PTGN_INFO("Initialized test scene");
	}

	void Update() final {
		PTGN_INFO("Updated test scene");
		// game.Stop();
	}

	void Shutdown() final {
		PTGN_INFO("Shutdown test scene");
	}

	void Unload() final {
		PTGN_INFO("Unloaded test scene");
	}
};

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
		game.scene.TransitionActive(
			from, to, { TransitionType::FadeThroughBlack, milliseconds{ 1000 } }
		);
	}
}

class Scene2 : public Scene {
public:
	Texture test{ "resources/sprites/bg2.png" };

	Scene2() = default;

	void Init() final {
		// game.draw.SetClearColor(color::Orange);
	}

	void Update() final {
		game.draw.Texture(test, { 400, 400 }, { 800, 800 });
		TransitionScene("scene2", "scene1");
	}
};

class Scene1 : public Scene {
public:
	Texture test{ "resources/sprites/bg1.png" };

	Scene1() = default;

	void Init() final {
		// game.draw.SetClearColor(color::Yellow);
	}

	void Update() final {
		game.draw.Texture(test, { 400, 400 }, { 800, 800 });
		TransitionScene("scene1", "scene2");
	}
};

class SceneTransitionTest : public Scene {
public:
	SceneTransitionTest() {}

	void Init() {
		game.scene.Load<Scene1>("scene1");
		game.scene.Load<Scene2>("scene2");
		game.window.SetSize({ 800, 800 });
		game.scene.AddActive("scene1");
	}
};

int main() {
	// game.Start<Tests>();
	game.Start<SceneTransitionTest>();
	return 0;
}