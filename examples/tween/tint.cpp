#include "app/application.h"
#include "core/math/easing.h"
#include "core/time/time.h"
#include "platform/input/input_handler.h"
#include "platform/input/mouse.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/ecs/components/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

struct TintEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;
	Sprite sprite4;

	void OnEnter() override {
		app().asset.Load("tree", "assets/tree.jpg");
		app().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "tree", { -300, -300 });
		sprite2 = CreateSprite(*this, "tree", { -300, 200 });
		sprite3 = CreateSprite(*this, "tree", { 200, -300 });
		sprite4 = CreateSprite(*this, "smile", { 200, 200 });

		TintTo(sprite1, color::Red, milliseconds{ 4000 }, SymmetricalEase::Linear);
		TintTo(sprite1, color::White, milliseconds{ 1000 }, SymmetricalEase::Linear, false);
		TintTo(sprite2, color::Green, milliseconds{ 4000 }, SymmetricalEase::InOutSine);
		TintTo(sprite2, color::White, milliseconds{ 1000 }, SymmetricalEase::InOutSine, false);
		TintTo(sprite3, color::Blue, milliseconds{ 4000 }, AsymmetricalEase::InSine);
		TintTo(sprite3, color::White, milliseconds{ 1000 }, AsymmetricalEase::InSine, false);
		TintTo(sprite4, { 0, 0, 255, 128 }, milliseconds{ 4000 }, AsymmetricalEase::InSine);
		TintTo(sprite4, color::White, milliseconds{ 1000 }, AsymmetricalEase::InSine, false);
	}

	void OnUpdate() override {
		if (input.MousePressed(Mouse::Left)) {
			TintTo(sprite1, color::Purple, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
		if (input.MousePressed(Mouse::Right)) {
			TintTo(sprite1, color::White, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
	}
};

int main(int, char**) {
	Application app{ "TintEffectScene: left/right: tint/untint" };
	app.StartWith<TintEffectScene>();
}