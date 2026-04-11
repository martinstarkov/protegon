#include "app/application.h"
#include "core/math/easing.h"
#include "core/time/time.h"
#include "platform/mouse.h"
#include "renderer/primitives/color.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

struct TintEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;
	Sprite sprite4;

	milliseconds tint_duration1{ 4000 };
	milliseconds tint_duration2{ 1000 };

	void OnEnter() override {
		ctx().asset.Load("tree", "examples/assets/jpg.jpg");
		ctx().asset.Load("smile", "examples/assets/smile.png");

		sprite1 = CreateSprite(*this, "tree", { -300, -300 });
		sprite2 = CreateSprite(*this, "tree", { -300, 200 });
		sprite3 = CreateSprite(*this, "tree", { 200, -300 });
		sprite4 = CreateSprite(*this, "smile", { 200, 200 });

		TintTo(sprite1, color::Red, tint_duration1, Ease::Linear);
		TintTo(sprite1, color::White, tint_duration2, Ease::Linear, false);
		TintTo(sprite2, color::Green, tint_duration1, Ease::InOutSine);
		TintTo(sprite2, color::White, tint_duration2, Ease::InOutSine, false);
		TintTo(sprite3, color::Blue, tint_duration1, Ease::InSine);
		TintTo(sprite3, color::White, tint_duration2, Ease::InSine, false);
		TintTo(sprite4, { 0, 0, 255, 128 }, tint_duration1, Ease::InSine);
		TintTo(sprite4, color::White, tint_duration2, Ease::InSine, false);
	}

	void OnUpdate() override {
		if (ctx().input.MousePressed(Mouse::Left)) {
			TintTo(sprite1, color::Purple, tint_duration1, Ease::Linear, true);
		}
		if (ctx().input.MousePressed(Mouse::Right)) {
			TintTo(sprite1, color::White, tint_duration1, Ease::Linear, true);
		}
	}
};

int main(int, char**) {
	Application app{ "TintEffectScene: left/right: tint/untint" };
	app.StartWith<TintEffectScene>();
}