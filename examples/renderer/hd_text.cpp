#include <string>

#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "platform/window/window.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int game_size{ 640, 360 }; // 2, 2

class ResolutionTextScene : public Scene {
	Text text;
	Text text_hd;

	std::string content{ "The quick brown fox jumps over the lazy dog" };
	Color color{ color::White };
	float font_size{ 20 };

	void OnEnter() override {
		app().asset.Load("background", "assets/bg.png");
		app().renderer.SetGameSize(game_size);

		CreateSprite(*this, "background", {});

		text = CreateText(*this, content, color, font_size);
		SetPosition(text, -2 * V2_float{ 0.0f, text.GetFontSize() });
		text.SetHD(false);

		text_hd = CreateText(*this, content, color, font_size);
		SetPosition(text_hd, 2 * V2_float{ 0.0f, text.GetFontSize() });
	}

	void OnUpdate() override {
		app().renderer.DrawText(
			content, -1 * V2_float{ 0.0f, text.GetFontSize() }, color, Origin::Center, font_size,
			{}, {}, {}, {}, false
		);
		app().renderer.DrawText(
			content, 1 * V2_float{ 0.0f, text.GetFontSize() }, color, Origin::Center, font_size, {},
			{}, {}, {}, true
		);
	}
};

int main(int, char**) {
	Application app{ "ResolutionTextScene", window_size };
	app.StartWith<ResolutionTextScene>();
}