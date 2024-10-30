#pragma once

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/log.h"
#include "utility/time.h"
#include "utility/tween.h"

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

struct TestTweenManager : public Test {
	Color color;
	V2_float pos;

	const V2_float size{ 40, 40 }; // rectangle size.

	const milliseconds duration{ 500 };

	const std::size_t key{ Hash("test_tween") };

	void Init() override {
		game.tween.Clear();

		game.tween.Load(key)
			.During(duration)
			.OnStart([](float v) { PTGN_LOG("Starting tween with value ", v); })
			.OnComplete([&](float v) {
				PTGN_LOG("Completed tween with value ", v);
				color = color::Green;
			})
			.OnUpdate([&](float v) {
				pos = { v * 800.0f, v * 800.0f };
			})
			.Start();

		PTGN_ASSERT(game.tween.Size() == 1);
	}

	void Draw() override {
		game.draw.Rect(pos, size, color);
	}
};

struct TestTweenPoints : public TestTweenManager {
	void Init() override {
		game.tween.Clear();

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
			})
			.Start();
	}
};

struct TestTweenRepeatPoints : public TestTweenPoints {
	const std::int64_t repeats{ 2 };

	void Init() override {
		game.tween.Clear();

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
			})
			.Start();

		PTGN_ASSERT(game.tween.Size() == 1);
	}
};

struct TestTweenYoyoPoints : public TestTweenRepeatPoints {
	void Init() override {
		game.tween.Clear();

		game.tween
			.Load(key)

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
			.Yoyo()

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
			.Yoyo()

			.During(duration)
			.OnStart([](float v) { PTGN_LOG("Starting bottom tween with value ", v); })
			.OnComplete([&](float v) {
				PTGN_LOG("Completed bottom tween with value ", v);
				color = color::Orange;
			})
			.OnUpdate([&](float v) {
				pos = { v * 800.0f, 800.0f };
			})
			.Reverse()
			.Repeat(repeats)
			.Yoyo()

			.During(duration)
			.OnStart([](float v) { PTGN_LOG("Starting left tween with value ", v); })
			.OnComplete([&](float v) {
				PTGN_LOG("Completed left tween with value ", v);
				color = color::Red;
			})
			.OnUpdate([&](float v) {
				pos = { 0.0f, v * 800.0f };
			})
			.Reverse()
			.Repeat(repeats)
			.Yoyo()
			.Start();

		PTGN_ASSERT(game.tween.Size() == 1);
	}
};

struct TestTweenTypes : public Test {
	const milliseconds duration{ 1000 };

	// Size of tween rectangles.
	V2_float size;

	// Tween, Color, Position
	std::vector<std::tuple<Tween, Color, V2_float>> tweens;

