#include "components/sprite.h"
#include "core/game.h"
#include "core/time.h"
#include "input/input_handler.h"
#include "input/mouse.h"
#include "math/easing.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tweens/tween_effects.h"

using namespace ptgn;

struct TintEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;
	Sprite sprite4;

	void Enter() override {
		LoadResource("tree", "resources/tree.jpg");
		LoadResource("smile", "resources/smile.png");

		sprite1 = CreateSprite(*this, "tree");
		sprite2 = CreateSprite(*this, "tree");
		sprite3 = CreateSprite(*this, "tree");
		sprite4 = CreateSprite(*this, "smile");

		sprite1.SetPosition({ 100, 100 });
		sprite2.SetPosition({ 100, 600 });
		sprite3.SetPosition({ 600, 100 });
		sprite4.SetPosition({ 600, 600 });

		TintTo(sprite1, color::Red, milliseconds{ 4000 }, SymmetricalEase::Linear);
		TintTo(sprite1, color::White, milliseconds{ 1000 }, SymmetricalEase::Linear, false);
		TintTo(sprite2, color::Green, milliseconds{ 4000 }, SymmetricalEase::InOutSine);
		TintTo(sprite2, color::White, milliseconds{ 1000 }, SymmetricalEase::InOutSine, false);
		TintTo(sprite3, color::Blue, milliseconds{ 4000 }, AsymmetricalEase::InSine);
		TintTo(sprite3, color::White, milliseconds{ 1000 }, AsymmetricalEase::InSine, false);
		TintTo(sprite4, { 0, 0, 255, 128 }, milliseconds{ 4000 }, AsymmetricalEase::InSine);
		TintTo(sprite4, color::White, milliseconds{ 1000 }, AsymmetricalEase::InSine, false);
	}

	void Update() override {
		if (game.input.MouseDown(Mouse::Left)) {
			TintTo(sprite1, color::Purple, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
		if (game.input.MouseDown(Mouse::Right)) {
			TintTo(sprite1, color::White, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TintEffectScene: left/right click to tint/untint");
	game.scene.Enter<TintEffectScene>("");
	return 0;
}