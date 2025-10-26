#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "debug/runtime/assert.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/relatives.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/ecs/entity_hierarchy.h"
#include "core/app/application.h"
#include "core/app/manager.h"
#include "core/scripting/script.h"
#include "core/util/time.h"
#include "debug/core/log.h"
#include "ecs/ecs.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/easing.h"
#include "math/geometry/rect.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "renderer/text/text.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"
#include "tween/tween.h"

using namespace ptgn;

class TweenScriptA : public Script<TweenScriptA, TweenScript> {
public:
	void OnComplete() override {
		PTGN_LOG("Completed tween A");
	}
};

class TweenScriptB : public Script<TweenScriptB, TweenScript> {
public:
	void OnPause() override {
		PTGN_LOG("Paused tween B");
	}
};

class TweenScriptC : public Script<TweenScriptC, TweenScript> {
public:
	void OnStart() override {
		PTGN_LOG("Starting tween C with value ", Tween{ entity }.GetProgress());
	}

	void OnProgress([[maybe_unused]] float f) override {
		// PTGN_LOG("Updated Value: ", Tween{ entity }.GetProgress());
	}

	void OnComplete() override {
		PTGN_LOG("Completed tween C with value ", Tween{ entity }.GetProgress());
	}

	void OnStop() override {
		PTGN_LOG("Stopped tween C with value ", Tween{ entity }.GetProgress());
	}

	void OnPause() override {
		PTGN_LOG("Paused tween C with value ", Tween{ entity }.GetProgress());
	}

	void OnResume() override {
		PTGN_LOG("Resumed tween C with value ", Tween{ entity }.GetProgress());
	}

	void OnRepeat() override {
		PTGN_ERROR("This repeat should never be triggered for tween C");
	}
};

class TweenScriptE : public Script<TweenScriptE, TweenScript> {
public:
	void OnRepeat() override {
		PTGN_LOG("Repeating tween E (repeat #", Tween{ entity }.GetRepeats(), ")");
	}
};

class TweenScriptG : public Script<TweenScriptG, TweenScript> {
public:
	void OnYoyo() override {
		PTGN_LOG("Yoyoing tween G (repeat #", Tween{ entity }.GetRepeats(), ")");
	}
};

class TweenScriptI : public Script<TweenScriptI, TweenScript> {
public:
	void OnRepeat() override {
		PTGN_LOG("Infinitely repeating tween I (repeat #", Tween{ entity }.GetRepeats(), ")");
	}
};

class TweenScriptCustom : public Script<TweenScriptCustom, TweenScript> {
public:
	TweenScriptCustom() {}

	void OnPointComplete() override {
		SetTint(GetParent(entity), Color::RandomOpaque());
	}
};

void SetProgress(const V2_float& size, const Entity& e, float progress) {
	V2_float res{ Application::Get().render_.GetGameSize() };
	auto width{ res.x - size.x };
	Entity target{ e };
	if (HasParent(e)) {
		target = GetParent(e);
	}
	SetPositionX(target, size.x * 0.5f - res.x * 0.5f + width * progress);
}

class TweenScene : public Scene {
public:
	milliseconds duration{ 1000 };
	std::int64_t repeats{ 2 };
	V2_float size{ 40.0f };

	V2_float GetNextPosition() const {
		V2_float res{ Application::Get().render_.GetGameSize() };
		static int count{ 0 };
		V2_float pos{ -res.x * 0.5f + size.x / 2.0f,
					  -res.y * 0.5f + size.y * static_cast<float>(count) };
		count++;
		return pos;
	}

	Tween CreateRectTween(const Color& color, const std::string& name) {
		auto rect	= CreateRect(*this, V2_float{}, V2_float{}, color, -1.0f, Origin::CenterTop);
		auto text	= CreateText(*this, name, color::Black);
		Tween tween = CreateTween(*this).During(duration);
		tween.OnProgress([this](auto e, float progress) { SetProgress(size, e, progress); });
		AddChild(rect, text, "text");
		AddChild(rect, tween, "tween");
		return tween;
	}

