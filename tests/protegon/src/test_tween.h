#pragma once

#include "common.h"
#include "protegon/tween.h"

// TODO: Add tests for tween easing
// TODO: Add tests for the following functions:
// tween.Backward();
// tween.Forward();
// tween.Destroy();
// tween.Stop();
// tween.Complete();
// tween.Rewind(dt);
// tween.Seek(0.5);
// tween.SetToValue(300);
// tween.SetFromValue(100);

int tween_test = 0;

enum class TweenTest {
	Manager,
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

void TestTweenManager() {
	TweenConfig config;
	config.on_start =
		std::function([](Tween& t, TweenType v) { PTGN_LOG("Starting tween with value ", v); });
	config.on_complete =
		std::function([](Tween& t, TweenType v) { PTGN_LOG("Completed tween with value ", v); });

	game.tween.Clear();

	PTGN_ASSERT(game.tween.Count() == 0);

	std::size_t key{ Hash("test_tween") };

	V2_float pos;
	Color color = color::Red;

	config.on_update = [&](auto& tween, auto v) {
		pos = { v, v };
	};
	config.on_complete = [&](auto& tween, auto v) {
		color = color::Green;
	};

	game.tween.Load(key, 0.0f, 800.0f, milliseconds{ 5000 }, config);

	PTGN_ASSERT(game.tween.Count() == 1);

	Timer timer;
	timer.Start();

	TestTweenLoop(
		[&]() {
			game.renderer.DrawRectangleFilled(pos, { 40, 40 }, color);

			if (timer.Elapsed() >= milliseconds{ 5000 }) {
				// Check that tween was automatically cleaned up.
				PTGN_ASSERT(game.tween.Count() == 0);
			}
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestTweenConfig() {
	V2_float size{ ws * 0.1f };

	struct TweenInfo {
		V2_float pos;
		Color color;
		Tween tween;
	};

	std::vector<TweenInfo> tweens;

	TweenConfig config0;
	config0.paused = true;

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

	TweenConfig config2;
	config2.reversed = true;

	TweenConfig config3;
	config3.repeat	  = 4;
	config3.on_repeat = std::function([](Tween& t, TweenType v) {
		PTGN_LOG("Repeating tween3 (repeat #", t.GetRepeats(), ")");
	});

	TweenConfig config4;
	config4.repeat	 = 4;
	config4.reversed = true;

	TweenConfig config5;
	config5.repeat	  = -1;
	config5.on_repeat = std::function([](Tween& t, TweenType v) {
		PTGN_LOG("Infinitely repeating tween5 (repeat #", t.GetRepeats(), ")");
	});

	TweenConfig config6;
	config6.repeat	 = -1;
	config6.reversed = true;

	TweenConfig config7;
	config7.yoyo	= true;
	config7.repeat	= 4;
	config7.on_yoyo = std::function([](Tween& t, TweenType v) {
		PTGN_LOG("Yoyoing tween7 (repeat #", t.GetRepeats(), ")");
	});

	TweenConfig config8;
	config8.yoyo	 = true;
	config8.repeat	 = 4;
	config8.reversed = true;

	TweenConfig config9;
	config9.yoyo   = true;
	config9.repeat = -1;

	TweenConfig config10;
	config10.yoyo	  = true;
	config10.repeat	  = -1;
	config10.reversed = true;

	tweens.push_back({ V2_float{ center.x, center.y - ws.y * 0.5f }, color::Red,
					   Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config0 } });
	tweens.push_back({ V2_float{ center.x, center.y - ws.y * 0.4f }, color::Blue,
					   Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config1 } });
	tweens.push_back({ V2_float{ center.x, center.y - ws.y * 0.3f }, color::Green,
					   Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config2 } });
	tweens.push_back({ V2_float{ center.x, center.y - ws.y * 0.2f }, color::Cyan,
					   Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config3 } });
	tweens.push_back({ V2_float{ center.x, center.y - ws.y * 0.1f }, color::Magenta,
					   Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config4 } });
	tweens.push_back({ V2_float{ center.x, center.y - ws.y * 0.0f }, color::Orange,
					   Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config5 } });
	tweens.push_back({ V2_float{ center.x, center.y + ws.y * 0.1f }, color::Black,
					   Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config6 } });
	tweens.push_back({ V2_float{ center.x, center.y + ws.y * 0.2f }, color::Brown,
					   Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config7 } });
	tweens.push_back({ V2_float{ center.x, center.y + ws.y * 0.3f }, color::Grey,
					   Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config8 } });
	tweens.push_back({ V2_float{ center.x, center.y + ws.y * 0.4f }, color::LightGrey,
					   Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config9 } });
	tweens.push_back({ V2_float{ center.x, center.y + ws.y * 0.5f }, color::Yellow,
					   Tween{ 0.0, 1.0f, milliseconds{ 3000 }, config10 } });

	bool paused{ false };

	PTGN_ASSERT(tweens.size() > 0);

	TestTweenLoop(
		[&](float dt) {
			for (auto& t : tweens) {
				t.tween.Step(dt);
			}

			if (game.input.KeyDown(Key::P)) {
				paused = !paused;
				if (paused) {
					for (auto& t : tweens) {
						t.tween.Pause();
					}
				} else {
					for (auto& t : tweens) {
						t.tween.Resume();
					}
				}
			}

			if (game.input.KeyDown(Key::R)) {
				for (auto& t : tweens) {
					t.tween.Start();
				}
			}

			if (game.input.KeyDown(Key::S)) {
				tweens[0].tween.Stop();
			}

			for (auto& t : tweens) {
				t.pos.x = ws.x * t.tween.GetValue();
				game.renderer.DrawRectangleFilled(t.pos, size, t.color);
			}
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
			case TweenTest::Callbacks: TestTweenConfig(); break;
			case TweenTest::Manager:   TestTweenManager(); break;
			default:				   PTGN_ERROR("Failed to find a valid tween test");
		}
	});

	game.window.SetTitle("");

	PTGN_INFO("All tween tests passed!");
}