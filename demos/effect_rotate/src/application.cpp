#include "components/sprite.h"
#include "core/game.h"
#include "input/input_handler.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tweens/tween_effects.h"

using namespace ptgn;

struct RotateEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void Enter() override {
		LoadResource("smile", "resources/smile.png");

		sprite1 = CreateSprite(*this, "smile", { 100, 100 });
		sprite2 = CreateSprite(*this, "smile", { 100, 600 });
		sprite3 = CreateSprite(*this, "smile", { 600, 100 });

		RotateTo(sprite1, DegToRad(180.0f), milliseconds{ 4000 }, SymmetricalEase::Linear);
		RotateTo(sprite1, DegToRad(0.0f), milliseconds{ 1000 }, SymmetricalEase::Linear, false);
		RotateTo(sprite2, DegToRad(-180.0f), milliseconds{ 4000 }, SymmetricalEase::InOutSine);
		RotateTo(sprite2, DegToRad(0.0f), milliseconds{ 1000 }, SymmetricalEase::InOutSine, false);
		RotateTo(sprite3, DegToRad(360.0f), milliseconds{ 4000 }, AsymmetricalEase::InSine);
		RotateTo(sprite3, DegToRad(0.0f), milliseconds{ 1000 }, AsymmetricalEase::InSine, false);
	}

	void Update() override {
		if (game.input.MouseDown(Mouse::Left)) {
			RotateTo(
				sprite1, DegToRad(360.0f), milliseconds{ 4000 }, SymmetricalEase::Linear, true
			);
		}
		if (game.input.MouseDown(Mouse::Right)) {
			RotateTo(sprite1, DegToRad(0.0f), milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("RotateEffectScene: left/right click to rotate");
	game.scene.Enter<RotateEffectScene>("");
	return 0;
}