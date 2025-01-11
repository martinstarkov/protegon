#include "protegon/protegon.h"

using namespace ptgn;

const V2_float resolution{ 800, 450 };

class FullscreenExample : public Scene {
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

	void Init() override {
		texts.clear();

		camera_size_text	 = texts.emplace_back("", color::Black);
		camera_pos_text		 = texts.emplace_back("", color::Black);
		window_position_text = texts.emplace_back("", color::Black);
		window_size_text	 = texts.emplace_back("", color::Black);
		window_mode			 = texts.emplace_back("", color::Black);
		border_mode			 = texts.emplace_back("", color::Black);
		resize_mode			 = texts.emplace_back("", color::Black);
		maximized			 = texts.emplace_back("", color::Black);
		minimized			 = texts.emplace_back("", color::Black);
		window_visible		 = texts.emplace_back("", color::Black);
	}

	void Shutdown() override {
		game.window.SetSetting(WindowSetting::Windowed);
		game.window.SetSetting(WindowSetting::Bordered);
		game.window.SetSetting(WindowSetting::FixedSize);
		game.window.SetSetting(WindowSetting::Shown);
		game.window.SetSize(resolution);
	}

	void Update() final {
		auto p = game.camera.GetPrimary();
		if (game.input.KeyDown(Key::Z)) {
			V2_float scale = game.window.GetSize() / resolution;
			p.CenterOnArea(resolution);
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
		Draw();
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

	void Draw() {
		Rect{ {}, game.window.GetSize(), Origin::TopLeft }.Draw({ 0, 0, 255, 10 });
		Rect{ {}, resolution, Origin::TopLeft }.Draw({ 255, 0, 0, 40 });
		Rect{ {}, resolution, Origin::TopLeft }.Draw({ 0, 255, 0, 40 }, 10.0f);

		Color color_0 = color::Green;
		Color color_1 = color::Blue;

		Rect rect_0{ V2_float{ resolution.x, 0.0f }, V2_float{ 30.0f, 30.0f },
					 Origin::TopRight };
		Rect rect_1{ V2_int{ 0, game.window.GetSize().y }, V2_float{ 30.0f, 30.0f },
					 Origin::BottomLeft };

		if (rect_0.Overlaps(game.input.GetMousePosition())) {
			color_0 = color::Red;
		}
		if (rect_1.Overlaps(game.input.GetMousePosition())) {
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

		RenderTarget rt{ color::Transparent };

		rect_0.Draw(color_0, -1.0f);
		rect_1.Draw(color_1, -1.0f, rt);

		game.input.GetMousePosition().Draw({ 128, 128, 0, 128 }, 4.0f);
		game.input.GetMousePosition().Draw({ 128, 0, 128, 128 }, 4.0f, rt);

		rt.Draw();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("FullscreenExample", resolution);
	game.scene.LoadActive<FullscreenExample>("fullscreen");
	return 0;
}