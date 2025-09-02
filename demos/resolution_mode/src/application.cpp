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

class ScalingModeScene : public Scene {
	void Enter() override {
		game.window.SetSetting(WindowSetting::Resizable);
		game.window.SetSize(window_size);
		LoadResource("background", "resources/test1.jpg");
		game.renderer.SetGameSize(resolution, ScalingMode::Disabled);
	}

	void Update() override {
		if (input.KeyDown(Key::Q)) {
			game.renderer.SetScalingMode(ScalingMode::Disabled);
		}
		if (input.KeyDown(Key::W)) {
			game.renderer.SetScalingMode(ScalingMode::Stretch);
		}
		if (input.KeyDown(Key::E)) {
			game.renderer.SetScalingMode(ScalingMode::Letterbox);
		}
		if (input.KeyDown(Key::R)) {
			game.renderer.SetScalingMode(ScalingMode::IntegerScale);
		}
		if (input.KeyDown(Key::T)) {
			game.renderer.SetScalingMode(ScalingMode::Overscan);
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
	game.Init("ScalingModeScene: QWERT: Switch Resolution Modes", resolution);
	game.scene.Enter<ScalingModeScene>("");
	return 0;
}