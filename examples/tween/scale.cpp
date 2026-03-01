#include "app/application.h"
#include "platform/input/input_handler.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/ecs/components/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

struct ScaleEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void OnEnter() override {
		app().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "smile", { -300, -300 });
		sprite2 = CreateSprite(*this, "smile", { -300, 200 });
		sprite3 = CreateSprite(*this, "smile", { 200, -300 });

		ScaleTo(sprite1, { 3.0f, 3.0f }, milliseconds{ 4000 }, SymmetricalEase::Linear);
		ScaleTo(sprite1, { 1.0f, 1.0f }, milliseconds{ 1000 }, SymmetricalEase::Linear, false);
		ScaleTo(sprite2, { 0.33f, 0.33f }, milliseconds{ 4000 }, SymmetricalEase::InOutSine);
		ScaleTo(sprite2, { 1.0f, 1.0f }, milliseconds{ 1000 }, SymmetricalEase::InOutSine, false);
		ScaleTo(sprite3, { 0.33f, 3.0f }, milliseconds{ 4000 }, AsymmetricalEase::InSine);
		ScaleTo(sprite3, { 1.0f, 1.0f }, milliseconds{ 1000 }, AsymmetricalEase::InSine, false);
	}

	void OnUpdate() override {
		if (input.MousePressed(Mouse::Left)) {
			ScaleTo(sprite1, { 5.0f, 5.0f }, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
		if (input.MousePressed(Mouse::Right)) {
			ScaleTo(sprite1, { 0.25f, 0.25f }, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
	}
};

int main(int, char**) {
	Application app{ "ScaleEffectScene: left/right click to scale" };
	app.StartWith<ScaleEffectScene>();
}