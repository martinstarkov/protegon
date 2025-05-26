#include "components/draw.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/time.h"
#include "math/vector2.h"
#include "rendering/resources/texture.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class AnimationExample : public Scene {
public:
	Animation animation;
	Animation sprite;

	void Enter() override {
		game.texture.Load("anim", "resources/animation.png");

		/*sprite = CreateSprite(manager, "anim");
		sprite.SetPosition(camera.primary.GetPosition());
		sprite.SetScale(3.0f);
		auto& crop	  = sprite.Add<TextureCrop>();
		crop.size	  = V2_int{ 16, 32 };
		crop.position = V2_int{ 0, 0 };*/

		animation =
			CreateAnimation(manager, "anim", milliseconds{ 500 }, 4, V2_int{ 16, 32 }, -1, {});
		animation.SetPosition(camera.primary.GetPosition());
		animation.SetScale(3.0f);

		animation.Start();
	}

	void Exit() {
		json j = *this;
		SaveJson(j, "resources/animation_scene.json");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("AnimationExample");
	game.scene.Enter<AnimationExample>("animation_example");
	return 0;
}