#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
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

struct TintEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;
	Sprite sprite4;

	milliseconds tint_duration1{ 4000 };
	milliseconds tint_duration2{ 1000 };

	void OnEnter() override {
		ctx().asset.Load("tree", "assets/jpg.jpg");
		ctx().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, { -300, -300 }, "tree");
		sprite2 = CreateSprite(*this, { -300, 200 }, "tree");
		sprite3 = CreateSprite(*this, { 200, -300 }, "tree");
		sprite4 = CreateSprite(*this, { 200, 200 }, "smile");

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
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<TintEffectScene>();
}