#include "runtime/graphics/text/text.h"

#include <algorithm>
#include <cmath>
#include <string_view>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/text_effect.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

struct TextEffectsScene : public Scene {
	float scale{ 40.0f };

	Text reveal_text;
	static constexpr std::string_view reveal_content{ "Sine reveal hides and shows this text" };
	float reveal_time{ 0.0f };

	std::size_t current_line{ 0 };

	Text CreateLine(
		std::string_view text_content, const Color& color = color::Black, float font_size = 26.0f,
		std::string_view font_key = {}
	) {
		float stride{ 34.0f };
		float top{ -static_cast<float>(game_size.y) * 0.5f + 16.0f };

		auto text{ CreateText(
			*this, { 0.0f, top + stride * static_cast<float>(current_line) }, Origin::CenterTop
		) };

		text.Content(text_content).Font(font_key).Size(font_size).Color(color);

		current_line++;

		return text;
	}

	void OnEnter() override {
		ctx().renderer.SetGameSize(game_size);
		SetBackgroundColor(color::LightGray);

		ctx().asset.Load("custom_font", "assets/retro_gaming.ttf");

		CreateLine("Plain black text (default font)", color::Black, 26.0f);
		CreateLine("Plain black text (custom font)", color::Black, 26.0f, "custom_font");
		CreateLine("Colored text", color::Green);

		CreateLine("Fake bold", color::Black).Bold(true, 0.25f);
		CreateLine("Italic shear", color::Black).Italic();
		CreateLine("Underline", color::Black).Underline();
		CreateLine("Strikethrough", color::Black).Strikethrough();

		CreateLine("Bold + italic + underline + strikethrough", color::Black)
			.Bold(true, 0.2f)
			.Italic()
			.Underline()
			.Strikethrough();

		auto rich{ CreateLine("Rich text: ", color::Black) };

		rich.Content("red bold ").Color(color::Red).Bold(true, 0.2f);

		rich.Content("green italic ").Color(color::Green).Bold(false).Italic();

		rich.Content("blue underline").Color(color::Blue).Italic(false).Underline();

		CreateLine("Black outline", color::White).Outline(color::Black, 1.5f, 1.0f);

		CreateLine("Soft red outline", color::White).Outline(color::Red, 2.5f, 2.0f);

		CreateLine("Drop shadow", color::Black)
			.Shadow(color::Black.WithAlpha(120), { 3.0f, -3.0f }, 2.0f, 2.5f);

		CreateLine("Outline + shadow", color::White)
			.Outline(color::Black, 1.5f, 1.0f)
			.Shadow(color::Black.WithAlpha(130), { 3.0f, -3.0f }, 2.0f, 2.5f);

		CreateLine("Outer glow", color::Blue).OuterGlow(color::Blue.WithAlpha(160), 4.0f, 3.0f);

		CreateLine("Inner glow", color::Blue).InnerGlow(color::White.WithAlpha(45), 3.0f, 2.0f);

		CreateLine("Full glow", color::Blue).Glow(color::Blue.WithAlpha(150), 4.0f, 2.0f, 3.0f);

		CreateLine("Outline + outer glow", color::White)
			.Outline(color::Black, 1.5f, 1.0f)
			.OuterGlow(color::Purple.WithAlpha(150), 4.0f, 3.0f);

		CreateLine("Wobble effect", color::Purple)
			.Effect(GlyphEffectType::Wobble, 4.0f, 2.5f, 2.0f);

		CreateLine("Wave effect", color::Purple).Effect(GlyphEffectType::Wave, 7.0f, 2.0f, 1.5f);

		CreateLine("Shake effect", color::Red).Effect(GlyphEffectType::Shake, 1.5f, 1.0f, 1.0f);

		CreateLine("Pulse effect", color::Green).Effect(GlyphEffectType::Pulse, 0.12f, 1.0f, 3.0f);

		reveal_text = CreateLine(reveal_content, color::Black);

		CreateLine("Tracking + kerning", color::Black).Tracking(0.5f).Kerning(0.5f);
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

		reveal_time += ctx().dt().count();

		float phase{ (std::sin(reveal_time * 2.0f) + 1.0f) * 0.5f };

		std::size_t min_reveal{ 4 };
		std::size_t max_reveal{ reveal_content.size() };

		auto reveal_count{ static_cast<std::size_t>(std::ceil(
			static_cast<float>(min_reveal) + phase * static_cast<float>(max_reveal - min_reveal)
		)) };

		reveal_text.Reveal(reveal_count);
	}
};

int main(int, char**) {
	Application app{ "TextEffectsScene", game_size };
	PTGN_WITH_EDITOR(app);
	app.StartWith<TextEffectsScene>();
}