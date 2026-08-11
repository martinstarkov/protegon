#include "app/application.h"
#include "app/editor.h"
#include "core/input/mouse.h"
#include "core/math/easing.h"
#include "core/util/time.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
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

		sprite1 = CreateSprite(*this, { -300, -300 }, "smile");
		sprite2 = CreateSprite(*this, { -300, 200 }, "smile");
		sprite3 = CreateSprite(*this, { 200, -300 }, "smile");

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
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<RotateEffectScene>();
}