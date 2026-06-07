#include "runtime/graphics/text/text.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/font_style.h"
#include "runtime/graphics/text/text_effect.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

struct TextScene : public Scene {
	static constexpr std::string_view font{ "arial" };

	std::string content{ "The quick brown fox jumps over the lazy dog" };
	float scale{ 40.0f };

	Text CreateLine(const Color& color, int index, std::string_view font_key = font) {
		float stride{ 44.0f };
		float font_size{ 30.0f };

		auto text{ ptgn::CreateText(
			*this,
			{ 0.0f, -static_cast<float>(game_size.y) * 0.5f + stride * static_cast<float>(index) },
			Origin::CenterTop
		) };

		text.Content(content).Font(font_key).Size(font_size).Color(color);

		return text;
	}

	void OnEnter() override {
		ctx().renderer.SetGameSize(game_size);
		SetBackgroundColor(color::LightGray);

		ctx().asset.Load(font, "assets/Arial.ttf");

		// Default font.
		CreateLine(color::Black, 0, {});

		// Colors.
		CreateLine(color::Black, 1);
		CreateLine(color::Green, 2);

		// Styles.
		CreateLine(color::Green, 3).Bold();
		CreateLine(color::Black, 4).Italic();
		CreateLine(color::Black, 5).Strikethrough();
		CreateLine(color::Black, 6).Underline();

		CreateLine(color::Black, 7)
			.Style(
				FontStyle::Bold | FontStyle::Italic | FontStyle::Strikethrough |
				FontStyle::Underline
			);

		CreateLine(color::Black, 8)
			.Content(" ")
			.Content("rich red")
			.Color(color::Red)
			.Bold(true, 0.2f)
			.Underline();

		CreateLine(color::Black, 9)
			.Content(" ")
			.Content("glowing")
			.Color(color::Blue)
			.Glow(color::Blue, 3.0f, 2.0f);

		CreateLine(color::Black, 10)
			.Content(" ")
			.Content("waving")
			.Color(color::Purple)
			.Effect(GlyphEffectType::Wave, 8.0f, 2.0f, 1.5f);
	}

	void OnUpdate() override {
		MoveWASD(ctx().camera, V2_float{ 300.0f } * ctx().dt().count());

		if (ctx().input.KeyHeld(Key::Q)) {
			scale += -10.0f * ctx().dt().count();
			ctx().camera.Zoom(V2_float{ 10.0f } * ctx().dt().count());
		} else if (ctx().input.KeyHeld(Key::E)) {
			scale += 10.0f * ctx().dt().count();
			ctx().camera.Zoom(-V2_float{ 10.0f } * ctx().dt().count());
		}

		scale = std::clamp(scale, 0.0001f, 10000.0f);
	}
};

int main(int, char**) {
	Application app{ "TextScene", game_size };
	PTGN_WITH_EDITOR(app);
	app.StartWith<TextScene>();
}