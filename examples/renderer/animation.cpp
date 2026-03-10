#include "runtime/animation/animation.h"

#include "app/application.h"
#include "core/event/dispatcher.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/input/input_handler.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/tween.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

using namespace ptgn;

struct MyAnimationScript1 : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<AnimationStart>([this](auto& e) { OnAnimationStart(); });
		d.Dispatch<AnimationUpdate>([this](auto& e) { OnAnimationUpdate(); });
		d.Dispatch<AnimationRepeat>([this](auto& e) { OnAnimationRepeat(); });
		d.Dispatch<AnimationFrameChange>([this](auto& e) { OnAnimationFrameChange(); });
		d.Dispatch<AnimationComplete>([this](auto& e) { OnAnimationComplete(); });
		d.Dispatch<AnimationPause>([this](auto& e) { OnAnimationPause(); });
		d.Dispatch<AnimationResume>([this](auto& e) { OnAnimationResume(); });
		d.Dispatch<AnimationStop>([this](auto& e) { OnAnimationStop(); });
	}

	void OnAnimationStart() {
		PTGN_LOG("OnAnimationStart");
	}

	void OnAnimationUpdate() {
		// PTGN_LOG("OnAnimationUpdate");
	}

	void OnAnimationRepeat() {
		PTGN_LOG("OnAnimationRepeat");
	}

	void OnAnimationFrameChange() {
		PTGN_LOG("OnAnimationFrameChange");
	}

	void OnAnimationComplete() {
		PTGN_LOG("OnAnimationComplete");
	}

	void OnAnimationPause() {
		PTGN_LOG("OnAnimationPause");
	}

	void OnAnimationResume() {
		PTGN_LOG("OnAnimationResume");
	}

	void OnAnimationStop() {
		PTGN_LOG("OnAnimationStop");
	}
};

class AnimationScene : public Scene {
public:
	Animation animation;
	Animation animation2;
	Animation sprite;

	void OnEnter() override {
		app().asset.Load("anim", "assets/animation.png");

		animation = CreateAnimation(
			*this, "anim", GetPosition(camera), 4, milliseconds{ 500 }, V2_int{ 16, 32 }, -1,
			{ 0, 32 }
		);

		SetScale(animation, 4.0f);

		AddScript<MyAnimationScript1>(animation);

		animation.Start();
	}

	void OnUpdate() override {
		if (input.KeyPressed(Key::R)) {
			animation.Resume();
		} else if (input.KeyPressed(Key::P)) {
			animation.Pause();
		}
		if (input.KeyPressed(Key::T)) {
			animation.Toggle();
		}
	}
};

int main(int, char**) {
	Application app{ "AnimationScene: (P)ause/(R)esume/(T)oggle" };
	app.StartWith<AnimationScene>();
}