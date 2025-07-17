#include "core/game.h"
#include "core/window.h"
#include "events/input_handler.h"
#include "rendering/renderer.h"
#include "rendering/resources/texture.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tweening/tween.h"

using namespace ptgn;

class TweenConfig00 : public TweenScript<TweenConfig00> {
public:
	void OnComplete([[maybe_unused]] TweenInfo info) override {
		PTGN_LOG("Completed tween 00");
	}
};

class TweenConfig0 : public TweenScript<TweenConfig0> {
public:
	void OnPause([[maybe_unused]] TweenInfo info) override {
		PTGN_LOG("Paused tween 0");
	}

	void OnResume([[maybe_unused]] TweenInfo info) override {
		PTGN_ERROR("Tween 0 should remain paused");
	}
};

class TweenConfig1 : public TweenScript<TweenConfig1> {
public:
	void OnStart([[maybe_unused]] TweenInfo info) override {
		PTGN_LOG("Starting tween1 with value ", info.progress);
	}

	void OnUpdate([[maybe_unused]] TweenInfo info
	) override { /*PTGN_LOG("Updated Value: ", info.progress);*/ }

	void OnComplete([[maybe_unused]] TweenInfo info) override {
		PTGN_LOG("Completed tween1 with value ", info.progress);
	}

	void OnStop([[maybe_unused]] TweenInfo info) override {
		PTGN_LOG("Stopped tween1 with value ", info.progress);
	}

	void OnPause([[maybe_unused]] TweenInfo info) override {
		PTGN_LOG("Paused tween1 with value ", info.progress);
	}

	void OnResume([[maybe_unused]] TweenInfo info) override {
		PTGN_LOG("Resumed tween1 with value ", info.progress);
	}

	void OnRepeat([[maybe_unused]] TweenInfo info) override {
		PTGN_ERROR("This repeat should never be triggered");
	}
};

class TweenConfig3 : public TweenScript<TweenConfig3> {
public:
	void OnRepeat([[maybe_unused]] TweenInfo info) override {
		PTGN_LOG("Repeating tween3 (repeat #", info.tween.GetRepeats(), ")");
	}
};

class TweenConfig5 : public TweenScript<TweenConfig5> {
public:
	void OnYoyo([[maybe_unused]] TweenInfo info) override {
		PTGN_LOG("Yoyoing tween5 (repeat #", info.tween.GetRepeats(), ")");
	}
};

class TweenConfig7 : public TweenScript<TweenConfig7> {
public:
	void OnRepeat([[maybe_unused]] TweenInfo info) override {
		PTGN_LOG("Infinitely repeating tween7 (repeat #", info.tween.GetRepeats(), ")");
	}
};

class TweenConfigCustom : public TweenScript<TweenConfigCustom> {
public:
	TweenConfigCustom() {}

	TweenConfigCustom(
		std::string_view name, Color* color, V2_float* pos, const Color& color_change
	) :
		name{ name }, color{ color }, pos{ pos }, color_change{ color_change } {}

	std::string_view name;
	Color* color{ nullptr };
	V2_float* pos{ nullptr };
	Color color_change{ color::Green };

	void OnStart([[maybe_unused]] TweenInfo info) override {
		PTGN_LOG("Starting ", name, " tween point: ", info.tween.GetCurrentIndex());
	}

	void OnComplete([[maybe_unused]] TweenInfo info) override {
		PTGN_LOG("Completed ", name, " tween point: ", info.tween.GetCurrentIndex());
		PTGN_ASSERT(color != nullptr);
		*color = color_change;
	}

	void OnUpdate([[maybe_unused]] TweenInfo info) override {
		PTGN_ASSERT(pos != nullptr);
		*pos = { info.progress * 800.0f, 0.0f };
	}
};

class TweenScene : public Scene {
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

