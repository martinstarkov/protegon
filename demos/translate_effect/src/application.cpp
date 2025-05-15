#include "components/draw.h"
#include "core/game.h"
#include "events/input_handler.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tweening/tween_effects.h"

using namespace ptgn;

struct TranslateEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void Enter() override {
		LoadResource("smile", "resources/smile.png");

		sprite1 = CreateSprite(manager, "smile");
		sprite2 = CreateSprite(manager, "smile");
		sprite3 = CreateSprite(manager, "smile");

		sprite1.SetPosition({ 100, 100 });
		sprite2.SetPosition({ 100, 600 });
		sprite3.SetPosition({ 600, 100 });

		TranslateTo(sprite1, { 600, 600 }, milliseconds{ 4000 }, TweenEase::Linear);
		TranslateTo(sprite1, { 100, 100 }, milliseconds{ 1000 }, TweenEase::Linear, false);
		TranslateTo(sprite2, { 600, 600 }, milliseconds{ 4000 }, TweenEase::InOutSine);
		TranslateTo(sprite2, { 100, 600 }, milliseconds{ 1000 }, TweenEase::InOutSine, false);
		TranslateTo(sprite3, { 600, 600 }, milliseconds{ 4000 }, TweenEase::InSine);
		TranslateTo(sprite3, { 600, 100 }, milliseconds{ 1000 }, TweenEase::InSine, false);
	}

	void Update() override {
		if (game.input.MouseDown(Mouse::Left)) {
			TranslateTo(
				sprite1, game.input.GetMousePosition(), milliseconds{ 1000 }, TweenEase::Linear,
				true
			);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TranslateEffectScene");
	game.scene.Enter<TranslateEffectScene>("");
	return 0;
}