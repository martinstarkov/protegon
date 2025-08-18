#include "core/game.h"
#include "core/window.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int resolution{ 320, 240 }; // 4, 3

class ResolutionScene : public Scene {
	void Enter() override {
		game.window.SetSetting(WindowSetting::Resizable);
		LoadResource("background", "resources/test1.jpg");
		game.renderer.SetLogicalResolution(resolution, LogicalResolutionMode::Disabled);
	}

	void Update() override {
		if (game.input.KeyDown(Key::Q)) {
			game.renderer.SetLogicalResolutionMode(LogicalResolutionMode::Disabled);
		}
		if (game.input.KeyDown(Key::W)) {
			game.renderer.SetLogicalResolutionMode(LogicalResolutionMode::Stretch);
		}
		if (game.input.KeyDown(Key::E)) {
			game.renderer.SetLogicalResolutionMode(LogicalResolutionMode::Letterbox);
		}
		if (game.input.KeyDown(Key::R)) {
			game.renderer.SetLogicalResolutionMode(LogicalResolutionMode::IntegerScale);
		}
		if (game.input.KeyDown(Key::T)) {
			game.renderer.SetLogicalResolutionMode(LogicalResolutionMode::Overscan);
		}

		DrawDebugTexture("background", { 0, 0 }, resolution, Origin::TopLeft);

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