#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int resolution{ 320, 240 }; // 4, 3

class ResolutionExampleScene : public Scene {
	Texture background{ "resources/test1.jpg" };

	void Init() override {
		game.renderer.SetResolution(resolution);
		game.renderer.SetResolutionMode(ResolutionMode::Disabled);
	}

	void Update() override {
		if (game.input.KeyDown(Key::W)) {
			game.renderer.SetResolutionMode(ResolutionMode::Disabled);
		}
		if (game.input.KeyDown(Key::E)) {
			game.renderer.SetResolutionMode(ResolutionMode::Stretch);
		}
		if (game.input.KeyDown(Key::R)) {
			game.renderer.SetResolutionMode(ResolutionMode::Letterbox);
		}

		background.Draw({ { 0, 0 }, resolution, Origin::TopLeft });

		Rect{ { -30, -30 }, { resolution.x + 60, 30 }, Origin::TopLeft }.Draw(color::Red);
		Rect{ { resolution.x, 0 }, { 30, resolution.y }, Origin::TopLeft }.Draw(color::Blue);
		Rect{ { -30, resolution.y }, { resolution.x + 60, 30 }, Origin::TopLeft }.Draw(color::Green
		);
		Rect{ { -30, 0 }, { 30, resolution.y }, Origin::TopLeft }.Draw(color::Teal);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ResolutionExample", window_size);
	game.scene.LoadActive<ResolutionExampleScene>("resolution_example_scene");
	return 0;
}
