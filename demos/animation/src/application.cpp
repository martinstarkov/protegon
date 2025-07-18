#include "components/draw.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/time.h"
#include "math/vector2.h"
#include "rendering/resources/texture.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class AnimationScene : public Scene {
public:
	Animation animation;
	Animation animation2;
	Animation sprite;

	void Enter() override {
		game.texture.Load("anim", "resources/animation.png");
		game.texture.Load("anim2", "resources/animation4.png");
		// game.texture.Load("anim3", "resources/animation3.png");

		/*sprite = CreateSprite(*this, "anim");
		sprite.SetPosition(camera.primary.GetPosition() + V2_int{ 64, 0 });
		sprite.SetScale(3.0f);
		auto& crop	  = sprite.Add<TextureCrop>();
		crop.size	  = V2_int{ 16, 32 };
		crop.position = V2_int{ 0, 0 };
		sprite.Hide();*/

		// animation2 =
		// CreateAnimation(*this, "anim3", milliseconds{ 1000 }, 16, V2_int{ 512, 512 }, -1, {});
		animation =
			CreateAnimation(*this, "anim2", milliseconds{ 2000 }, 16, V2_int{ 512, 512 }, -1, {});
		// CreateAnimation(*this, "anim", milliseconds{ 500 }, 4, V2_int{ 16, 32 }, -1, {});
		animation.SetScale(1.0f);
		// animation2.SetScale(0.5f);
		animation.SetPosition(camera.primary.GetPosition());
		// animation2.SetPosition(camera.primary.GetPosition());

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