#include "components/draw.h"
#include "core/game.h"
#include "events/input_handler.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tweening/tween_effects.h"

using namespace ptgn;

struct FadeEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;

	void Enter() override {
		LoadResource("tree", "resources/tree.jpg");
		LoadResource("smile", "resources/smile.png");

		sprite1 = CreateSprite(*this, "tree");
		sprite2 = CreateSprite(*this, "smile");

		sprite1.SetTint(color::Transparent);
		sprite1.SetPosition({ 100, 100 });
		sprite2.SetPosition({ 600, 600 });

		FadeIn(sprite1, milliseconds{ 4000 }, SymmetricalEase::Linear);
		FadeOut(sprite1, milliseconds{ 4000 }, SymmetricalEase::Linear, false);
		FadeOut(sprite2, milliseconds{ 4000 }, AsymmetricalEase::InSine);
		FadeIn(sprite2, milliseconds{ 4000 }, AsymmetricalEase::InSine, false);
	}

	void Update() override {
		if (game.input.MouseDown(Mouse::Left)) {
			FadeIn(sprite1, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
		if (game.input.MouseDown(Mouse::Right)) {
			FadeOut(sprite1, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("FadeEffectScene");
	game.scene.Enter<FadeEffectScene>("");
	return 0;
}