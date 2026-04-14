#include "app/application.h"
#include "core/math/easing.h"
#include "core/time/time.h"
#include "core/input/mouse.h"
#include "core/graphics/color.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

struct TranslateEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	milliseconds translate_duration1{ 4000 };
	milliseconds translate_duration2{ 1000 };

	void OnEnter() override {
		SetBackgroundColor(color::LightBlue);

		ctx().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "smile", { -300, -300 });
		sprite2 = CreateSprite(*this, "smile", { -300, 200 });
		sprite3 = CreateSprite(*this, "smile", { 200, -300 });

		TranslateTo(sprite1, { 200, 200 }, translate_duration1, Ease::Linear);
		TranslateTo(sprite1, { -300, -300 }, translate_duration2, Ease::Linear, false);
		TranslateTo(sprite2, { 200, 200 }, translate_duration1, Ease::InOutSine);
		TranslateTo(sprite2, { -300, 200 }, translate_duration2, Ease::InOutSine, false);
		TranslateTo(sprite3, { 200, 200 }, translate_duration1, Ease::InSine);
		TranslateTo(sprite3, { 200, -300 }, translate_duration2, Ease::InSine, false);
	}

	void OnUpdate() override {
		if (ctx().input.MousePressed(Mouse::Left)) {
			TranslateTo(
				sprite1, ctx().input.GetMousePosition(), translate_duration2, Ease::Linear, true
			);
		}
	}
};

int main(int, char**) {
	Application app{ "TranslateEffectScene: left click to translate to mouse" };
	app.StartWith<TranslateEffectScene>();
}