	void Enter() override {
		// Basic tween configurations
		Tween tweenA{ CreateRectTween(color::White, "A") };
		Tween tweenB{ CreateRectTween(color::Red, "B") };
		Tween tweenC{ CreateRectTween(color::Blue, "C") };
		Tween tweenD{ CreateRectTween(color::Green, "D") };
		Tween tweenE{ CreateRectTween(color::Cyan, "E") };
		Tween tweenF{ CreateRectTween(color::Magenta, "F") };
		Tween tweenG{ CreateRectTween(color::Orange, "G") };
		Tween tweenH{ CreateRectTween(color::LimeGreen, "H") };
		Tween tweenI{ CreateRectTween(color::Brown, "I") };
		Tween tweenJ{ CreateRectTween(color::Gray, "J") };
		Tween tweenK{ CreateRectTween(color::LightGray, "K") };
		Tween tweenL{ CreateRectTween(color::Yellow, "L") };
		Tween tweenM{ CreateRectTween(color::Pink, "M") };
		Tween tweenN{ CreateRectTween(color::Purple, "N") };
		Tween tweenO{ CreateRectTween(color::Teal, "O") };
		Tween tweenP{ CreateRectTween(color::DarkRed, "P") };

		// Behaviors
		tweenA.AddScript<TweenScriptA>(); // TODO: Add destroy on completion

		tweenB.AddScript<TweenScriptB>(); // Pause after starting

		tweenC.AddScript<TweenScriptC>();

		tweenD.Reverse();
		tweenE.Repeat(repeats).AddScript<TweenScriptE>();
		tweenF.Repeat(repeats).Reverse();

		tweenG.Yoyo().Repeat(repeats).AddScript<TweenScriptG>();
		tweenH.Yoyo().Repeat(repeats).Reverse();

		tweenI.Repeat(-1).AddScript<TweenScriptI>();
		tweenJ.Repeat(-1).Reverse();
		tweenK.Yoyo().Repeat(-1);
		tweenL.Yoyo().Repeat(-1).Reverse();

		tweenM.Ease(SymmetricalEase::InOutQuart)
			.Yoyo()
			.Repeat(-1)
			.Reverse()
			.OnRepeat([](auto entity) { PTGN_LOG("Lambda repeat: ", Tween{ entity }.GetRepeats()); }
			);

		tweenN.AddScript<TweenScriptCustom>()
			.During(duration)
			.OnProgress([this](auto e, float progress) { SetProgress(size, e, progress); })
			.AddScript<TweenScriptCustom>()
			.Reverse();

		tweenO.AddScript<TweenScriptCustom>()
			.Repeat(repeats)
			.During(duration)
			.OnProgress([this](auto e, float progress) { SetProgress(size, e, progress); })
			.Repeat(repeats)
			.Reverse()
			.AddScript<TweenScriptCustom>();

		tweenP.AddScript<TweenScriptCustom>()
			.Yoyo()
			.Repeat(repeats)
			.During(duration)
			.OnProgress([this](auto e, float progress) { SetProgress(size, e, progress); })
			.AddScript<TweenScriptCustom>()
			.Yoyo()
			.Repeat(repeats)
			.Reverse();

		Refresh();

		auto tween_count{ EntitiesWith<Rect>().GetVector().size() };

		PTGN_ASSERT(tween_count > 0);

		V2_float res{ Application::Get().render_.GetGameSize() };
		size   = { 0.0f, res.y / static_cast<float>(tween_count) };
		size.x = std::clamp(size.y, 5.0f, 30.0f);

		for (auto e : EntitiesWithout<Parent>()) {
			PTGN_ASSERT(e.Has<Rect>());
			e.Get<Rect>() = Rect{ size };
			auto position{ GetNextPosition() };
			SetPosition(e, position);
			SetPosition(GetChild(e, "text"), -GetOriginOffset(Origin::CenterTop, size));
			Tween{ GetChild(e, "tween") }.Start();
		}

		tweenB.Pause();
	}

	void Update() override {
		if (input.KeyDown(Key::T)) {
			for (auto e : EntitiesWithout<Parent>()) {
				PTGN_ASSERT(e.Has<Rect>());
				Tween tween{ GetChild(e, "tween") };
				if (tween.IsPaused()) {
					tween.Resume();
				} else {
					tween.Pause();
				}
			}
		}

		if (input.KeyDown(Key::R)) {
			for (auto e : EntitiesWithout<Parent>()) {
				PTGN_ASSERT(e.Has<Rect>());
				Tween tween{ GetChild(e, "tween") };
				tween.Start();
			}
		}

		if (input.KeyDown(Key::S)) {
			for (auto e : EntitiesWithout<Parent>()) {
				PTGN_ASSERT(e.Has<Rect>());
				Tween tween{ GetChild(e, "tween") };
				tween.Stop();
			}
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("TweenScene: (T)oggle pause, (R)estart, (S)top");
	Application::Get().scene_.Enter<TweenScene>("");
	return 0;
}