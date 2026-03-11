#include "runtime/graphics/text.h"

#include <string>
#include <string_view>

#include "app/application.h"
#include "app/context.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/font.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

struct TextScene : public Scene {
	static constexpr std::string_view font{ "arial" };
	std::string content{ "The quick brown fox jumps over the lazy dog" };

	Text CreateText(const Color& color, int index, std::string_view font_key = font) {
		constexpr float stride{ 44.0f };
		float font_size{ 30.0f };

		auto text = ptgn::CreateText(*this, content, color, font_size, font_key);
		SetDrawOrigin(text, Origin::CenterTop);
		SetPosition(text, { 0.0f, -game_size.y * 0.5f + stride * static_cast<float>(index) });
		return text;
	}

	void OnEnter() override {
		app().renderer.SetGameSize(game_size);
		SetBackgroundColor(color::LightGray);

		app().asset.Load(font, "assets/Arial.ttf");

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

int main(int, char**) {
	Application app{ "TextScene", game_size };
	app.StartWith<TextScene>();
}