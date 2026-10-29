#include <optional>

#include "app/application.h"
#include "app/editor.h"
#include "core/input/mouse.h"
#include "core/math/easing.h"
#include "core/util/time.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

struct BounceEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	milliseconds bounce_duration{ 5000 };

	void OnEnter() override {
		ctx().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, { 250, 0 }, "smile");
		sprite2 = CreateSprite(*this, { 0, 0 }, "smile");
		sprite3 = CreateSprite(*this, { -250, 0 }, "smile");

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
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<BounceEffectScene>();
}