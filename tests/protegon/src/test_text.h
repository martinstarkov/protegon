#pragma once

#include "protegon/text.h"
#include "protegon/window.h"
#include "core/game.h"

using namespace ptgn;

bool TestText() {
	PTGN_INFO("Starting text tests...");

	window::SetSize({ 800, 660 });
	window::Show();

	V2_int window_size = window::GetSize();

	std::size_t font_key = Hash("different_font");

	std::string content = "The quick brown fox jumps over the lazy dog";

	int ysize = 40;

	Font font{ "resources/fonts/arial.ttf", ysize };

	PTGN_ASSERT(font.IsValid());

	PTGN_ASSERT(font.GetHeight() > 0);

	PTGN_ASSERT(!font::Has(font_key));
	
	font::Load(font_key, "resources/fonts/retro_gaming.ttf", ysize);

	PTGN_ASSERT(font::Has(font_key));

	Text test001; // empty text

	// Different colored texts

	Text test002{ font, content, color::Black };
	PTGN_ASSERT(test002.IsValid());
	PTGN_ASSERT(test002.GetVisibility() == true);
	// Actual size needed to render font without stretching.
	V2_int text_size = Text::GetSize(font, content);
	Text test003{ font, content, color::Pink };
	Text test004{ font, content, color::Pink, FontStyle::Bold };
	Text test005{ font, content, color::Black, FontStyle::Italic };
	Text test006{ font, content, color::Black, FontStyle::Strikethrough };
	Text test007{ font, content, color::Black, FontStyle::Underline };
	Text test008{ font, content, color::Black, FontStyle::Bold && FontStyle::Italic && FontStyle::Strikethrough && FontStyle::Underline };
	Text test009{ font, content, color::Black, FontStyle::Normal, FontRenderMode::Shaded, color::Gold };
	// Visually this should be bright blue but isnt due to alpha blending (i.e. this works as intended).
	Text test010{ font, content, { 0, 0, 255, 50 }, FontStyle::Normal, FontRenderMode::Blended };
	Text test011{ font_key, "Press T to toggle my visibility!", color::Black };
	Text test012{ font_key, content + "!", color::Red, FontStyle::Bold && FontStyle::Italic && FontStyle::Strikethrough && FontStyle::Underline, FontRenderMode::Shaded, color::Cyan};

	/*const FontOrKey& font,
	const std::string& content,
	const Color& text_color,
	FontStyle font_style = FontStyle::Normal,
	FontRenderMode render_mode = FontRenderMode::Solid,
	const Color& shading_color = color::White*/

	window::RepeatUntilQuit([&]() {
		renderer::SetDrawColor(color::White);
		renderer::Clear();

		//test001.Draw({ { 0, 0 }, { 30, 30 } });

		int yoffset = 4;
		int ystride = ysize + yoffset;

		test002.Draw(Rectangle<int>{ { 0, ystride * 0 }, { window_size.x, ysize } });
		test003.Draw(Rectangle<int>{ { 0, ystride * 1 }, text_size }); // Actual size needed to render font without stretching.
		test004.Draw(Rectangle<int>{ { 0, ystride * 2 }, { window_size.x, ysize } });
		test005.Draw(Rectangle<int>{ { 0, ystride * 3 }, { window_size.x, ysize } });
		test006.Draw(Rectangle<int>{ { 0, ystride * 4 }, { window_size.x, ysize } });
		test007.Draw(Rectangle<int>{ { 0, ystride * 5 }, { window_size.x, ysize } });
		test008.Draw(Rectangle<int>{ { 0, ystride * 6 }, { window_size.x, ysize } });
		test009.Draw(Rectangle<int>{ { 0, ystride * 7 }, { window_size.x, ysize } });
		test010.Draw(Rectangle<int>{ { 0, ystride * 8 }, { window_size.x, ysize } });
		test011.Draw(Rectangle<int>{ { 0, ystride * 9 }, { window_size.x, ysize } });
		test012.Draw(Rectangle<int>{ { 0, ystride * 10 }, { window_size.x, ysize } });

		if (input::KeyDown(Key::T)) {
			test011.SetVisibility(!test011.GetVisibility());
		}

		renderer::Present();
	});

	PTGN_ASSERT(font::Has(font_key));

	font::Unload(font_key);

	PTGN_ASSERT(!font::Has(font_key));
	
	PTGN_INFO("All text passed!");
	return true;
}