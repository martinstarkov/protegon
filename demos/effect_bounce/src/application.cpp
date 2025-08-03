#include "components/sprite.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/time.h"
#include "input/input_handler.h"
#include "input/mouse.h"
#include "math/easing.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tweens/tween_effects.h"

using namespace ptgn;

struct BounceEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void Enter() override {
		LoadResource("smile", "resources/smile.png");

		sprite1 = CreateSprite(*this, "smile");
		sprite2 = CreateSprite(*this, "smile");
		sprite3 = CreateSprite(*this, "smile");

		sprite1.SetPosition({ 150, 400 });
		sprite2.SetPosition({ 400, 400 });
		sprite3.SetPosition({ 650, 400 });

		Bounce(sprite1, { 0, -400 }, milliseconds{ 8000 }, -1, AsymmetricalEase::InSine, {}, true);
		Bounce(sprite2, { 0, -400 }, milliseconds{ 8000 }, -1, AsymmetricalEase::OutSine, {}, true);
		Bounce(
			sprite3, { 0, -400 }, milliseconds{ 8000 }, -1, SymmetricalEase::InOutSine, {}, true
		);
	}

	void Update() override {
		if (game.input.MouseDown(Mouse::Left)) {
			SymmetricalBounce(
				sprite1, { 0, -400 }, milliseconds{ 8000 }, -1, SymmetricalEase::Linear, {}, true
			);
			SymmetricalBounce(
				sprite2, { 0, -400 }, milliseconds{ 8000 }, -1, SymmetricalEase::InOutSine, {}, true
			);
			SymmetricalBounce(
				sprite3, { 0, -400 }, milliseconds{ 8000 }, -1, SymmetricalEase::InOutElastic, {},
				true
			);
		}
		if (game.input.MouseDown(Mouse::Right)) {
			Bounce(
				sprite1, { 0, -400 }, milliseconds{ 8000 }, -1, AsymmetricalEase::InSine, {}, true
			);
			Bounce(
				sprite2, { 0, -400 }, milliseconds{ 8000 }, -1, AsymmetricalEase::OutSine, {}, true
			);
			Bounce(
				sprite3, { 0, -400 }, milliseconds{ 8000 }, -1, SymmetricalEase::InOutSine, {}, true
			);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("BounceEffectScene: left/right click switches bounce type");
	game.scene.Enter<BounceEffectScene>("");
	return 0;
}