#include "runtime/graphics/text/text.h"

#include <string>
#include <string_view>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/draw_context.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text_system.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

struct TextScene : public Scene {
	TextSystem text_system;

	static constexpr std::string_view font{ "arial" };
	std::string content{ "The quick brown fox jumps over the lazy dog" };

	impl::MsdfFontData msdf_font;

	Text CreateText(const Color& color, int index, std::string_view font_key = font) {
		constexpr float stride{ 44.0f };
		float font_size{ 30.0f };

		auto text = ptgn::CreateText(
			*this, { 0.0f, -game_size.y * 0.5f + stride * static_cast<float>(index) }, content,
			color, font_size, font_key, Origin::CenterTop
		);
		return text;
	}

	float scale{ 40.0f };

	void OnEnter() override {
		ctx().renderer.SetGameSize(game_size);
		SetBackgroundColor(color::LightGray);

		ctx().asset.Load(font, "assets/Arial.ttf");

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

		msdf_font = { ctx().global_renderer_, "assets/fonts/LiberationSans-Regular.ttf", 0, {} };
	}

	void OnUpdate() override {
		MoveWASD(ctx().camera, V2_float{ 300 } * ctx().dt().count());
		if (ctx().input.KeyHeld(Key::Q)) {
			scale += -10.0f * ctx().dt().count();
			ctx().camera.Zoom(V2_float{ 10.0f } * ctx().dt().count());
		} else if (ctx().input.KeyHeld(Key::E)) {
			scale += 10.0f * ctx().dt().count();
			ctx().camera.Zoom(-V2_float{ 10.0f } * ctx().dt().count());
		}
		scale = std::clamp(scale, 0.0001f, 10000.0f);
	}

	void OnRender() override {
		DrawContext draw_context{ ctx().global_renderer_ };

		/*draw_context.DrawTexture(
			msdf_font.GetAtlasTexture(), {}, 0.0f, msdf_font.GetAtlasSize(), Origin::Center,
			color::White, impl::GetDefaultTextureCoordinates<false>(), std::nullopt, -1
		);*/

		text_system.DrawText(draw_context, {}, &msdf_font);
	}
};

int main(int, char**) {
	Application app{ "TextScene", game_size };
	app.StartWith<TextScene>();
}