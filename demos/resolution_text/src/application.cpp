#include <string>

#include "components/sprite.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/window.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/font.h"
#include "renderer/renderer.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int resolution{ 640, 360 }; // 2, 2

class ResolutionTextScene : public Scene {
	Text text;
	Text text_hd;

	std::string content{ "The quick brown fox jumps over the lazy dog" };
	Color color{ color::White };
	FontSize font_size{ 20 };

	void Enter() override {
		game.window.SetResizable();
		LoadResource("background", "resources/bg.png");
		game.renderer.SetGameSize(resolution);

		CreateSprite(*this, "background", {});

		text = CreateText(*this, content, color, font_size);
		SetPosition(text, -2 * V2_float{ 0.0f, text.GetFontSize() });
		text.SetHD(false);

		text_hd = CreateText(*this, content, color, font_size);
		SetPosition(text_hd, 2 * V2_float{ 0.0f, text.GetFontSize() });
	}

	void Update() override {
		game.renderer.DrawText(
			content, -1 * V2_float{ 0.0f, text.GetFontSize() }, color, Origin::Center, font_size,
			{}, {}, {}, {}, false
		);
		game.renderer.DrawText(
			content, 1 * V2_float{ 0.0f, text.GetFontSize() }, color, Origin::Center, font_size, {},
			{}, {}, {}, true
		);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ResolutionTextScene", window_size);
	game.scene.Enter<ResolutionTextScene>("");
	return 0;
}