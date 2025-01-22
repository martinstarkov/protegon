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
		SceneTransition t{ TransitionType::FadeThroughColor, milliseconds{ 1000 } };
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
		EnterScene("text2");
	}
};

class TextScene : public Scene {
public:
	std::string_view transition_to;
	std::string_view content;
	Color text_color;
	Color bg_color;

	TextScene(
		std::string_view transition_to, std::string_view content, const Color& text_color,
		const Color& bg_color = color::Black
	) :
		transition_to{ transition_to },
		content{ content },
		text_color{ text_color },
		bg_color{ bg_color } {}

	void Update() final {
		EnterScene(transition_to);
		Rect::Fullscreen().Draw(bg_color);
		Text{ content, text_color }.Draw({ game.window.GetCenter(), {}, Origin::Center });
	}
};

class Text2 : public TextScene {
public:
	Text2() : TextScene{ "scene1", "Good Bye!", color::Red } {}
};

class Text1 : public TextScene {
public:
	Text1() : TextScene{ "scene2", "Welcome!", color::Blue } {}
};

class Scene1 : public Scene {
public:
	Scene1() = default;
	Texture test{ "resources/bg1.png" };

	void Update() final {
		test.Draw();
		EnterScene("text1");
	}
};

class SceneTransitionExample : public Scene {
public:
	SceneTransitionExample() {
		game.scene.Load<Scene1>("scene1");
		game.scene.Load<Scene2>("scene2");
		game.scene.Load<Text1>("text1");
		game.scene.Load<Text2>("text2");
	}

	void Enter() override {
		game.scene.Enter("scene1");
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SceneTransitionExample", window_size);
	game.scene.Enter<SceneTransitionExample>(
		"scene_transition_example",
		SceneTransition{ TransitionType::FadeThroughColor, milliseconds{ 500 } }
			.SetFadeColorDuration(milliseconds{ 500 })
	);
	return 0;
}
