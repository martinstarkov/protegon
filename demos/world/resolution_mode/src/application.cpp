#include "core/ecs/components/sprite.h"
#include "core/app/application.h"
#include "core/app/window.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int resolution{ 320, 240 }; // 4, 3

class ScalingModeScene : public Scene {
	void Enter() override {
		SetBackgroundColor(color::LightBlue);
		game.window.SetResizable();
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

		game.renderer.DrawTexture("background", V2_int{ 0, 0 }, resolution, Origin::Center);

		game.renderer.DrawRect(
			V2_int{ -resolution.x * 0.5f, -resolution.y * 0.5f }, V2_int{ resolution.x, 30 },
			color::Red, -1.0f, Origin::TopLeft
		);
		game.renderer.DrawRect(
			V2_int{ resolution.x * 0.5f - 30, -resolution.y * 0.5f }, V2_int{ 30, resolution.y },
			color::Green, -1.0f, Origin::TopLeft
		);
		game.renderer.DrawRect(
			V2_int{ -resolution.x * 0.5f, resolution.y * 0.5f - 30 }, V2_int{ resolution.x, 30 },
			color::Blue, -1.0f, Origin::TopLeft
		);
		game.renderer.DrawRect(
			V2_int{ -resolution.x * 0.5f, -resolution.y * 0.5f }, V2_int{ 30, resolution.y },
			color::Teal, -1.0f, Origin::TopLeft
		);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ScalingModeScene: QWERT: Switch Resolution Modes", resolution);
	game.scene.Enter<ScalingModeScene>("");
	return 0;
}