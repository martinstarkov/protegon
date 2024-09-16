#pragma once

#include <memory>
#include <new>
#include <string>
#include <vector>

#include "common.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "protegon/color.h"
#include "protegon/font.h"
#include "protegon/game.h"
#include "protegon/hash.h"
#include "protegon/polygon.h"
#include "protegon/text.h"
#include "protegon/vector2.h"
#include "renderer/origin.h"

struct TestTextStyles : public Test {
	float ysize	  = 40.0f;
	float yoffset = 4.0f;

	std::size_t font_key = Hash("different_font");
	std::string content	 = "The quick brown fox jumps over the lazy dog";

	Font font{ "resources/fonts/Arial.ttf", static_cast<int>(ysize) };

	Text test001; // empty text
	Text test002;
	V2_int text_size;
	Text test003;
	Text test004;
	Text test005;
	Text test006;
	Text test007;
	Text test008;
	Text test009;
	Text test010;
	Text test011;
	Text test012;

	void Shutdown() override {
		game.font.Unload(font_key);
		PTGN_ASSERT(!game.font.Has(font_key));
	}

	void Init() override {
		// PTGN_ASSERT(font.IsValid());
		// PTGN_ASSERT(font.GetHeight() > 0);
		// PTGN_ASSERT(!game.font.Has(font_key));
		// PTGN_ASSERT(game.font.Has(font_key));

		game.font.Load(font_key, font);

		// Different colored texts

		test002 = { font, content, color::Black };

		// PTGN_ASSERT(test002.IsValid());
		// PTGN_ASSERT(test002.GetVisibility() == true);

		// Actual size needed to render font without stretching.
		text_size = Text::GetSize(font, content);
		test003	  = { font, content, color::Pink };
		test004	  = { font, content, color::Pink, FontStyle::Bold };
		test005	  = { font, content, color::Black, FontStyle::Italic };
		test006	  = { font, content, color::Black, FontStyle::Strikethrough };
		test007	  = { font, content, color::Black, FontStyle::Underline };
		test008	  = { font, content, color::Black,
					  FontStyle::Bold & FontStyle::Italic & FontStyle::Strikethrough &
						  FontStyle::Underline };
		test009	  = { font,		  content, color::Black, FontStyle::Normal, FontRenderMode::Shaded,
					  color::Gold };
		// Visually this should be bright blue but isnt due to alpha blending (i.e.
		// this works as intended).
		test010 = { font, content, { 0, 0, 255, 50 }, FontStyle::Normal, FontRenderMode::Blended };
		test011 = { font_key, "Press T to toggle my visibility!", color::Black };
		test012 = { font_key,
					content + "!",
					color::Red,
					FontStyle::Bold & FontStyle::Italic & FontStyle::Strikethrough &
						FontStyle::Underline,
					FontRenderMode::Shaded,
					color::Cyan };
	}

	void Update() override {
		float ystride{ ysize + yoffset };

		test002.Draw(Rectangle{ { 0.0f, ystride * 0.0f }, { ws.x, ysize }, Origin::TopLeft });
		// Actual size needed to render font without stretching.
		test003.Draw(Rectangle{ { 0.0f, ystride * 1.0f }, V2_float{ text_size }, Origin::TopLeft });
		test004.Draw(Rectangle{ { 0.0f, ystride * 2.0f }, { ws.x, ysize }, Origin::TopLeft });
		test005.Draw(Rectangle{ { 0.0f, ystride * 3.0f }, { ws.x, ysize }, Origin::TopLeft });
		test006.Draw(Rectangle{ { 0.0f, ystride * 4.0f }, { ws.x, ysize }, Origin::TopLeft });
		test007.Draw(Rectangle{ { 0.0f, ystride * 5.0f }, { ws.x, ysize }, Origin::TopLeft });
		test008.Draw(Rectangle{ { 0.0f, ystride * 6.0f }, { ws.x, ysize }, Origin::TopLeft });
		test009.Draw(Rectangle{ { 0.0f, ystride * 7.0f }, { ws.x, ysize }, Origin::TopLeft });
		test010.Draw(Rectangle{ { 0.0f, ystride * 8.0f }, { ws.x, ysize }, Origin::TopLeft });
		test011.Draw(Rectangle{ { 0.0f, ystride * 9.0f }, { ws.x, ysize }, Origin::TopLeft });
		test012.Draw(Rectangle{ { 0.0f, ystride * 10.0f }, { ws.x, ysize }, Origin::TopLeft });

		if (game.input.KeyDown(Key::T)) {
			test011.SetVisibility(!test011.GetVisibility());
		}
	}
};

void TestText() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestTextStyles());

	AddTests(tests);
}