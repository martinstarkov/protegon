#include "core/app/game.h"
#include "core/app/window.h"
#include "core/ecs/components/sprite.h"
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
constexpr V2_int game_size{ 320, 240 }; // 4, 3

class ScalingModeScene : public Scene {
	void OnEnter() override {
		SetBackgroundColor(color::LightBlue);
		game.window.SetResizable();
		game.window.SetSize(window_size);
		LoadResource("background", "resources/test1.jpg");
		game.renderer.SetGameSize(game_size, ScalingMode::Disabled);
	}

	void OnUpdate() override {
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

		game.renderer.DrawTexture("background", V2_int{ 0, 0 }, game_size, Origin::Center);

		game.renderer.DrawRect(
			V2_int{ -game_size.x * 0.5f, -game_size.y * 0.5f }, V2_int{ game_size.x, 30 },
			color::Red, -1.0f, Origin::TopLeft
		);
		game.renderer.DrawRect(
			V2_int{ game_size.x * 0.5f - 30, -game_size.y * 0.5f }, V2_int{ 30, game_size.y },
			color::Green, -1.0f, Origin::TopLeft
		);
		game.renderer.DrawRect(
			V2_int{ -game_size.x * 0.5f, game_size.y * 0.5f - 30 }, V2_int{ game_size.x, 30 },
			color::Blue, -1.0f, Origin::TopLeft
		);
		game.renderer.DrawRect(
			V2_int{ -game_size.x * 0.5f, -game_size.y * 0.5f }, V2_int{ 30, game_size.y },
			color::Teal, -1.0f, Origin::TopLeft
		);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application app{ "ScalingModeScene: QWERT: Switch Resolution Modes", game_size };
	app.StartWith<ScalingModeScene>();
}