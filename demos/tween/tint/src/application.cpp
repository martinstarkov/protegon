#include "ecs/components/sprite.h"
#include "core/app/application.h"
#include "core/util/time.h"
#include "core/input/input_handler.h"
#include "core/input/mouse.h"
#include "math/easing.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"
#include "tween/tween_effect.h"

using namespace ptgn;

struct TintEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;
	Sprite sprite4;

	void Enter() override {
		LoadResource("tree", "resources/tree.jpg");
		LoadResource("smile", "resources/smile.png");

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

	void Update() override {
		if (input.MouseDown(Mouse::Left)) {
			TintTo(sprite1, color::Purple, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
		if (input.MouseDown(Mouse::Right)) {
			TintTo(sprite1, color::White, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("TintEffectScene: left/right: tint/untint");
	Application::Get().scene_.Enter<TintEffectScene>("");
	return 0;
}