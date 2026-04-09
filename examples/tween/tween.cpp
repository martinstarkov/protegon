#include "runtime/animation/tween.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "app/application.h"
#include "core/assert.h"
#include "core/log.h"
#include "core/math/easing.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "ecs/ecs.h"
#include "platform/input/key.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/relatives.h"
#include "runtime/event/event_dispatcher.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_view.h"
#include "runtime/scripting/script.h"

using namespace ptgn;

class TweenScriptA : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<event::TweenComplete>([this](auto e) { PTGN_LOG("Completed tween A"); });
	}
};

class TweenScriptB : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<event::TweenPause>([this](auto e) { PTGN_LOG("Paused tween B"); });
	}
};

class TweenScriptC : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<event::TweenRepeat>([this]() {
			PTGN_ERROR("This repeat should never be triggered for tween C");
		});
		d.Dispatch<event::TweenResume>([this]() {
			PTGN_LOG("Resumed tween C with value ", Tween{ entity }.GetProgress());
		});
		d.Dispatch<event::TweenPause>([this]() {
			PTGN_LOG("Paused tween C with value ", Tween{ entity }.GetProgress());
		});
		d.Dispatch<event::TweenStop>([this]() {
			PTGN_LOG("Stopped tween C with value ", Tween{ entity }.GetProgress());
		});
		d.Dispatch<event::TweenComplete>([this]() {
			PTGN_LOG("Completed tween C with value ", Tween{ entity }.GetProgress());
		});
		d.Dispatch<event::TweenStart>([this]() {
			PTGN_LOG("Starting tween C with value ", Tween{ entity }.GetProgress());
		});
		d.Dispatch<event::TweenProgress>([]() {
			// PTGN_LOG("Updated Value: ", Tween{ entity }.GetProgress());
		});
	}
};

class TweenScriptE : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<event::TweenRepeat>([this]() {
			PTGN_LOG("Repeating tween E (repeat #", Tween{ entity }.GetRepeats(), ")");
		});
	}
};

class TweenScriptG : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<event::TweenYoyo>([this]() {
			PTGN_LOG("Yoyoing tween G (repeat #", Tween{ entity }.GetRepeats(), ")");
		});
	}
};

class TweenScriptI : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<event::TweenRepeat>([this]() {
			PTGN_LOG("Infinitely repeating tween I (repeat #", Tween{ entity }.GetRepeats(), ")");
		});
	}
};

class TweenScriptCustom : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<event::TweenPointComplete>([this]() {
			SetTint(GetParent(entity), Color::RandomOpaque());
		});
	}
};

void SetProgress(V2_float size, const event::TweenProgress& event) {
	V2_float res{ event.tween.GetScene().ctx().renderer.GetGameSize() };
	auto width{ res.x - size.x };
	SetPositionX(event.parent, size.x * 0.5f - res.x * 0.5f + width * event.progress);
}

class TweenScene : public Scene {
public:
	milliseconds duration{ 1000 };
	std::int64_t repeats{ 2 };
	V2_float size{ 40.0f };

	V2_float GetNextPosition() const {
		V2_float res{ ctx().renderer.GetGameSize() };
		static int count{ 0 };
		V2_float pos{ -res.x * 0.5f + size.x / 2.0f,
					  -res.y * 0.5f + size.y * static_cast<float>(count) };
		count++;
		return pos;
	}

	Tween CreateRectTween(const Color& color, const std::string& name) {
		auto rect	= CreateRect(*this, V2_float{}, V2_float{}, color, Solid{}, Origin::CenterTop);
		auto text	= CreateText(*this, {}, name, color::Black);
		Tween tween = CreateTween(*this).During(duration);
		tween.OnProgress([this](auto p) { SetProgress(size, p); });
		AddChild(rect, text, "text");
		AddChild(rect, tween, "tween");
		return tween;
	}

	void OnEnter() override {
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

		tweenI.Repeat().AddScript<TweenScriptI>();
		tweenJ.Repeat().Reverse();
		tweenK.Yoyo().Repeat();
		tweenL.Yoyo().Repeat().Reverse();

		tweenM.Ease(Ease::InOutQuart).Yoyo().Repeat().Reverse().OnRepeat([](auto p) {
			PTGN_LOG("Lambda repeat: ", p.tween.GetRepeats());
		});

		tweenN.AddScript<TweenScriptCustom>()
			.During(duration)
			.OnProgress([this](auto p) { SetProgress(size, p); })
			.AddScript<TweenScriptCustom>()
			.Reverse();

		tweenO.AddScript<TweenScriptCustom>()
			.Repeat(repeats)
			.During(duration)
			.OnProgress([this](auto p) { SetProgress(size, p); })
			.Repeat(repeats)
			.Reverse()
			.AddScript<TweenScriptCustom>();

		tweenP.AddScript<TweenScriptCustom>()
			.Yoyo()
			.Repeat(repeats)
			.During(duration)
			.OnProgress([this](auto p) { SetProgress(size, p); })
			.AddScript<TweenScriptCustom>()
			.Yoyo()
			.Repeat(repeats)
			.Reverse();

		Refresh();

		auto tween_count{ EntitiesWith<Rect>().view.GetVector().size() };

		PTGN_ASSERT(tween_count > 0);

		V2_float res{ ctx().renderer.GetGameSize() };
		size   = { 0.0f, res.y / static_cast<float>(tween_count) };
		size.x = std::clamp(size.y, 5.0f, 30.0f);

		for (auto e : EntitiesWithout<impl::Parent>()) {
			if (!e.Has<Rect>()) {
				continue;
			}
			e.Get<Rect>() = Rect{ size };
			auto position{ GetNextPosition() };
			SetPosition(e, position);
			SetPosition(GetChild(e, "text"), -GetOriginOffset(Origin::CenterTop, size));
			Tween{ GetChild(e, "tween") }.Start();
		}

		tweenB.Pause();
	}

	void OnUpdate() override {
		if (ctx().input.KeyPressed(Key::T)) {
			for (auto e : EntitiesWithout<impl::Parent>()) {
				if (!e.Has<Rect>()) {
					continue;
				}
				Tween tween{ GetChild(e, "tween") };
				if (tween.IsPaused()) {
					tween.Resume();
				} else {
					tween.Pause();
				}
			}
		}

		if (ctx().input.KeyPressed(Key::R)) {
			for (auto e : EntitiesWithout<impl::Parent>()) {
				if (!e.Has<Rect>()) {
					continue;
				}
				Tween tween{ GetChild(e, "tween") };
				tween.Start();
			}
		}

		if (ctx().input.KeyPressed(Key::S)) {
			for (auto e : EntitiesWithout<impl::Parent>()) {
				if (!e.Has<Rect>()) {
					continue;
				}
				Tween tween{ GetChild(e, "tween") };
				tween.Stop();
			}
		}
	}
};

int main(int, char**) {
	Application app{ "TweenScene: (T)oggle pause, (R)estart, (S)top" };
	app.StartWith<TweenScene>();
}