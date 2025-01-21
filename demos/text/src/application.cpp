#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct TextExample : public Scene {
	float ysize	  = 40.0f;
	float yoffset = 4.0f;

	std::string_view font_key = "different_font";
	std::string content		  = "The quick brown fox jumps over the lazy dog";

	Font font{ "resources/Arial.ttf", static_cast<int>(ysize) };

	V2_int text_size;
	std::vector<Text> texts;

	Text toggle_text; // Visibility toggle text.

	V2_float ws;

	void Enter() override {
		ws = game.window.GetSize();
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

	void Exit() override {
		game.font.Unload(font_key);
		PTGN_ASSERT(!game.font.Has(font_key));
	}

	void Update() override {
		float ystride{ ysize + yoffset };

		V2_float size{ ws.x, ysize };

		for (std::size_t i = 0; i < texts.size(); ++i) {
			texts[i].Draw({ { 0.0f, ystride * static_cast<float>(i) },
							i == 1 ? V2_float{ text_size } : size,
							Origin::TopLeft });
		}

		if (game.input.KeyDown(Key::T)) {
			toggle_text.ToggleVisibility();
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TextExample: T (toggle visibility)", window_size);
	game.scene.Enter<TextExample>("text_example");
	return 0;
}