#pragma once

#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "core/manager.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/font.h"
#include "renderer/origin.h"
#include "renderer/text.h"
#include "utility/debug.h"

struct TestTextStyles : public Test {
	float ysize	  = 40.0f;
	float yoffset = 4.0f;

	std::string_view font_key = "different_font";
	std::string content		  = "The quick brown fox jumps over the lazy dog";

	Font font{ "resources/fonts/Arial.ttf", static_cast<int>(ysize) };

	V2_int text_size;
	std::vector<Text> texts;

	Text toggle_text; // Visibility toggle text.

	void Shutdown() override {
		game.font.Unload(font_key);
		PTGN_ASSERT(!game.font.Has(font_key));
	}

	void Init() override {
		texts.clear();

		// PTGN_ASSERT(font.IsValid());
		// PTGN_ASSERT(font.GetHeight() > 0);
		// PTGN_ASSERT(!game.font.Has(font_key));
		// PTGN_ASSERT(game.font.Has(font_key));

		game.font.Load(font_key, font);

		// Different colored texts

		texts.emplace_back(content, color::Black, font);

		// PTGN_ASSERT(test002.IsValid());
		// PTGN_ASSERT(test002.GetVisibility() == true);

		// Actual size needed to render font without stretching.
		text_size = Text::GetSize(font, content);
		texts.emplace_back(content, color::Pink, font);
		texts.emplace_back(content, color::Pink, font, FontStyle::Bold);
		texts.emplace_back(content, color::Black, font, FontStyle::Italic);
		texts.emplace_back(content, color::Black, font, FontStyle::Strikethrough);
		texts.emplace_back(content, color::Black, font, FontStyle::Underline);
		texts.emplace_back(
			content, color::Black, font,
			FontStyle::Bold & FontStyle::Italic & FontStyle::Strikethrough & FontStyle::Underline
		);
		texts.emplace_back(
			content, color::Black, font, FontStyle::Normal, FontRenderMode::Shaded, color::Gold
		);
		// Visually this should be bright blue but isnt due to alpha blending (i.e.
		// this works as intended).
		texts.emplace_back(
			content, Color{ 0, 0, 255, 50 }, font, FontStyle::Normal, FontRenderMode::Blended
		);

		toggle_text =
			texts.emplace_back("Press T to toggle my visibility!", color::Black, font_key);
		texts.emplace_back(
			content + "!", color::Red, font_key,
			FontStyle::Bold & FontStyle::Italic & FontStyle::Strikethrough & FontStyle::Underline,
			FontRenderMode::Shaded, color::Cyan
		);
	}

	void Update() override {
		float ystride{ ysize + yoffset };

		V2_float size{ ws.x, ysize };

		for (std::size_t i = 0; i < texts.size(); ++i) {
			game.draw.Text(
				texts[i], { 0.0f, ystride * static_cast<float>(i) }, Origin::TopLeft,
				i == 1 ? V2_float{ text_size } : size
			);
		}

		if (game.input.KeyDown(Key::T)) {
			toggle_text.ToggleVisibility();
		}
	}
};

void TestText() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestTextStyles());

	AddTests(tests);
}