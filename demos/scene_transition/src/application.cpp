#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

static void EnterScene(std::string_view key, milliseconds duration = milliseconds{ 250 }) {
	if (game.input.KeyDown(Key::W)) {
		game.scene.Enter(key, { TransitionType::CoverDown, duration });
	} else if (game.input.KeyDown(Key::S)) {
		game.scene.Enter(key, { TransitionType::CoverUp, duration });
	} else if (game.input.KeyDown(Key::D)) {
		game.scene.Enter(key, { TransitionType::CoverLeft, duration });
	} else if (game.input.KeyDown(Key::A)) {
		game.scene.Enter(key, { TransitionType::CoverRight, duration });
	} else if (game.input.KeyDown(Key::T)) {
		game.scene.Enter(key, { TransitionType::UncoverDown, duration });
	} else if (game.input.KeyDown(Key::G)) {
		game.scene.Enter(key, { TransitionType::UncoverUp, duration });
	} else if (game.input.KeyDown(Key::F)) {
		game.scene.Enter(key, { TransitionType::UncoverLeft, duration });
	} else if (game.input.KeyDown(Key::H)) {
		game.scene.Enter(key, { TransitionType::UncoverRight, duration });
	} else if (game.input.KeyDown(Key::DOWN)) {
		game.scene.Enter(key, { TransitionType::PushDown, duration });
	} else if (game.input.KeyDown(Key::UP)) {
		game.scene.Enter(key, { TransitionType::PushUp, duration });
	} else if (game.input.KeyDown(Key::LEFT)) {
		game.scene.Enter(key, { TransitionType::PushLeft, duration });
	} else if (game.input.KeyDown(Key::RIGHT)) {
		game.scene.Enter(key, { TransitionType::PushRight, duration });
	} else if (game.input.KeyDown(Key::Q)) {
		game.scene.Enter(key, { TransitionType::Fade, milliseconds{ 4000 } });
	} else if (game.input.KeyDown(Key::E)) {
		SceneTransition t{ TransitionType::FadeThroughColor, milliseconds{ 3000 } };
		t.SetFadeColor(color::Black);
		t.SetFadeColorDuration(milliseconds{ 1000 });
		game.scene.Enter(key, t);
	}
}

class Scene2 : public Scene {
public:
	Scene2() = default;
	Texture test{ "resources/bg2.png" };

	void Update() final {
		test.Draw();
		EnterScene("scene1");
	}
};

class Scene1 : public Scene {
public:
	Scene1() = default;
	Texture test{ "resources/bg1.png" };

	void Update() final {
		test.Draw();
		EnterScene("scene2");
	}
};

class SceneTransitionExample : public Scene {
public:
	SceneTransitionExample() {
		game.scene.Load<Scene1>("scene1");
		game.scene.Load<Scene2>("scene2");
	}

	void Enter() override {
		game.scene.Enter("scene1");
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SceneTransitionExample", window_size);
	game.scene.Enter<SceneTransitionExample>("scene_transition_example");
	return 0;
}
