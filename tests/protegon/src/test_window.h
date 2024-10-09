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
#include "protegon/math.h"
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

	Text window_position_text;
	Text viewport_size_text;
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
		game.draw.SetClearColor(color::Silver);
		game.window.SetSize(og_window_size);

		texts.clear();

		camera_size_text	 = texts.emplace_back(font, "", color::Black);
		camera_pos_text		 = texts.emplace_back(font, "", color::Black);
		window_position_text = texts.emplace_back(font, "", color::Black);
		viewport_size_text	 = texts.emplace_back(font, "", color::Black);
		window_size_text	 = texts.emplace_back(font, "", color::Black);
		window_mode			 = texts.emplace_back(font, "", color::Black);
		border_mode			 = texts.emplace_back(font, "", color::Black);
		resize_mode			 = texts.emplace_back(font, "", color::Black);
		maximized			 = texts.emplace_back(font, "", color::Black);
		minimized			 = texts.emplace_back(font, "", color::Black);
		window_visible		 = texts.emplace_back(font, "", color::Black);
	}

	void Shutdown() override {
		game.window.SetSetting(WindowSetting::Windowed);
		game.window.SetSetting(WindowSetting::Bordered);
		game.window.SetSetting(WindowSetting::FixedSize);
		game.window.SetSetting(WindowSetting::Shown);
		game.draw.SetViewportSize(og_window_size);
		game.window.SetSize(og_window_size);
	}

	void Update() final {
		auto& p = game.camera.GetPrimary();
		if (game.input.KeyDown(Key::Z)) {
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
		game.draw.Rectangle({}, game.window.GetSize(), { 0, 0, 255, 10 }, Origin::TopLeft);
		game.draw.Rectangle({}, og_window_size, { 255, 0, 0, 40 }, Origin::TopLeft);
		game.draw.Rectangle({}, og_window_size, { 0, 255, 0, 20 }, Origin::TopLeft, 10.0f);

		camera_pos_text.SetContent(
			"Camera Position: " + ToString(game.camera.GetPrimary().GetPosition())
		);
		camera_size_text.SetContent("Camera Size: " + ToString(game.camera.GetPrimary().GetSize()));
		window_position_text.SetContent("Window Position: " + ToString(game.window.GetPosition()));
		viewport_size_text.SetContent("Viewport Size: " + ToString(game.draw.GetViewportSize()));
		window_size_text.SetContent("Window Size: " + ToString(game.window.GetSize()));

		UpdateOptions(
			window_mode, "Window Mode (Q/W): ",
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
		for (std::size_t i = 0; i < texts.size(); i++) {
			const auto& t = texts[i];
			Rectangle<float> rect;
			rect.origin = Origin::BottomLeft;
			rect.pos.x	= text_offset.x;
			rect.pos.y	= text_offset.y - offset.y;
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