#include "app/application.h"
#include "platform/input/input_handler.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

struct RotateEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void OnEnter() override {
		app().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "smile", { -300, -300 });
		sprite2 = CreateSprite(*this, "smile", { -300, 200 });
		sprite3 = CreateSprite(*this, "smile", { 200, -300 });

		RotateTo(sprite1, DegToRad(180.0f), milliseconds{ 4000 }, Ease::Linear);
		RotateTo(sprite1, DegToRad(0.0f), milliseconds{ 1000 }, Ease::Linear, false);
		RotateTo(sprite2, DegToRad(-180.0f), milliseconds{ 4000 }, Ease::InOutSine);
		RotateTo(sprite2, DegToRad(0.0f), milliseconds{ 1000 }, Ease::InOutSine, false);
		RotateTo(sprite3, DegToRad(360.0f), milliseconds{ 4000 }, Ease::InSine);
		RotateTo(sprite3, DegToRad(0.0f), milliseconds{ 1000 }, Ease::InSine, false);
	}

	void OnUpdate() override {
		if (input.MousePressed(Mouse::Left)) {
			RotateTo(sprite1, DegToRad(360.0f), milliseconds{ 4000 }, Ease::Linear, true);
		}
		if (input.MousePressed(Mouse::Right)) {
			RotateTo(sprite1, DegToRad(0.0f), milliseconds{ 4000 }, Ease::Linear, true);
		}
	}
};

int main(int, char**) {
	Application app{ "RotateEffectScene: left/right click to rotate" };
	app.StartWith<RotateEffectScene>();
}