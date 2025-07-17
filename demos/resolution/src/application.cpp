#include "protegon/protegon.h"
#include "rendering/api/origin.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int resolution{ 320, 240 }; // 4, 3

class ResolutionScene : public Scene {
	void Enter() override {
		LoadResource("background", "resources/test1.jpg");
		/*game.renderer.SetResolution(resolution);
		game.renderer.SetResolutionMode(ResolutionMode::Disabled);*/
	}

	void Update() override {
		/*if (game.input.KeyDown(Key::W)) {
			game.renderer.SetResolutionMode(ResolutionMode::Disabled);
		}
		if (game.input.KeyDown(Key::E)) {
			game.renderer.SetResolutionMode(ResolutionMode::Stretch);
		}
		if (game.input.KeyDown(Key::R)) {
			game.renderer.SetResolutionMode(ResolutionMode::Letterbox);
		}*/

		DrawDebugTexture("background", { 0, 0 }, window_size, Origin::TopLeft);

		DrawDebugRect({ 0, 0 }, { resolution.x, 30 }, color::Red, Origin::TopLeft, -1.0f);
		DrawDebugRect(
			{ resolution.x - 30, 0 }, { 30, resolution.y }, color::Green, Origin::TopLeft, -1.0f
		);
		DrawDebugRect(
			{ 0, resolution.y - 30 }, { resolution.x, 30 }, color::Blue, Origin::TopLeft, -1.0f
		);
		DrawDebugRect({ 0, 0 }, { 30, resolution.y }, color::Teal, Origin::TopLeft, -1.0f);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ResolutionScene", window_size);
	game.scene.Enter<ResolutionScene>("");
	return 0;
}
