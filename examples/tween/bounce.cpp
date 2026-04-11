#include <optional>

#include "app/application.h"
#include "core/math/easing.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/mouse.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

struct BounceEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	milliseconds bounce_duration{ 5000 };

	void OnEnter() override {
		ctx().asset.Load("smile", "examples/assets/smile.png");

		sprite1 = CreateSprite(*this, "smile", V2_float{ 250, 0 });
		sprite2 = CreateSprite(*this, "smile", V2_float{ 0, 0 });
		sprite3 = CreateSprite(*this, "smile", V2_float{ -250, 0 });

		Bounce(sprite1, { 0, -400 }, bounce_duration, std::nullopt, Ease::InSine, {}, true);
		Bounce(sprite2, { 0, -400 }, bounce_duration, std::nullopt, Ease::OutSine, {}, true);
		Bounce(sprite3, { 0, -400 }, bounce_duration, std::nullopt, Ease::InOutSine, {}, true);
	}

	void OnUpdate() override {
		if (ctx().input.MousePressed(Mouse::Left)) {
			SymmetricalBounce(
				sprite1, { 0, -400 }, bounce_duration, std::nullopt, Ease::Linear, {}, true
			);
			SymmetricalBounce(
				sprite2, { 0, -400 }, bounce_duration, std::nullopt, Ease::InOutSine, {}, true
			);
			SymmetricalBounce(
				sprite3, { 0, -400 }, bounce_duration, std::nullopt, Ease::InOutElastic, {}, true
			);
		}
		if (ctx().input.MousePressed(Mouse::Right)) {
			Bounce(sprite1, { 0, -400 }, bounce_duration, std::nullopt, Ease::InSine, {}, true);
			Bounce(sprite2, { 0, -400 }, bounce_duration, std::nullopt, Ease::OutSine, {}, true);
			Bounce(sprite3, { 0, -400 }, bounce_duration, std::nullopt, Ease::InOutSine, {}, true);
		}
	}
};

int main(int, char**) {
	Application app{ "BounceEffectScene: left/right click switches bounce type" };
	app.StartWith<BounceEffectScene>();
}