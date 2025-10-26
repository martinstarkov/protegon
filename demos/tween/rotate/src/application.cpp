#include "core/ecs/components/sprite.h"
#include "core/app/application.h"
#include "core/input/input_handler.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"
#include "tweens/tween_effects.h"

using namespace ptgn;

struct RotateEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void Enter() override {
		LoadResource("smile", "resources/smile.png");

		sprite1 = CreateSprite(*this, "smile", { -300, -300 });
		sprite2 = CreateSprite(*this, "smile", { -300, 200 });
		sprite3 = CreateSprite(*this, "smile", { 200, -300 });

		RotateTo(sprite1, DegToRad(180.0f), milliseconds{ 4000 }, SymmetricalEase::Linear);
		RotateTo(sprite1, DegToRad(0.0f), milliseconds{ 1000 }, SymmetricalEase::Linear, false);
		RotateTo(sprite2, DegToRad(-180.0f), milliseconds{ 4000 }, SymmetricalEase::InOutSine);
		RotateTo(sprite2, DegToRad(0.0f), milliseconds{ 1000 }, SymmetricalEase::InOutSine, false);
		RotateTo(sprite3, DegToRad(360.0f), milliseconds{ 4000 }, AsymmetricalEase::InSine);
		RotateTo(sprite3, DegToRad(0.0f), milliseconds{ 1000 }, AsymmetricalEase::InSine, false);
	}

	void Update() override {
		if (input.MouseDown(Mouse::Left)) {
			RotateTo(
				sprite1, DegToRad(360.0f), milliseconds{ 4000 }, SymmetricalEase::Linear, true
			);
		}
		if (input.MouseDown(Mouse::Right)) {
			RotateTo(sprite1, DegToRad(0.0f), milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("RotateEffectScene: left/right click to rotate");
	game.scene.Enter<RotateEffectScene>("");
	return 0;
}