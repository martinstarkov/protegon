#include "renderer/primitives/text.h"

#include <string>
#include <string_view>

#include "app/application.h"
#include "core/graphics/color.h"
#include "platform/window/window.h"
#include "renderer/primitives/font.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 } ga

	struct TextScene : public Scene {
	static constexpr std::string_view font{ "arial" };
	std::string content{ "The quick brown fox jumps over the lazy dog" };

	Text CreateText(const Color& color, int index, std::string_view font_key = font) {
		constexpr float stride{ 44.0f };
		FontSize font_size{ 30 };

		auto text = ptgn::CreateText(*this, content, color, font_size, font_key);
		SetDrawOrigin(text, Origin::CenterTop);
		SetPosition(text, { 0.0f, -game_size.y * 0.5f + stride * static_cast<float>(index) });
		return text;
	}

	void OnEnter() override {
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