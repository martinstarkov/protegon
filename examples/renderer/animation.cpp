#include "runtime/graphics/animation.h"

#include "app/application.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/input/input_handler.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scripting/script.h"

using namespace ptgn;

struct MyAnimationScript1 : public Script<MyAnimationScript1, AnimationScript> {
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
		app().asset.Load("anim2", "assets/animation4.png");
		// app().asset.Load("anim3", "assets/animation3.png");

		/*sprite = CreateSprite(*this, "anim", GetPosition(camera) + V2_int{ 64, 0 });
		SetScale(sprite, 3.0f);
		auto& crop	  = sprite.Add<TextureCrop>();
		crop.size	  = V2_int{ 16, 32 };
		crop.position = V2_int{ 0, 0 };
		Hide(sprite);*/

		// animation2 =
		// CreateAnimation(*this, "anim3", GetPosition(camera), 16, milliseconds{ 1000 },
		// V2_int{ 512, 512 }, -1, {});
		animation = CreateAnimation(
			*this, "anim2", GetPosition(camera), 16, milliseconds{ 2000 }, V2_int{ 512, 512 }, -1,
			{}
		);
		AddScript<MyAnimationScript1>(animation);
		// CreateAnimation(*this, "anim", GetPosition(camera), 4, milliseconds{ 500
		// },V2_int{ 16, 32 }, -1, {}); SetScale(animation2, 0.5f);

		animation.Start();
		// animation2.Start();
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

	void OnExit() override {
		json j = *this;
		SaveJson(j, "assets/animation_scene.json");
	}
};

int main(int, char**) {
	Application app{ "AnimationScene: (P)ause/(R)esume/(T)oggle" };
	app.StartWith<AnimationScene>();
}