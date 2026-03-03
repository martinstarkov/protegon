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

		impl::DrawQuadTexture(
			app().renderer, *app().asset.GetTexture("background"), Transform{}, game_size,
			Origin::Center, color::White, Depth{}, BlendMode::Blend,
			GetTextureCoordinates({}, false)
		);

		impl::DrawShape(
			app().renderer, Rect{ game_size.x, 30 },
			Transform{ V2_float{ -game_size.x * 0.5f, -game_size.y * 0.5f } }, color::Red,
			FillStyle::Solid(), Origin::TopLeft, Depth{}, BlendMode::Blend
		);
		impl::DrawShape(
			app().renderer, Rect{ 30, game_size.y },
			Transform{ V2_float{ game_size.x * 0.5f - 30, -game_size.y * 0.5f } }, color::Green,
			FillStyle::Solid(), Origin::TopLeft, Depth{}, BlendMode::Blend
		);
		impl::DrawShape(
			app().renderer, Rect{ game_size.x, 30 },
			Transform{ V2_float{ -game_size.x * 0.5f, game_size.y * 0.5f - 30 } }, color::Blue,
			FillStyle::Solid(), Origin::TopLeft, Depth{}, BlendMode::Blend
		);
		impl::DrawShape(
			app().renderer, Rect{ 30, game_size.y },
			Transform{ V2_float{ -game_size.x * 0.5f, -game_size.y * 0.5f } }, color::Teal,
			FillStyle::Solid(), Origin::TopLeft, Depth{}, BlendMode::Blend
		);
	}
};

int main(int, char**) {
	Application app{ "ScalingModeScene: QWERT: Switch Resolution Modes", game_size };
	app.StartWith<ScalingModeScene>();
}