#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "platform/window/window.h"
#include "renderer/renderer.h"
#include "runtime/ecs/components/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int game_size{ 320, 240 }; // 4, 3

class ScalingModeScene : public Scene {
	void OnEnter() override {
		SetBackgroundColor(color::LightBlue);

		app().window.SetSize(window_size);
		app().asset.Load("background", "assets/test1.jpg");
		app().renderer.SetGameSize(game_size, ScalingMode::Disabled);
	}

	void OnUpdate() override {
		if (input.KeyPressed(Key::Q)) {
			app().renderer.SetScalingMode(ScalingMode::Disabled);
		}
		if (input.KeyPressed(Key::W)) {
			app().renderer.SetScalingMode(ScalingMode::Stretch);
		}
		if (input.KeyPressed(Key::E)) {
			app().renderer.SetScalingMode(ScalingMode::Letterbox);
		}
		if (input.KeyPressed(Key::R)) {
			app().renderer.SetScalingMode(ScalingMode::IntegerScale);
		}
		if (input.KeyPressed(Key::T)) {
			app().renderer.SetScalingMode(ScalingMode::Overscan);
		}

		app().renderer.DrawTexture("background", V2_int{ 0, 0 }, game_size, Origin::Center);

		app().renderer.DrawRect(
			V2_int{ -game_size.x * 0.5f, -game_size.y * 0.5f }, V2_int{ game_size.x, 30 },
			color::Red, -1.0f, Origin::TopLeft
		);
		app().renderer.DrawRect(
			V2_int{ game_size.x * 0.5f - 30, -game_size.y * 0.5f }, V2_int{ 30, game_size.y },
			color::Green, -1.0f, Origin::TopLeft
		);
		app().renderer.DrawRect(
			V2_int{ -game_size.x * 0.5f, game_size.y * 0.5f - 30 }, V2_int{ game_size.x, 30 },
			color::Blue, -1.0f, Origin::TopLeft
		);
		app().renderer.DrawRect(
			V2_int{ -game_size.x * 0.5f, -game_size.y * 0.5f }, V2_int{ 30, game_size.y },
			color::Teal, -1.0f, Origin::TopLeft
		);
	}
};

int main(int, char**) {
	Application app{ "ScalingModeScene: QWERT: Switch Resolution Modes", game_size };
	app.StartWith<ScalingModeScene>();
}