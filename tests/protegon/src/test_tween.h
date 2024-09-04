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
	Points,
	RepeatPoints,
	Count
};

template <typename T, typename... Ts>
void TestTweenLoop(float dt, const T& function, const std::string& name, const Ts&... message) {
	TestLoop(
		dt, test_instructions, tween_test, (int)TweenTest::Count, test_switch_keys, function, name,
		message...
	);
}

void TestTweenPoints(float dt) {
	static V2_float pos;
	static Color color = std::invoke([&]() {
		game.tween.Clear();

		std::size_t key{ Hash("test_tween_point") };

		const milliseconds duration{ 1000 };

		auto& tween =
			game.tween.Load(key)
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting top tween with value ", v); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed top tween with value ", v);
					color = color::Green;
				})
				.OnUpdate([&](float v) {
					pos = { v * 800.0f, 0.0f };
				})
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting right tween with value ", v); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed right tween with value ", v);
					color = color::Purple;
				})
				.OnUpdate([&](float v) {
					pos = { 800.0f, v * 800.0f };
				})
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting bottom tween with value ", v); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed bottom tween with value ", v);
					color = color::Orange;
				})
				.Reverse()
				.OnUpdate([&](float v) {
					pos = { v * 800.0f, 800.0f };
				})
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting left tween with value ", v); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed left tween with value ", v);
					color = color::Red;
				})
				.Reverse()
				.OnUpdate([&](float v) {
					pos = { 0.0f, v * 800.0f };
				});
		tween.Start();

		PTGN_ASSERT(game.tween.Count() == 1);

		return color::Red;
	});

	static Timer timer{ true };

	TestTweenLoop(
		dt,
		[&]() {
			game.renderer.DrawRectangleFilled(pos, { 40, 40 }, color);

			// if (timer.Elapsed() >= milliseconds{ 2000 }) {
			//	// Check that tween was automatically cleaned up.
			//	PTGN_ASSERT(game.tween.Count() == 0);
			// }
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestTweenRepeatPoints(float dt) {
	static V2_float pos;
	static Color color = std::invoke([&]() {
		game.tween.Clear();

		std::size_t key{ Hash("test_tween_point") };

		const milliseconds duration{ 1000 };
		const std::int64_t repeats{ 3 };

		auto& tween =
			game.tween.Load(key)
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting top tween with value ", v); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed top tween with value ", v);
					color = color::Green;
				})
				.OnUpdate([&](float v) {
					pos = { v * 800.0f, 0.0f };
				})
				.Repeat(repeats)
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting right tween with value ", v); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed right tween with value ", v);
					color = color::Purple;
				})
				.OnUpdate([&](float v) {
					pos = { 800.0f, v * 800.0f };
				})
				.Repeat(repeats)
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting bottom tween with value ", v); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed bottom tween with value ", v);
					color = color::Orange;
				})
				.Repeat(repeats)
				.Reverse()
				.OnUpdate([&](float v) {
					pos = { v * 800.0f, 800.0f };
				})
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting left tween with value ", v); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed left tween with value ", v);
					color = color::Red;
				})
				.Repeat(repeats)
				.Reverse()
				.OnUpdate([&](float v) {
					pos = { 0.0f, v * 800.0f };
				});
		tween.Start();

		PTGN_ASSERT(game.tween.Count() == 1);

		return color::Red;
	});

	static Timer timer{ true };

	TestTweenLoop(
		dt,
		[&]() {
			game.renderer.DrawRectangleFilled(pos, { 40, 40 }, color);

			// if (timer.Elapsed() >= milliseconds{ 2000 }) {
			//	// Check that tween was automatically cleaned up.
			//	PTGN_ASSERT(game.tween.Count() == 0);
			// }
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestTweenManager(float dt) {
	static V2_float pos;
	static Color color = std::invoke([&]() {
		game.tween.Clear();

		PTGN_ASSERT(game.tween.Count() == 0);

		std::size_t key{ Hash("test_tween") };

		game.tween.Load(key)
			.During(milliseconds{ 2000 })
			.OnStart([](float v) { PTGN_LOG("Starting tween with value ", v); })
			.OnComplete([&](float v) {
				PTGN_LOG("Completed tween with value ", v);
				color = color::Green;
			})
			.OnUpdate([&](float v) {
				pos = { v * 800.0f, v * 800.0f };
			})
			.Start();

		PTGN_ASSERT(game.tween.Count() == 1);

		return color::Red;
	});

	static Timer timer{ true };

	TestTweenLoop(
		dt,
		[&]() {
			game.renderer.DrawRectangleFilled(pos, { 40, 40 }, color);

			// if (timer.Elapsed() >= milliseconds{ 2000 }) {
			//	// Check that tween was automatically cleaned up.
			//	PTGN_ASSERT(game.tween.Count() == 0);
			// }
		},
		PTGN_FUNCTION_NAME()
	);
}

struct TweenInfo {
	V2_float pos;
	Color color;
	Tween tween;
};

constexpr const int tween_count = 12;

void TestTweenConfigSetup(V2_float& size, std::array<TweenInfo, tween_count>& tweens) {
	const milliseconds duration{ 1000 };

	Tween config0{ duration };
	config0.Pause();
	config0.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	Tween config1{ duration };
	config1.OnStart([](float v) { PTGN_LOG("Starting tween1 with value ", v); });
	config1.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });
	config1.OnComplete([](float v) { PTGN_LOG("Completed tween1 with value ", v); });
	config1.OnStop([](float v) { PTGN_LOG("Stopped tween1 with value ", v); });
	config1.OnPause([](float v) { PTGN_LOG("Paused tween1 with value ", v); });
	config1.OnResume([](float v) { PTGN_LOG("Resumed tween1 with value ", v); });
	config1.OnRepeat([](float v) { PTGN_ERROR("This repeat should never be triggered"); });

	Tween config2{ duration };
	config2.Reverse();
	config2.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	Tween config3{ duration };
	config3.Repeat(4);
	config3.OnRepeat([](Tween& t) { PTGN_LOG("Repeating tween3 (repeat #", t.GetRepeats(), ")"); });
	config3.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	Tween config4{ duration };
	config4.Repeat(4);
	config4.Reverse();
	config4.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	Tween config5{ duration };
	config5.Yoyo();
	config5.Repeat(4);
	config5.OnYoyo([](Tween& t) { PTGN_LOG("Yoyoing tween5 (repeat #", t.GetRepeats(), ")"); });
	config5.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	Tween config6{ duration };
	config6.Yoyo();
	config6.Repeat(4);
	config6.Reverse();
	config6.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	Tween config7{ duration };
	config7.Repeat(-1);
	config7.OnRepeat([](Tween& t) {
		PTGN_LOG("Infinitely repeating tween7 (repeat #", t.GetRepeats(), ")");
	});
	config7.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	Tween config8{ duration };
	config8.Repeat(-1);
	config8.Reverse();
	config8.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	Tween config9{ duration };
	config9.Yoyo();
	config9.Repeat(-1);
	config9.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	Tween config10{ duration };
	config10.Yoyo();
	config10.Repeat(-1);
	config10.Reverse();
	config10.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	Tween config11{ duration };
	config11.Ease(TweenEase::OutSine);
	config11.Yoyo();
	config11.Repeat(-1);
	config11.Reverse();
	config11.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

	std::vector<Tween> configs;

	// Non repeated tweens.
	configs.push_back(config0);
	configs.push_back(config1);
	configs.push_back(config2);

	// Finitely repeated tweens.
	configs.push_back(config3);
	configs.push_back(config4);

	// Finitely repeated yoyo tweens.
	configs.push_back(config5);
	configs.push_back(config6);

	// Infinitely repeated tweens.
	configs.push_back(config7);
	configs.push_back(config8);

	// Infinitely repeated yoyo tweens.
	configs.push_back(config9);
	configs.push_back(config10);

	// Infinitely repeated yoyo easing tweens.
	configs.push_back(config11);

	size   = { 0, ws.y / static_cast<float>(configs.size()) };
	size.x = std::clamp(size.y, 5.0f, 30.0f);

	auto get_pos = [&](std::size_t i) {
		return V2_float{ center.x, size.y * i };
	};

	std::array<Color, 12> colors{
		color::Red,	 color::Blue,  color::Green, color::Cyan,	   color::Magenta, color::Orange,
		color::Lime, color::Brown, color::Grey,	 color::LightGrey, color::Yellow,  color::Pink,
	};

	PTGN_ASSERT(configs.size() <= tween_count);

	for (std::size_t i = 0; i < configs.size(); ++i) {
		tweens[i].color = colors[i];
		tweens[i].pos	= get_pos(i);
		tweens[i].tween = configs[i];
	}

	PTGN_ASSERT(tweens.size() > 0);
}

void TestTweenConfig(float dt) {
	static V2_float size;
	static auto get_tweens = []() {
		std::array<TweenInfo, tween_count> tweens;
		TestTweenConfigSetup(size, tweens);
		return tweens;
	};

	static auto tweens = get_tweens();

	TestTweenLoop(
		dt,
		[&](float dt_) {
			for (auto& t : tweens) {
				if (t.tween.IsValid()) {
					t.tween.Step(dt_);
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
					t.pos.x = ws.x * t.tween.GetProgress();
					game.renderer.DrawRectangleFilled(t.pos, size, t.color, Origin::CenterTop);
				}
			}
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestTween() {
	game.PushLoopFunction([&](float dt) {
		game.window.SetSize({ 800, 800 });
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();
		game.renderer.SetClearColor(color::White);
		switch (static_cast<TweenTest>(tween_test)) {
			case TweenTest::Callbacks:	  TestTweenConfig(dt); break;
			case TweenTest::Manager:	  TestTweenManager(dt); break;
			case TweenTest::Points:		  TestTweenPoints(dt); break;
			case TweenTest::RepeatPoints: TestTweenRepeatPoints(dt); break;
			default:					  PTGN_ERROR("Failed to find a valid tween test");
		}
	});
}