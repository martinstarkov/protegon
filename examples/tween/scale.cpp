#include "app/application.h"
#include "platform/input/input_handler.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

struct ScaleEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	milliseconds scale_duration1{ 4000 };
	milliseconds scale_duration2{ 1000 };

	void OnEnter() override {
		ctx().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "smile", { -300, -300 });
		sprite2 = CreateSprite(*this, "smile", { -300, 200 });
		sprite3 = CreateSprite(*this, "smile", { 200, -300 });

		ScaleTo(sprite1, { 3.0f, 3.0f }, scale_duration1, Ease::Linear);
		ScaleTo(sprite1, { 1.0f, 1.0f }, scale_duration2, Ease::Linear, false);
		ScaleTo(sprite2, { 0.33f, 0.33f }, scale_duration1, Ease::InOutSine);
		ScaleTo(sprite2, { 1.0f, 1.0f }, scale_duration2, Ease::InOutSine, false);
		ScaleTo(sprite3, { 0.33f, 3.0f }, scale_duration1, Ease::InSine);
		ScaleTo(sprite3, { 1.0f, 1.0f }, scale_duration2, Ease::InSine, false);
	}

	void OnUpdate() override {
		if (ctx().input.MousePressed(Mouse::Left)) {
			ScaleTo(sprite1, { 5.0f, 5.0f }, scale_duration1, Ease::Linear, true);
		}
		if (ctx().input.MousePressed(Mouse::Right)) {
			ScaleTo(sprite1, { 0.25f, 0.25f }, scale_duration1, Ease::Linear, true);
		}
	}
};

int main(int, char**) {
	Application app{ "ScaleEffectScene: left/right click to scale" };
	app.StartWith<ScaleEffectScene>();
}