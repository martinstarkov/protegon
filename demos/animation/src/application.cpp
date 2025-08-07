#include "components/animation.h"
#include "core/game.h"
#include "core/time.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class AnimationScene : public Scene {
public:
	Animation animation;
	Animation animation2;
	Animation sprite;

	void Enter() override {
		LoadResource("anim", "resources/animation.png");
		LoadResource("anim2", "resources/animation4.png");
		// LoadResource("anim3", "resources/animation3.png");

		/*sprite = CreateSprite(*this, "anim", camera.primary.GetPosition() + V2_int{ 64, 0 });
		SetScale(sprite, 3.0f);
		auto& crop	  = sprite.Add<TextureCrop>();
		crop.size	  = V2_int{ 16, 32 };
		crop.position = V2_int{ 0, 0 };
		Hide(sprite);*/

		// animation2 =
		// CreateAnimation(*this, "anim3", camera.primary.GetPosition(), 16, milliseconds{ 1000 },
		// V2_int{ 512, 512 }, -1, {});
		animation = CreateAnimation(
			*this, "anim2", camera.primary.GetPosition(), 16, milliseconds{ 2000 },
			V2_int{ 512, 512 }, -1, {}
		);
		// CreateAnimation(*this, "anim", camera.primary.GetPosition(), 4, milliseconds{ 500
		// },V2_int{ 16, 32 }, -1, {}); SetScale(animation2, 0.5f);

		animation.Start();
		// animation2.Start();
	}

	void Exit() override {
		json j = *this;
		SaveJson(j, "resources/animation_scene.json");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("AnimationScene");
	game.scene.Enter<AnimationScene>("");
	return 0;
}