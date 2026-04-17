#include "app/application.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/log.h"
#include "core/math/easing.h"
#include "core/util/time.h"
#include "runtime/animation/tween.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

struct FadeEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;

	milliseconds fade_duration{ 4000 };

	void OnEnter() override {
		SetBackgroundColor(color::LightBlue);

		ctx().asset.Load("tree", "assets/jpg.jpg");
		ctx().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "tree", { -200, -200 });
		sprite2 = CreateSprite(*this, "smile", { 200, 200 });

		SetTint(sprite1, color::Transparent);

		FadeIn(sprite1, fade_duration, Ease::Linear);
		FadeOut(sprite1, fade_duration, Ease::Linear, false);
		FadeOut(sprite2, fade_duration, Ease::InSine);
		FadeIn(sprite2, fade_duration, Ease::InSine, false);
	}

	void OnUpdate() override {
		if (ctx().input.MousePressed(Mouse::Left)) {
			FadeIn(sprite1, fade_duration, Ease::Linear, true);
		}
		if (ctx().input.MousePressed(Mouse::Right)) {
			FadeOut(sprite1, fade_duration, Ease::Linear, true);
		}
		if (ctx().input.KeyPressed(Key::T)) {
			FadeOut(GetRenderTarget(), fade_duration).OnComplete([]() {
				PTGN_LOG("Finished fading out scene");
			});
		}
		if (ctx().input.KeyPressed(Key::R)) {
			FadeIn(GetRenderTarget(), fade_duration).OnComplete([]() {
				PTGN_LOG("Finished fading in scene");
			});
		}
	}
};

int main(int, char**) {
	Application app{ "FadeEffectScene: R/T: Scene Fade In/Out, Left/Right: Tree Fade In/Out" };
	app.StartWith<FadeEffectScene>();
}