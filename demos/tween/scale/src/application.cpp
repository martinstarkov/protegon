#include "ecs/components/sprite.h"
#include "core/app/application.h"
#include "core/input/input_handler.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tween/tween_effect.h"

using namespace ptgn;

struct ScaleEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void Enter() override {
		LoadResource("smile", "resources/smile.png");

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

	void Update() override {
		if (input.MouseDown(Mouse::Left)) {
			ScaleTo(sprite1, { 5.0f, 5.0f }, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
		if (input.MouseDown(Mouse::Right)) {
			ScaleTo(sprite1, { 0.25f, 0.25f }, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("ScaleEffectScene: left/right click to scale");
	Application::Get().scene_.Enter<ScaleEffectScene>("");
	return 0;
}