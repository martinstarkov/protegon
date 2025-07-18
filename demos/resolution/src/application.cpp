#include "core/game.h"
#include "events/input_handler.h"
#include "events/key.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int resolution{ 320, 240 }; // 4, 3

class ResolutionScene : public Scene {
	void Enter() override {
		LoadResource("background", "resources/test1.jpg");
		game.renderer.SetResolution(resolution);
		game.renderer.SetResolutionMode(ResolutionMode::Disabled);
	}

	void Update() override {
		camera.primary.CenterOnArea(resolution);

		if (game.input.KeyDown(Key::Q)) {
			game.renderer.SetResolutionMode(ResolutionMode::Disabled);
		}
		if (game.input.KeyDown(Key::W)) {
			game.renderer.SetResolutionMode(ResolutionMode::Stretch);
		}
		if (game.input.KeyDown(Key::E)) {
			game.renderer.SetResolutionMode(ResolutionMode::Letterbox);
		}
		if (game.input.KeyDown(Key::R)) {
			game.renderer.SetResolutionMode(ResolutionMode::IntegerScale);
		}
		if (game.input.KeyDown(Key::T)) {
			game.renderer.SetResolutionMode(ResolutionMode::Overscan);
		}

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
