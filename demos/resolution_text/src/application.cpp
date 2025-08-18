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
	V2_int center{ resolution / 2 };

	void Enter() override {
		game.window.SetSetting(WindowSetting::Resizable);
		LoadResource("background", "resources/bg.png");
		game.renderer.SetLogicalResolution(resolution);

		// TODO: Readd once debug stuff is drawn on top.
		// CreateSprite(*this, "background", resolution / 2.0f);

		text = CreateText(*this, content, color, font_size);
		SetPosition(text, center - 2 * V2_float{ 0.0f, text.GetFontSize() });
		text.SetHD(false);

		text_hd = CreateText(*this, content, color, font_size);
		SetPosition(text_hd, center - 1 * V2_float{ 0.0f, text.GetFontSize() });
	}

	void Update() override {
		DrawDebugText(
			content, center - 0 * V2_float{ 0.0f, text.GetFontSize() }, color, Origin::Center,
			font_size, false
		);
		DrawDebugText(
			content, center + 1 * V2_float{ 0.0f, text.GetFontSize() }, color, Origin::Center,
			font_size, true
		);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ResolutionTextScene", window_size);
	game.scene.Enter<ResolutionTextScene>("");
	return 0;
}