#include "app/application.h"
#include "core/math/easing.h"
#include "core/time/time.h"
#include "platform/input/mouse.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

struct RotateEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	milliseconds rotation_duration1{ 4000 };
	milliseconds rotation_duration2{ 1000 };

	void OnEnter() override {
		ctx().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "smile", { -300, -300 });
		sprite2 = CreateSprite(*this, "smile", { -300, 200 });
		sprite3 = CreateSprite(*this, "smile", { 200, -300 });

		RotateTo(sprite1, 180.0f, rotation_duration1, Ease::Linear);
		RotateTo(sprite1, 0.0f, rotation_duration2, Ease::Linear, false);
		RotateTo(sprite2, -180.0f, rotation_duration1, Ease::InOutSine);
		RotateTo(sprite2, 0.0f, rotation_duration2, Ease::InOutSine, false);
		RotateTo(sprite3, 360.0f, rotation_duration1, Ease::InSine);
		RotateTo(sprite3, 0.0f, rotation_duration2, Ease::InSine, false);
	}

	void OnUpdate() override {
		if (ctx().input.MousePressed(Mouse::Left)) {
			RotateTo(sprite1, 360.0f, rotation_duration1, Ease::Linear, true);
		}
		if (ctx().input.MousePressed(Mouse::Right)) {
			RotateTo(sprite1, 0.0f, rotation_duration1, Ease::Linear, true);
		}
	}
};

int main(int, char**) {
	Application app{ "RotateEffectScene: left/right click to rotate" };
	app.StartWith<RotateEffectScene>();
}