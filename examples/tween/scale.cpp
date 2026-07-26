#include "app/application.h"
#include "core/editor.h"
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

struct ScaleEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	milliseconds scale_duration1{ 4000 };
	milliseconds scale_duration2{ 1000 };

	void OnEnter() override {
		ctx().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, { -300, -300 }, "smile");
		sprite2 = CreateSprite(*this, { -300, 200 }, "smile");
		sprite3 = CreateSprite(*this, { 200, -300 }, "smile");

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
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ScaleEffectScene>();
}