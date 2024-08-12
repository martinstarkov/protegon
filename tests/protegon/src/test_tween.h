#pragma once

#include "common.h"
#include "protegon/tween.h"

int tween_test = 0;

enum class TweenTest {
	Callbacks,
	Count
};

template <typename T, typename... Ts>
void TestTweenLoop(const T& function, const std::string& name, const Ts&... message) {
	TestLoop(
		test_instructions, tween_test, (int)TweenTest::Count, test_switch_keys, function, name,
		message...
	);
}

void TestTweenCallbacks() {
	V2_float p1{ center.x, center.y - ws.y * 0.5f };
	V2_float p2{ center.x, center.y };
	V2_float p3{ center.x, center.y + ws.y * 0.5f };

	V2_float size{ ws * 0.25f };

	Tween tween1;
	TweenConfig config1;

	config1.on_start =
		std::function([](Tween& t, TweenType v) { PTGN_LOG("Starting tween1 with value ", v); });

	config1.on_update = std::function([](Tween& t, TweenType v) {});

	config1.on_complete =
		std::function([](Tween& t, TweenType v) { PTGN_LOG("Completed tween1 with value ", v); });

	config1.on_stop =
		std::function([](Tween& t, TweenType v) { PTGN_LOG("Stopped tween1 with value ", v); });

	config1.on_pause =
		std::function([](Tween& t, TweenType v) { PTGN_LOG("Paused tween1 with value ", v); });

	config1.on_resume =
		std::function([](Tween& t, TweenType v) { PTGN_LOG("Resumed tween1 with value ", v); });

	config1.on_repeat = std::function([](Tween& t, TweenType v) {
		PTGN_ERROR("This repeat should never be triggered");
	});

	Tween tween2;
	TweenConfig config2;

	config2.repeat = 4;

	config2.on_repeat = std::function([](Tween& t, TweenType v) {
		PTGN_LOG("Repeating tween2 (repeat #", t.GetRepeats(), ")");
	});

	Tween tween3;
	TweenConfig config3;

	config3.yoyo = true;

	config3.on_yoyo = std::function([](Tween& t, TweenType v) {
		PTGN_LOG("Yoyoing tween3 (repeat #", t.GetRepeats(), ")");
	});

	tween1 = Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config1 };
	tween2 = Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config2 };
	tween3 = Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config3 };

	bool paused{ false };

	TestTweenLoop(
		[&](float dt) {
			tween1.Step(dt);
			tween2.Step(dt);
			tween3.Step(dt);

			if (game.input.KeyDown(Key::P)) {
				paused = !paused;
				if (paused) {
					tween1.Pause();
					tween2.Pause();
					tween3.Pause();
				} else {
					tween1.Resume();
					tween2.Resume();
					tween3.Resume();
				}
			}

			if (game.input.KeyDown(Key::R)) {
				tween1.Start();
			}

			if (game.input.KeyDown(Key::S)) {
				tween1.Stop();
			}

			p1.x = ws.x * tween1.GetValue();
			p2.x = ws.x * tween2.GetValue();
			p3.x = ws.x * tween3.GetValue();

			game.renderer.DrawRectangleFilled(p1, size, color::Blue);
			game.renderer.DrawRectangleFilled(p2, size, color::Red);
			game.renderer.DrawRectangleFilled(p3, size, color::Magenta);
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestTween() {
	PTGN_INFO("Starting tween tests...");

	game.window.SetSize({ 800, 800 });
	ws	   = game.window.GetSize();
	center = game.window.GetCenter();
	game.window.Show();
	game.renderer.SetClearColor(color::White);

	game.LoopUntilQuit([&](float dt) {
		switch (static_cast<TweenTest>(tween_test)) {
			case TweenTest::Callbacks: TestTweenCallbacks(); break;
			default:				   PTGN_ERROR("Failed to find a valid tween test");
		}
	});

	game.window.SetTitle("");

	PTGN_INFO("All tween tests passed!");
}