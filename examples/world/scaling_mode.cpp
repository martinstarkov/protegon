#include "renderer/pipeline/scaling_mode.h"

#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "platform/key.h"
#include "platform/window.h"
#include "core/graphics/color.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"

#include "runtime/scene/scene_input.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int game_size{ 320, 240 }; // 4, 3

class ScalingModeScene : public Scene {
	void OnEnter() override {
		SetBackgroundColor(color::LightBlue);

		ctx().window.SetSize(window_size);
		ctx().asset.Load("background", "assets/outlined.jpg");
		ctx().renderer.SetGameSize(game_size, ScalingMode::Disabled);

		auto s1 = CreateSprite(*this, "background", {}, Origin::Center);
		SetDisplaySize(s1, game_size);
		CreateRect(
			*this, V2_float{ -game_size.x * 0.5f, -game_size.y * 0.5f },
			V2_float{ game_size.x, 30 }, color::Red, Solid{}, Origin::TopLeft
		);
		CreateRect(
			*this, V2_float{ game_size.x * 0.5f - 30, -game_size.y * 0.5f },
			V2_float{ 30, game_size.y }, color::Green, Solid{}, Origin::TopLeft
		);
		CreateRect(
			*this, V2_float{ -game_size.x * 0.5f, game_size.y * 0.5f - 30 },
			V2_float{ game_size.x, 30 }, color::Blue, Solid{}, Origin::TopLeft
		);
		CreateRect(
			*this, V2_float{ -game_size.x * 0.5f, -game_size.y * 0.5f },
			V2_float{ 30, game_size.y }, color::Teal, Solid{}, Origin::TopLeft
		);
	}

	void OnUpdate() override {
		if (ctx().input.KeyPressed(Key::Q)) {
			ctx().renderer.SetScalingMode(ScalingMode::Disabled);
		}
		if (ctx().input.KeyPressed(Key::W)) {
			ctx().renderer.SetScalingMode(ScalingMode::Stretch);
		}
		if (ctx().input.KeyPressed(Key::E)) {
			ctx().renderer.SetScalingMode(ScalingMode::Letterbox);
		}
		if (ctx().input.KeyPressed(Key::R)) {
			ctx().renderer.SetScalingMode(ScalingMode::IntegerScale);
		}
		if (ctx().input.KeyPressed(Key::T)) {
			ctx().renderer.SetScalingMode(ScalingMode::Overscan);
		}
	}
};

int main(int, char**) {
	Application app{ "ScalingModeScene: QWERT: Switch Resolution Modes", game_size };
	app.StartWith<ScalingModeScene>();
}