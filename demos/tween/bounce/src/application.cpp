#include "core/ecs/components/sprite.h"
#include "core/ecs/entity.h"
#include "core/app/application.h"
#include "core/util/time.h"
#include "core/input/input_handler.h"
#include "core/input/mouse.h"
#include "math/easing.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"
#include "tween/tween_effect.h"

using namespace ptgn;

struct BounceEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void Enter() override {
		LoadResource("smile", "resources/smile.png");

		sprite1 = CreateSprite(*this, "smile", V2_float{ 250, 0 });
		sprite2 = CreateSprite(*this, "smile", V2_float{ 0, 0 });
		sprite3 = CreateSprite(*this, "smile", V2_float{ -250, 0 });

		Bounce(sprite1, { 0, -400 }, milliseconds{ 8000 }, -1, AsymmetricalEase::InSine, {}, true);
		Bounce(sprite2, { 0, -400 }, milliseconds{ 8000 }, -1, AsymmetricalEase::OutSine, {}, true);
		Bounce(
			sprite3, { 0, -400 }, milliseconds{ 8000 }, -1, SymmetricalEase::InOutSine, {}, true
		);
	}

	void Update() override {
		if (input.MouseDown(Mouse::Left)) {
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
		if (input.MouseDown(Mouse::Right)) {
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
	Application::Get().Init("BounceEffectScene: left/right click switches bounce type");
	Application::Get().scene_.Enter<BounceEffectScene>("");
	return 0;
}