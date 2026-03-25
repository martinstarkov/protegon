#include "app/application.h"
#include "core/math/easing.h"
#include "core/time/time.h"
#include "platform/input/input_handler.h"
#include "platform/input/mouse.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"

#include "runtime/scene/scene_manager.h"

using namespace ptgn;

struct BounceEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void OnEnter() override {
		ctx().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "smile", V2_float{ 250, 0 });
		sprite2 = CreateSprite(*this, "smile", V2_float{ 0, 0 });
		sprite3 = CreateSprite(*this, "smile", V2_float{ -250, 0 });

		Bounce(sprite1, { 0, -400 }, milliseconds{ 8000 }, -1, Ease::InSine, {}, true);
		Bounce(sprite2, { 0, -400 }, milliseconds{ 8000 }, -1, Ease::OutSine, {}, true);
		Bounce(sprite3, { 0, -400 }, milliseconds{ 8000 }, -1, Ease::InOutSine, {}, true);
	}

	void OnUpdate() override {
		if (ctx().input.MousePressed(Mouse::Left)) {
			SymmetricalBounce(
				sprite1, { 0, -400 }, milliseconds{ 8000 }, -1, Ease::Linear, {}, true
			);
			SymmetricalBounce(
				sprite2, { 0, -400 }, milliseconds{ 8000 }, -1, Ease::InOutSine, {}, true
			);
			SymmetricalBounce(
				sprite3, { 0, -400 }, milliseconds{ 8000 }, -1, Ease::InOutElastic, {}, true
			);
		}
		if (ctx().input.MousePressed(Mouse::Right)) {
			Bounce(sprite1, { 0, -400 }, milliseconds{ 8000 }, -1, Ease::InSine, {}, true);
			Bounce(sprite2, { 0, -400 }, milliseconds{ 8000 }, -1, Ease::OutSine, {}, true);
			Bounce(sprite3, { 0, -400 }, milliseconds{ 8000 }, -1, Ease::InOutSine, {}, true);
		}
	}
};

int main(int, char**) {
	Application app{ "BounceEffectScene: left/right click switches bounce type" };
	app.StartWith<BounceEffectScene>();
}