	void Init() override {
		tweens.clear();

		Tween config0{
			std::get<Tween>(tweens.emplace_back(Tween{ duration }, color::Red, V2_float{}))
		};
		Tween config1{
			std::get<Tween>(tweens.emplace_back(Tween{ duration }, color::Blue, V2_float{}))
		};
		Tween config2{
			std::get<Tween>(tweens.emplace_back(Tween{ duration }, color::Green, V2_float{}))
		};
		Tween config3{
			std::get<Tween>(tweens.emplace_back(Tween{ duration }, color::Cyan, V2_float{}))
		};
		Tween config4{
			std::get<Tween>(tweens.emplace_back(Tween{ duration }, color::Magenta, V2_float{}))
		};
		Tween config5{
			std::get<Tween>(tweens.emplace_back(Tween{ duration }, color::Orange, V2_float{}))
		};
		Tween config6{
			std::get<Tween>(tweens.emplace_back(Tween{ duration }, color::Lime, V2_float{}))
		};
		Tween config7{
			std::get<Tween>(tweens.emplace_back(Tween{ duration }, color::Brown, V2_float{}))
		};
		Tween config8{
			std::get<Tween>(tweens.emplace_back(Tween{ duration }, color::Gray, V2_float{}))
		};
		Tween config9{
			std::get<Tween>(tweens.emplace_back(Tween{ duration }, color::LightGray, V2_float{}))
		};
		Tween config10{
			std::get<Tween>(tweens.emplace_back(Tween{ duration }, color::Yellow, V2_float{}))
		};
		Tween config11{
			std::get<Tween>(tweens.emplace_back(Tween{ duration }, color::Pink, V2_float{}))
		};

		config0.Pause();
		config0.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config1.OnStart([](float v) { PTGN_LOG("Starting tween1 with value ", v); });
		config1.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });
		config1.OnComplete([](float v) { PTGN_LOG("Completed tween1 with value ", v); });
		config1.OnStop([](float v) { PTGN_LOG("Stopped tween1 with value ", v); });
		config1.OnPause([](float v) { PTGN_LOG("Paused tween1 with value ", v); });
		config1.OnResume([](float v) { PTGN_LOG("Resumed tween1 with value ", v); });
		config1.OnRepeat([](float v) { PTGN_ERROR("This repeat should never be triggered"); });

		config2.Reverse();
		config2.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config3.Repeat(4);
		config3.OnRepeat([](Tween t) {
			PTGN_LOG("Repeating tween3 (repeat #", t.GetRepeats(), ")");
		});
		config3.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config4.Repeat(4);
		config4.Reverse();
		config4.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config5.Yoyo();
		config5.Repeat(4);
		config5.OnYoyo([](Tween t) { PTGN_LOG("Yoyoing tween5 (repeat #", t.GetRepeats(), ")"); });
		config5.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config6.Yoyo();
		config6.Repeat(4);
		config6.Reverse();
		config6.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config7.Repeat(-1);
		config7.OnRepeat([](Tween t) {
			PTGN_LOG("Infinitely repeating tween7 (repeat #", t.GetRepeats(), ")");
		});
		config7.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config8.Repeat(-1);
		config8.Reverse();
		config8.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config9.Yoyo();
		config9.Repeat(-1);
		config9.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config10.Yoyo();
		config10.Repeat(-1);
		config10.Reverse();
		config10.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config11.Ease(TweenEase::OutSine);
		config11.Yoyo();
		config11.Repeat(-1);
		config11.Reverse();
		config11.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		size   = { 0, ws.y / static_cast<float>(tweens.size()) };
		size.x = std::clamp(size.y, 5.0f, 30.0f);

		auto get_pos = [&](std::size_t i) {
			return V2_float{ center.x, size.y * static_cast<float>(i) };
		};

		for (std::size_t i = 0; i < tweens.size(); ++i) {
			std::get<V2_float>(tweens[i]) = get_pos(i);
			auto& t						  = std::get<Tween>(tweens[i]);
			if (!t.IsValid()) {
				continue;
			}
			t.Start();
		}
	}

	void Update() override {
		for (auto& [t, c, p] : tweens) {
			if (!t.IsValid()) {
				continue;
			}
			t.Step(dt);
		}

		if (game.input.KeyDown(Key::P)) {
			for (auto& [t, c, p] : tweens) {
				if (!t.IsValid()) {
					continue;
				}

				if (t.IsPaused()) {
					t.Resume();
				} else {
					t.Pause();
				}
			}
		}

		if (game.input.KeyDown(Key::R)) {
			for (auto& [t, c, p] : tweens) {
				if (!t.IsValid()) {
					continue;
				}

				t.Start();
			}
		}

		if (game.input.KeyDown(Key::S)) {
			PTGN_ASSERT(!tweens.empty());
			auto& [t, c, p] = tweens[0];
			PTGN_ASSERT(t.IsValid());
			t.Stop();
		}
	}

	void Draw() override {
		for (auto& [t, c, p] : tweens) {
			if (!t.IsValid()) {
				continue;
			}

			p.x = ws.x * t.GetProgress();
			game.draw.Rect(p, size, c, Origin::CenterTop);
		}
	}
};

void TestTween() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestTweenTypes());
	tests.emplace_back(new TestTweenManager());
	tests.emplace_back(new TestTweenPoints());
	tests.emplace_back(new TestTweenRepeatPoints());
	tests.emplace_back(new TestTweenYoyoPoints());

	AddTests(tests);
}