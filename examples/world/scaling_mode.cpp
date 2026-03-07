#include "renderer/primitives/scaling_mode.h"

#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "platform/input/key.h"
#include "platform/window/window.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int game_size{ 320, 240 }; // 4, 3

class ScalingModeScene : public Scene {
	void OnEnter() override {
		SetBackgroundColor(color::LightBlue);

		app().window.SetSize(window_size);
		app().asset.Load("background", "assets/outlined.jpg");
		app().renderer.SetGameSize(game_size, ScalingMode::Disabled);

		auto s1 = CreateSprite(*this, "background", {}, Origin::Center);
		SetTextureSize(s1, game_size);
		CreateRect(
			*this, V2_float{ -game_size.x * 0.5f, -game_size.y * 0.5f },
			V2_float{ game_size.x, 30 }, color::Red, FillStyle::Solid(), Origin::TopLeft
		);
		CreateRect(
			*this, V2_float{ game_size.x * 0.5f - 30, -game_size.y * 0.5f },
			V2_float{ 30, game_size.y }, color::Green, FillStyle::Solid(), Origin::TopLeft
		);
		CreateRect(
			*this, V2_float{ -game_size.x * 0.5f, game_size.y * 0.5f - 30 },
			V2_float{ game_size.x, 30 }, color::Blue, FillStyle::Solid(), Origin::TopLeft
		);
		CreateRect(
			*this, V2_float{ -game_size.x * 0.5f, -game_size.y * 0.5f },
			V2_float{ 30, game_size.y }, color::Teal, FillStyle::Solid(), Origin::TopLeft
		);
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
	}
};

int main(int, char**) {
	Application app{ "ScalingModeScene: QWERT: Switch Resolution Modes", game_size };
	app.StartWith<ScalingModeScene>();
}