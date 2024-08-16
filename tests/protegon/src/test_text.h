#pragma once

#include "protegon/game.h"
#include "protegon/text.h"
#include "utility/debug.h"

using namespace ptgn;

std::size_t font_key = Hash("different_font");

std::string content = "The quick brown fox jumps over the lazy dog";

int ysize = 40;

bool TestText() {
	PTGN_INFO("Starting text tests...");

	static Font font{ "resources/fonts/Arial.ttf", ysize };
	game.font.Load(font_key, "resources/fonts/retro_gaming.ttf", ysize);

	// PTGN_ASSERT(font.IsValid());
	//
	// PTGN_ASSERT(font.GetHeight() > 0);
	//
	// PTGN_ASSERT(!game.font.Has(font_key));

	// PTGN_ASSERT(game.font.Has(font_key));

	static Text test001; // empty text

	// Different colored texts

	static Text test002{ font, content, color::Black };

	// PTGN_ASSERT(test002.IsValid());
	// PTGN_ASSERT(test002.GetVisibility() == true);

	// Actual size needed to render font without stretching.
	static V2_int text_size = Text::GetSize(font, content);
	static Text test003{ font, content, color::Pink };
	static Text test004{ font, content, color::Pink, FontStyle::Bold };
	static Text test005{ font, content, color::Black, FontStyle::Italic };
	static Text test006{ font, content, color::Black, FontStyle::Strikethrough };
	static Text test007{ font, content, color::Black, FontStyle::Underline };
	static Text test008{ font, content, color::Black,
						 FontStyle::Bold && FontStyle::Italic && FontStyle::Strikethrough &&
							 FontStyle::Underline };
	static Text test009{
		font, content, color::Black, FontStyle::Normal, FontRenderMode::Shaded, color::Gold
	};
	// Visually this should be bright blue but isnt due to alpha blending (i.e.
	// this works as intended).
	static Text test010{
		font, content, { 0, 0, 255, 50 }, FontStyle::Normal, FontRenderMode::Blended
	};
	static Text test011{ font_key, "Press T to toggle my visibility!", color::Black };
	static Text test012{ font_key,
						 content + "!",
						 color::Red,
						 FontStyle::Bold && FontStyle::Italic && FontStyle::Strikethrough &&
							 FontStyle::Underline,
						 FontRenderMode::Shaded,
						 color::Cyan };

	game.window.SetSize({ 800, 660 });
	game.window.Show();

	/*const FontOrKey& font,
	const std::string& content,
	const Color& text_color,
	FontStyle font_style = FontStyle::Normal,
	FontRenderMode render_mode = FontRenderMode::Solid,
	const Color& shading_color = color::White*/
	game.renderer.SetClearColor(color::White);

	game.PushLoopFunction([&]() {
		game.scene.GetTopActive().camera.ResetPrimaryToWindow();
		// test001.Draw({ { 0, 0 }, { 30, 30 } });

		int ws_x = game.window.GetSize().x;

		int yoffset = 4;
		int ystride = ysize + yoffset;

		test002.Draw(Rectangle<int>{ { 0, ystride * 0 }, { ws_x, ysize } });
		test003.Draw(Rectangle<int>{ { 0, ystride * 1 }, text_size }
		); // Actual size needed to render font without stretching.
		test004.Draw(Rectangle<int>{ { 0, ystride * 2 }, { ws_x, ysize } });
		test005.Draw(Rectangle<int>{ { 0, ystride * 3 }, { ws_x, ysize } });
		test006.Draw(Rectangle<int>{ { 0, ystride * 4 }, { ws_x, ysize } });
		test007.Draw(Rectangle<int>{ { 0, ystride * 5 }, { ws_x, ysize } });
		test008.Draw(Rectangle<int>{ { 0, ystride * 6 }, { ws_x, ysize } });
		test009.Draw(Rectangle<int>{ { 0, ystride * 7 }, { ws_x, ysize } });
		test010.Draw(Rectangle<int>{ { 0, ystride * 8 }, { ws_x, ysize } });
		test011.Draw(Rectangle<int>{ { 0, ystride * 9 }, { ws_x, ysize } });
		test012.Draw(Rectangle<int>{ { 0, ystride * 10 }, { ws_x, ysize } });

		if (game.input.KeyDown(Key::T)) {
			test011.SetVisibility(!test011.GetVisibility());
		}
		if (game.input.KeyDown(Key::ESCAPE)) {
			game.PopLoopFunction();
		}
	});

	/*game.window.SetTitle("");

	PTGN_ASSERT(game.font.Has(font_key));

	game.font.Unload(font_key);

	PTGN_ASSERT(!game.font.Has(font_key));*/

	PTGN_INFO("All text tests passed!");
	return true;
}