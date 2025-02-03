#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct BasicRectangleScene : public Scene {
	void Enter() override {
		auto r = manager.CreateEntity();
		r.Add<Rect>();
		r.Add<Transform>(game.window.GetCenter() + V2_float{ 200, 200 });
		r.Add<Size>(V2_float{ 200, 100 });
		r.Add<Origin>(Origin::Center);
		r.Add<Tint>(color::Red);
		r.Add<Visible>();

		auto t1 = manager.CreateEntity();
		t1.Add<Triangle>(V2_float{ -150, 0 }, V2_float{ 0, -180 }, V2_float{ 150, 0 });
		t1.Add<Transform>(game.window.GetCenter() + V2_float{ 0, 240 });
		t1.Add<Tint>(color::Blue);
		t1.Add<Visible>();

		auto c = manager.CreateEntity();
		c.Add<Circle>();
		c.Add<Transform>(game.window.GetCenter() + V2_float{ 200, 200 });
		c.Add<Radius>(V2_float{ 40 });
		c.Add<Tint>(color::Green);
		c.Add<Visible>();

		auto t2 = manager.CreateEntity();
		t2.Add<Triangle>(V2_float{ -150, 0 }, V2_float{ 0, -180 }, V2_float{ 150, 0 });
		t2.Add<LineWidth>(10.0f);
		t2.Add<Transform>(game.window.GetCenter() + V2_float{ 0, -180 });
		t2.Add<Tint>(color::Blue);
		t2.Add<Visible>();

		game.texture.Load("test", "resources/test.png");
		auto s1 = manager.CreateEntity();
		s1.Add<Transform>(
			game.window.GetCenter() + V2_float{ 120, -120 }
			/*, half_pi<float> / 2.0f, V2_float{ 1.0f }*/
		);
		s1.Add<Sprite>("test");
		// s1.Add<Size>(V2_float{ 800, 800 });
		// s1.Add<Offset>(V2_float{ 0, 0 });
		// s1.Add<Tint>(color::White);
		s1.Add<Visible>();

		auto r2 = manager.CreateEntity();
		r2.Add<Rect>();
		r2.Add<Transform>(game.window.GetCenter() + V2_float{ -100, 0 });
		r2.Add<LineWidth>(10.0f);
		r2.Add<Size>(V2_float{ 200, 200 });
		r2.Add<Origin>(Origin::Center);
		r2.Add<Tint>(color::Pink);
		r2.Add<Visible>();

		auto c2 = manager.CreateEntity();
		c2.Add<Circle>();
		c2.Add<Transform>(game.window.GetCenter() - V2_float{ 200, 200 });
		c2.Add<LineWidth>(-1.0f);
		c2.Add<Radius>(V2_float{ 50 });
		c2.Add<Tint>(color::DarkGreen);
		c2.Add<Visible>();

		manager.Refresh();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("BasicRectangleExample", window_size, color::Transparent);
	game.Start<BasicRectangleScene>("basic_rectangle_example");
	return 0;
}
