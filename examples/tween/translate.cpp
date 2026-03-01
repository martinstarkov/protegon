#include "app/application.h"
#include "platform/input/input_handler.h"
#include "platform/window/window.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/ecs/components/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

struct TranslateEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void OnEnter() override {
		SetBackgroundColor(color::LightBlue);

		app().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "smile", { -300, -300 });
		sprite2 = CreateSprite(*this, "smile", { -300, 200 });
		sprite3 = CreateSprite(*this, "smile", { 200, -300 });

		TranslateTo(sprite1, { 200, 200 }, milliseconds{ 4000 }, SymmetricalEase::Linear);
		TranslateTo(sprite1, { -300, -300 }, milliseconds{ 1000 }, SymmetricalEase::Linear, false);
		TranslateTo(sprite2, { 200, 200 }, milliseconds{ 4000 }, SymmetricalEase::InOutSine);
		TranslateTo(
			sprite2, { -300, 200 }, milliseconds{ 1000 }, SymmetricalEase::InOutSine, false
		);
		TranslateTo(sprite3, { 200, 200 }, milliseconds{ 4000 }, AsymmetricalEase::InSine);
		TranslateTo(sprite3, { 200, -300 }, milliseconds{ 1000 }, AsymmetricalEase::InSine, false);
	}

	void OnUpdate() override {
		PTGN_LOG(
			"WindowTL: ", input.GetMousePosition(ViewportType::WindowTopLeft),
			", WindowC: ", input.GetMousePosition(ViewportType::WindowCenter),
			", Display: ", input.GetMousePosition(ViewportType::Display),
			", Game: ", input.GetMousePosition(ViewportType::Game),
			", World: ", input.GetMousePosition(ViewportType::World)
		);
		if (input.MousePressed(Mouse::Left)) {
			TranslateTo(
				sprite1, input.GetMousePosition(), milliseconds{ 1000 }, SymmetricalEase::Linear,
				true
			);
		}
	}
};

int main(int, char**) {
	Application app{ "TranslateEffectScene: left click to translate to mouse" };
	app.StartWith<TranslateEffectScene>();
}