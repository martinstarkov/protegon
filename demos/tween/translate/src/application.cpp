#include "core/ecs/components/sprite.h"
#include "core/app/application.h"
#include "core/app/window.h"
#include "core/input/input_handler.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"
#include "tween/tween_effect.h"

using namespace ptgn;

struct TranslateEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void Enter() override {
		Application::Get().window_.SetResizable();
		SetBackgroundColor(color::LightBlue);

		LoadResource("smile", "resources/smile.png");

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

	void Update() override {
		PTGN_LOG(
			"WindowTL: ", input.GetMousePosition(ViewportType::WindowTopLeft),
			", WindowC: ", input.GetMousePosition(ViewportType::WindowCenter),
			", Display: ", input.GetMousePosition(ViewportType::Display),
			", Game: ", input.GetMousePosition(ViewportType::Game),
			", World: ", input.GetMousePosition(ViewportType::World)
		);
		if (input.MouseDown(Mouse::Left)) {
			TranslateTo(
				sprite1, input.GetMousePosition(), milliseconds{ 1000 }, SymmetricalEase::Linear,
				true
			);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("TranslateEffectScene: left click to translate to mouse");
	Application::Get().scene_.Enter<TranslateEffectScene>("");
	return 0;
}