		Tween config00{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::Black, V2_float{})
		) };
		Tween config0{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::Red, V2_float{})
		) };
		Tween config1{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::Blue, V2_float{})
		) };
		Tween config2{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::Green, V2_float{})
		) };
		Tween config3{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::Cyan, V2_float{})
		) };
		Tween config4{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::Magenta, V2_float{})
		) };
		Tween config5{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::Orange, V2_float{})
		) };
		Tween config6{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::LimeGreen, V2_float{})
		) };
		Tween config7{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::Brown, V2_float{})
		) };
		Tween config8{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::Gray, V2_float{})
		) };
		Tween config9{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::LightGray, V2_float{})
		) };
		Tween config10{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::Yellow, V2_float{})
		) };
		Tween config11{ std::get<Tween>(
			tweens.emplace_back(CreateTween(*this).During(duration), color::Pink, V2_float{})
		) };
		Tween config12{
			std::get<Tween>(tweens.emplace_back(CreateTween(*this), color::Purple, V2_float{}))
		};
		config12.During(duration).AddTweenScript<TweenConfigCustom>(
			"regular", &color, &pos, color::Green
		);
		config12.During(duration).AddTweenScript<TweenConfigCustom>(
			"regular", &color, &pos, color::Purple
		);
		config12.Reverse();

		Tween config13{
			std::get<Tween>(tweens.emplace_back(CreateTween(*this), color::Teal, V2_float{}))
		};
		config13.During(duration).AddTweenScript<TweenConfigCustom>(
			"repeat", &color, &pos, color::Green
		);
		config13.Repeat(repeats)
			.During(duration)
			.Repeat(repeats)
			.Reverse()
			.AddTweenScript<TweenConfigCustom>("repeat", &color, &pos, color::Purple);

		Tween config14{
			std::get<Tween>(tweens.emplace_back(CreateTween(*this), color::DarkRed, V2_float{}))
		};

		config14.During(duration).AddTweenScript<TweenConfigCustom>(
			"yoyo", &color, &pos, color::Green
		);
		config14.Yoyo().Repeat(repeats).During(duration).AddTweenScript<TweenConfigCustom>(
			"yoyo", &color, &pos, color::Purple
		);
		config14.Yoyo().Repeat(repeats).Reverse();

		// TODO: Add destruction upon completion.
		// Destroyed upon completion.
		config00.AddTweenScript<TweenConfig00>();

		// Paused after starting.
		config0.AddTweenScript<TweenConfig0>();
		config1.AddTweenScript<TweenConfig1>();

		config2.Reverse();

		config3.Repeat(repeats);
		config3.AddTweenScript<TweenConfig3>();

		config4.Repeat(repeats);
		config4.Reverse();

		config5.Yoyo();
		config5.Repeat(repeats);
		config5.AddTweenScript<TweenConfig5>();

		config6.Yoyo();
		config6.Repeat(repeats);
		config6.Reverse();

		config7.Repeat(-1);
		config7.AddTweenScript<TweenConfig7>();

		config8.Repeat(-1);
		config8.Reverse();

		config9.Yoyo();
		config9.Repeat(-1);

		config10.Yoyo();
		config10.Repeat(-1);
		config10.Reverse();

		config11.Ease(AsymmetricalEase::OutSine);
		config11.Yoyo();
		config11.Repeat(-1);
		config11.Reverse();

		size   = { 0, game.window.GetSize().y / static_cast<float>(tweens.size()) };
		size.x = std::clamp(size.y, 5.0f, 30.0f);

		auto get_pos = [&](std::size_t i) {
			return V2_float{ game.window.GetCenter().x, size.y * static_cast<float>(i) };
		};

		for (std::size_t i = 0; i < tweens.size(); ++i) {
			std::get<V2_float>(tweens[i]) = get_pos(i);
			auto& t						  = std::get<Tween>(tweens[i]);
			t.Start();
		}

		config0.Pause();
	}

	void Update() override {
		if (game.input.KeyDown(Key::P)) {
			for (auto& [t, c, p] : tweens) {
				if (t.IsPaused()) {
					t.Resume();
				} else {
					t.Pause();
				}
			}
		}

		if (game.input.KeyDown(Key::R)) {
			for (auto& [t, c, p] : tweens) {
				t.Start();
			}
		}

		if (game.input.KeyDown(Key::S)) {
			PTGN_ASSERT(!tweens.empty());
			auto& [t, c, p] = tweens[0];
			t.Stop();
		}

		Draw();
	}

	void Draw() {
		for (auto& [t, c, p] : tweens) {
			p.x = game.window.GetSize().x * t.GetProgress();
			DrawDebugRect(p, size, c, Origin::CenterTop, -1.0f);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TweenScene");
	game.scene.Enter<TweenScene>("");
	return 0;
}