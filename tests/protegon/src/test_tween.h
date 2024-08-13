#pragma once

#include <array>

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
	Callbacks,
	Manager,
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

	game.tween.Load(key, 0.0f, 800.0f, milliseconds{ 2000 }, config);

	PTGN_ASSERT(game.tween.Count() == 1);

	Timer timer;
	timer.Start();

	TestTweenLoop(
		[&]() {
			game.renderer.DrawRectangleFilled(pos, { 40, 40 }, color);

			if (timer.Elapsed() >= milliseconds{ 2000 }) {
				// Check that tween was automatically cleaned up.
				PTGN_ASSERT(game.tween.Count() == 0);
			}
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestTweenConfig() {
	struct TweenInfo {
		V2_float pos;
		Color color;
		Tween tween;
	};

	TweenConfig config0;
	config0.paused = true;
	config0.on_update =
		std::function([](Tween& t, TweenType v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	TweenConfig config1;
	config1.on_start =
		std::function([](Tween& t, TweenType v) { PTGN_LOG("Starting tween1 with value ", v); });
	config1.on_update =
		std::function([](Tween& t, TweenType v) { /*PTGN_LOG("Updated Value: ", v);*/ });
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
	config2.on_update =
		std::function([](Tween& t, TweenType v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	TweenConfig config3;
	config3.repeat	  = 4;
	config3.on_repeat = std::function([](Tween& t, TweenType v) {
		PTGN_LOG("Repeating tween3 (repeat #", t.GetRepeats(), ")");
	});
	config3.on_update =
		std::function([](Tween& t, TweenType v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	TweenConfig config4;
	config4.repeat	 = 4;
	config4.reversed = true;
	config4.on_update =
		std::function([](Tween& t, TweenType v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	TweenConfig config5;
	config5.repeat	  = -1;
	config5.on_repeat = std::function([](Tween& t, TweenType v) {
		PTGN_LOG("Infinitely repeating tween5 (repeat #", t.GetRepeats(), ")");
	});
	config5.on_update =
		std::function([](Tween& t, TweenType v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	TweenConfig config6;
	config6.repeat	 = -1;
	config6.reversed = true;
	config6.on_update =
		std::function([](Tween& t, TweenType v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	TweenConfig config7;
	config7.yoyo	= true;
	config7.repeat	= 4;
	config7.on_yoyo = std::function([](Tween& t, TweenType v) {
		PTGN_LOG("Yoyoing tween7 (repeat #", t.GetRepeats(), ")");
	});
	config7.on_update =
		std::function([](Tween& t, TweenType v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	TweenConfig config8;
	config8.yoyo	 = true;
	config8.repeat	 = 4;
	config8.reversed = true;
	config8.on_update =
		std::function([](Tween& t, TweenType v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	TweenConfig config9;
	config9.yoyo   = true;
	config9.repeat = -1;
	config9.on_update =
		std::function([](Tween& t, TweenType v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	TweenConfig config10;
	config10.yoyo	  = true;
	config10.repeat	  = -1;
	config10.reversed = true;
	config10.on_update =
		std::function([](Tween& t, TweenType v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	TweenConfig config11;
	config11.ease	  = TweenEase::OutSine;
	config11.yoyo	  = true;
	config11.repeat	  = -1;
	config11.reversed = true;
	config11.on_update =
		std::function([](Tween& t, TweenType v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	std::vector<TweenConfig> configs;

	// Non repeated tweens.
	configs.push_back(config0);
	configs.push_back(config1);
	configs.push_back(config2);

	// Finitely repeated tweens.
	configs.push_back(config3);
	configs.push_back(config4);

	// Infinitely repeated tweens.
	configs.push_back(config5);
	configs.push_back(config6);

	// Finitely repeated yoyo tweens.
	configs.push_back(config7);
	configs.push_back(config8);

	// Infinitely repeated yoyo tweens.
	configs.push_back(config9);
	configs.push_back(config10);

	// Infinitely repeated yoyo easing tweens.
	configs.push_back(config11);

	constexpr const int tween_count = 12;

	V2_float size{ 0, ws.y / static_cast<float>(configs.size()) };
	size.x = std::clamp(size.y, 5.0f, 30.0f);

	auto get_pos = [&](std::size_t i) {
		return V2_float{ center.x, size.y * i };
	};

	std::array<Color, tween_count> colors{
		color::Red,	  color::Blue,	color::Green, color::Cyan,		color::Magenta, color::Orange,
		color::Black, color::Brown, color::Grey,  color::LightGrey, color::Yellow,	color::Pink,
	};

	std::array<TweenInfo, tween_count> tweens;

	PTGN_ASSERT(configs.size() <= tween_count);

	for (std::size_t i = 0; i < configs.size(); ++i) {
		tweens[i].color = colors[i];
		tweens[i].pos	= get_pos(i);
		tweens[i].tween = Tween{ 0.0f, 1.0f, milliseconds{ 1000 }, configs[i] };
	}

	PTGN_ASSERT(tweens.size() > 0);

	TestTweenLoop(
		[&](float dt) {
			for (auto& t : tweens) {
				if (t.tween.IsValid()) {
					t.tween.Step(dt);
				}
			}

			if (game.input.KeyDown(Key::P)) {
				for (auto& t : tweens) {
					if (t.tween.IsValid()) {
						if (t.tween.IsPaused()) {
							t.tween.Resume();
						} else {
							t.tween.Pause();
						}
					}
				}
			}

			if (game.input.KeyDown(Key::R)) {
				for (auto& t : tweens) {
					if (t.tween.IsValid()) {
						t.tween.Start();
					}
				}
			}

			if (game.input.KeyDown(Key::S)) {
				PTGN_ASSERT(tweens[0].tween.IsValid());
				tweens[0].tween.Stop();
			}

			for (auto& t : tweens) {
				if (t.tween.IsValid()) {
					t.pos.x = ws.x * t.tween.GetValue();
					game.renderer.DrawRectangleFilled(
						t.pos, size, t.color, 0.0f, { 0.5f, 0.5f }, Origin::CenterTop
					);
				}
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