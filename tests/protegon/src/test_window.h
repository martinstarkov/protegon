#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common.h"
#include "core/window.h"
#include "event/key.h"
#include "protegon/color.h"
#include "protegon/font.h"
#include "protegon/game.h"
#include "protegon/log.h"
#include "protegon/polygon.h"
#include "protegon/text.h"
#include "protegon/timer.h"
#include "protegon/vector2.h"
#include "renderer/origin.h"
#include "utility/string.h"
#include "utility/time.h"

class WindowSettingTest : public Test {
	Font font{ "resources/fonts/retro_gaming.ttf", 18 };

	std::vector<Text> texts;

	Text window_resize_hint;
	Text window_size_1;
	Text window_size_2;
	Text window_size_3;
	Text window_mode;
	Text border_mode;
	Text resize_mode;
	Text maximized;
	Text minimized;
	Text window_visible;

	Timer show_timer;

	const V2_float text_offset{ 30.0f, 30.0f };

	V2_float og_window_size;

	void Init() final {
		game.draw.SetClearColor(color::Silver);
		og_window_size = ws;
		game.window.SetSize(og_window_size);

		texts.clear();

		window_resize_hint = texts.emplace_back(font, "", color::Black);
		window_size_1	   = texts.emplace_back(font, "", color::Black);
		window_size_2	   = texts.emplace_back(font, "", color::Black);
		window_size_3	   = texts.emplace_back(font, "", color::Black);
		window_mode		   = texts.emplace_back(font, "", color::Black);
		border_mode		   = texts.emplace_back(font, "", color::Black);
		resize_mode		   = texts.emplace_back(font, "", color::Black);
		maximized		   = texts.emplace_back(font, "", color::Black);
		minimized		   = texts.emplace_back(font, "", color::Black);
		window_visible	   = texts.emplace_back(font, "", color::Black);
	}

	void Shutdown() {
		game.window.SetSetting(WindowSetting::Windowed);
		game.window.SetSetting(WindowSetting::Bordered);
		game.window.SetSetting(WindowSetting::FixedSize);
		game.window.SetSetting(WindowSetting::Shown);
		game.window.SetSize(og_window_size);
	}

	void Update() final {
		if (game.input.KeyDown(Key::Z)) {
			game.window.SetSize(og_window_size);
		}
		if (game.input.KeyDown(Key::Q)) {
			game.window.SetSetting(WindowSetting::Windowed);
		}
		if (game.input.KeyDown(Key::W)) {
			game.window.SetSetting(WindowSetting::Fullscreen);
		}
		if (game.input.KeyDown(Key::E)) {
			game.window.SetSetting(WindowSetting::Borderless);
		}
		if (game.input.KeyDown(Key::R)) {
			game.window.SetSetting(WindowSetting::Bordered);
		}
		if (game.input.KeyDown(Key::T)) {
			game.window.SetSetting(WindowSetting::Resizable);
		}
		if (game.input.KeyDown(Key::Y)) {
			game.window.SetSetting(WindowSetting::FixedSize);
		}
		if (game.input.KeyDown(Key::U)) {
			game.window.SetSetting(WindowSetting::Maximized);
		}
		if (game.input.KeyDown(Key::I)) {
			game.window.SetSetting(WindowSetting::Minimized);
		}
		if (game.input.KeyDown(Key::O)) {
			game.window.SetSetting(WindowSetting::Shown);
		}
		if (game.input.KeyDown(Key::P)) {
			game.window.SetSetting(WindowSetting::Hidden);
			show_timer.Start();
		}

		if (show_timer.IsRunning() && show_timer.Completed(milliseconds{ 500 })) {
			show_timer.Stop();
			game.window.SetSetting(WindowSetting::Shown);
		}
	}

	void UpdateOptions(
		Text& text, const std::string& prefix,
		const std::vector<std::pair<WindowSetting, std::string>>& settings
	) {
		text.SetContent(prefix);
		for (const auto& [setting, suffix] : settings) {
			if (game.window.GetSetting(setting)) {
				text.SetContent(prefix + suffix);
			}
		}
	};

	void Draw() final {
		window_resize_hint.SetContent("Z to Force Window Size to " + ToString(og_window_size));
		window_size_1.SetContent("Window Drawable Size: " + ToString(game.window.GetSize(0)));
		window_size_2.SetContent("Window Pixel Size: " + ToString(game.window.GetSize(1)));
		window_size_3.SetContent("Window Size: " + ToString(game.window.GetSize(2)));

		UpdateOptions(
			window_mode, "Window Mode (Q/W): ",
			{ { WindowSetting::Windowed, "Windowed" }, { WindowSetting::Fullscreen, "Fullscreen" } }
		);
		UpdateOptions(
			border_mode, "Border Mode (E/R): ",
			{ { WindowSetting::Borderless, "Borderless" }, { WindowSetting::Bordered, "Bordered" } }
		);
		UpdateOptions(
			resize_mode, "Resize Mode (T/Y): ",
			{ { WindowSetting::Resizable, "Resizable" }, { WindowSetting::FixedSize, "FixedSize" } }
		);
		UpdateOptions(maximized, "Maximized (U): ", { { WindowSetting::Maximized, "True" } });
		UpdateOptions(minimized, "Minimized (I): ", { { WindowSetting::Minimized, "True" } });
		UpdateOptions(
			window_visible, "Window Visible (O/P): ",
			{ { WindowSetting::Shown, "Shown" }, { WindowSetting::Hidden, "Hidden" } }
		);

		V2_float offset;
		for (std::size_t i = 0; i < texts.size(); i++) {
			const auto& t = texts[i];
			Rectangle<float> rect;
			rect.origin = Origin::TopLeft;
			rect.pos.x	= text_offset.x;
			rect.pos.y	= text_offset.y + offset.y;
			rect.size	= t.GetSize();
			t.Draw(rect);
			offset += rect.size;
		}
	}
};

void TestWindow() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new WindowSettingTest());

	AddTests(tests);
}