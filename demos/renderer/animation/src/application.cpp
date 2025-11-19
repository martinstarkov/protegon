#include "ecs/components/animation.h"
#include "core/app/application.h"
#include "core/scripting/script.h"
#include "core/util/time.h"
#include "core/input/input_handler.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

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

	void Enter() override {
		LoadResource("anim", "resources/animation.png");
		LoadResource("anim2", "resources/animation4.png");
		// LoadResource("anim3", "resources/animation3.png");

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

	void Update() override {
		if (input.KeyDown(Key::R)) {
			animation.Resume();
		} else if (input.KeyDown(Key::P)) {
			animation.Pause();
		}
		if (input.KeyDown(Key::T)) {
			animation.Toggle();
		}
	}

	void Exit() override {
		json j = *this;
		SaveJson(j, "resources/animation_scene.json");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("AnimationScene: (P)ause/(R)esume/(T)oggle");
	Application::Get().scene_.Enter<AnimationScene>("");
	return 0;
}