#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

class TweenExampleScene : public Scene {
public:
	Color color;
	V2_float pos;

	V2_float size{ 40, 40 }; // rectangle size.

	const std::size_t key{ Hash("test_tween") };

	const milliseconds duration{ 1000 };

	const std::int64_t repeats{ 2 };

	// Tween, Color, Position
	std::vector<std::tuple<Tween, Color, V2_float>> tweens;

	void Enter() override {
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
		Tween config12{ std::get<Tween>(tweens.emplace_back(
			Tween{}
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting first tween point"); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed first tween point");
					color = color::Green;
				})
				.OnUpdate([&](float v) {
					pos = { v * 800.0f, 0.0f };
				})
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting second tween point"); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed second tween point");
					color = color::Purple;
				})
				.Reverse()
				.OnUpdate([&](float v) {
					pos = { v * 800.0f, 0.0f };
				}),
			color::Purple, V2_float{}
		)) };

		Tween config13{ std::get<Tween>(tweens.emplace_back(
			Tween{}
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting first repeated tween point"); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed first repeated tween point");
					color = color::Green;
				})
				.Repeat(repeats)
				.OnUpdate([&](float v) {
					pos = { v * 800.0f, 0.0f };
				})
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting second repeated tween point"); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed second repeated tween point");
					color = color::Purple;
				})
				.Repeat(repeats)
				.Reverse()
				.OnUpdate([&](float v) {
					pos = { v * 800.0f, 0.0f };
				}),
			color::Teal, V2_float{}
		)) };

		Tween config14{ std::get<Tween>(tweens.emplace_back(
			Tween{}
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting first yoyoed tween point"); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed first yoyoed tween point");
					color = color::Green;
				})
				.Yoyo()
				.Repeat(repeats)
				.OnUpdate([&](float v) {
					pos = { v * 800.0f, 0.0f };
				})
				.During(duration)
				.OnStart([](float v) { PTGN_LOG("Starting second yoyoed tween point"); })
				.OnComplete([&](float v) {
					PTGN_LOG("Completed second yoyoed tween point");
					color = color::Purple;
				})
				.Yoyo()
				.Repeat(repeats)
				.Reverse()
				.OnUpdate([&](float v) {
					pos = { v * 800.0f, 0.0f };
				}),
			color::DarkRed, V2_float{}
		)) };

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

		config3.Repeat(repeats);
		config3.OnRepeat([](Tween t) {
			PTGN_LOG("Repeating tween3 (repeat #", t.GetRepeats(), ")");
		});
		config3.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config4.Repeat(repeats);
		config4.Reverse();
		config4.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config5.Yoyo();
		config5.Repeat(repeats);
		config5.OnYoyo([](Tween t) { PTGN_LOG("Yoyoing tween5 (repeat #", t.GetRepeats(), ")"); });
		config5.OnUpdate([](float v) { /*PTGN_LOG("Updated Value: ", v);*/ });

		config6.Yoyo();
		config6.Repeat(repeats);
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

		size   = { 0, game.window.GetSize().y / static_cast<float>(tweens.size()) };
		size.x = std::clamp(size.y, 5.0f, 30.0f);

		auto get_pos = [&](std::size_t i) {
			return V2_float{ game.window.GetCenter().x, size.y * static_cast<float>(i) };
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
			t.Step(game.dt());
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

		Draw();
	}

	void Draw() {
		for (auto& [t, c, p] : tweens) {
			if (!t.IsValid()) {
				continue;
			}

			p.x = game.window.GetSize().x * t.GetProgress();
			Rect{ p, size, Origin::CenterTop }.Draw(c);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TweenExample", window_size);
	game.scene.Enter<TweenExampleScene>("tween_example");
	return 0;
}