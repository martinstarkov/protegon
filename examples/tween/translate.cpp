#include "app/application.h"
#include "app/context.h"
#include "core/log.h"
#include "platform/input/input_handler.h"
#include "platform/window/window.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

struct TranslateEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void OnEnter() override {
		// TODO: Fix scene bg color.
		// SetBackgroundColor(color::LightBlue);

		app().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "smile", { -300, -300 });
		sprite2 = CreateSprite(*this, "smile", { -300, 200 });
		sprite3 = CreateSprite(*this, "smile", { 200, -300 });

		TranslateTo(sprite1, { 200, 200 }, milliseconds{ 4000 }, Ease::Linear);
		TranslateTo(sprite1, { -300, -300 }, milliseconds{ 1000 }, Ease::Linear, false);
		TranslateTo(sprite2, { 200, 200 }, milliseconds{ 4000 }, Ease::InOutSine);
		TranslateTo(sprite2, { -300, 200 }, milliseconds{ 1000 }, Ease::InOutSine, false);
		TranslateTo(sprite3, { 200, 200 }, milliseconds{ 4000 }, Ease::InSine);
		TranslateTo(sprite3, { 200, -300 }, milliseconds{ 1000 }, Ease::InSine, false);
	}

	void OnUpdate() override {
		if (input.MousePressed(Mouse::Left)) {
			TranslateTo(
				sprite1, input.GetMousePosition(), milliseconds{ 1000 }, Ease::Linear, true
			);
		}
	}
};

int main(int, char**) {
	Application app{ "TranslateEffectScene: left click to translate to mouse" };
	app.StartWith<TranslateEffectScene>();
}