#include <string>
#include <string_view>

#include "core/entity.h"
#include "core/game.h"
#include "renderer/api/color.h"
#include "renderer/font.h"
#include "renderer/text.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

struct TextScene : public Scene {
	static constexpr std::string_view font{ "arial" };
	std::string content{ "The quick brown fox jumps over the lazy dog" };

	Text CreateText(const Color& color, int index, std::string_view font_key = font) {
		constexpr float stride{ 44.0f };

		return ptgn::CreateText(*this, content, color, font_key)
			.SetOrigin(Origin::TopLeft)
			.SetPosition({ 0.0f, stride * static_cast<float>(index) });
	}

	void Enter() override {
		LoadResource(font, "resources/Arial.ttf");

		// Default font.
		CreateText(color::Black, 0, {});

		// Colors.
		CreateText(color::Black, 1);
		CreateText(color::Green, 2);

		// Styles.
		CreateText(color::Green, 3).SetFontStyle(FontStyle::Bold);
		CreateText(color::Black, 4).SetFontStyle(FontStyle::Italic);
		CreateText(color::Black, 5).SetFontStyle(FontStyle::Strikethrough);
		CreateText(color::Black, 6).SetFontStyle(FontStyle::Underline);
		CreateText(color::Black, 7)
			.SetFontStyle(
				FontStyle::Bold & FontStyle::Italic & FontStyle::Strikethrough &
				FontStyle::Underline
			);

		// Shading.
		CreateText(color::Black, 8)
			.SetFontRenderMode(FontRenderMode::Shaded)
			.SetShadingColor(color::Gold);

		// Blending (visually this text should be bright blue but isnt due to alpha blending).
		CreateText(Color{ 0, 0, 255, 50 }, 9).SetFontRenderMode(FontRenderMode::Blended);

		// Everything at the same time.
		CreateText(color::Red, 10)
			.SetFontStyle(
				FontStyle::Bold & FontStyle::Italic & FontStyle::Strikethrough &
				FontStyle::Underline
			)
			.SetFontRenderMode(FontRenderMode::Shaded)
			.SetShadingColor(color::Cyan);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TextScene", { 800, 800 }, color::LightGray);
	game.scene.Enter<TextScene>("");
	return 0;
}