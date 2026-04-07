#include "runtime/animation/animation.h"

#include "app/application.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/input/key.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/event/event_dispatcher.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

using namespace ptgn;

struct MyAnimationScript1 : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<event::AnimationStart>(&OnAnimationStart, this);
		d.Dispatch<event::AnimationUpdate>(&OnAnimationUpdate, this);
		d.Dispatch<event::AnimationLoopComplete>(&OnAnimationLoopComplete, this);
		d.Dispatch<event::AnimationFrameChange>(&OnAnimationFrameChange, this);
		d.Dispatch<event::AnimationComplete>(&OnAnimationComplete, this);
		d.Dispatch<event::AnimationPause>(&OnAnimationPause, this);
		d.Dispatch<event::AnimationResume>(&OnAnimationResume, this);
		d.Dispatch<event::AnimationStop>(&OnAnimationStop, this);
	}

	void OnAnimationStart() const {
		PTGN_LOG("OnAnimationStart");
	}

	void OnAnimationUpdate() const {
		// PTGN_LOG("OnAnimationUpdate");
	}

	void OnAnimationLoopComplete() const {
		PTGN_LOG("OnAnimationLoopComplete");
	}

	void OnAnimationFrameChange() const {
		PTGN_LOG("OnAnimationFrameChange");
	}

	void OnAnimationComplete() const {
		PTGN_LOG("OnAnimationComplete");
	}

	void OnAnimationPause() const {
		PTGN_LOG("OnAnimationPause");
	}

	void OnAnimationResume() const {
		PTGN_LOG("OnAnimationResume");
	}

	void OnAnimationStop() const {
		PTGN_LOG("OnAnimationStop");
	}
};

class AnimationScene : public Scene {
public:
	Animation animation;
	Animation animation2;
	Animation sprite;

	void OnEnter() override {
		ctx().asset.Load("anim", "assets/animation.png");

		animation = CreateAnimation(
			*this, "anim", GetPosition(ctx().camera),
			{ 4, milliseconds{ 500 }, V2_int{ 16, 32 }, -1, { 0, 32 } }
		);

		SetScale(animation, 4.0f);

		AddScript<MyAnimationScript1>(animation);

		animation.Start();
	}

	void OnUpdate() override {
		if (ctx().input.KeyPressed(Key::R)) {
			animation.Resume();
		} else if (ctx().input.KeyPressed(Key::P)) {
			animation.Pause();
		}
		if (ctx().input.KeyPressed(Key::T)) {
			animation.Toggle();
		}
	}
};

int main(int, char**) {
	Application app{ "AnimationScene: (P)ause/(R)esume/(T)oggle" };
	app.StartWith<AnimationScene>();
}