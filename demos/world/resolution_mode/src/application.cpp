#include "ecs/components/sprite.h"
#include "core/app/application.h"
#include "core/app/window.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "ecs/components/origin.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int resolution{ 320, 240 }; // 4, 3

class ScalingModeScene : public Scene {
	void Enter() override {
		SetBackgroundColor(color::LightBlue);
		Application::Get().window_.SetResizable();
		Application::Get().window_.SetSize(window_size);
		LoadResource("background", "resources/test1.jpg");
		Application::Get().render_.SetGameSize(resolution, ScalingMode::Disabled);
	}

	void Update() override {
		if (input.KeyDown(Key::Q)) {
			Application::Get().render_.SetScalingMode(ScalingMode::Disabled);
		}
		if (input.KeyDown(Key::W)) {
			Application::Get().render_.SetScalingMode(ScalingMode::Stretch);
		}
		if (input.KeyDown(Key::E)) {
			Application::Get().render_.SetScalingMode(ScalingMode::Letterbox);
		}
		if (input.KeyDown(Key::R)) {
			Application::Get().render_.SetScalingMode(ScalingMode::IntegerScale);
		}
		if (input.KeyDown(Key::T)) {
			Application::Get().render_.SetScalingMode(ScalingMode::Overscan);
		}

		Application::Get().render_.DrawTexture("background", V2_int{ 0, 0 }, resolution, Origin::Center);

		Application::Get().render_.DrawRect(
			V2_int{ -resolution.x * 0.5f, -resolution.y * 0.5f }, V2_int{ resolution.x, 30 },
			color::Red, -1.0f, Origin::TopLeft
		);
		Application::Get().render_.DrawRect(
			V2_int{ resolution.x * 0.5f - 30, -resolution.y * 0.5f }, V2_int{ 30, resolution.y },
			color::Green, -1.0f, Origin::TopLeft
		);
		Application::Get().render_.DrawRect(
			V2_int{ -resolution.x * 0.5f, resolution.y * 0.5f - 30 }, V2_int{ resolution.x, 30 },
			color::Blue, -1.0f, Origin::TopLeft
		);
		Application::Get().render_.DrawRect(
			V2_int{ -resolution.x * 0.5f, -resolution.y * 0.5f }, V2_int{ 30, resolution.y },
			color::Teal, -1.0f, Origin::TopLeft
		);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("ScalingModeScene: QWERT: Switch Resolution Modes", resolution);
	Application::Get().scene_.Enter<ScalingModeScene>("");
	return 0;
}