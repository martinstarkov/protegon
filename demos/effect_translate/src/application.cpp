#include "components/sprite.h"
#include "core/game.h"
#include "input/input_handler.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tweens/tween_effects.h"

using namespace ptgn;

struct TranslateEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void Enter() override {
		LoadResource("smile", "resources/smile.png");

		sprite1 = CreateSprite(*this, "smile", { 100, 100 });
		sprite2 = CreateSprite(*this, "smile", { 100, 600 });
		sprite3 = CreateSprite(*this, "smile", { 600, 100 });

		TranslateTo(sprite1, { 600, 600 }, milliseconds{ 4000 }, SymmetricalEase::Linear);
		TranslateTo(sprite1, { 100, 100 }, milliseconds{ 1000 }, SymmetricalEase::Linear, false);
		TranslateTo(sprite2, { 600, 600 }, milliseconds{ 4000 }, SymmetricalEase::InOutSine);
		TranslateTo(sprite2, { 100, 600 }, milliseconds{ 1000 }, SymmetricalEase::InOutSine, false);
		TranslateTo(sprite3, { 600, 600 }, milliseconds{ 4000 }, AsymmetricalEase::InSine);
		TranslateTo(sprite3, { 600, 100 }, milliseconds{ 1000 }, AsymmetricalEase::InSine, false);
	}

	void Update() override {
		if (input.MouseDown(Mouse::Left)) {
			TranslateTo(
				sprite1, input.GetMousePosition(), milliseconds{ 1000 }, SymmetricalEase::Linear,
				true
			);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TranslateEffectScene: left click to translate to mouse");
	game.scene.Enter<TranslateEffectScene>("");
	return 0;
}