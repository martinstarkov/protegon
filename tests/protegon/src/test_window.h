#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "core/window.h"
#include "event/key.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/font.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "renderer/text.h"
#include "utility/string.h"
#include "utility/time.h"
#include "utility/timer.h"

class WindowSettingTest : public Test {
	Font font{ "resources/fonts/retro_gaming.ttf", 18 };

	std::vector<Text> texts;

	Text window_position_text;
	Text window_size_text;
	Text camera_size_text;
	Text camera_pos_text;
	Text window_mode;
	Text border_mode;
	Text resize_mode;
	Text maximized;
	Text minimized;
	Text window_visible;

	Timer show_timer;

	std::string origin_string;
	int origin{ 0 };

	const V2_float text_offset{ 30.0f, 450.0f - 30.0f };

	const V2_float og_window_size{ 800, 450 };

	void Init() override {
		game.draw.SetClearColor(color::White);
		game.window.SetSize(og_window_size);

		texts.clear();

		camera_size_text	 = texts.emplace_back("", color::Black, font);
		camera_pos_text		 = texts.emplace_back("", color::Black, font);
		window_position_text = texts.emplace_back("", color::Black, font);
		window_size_text	 = texts.emplace_back("", color::Black, font);
		window_mode			 = texts.emplace_back("", color::Black, font);
		border_mode			 = texts.emplace_back("", color::Black, font);
		resize_mode			 = texts.emplace_back("", color::Black, font);
		maximized			 = texts.emplace_back("", color::Black, font);
		minimized			 = texts.emplace_back("", color::Black, font);
		window_visible		 = texts.emplace_back("", color::Black, font);
	}

	void Shutdown() override {
		game.window.SetSetting(WindowSetting::Windowed);
		game.window.SetSetting(WindowSetting::Bordered);
		game.window.SetSetting(WindowSetting::FixedSize);
		game.window.SetSetting(WindowSetting::Shown);
		game.window.SetSize(og_window_size);
	}

	void Update() final {
		auto p = game.camera.GetPrimary();
		if (game.input.KeyDown(Key::Z)) {
			V2_float scale = game.window.GetSize() / og_window_size;
			p.CenterOnArea(og_window_size);
		}
		if (game.input.KeyDown(Key::X)) {
			p.SetToWindow();
		}
		if (game.input.KeyDown(Key::V)) {
			game.window.SetPosition({ 0, 0 });
		}
		if (game.input.KeyDown(Key::C)) {
			game.window.Center();
		}
		if (game.input.KeyDown(Key::Q)) {
			game.window.SetSetting(WindowSetting::Windowed);
		}
		if (game.input.KeyDown(Key::W)) {
			game.window.SetSetting(WindowSetting::Fullscreen);
		}
		if (game.input.KeyDown(Key::R)) {
			game.window.SetSetting(WindowSetting::Borderless);
		}
		if (game.input.KeyDown(Key::T)) {
			game.window.SetSetting(WindowSetting::Bordered);
		}
		if (game.input.KeyDown(Key::Y)) {
			game.window.SetSetting(WindowSetting::Resizable);
		}
		if (game.input.KeyDown(Key::U)) {
			game.window.SetSetting(WindowSetting::FixedSize);
		}
		if (game.input.KeyDown(Key::I)) {
			game.window.SetSetting(WindowSetting::Maximized);
		}
		if (game.input.KeyDown(Key::O)) {
			game.window.SetSetting(WindowSetting::Minimized);
		}
		if (game.input.KeyDown(Key::P)) {
			game.window.SetSetting(WindowSetting::Shown);
		}
		if (game.input.KeyDown(Key::L)) {
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
		game.draw.Rect({ {}, game.window.GetSize(), Origin::TopLeft }, { 0, 0, 255, 10 });
		game.draw.Rect({ {}, og_window_size, Origin::TopLeft }, { 255, 0, 0, 40 });
		game.draw.Rect({ {}, og_window_size, Origin::TopLeft }, { 0, 255, 0, 40 }, 10.0f);

		Color color_0 = color::Green;
		Color color_1 = color::Blue;

		Rect rect_0{ V2_float{ og_window_size.x, 0.0f }, V2_float{ 30.0f, 30.0f },
					 Origin::TopRight };
		Rect rect_1{ V2_int{ 0, game.window.GetSize().y }, V2_float{ 30.0f, 30.0f },
					 Origin::BottomLeft };

		if (rect_0.Overlaps(game.input.GetMousePosition(0))) {
			color_0 = color::Red;
		}
		if (rect_1.Overlaps(game.input.GetMousePosition(1))) {
			color_1 = color::Red;
		}

		camera_pos_text.SetContent(
			"Camera Position: " + ToString(game.camera.GetPrimary().GetPosition())
		);
		camera_size_text.SetContent("Camera Size: " + ToString(game.camera.GetPrimary().GetSize()));
		window_position_text.SetContent("Window Position: " + ToString(game.window.GetPosition()));
		window_size_text.SetContent("Window Size: " + ToString(game.window.GetSize()));

		UpdateOptions(
			window_mode, "Window Mode (Q/W/Z/X): ",
			{ { WindowSetting::Windowed, "Windowed" }, { WindowSetting::Fullscreen, "Fullscreen" } }
		);
		UpdateOptions(
			border_mode, "Border Mode (R/T): ",
			{ { WindowSetting::Borderless, "Borderless" }, { WindowSetting::Bordered, "Bordered" } }
		);
		UpdateOptions(
			resize_mode, "Resize Mode (Y/U): ",
			{ { WindowSetting::Resizable, "Resizable" }, { WindowSetting::FixedSize, "FixedSize" } }
		);
		UpdateOptions(maximized, "Maximized (I): ", { { WindowSetting::Maximized, "True" } });
		UpdateOptions(minimized, "Minimized (O): ", { { WindowSetting::Minimized, "True" } });
		UpdateOptions(
			window_visible, "Window Visible (P/L): ",
			{ { WindowSetting::Shown, "Shown" }, { WindowSetting::Hidden, "Hidden" } }
		);

		V2_float offset;
		for (const auto& t : texts) {
			t.Draw({ { text_offset.x, text_offset.y - offset.y }, {}, Origin::BottomLeft });
			offset += t.GetSize();
		}
		rect_0.Draw(color_0, -1.0f, { 0.0f, 0 });
		rect_1.Draw(color_1, -1.0f, { 0.0f, 1 });

		game.draw.Point(game.input.GetMousePosition(0), { 128, 128, 0, 128 }, 4.0f, { 0.0f, 0 });
		game.draw.Point(game.input.GetMousePosition(1), { 128, 0, 128, 128 }, 4.0f, { 0.0f, 1 });
	}
};

void TestWindow() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new WindowSettingTest());

	AddTests(tests);
}