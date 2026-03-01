#include "app/application.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/easing.h"
#include "core/time/time.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "renderer/renderer.h"
#include "runtime/animation/tween.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/sprite.h"
#include "runtime/ecs/entity.h"
#include "runtime/input/scene_input.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

struct FadeEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;

	void OnEnter() override {
		SetBackgroundColor(color::LightBlue);

		app().asset.Load("tree", "assets/tree.jpg");
		app().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "tree", { -200, -200 });
		sprite2 = CreateSprite(*this, "smile", { 200, 200 });

		SetTint(sprite1, color::Transparent);

		FadeIn(sprite1, milliseconds{ 4000 }, SymmetricalEase::Linear);
		FadeOut(sprite1, milliseconds{ 4000 }, SymmetricalEase::Linear, false);
		FadeOut(sprite2, milliseconds{ 4000 }, AsymmetricalEase::InSine);
		FadeIn(sprite2, milliseconds{ 4000 }, AsymmetricalEase::InSine, false);
	}

	void OnUpdate() override {
		if (input.MousePressed(Mouse::Left)) {
			FadeIn(sprite1, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
		if (input.MousePressed(Mouse::Right)) {
			FadeOut(sprite1, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
		if (input.KeyPressed(Key::T)) {
			FadeOut(GetRenderTarget(), milliseconds{ 3000 }).OnComplete([](Entity) {
				PTGN_LOG("Finished fading out scene");
			});
		}
		if (input.KeyPressed(Key::R)) {
			FadeIn(GetRenderTarget(), milliseconds{ 3000 }).OnComplete([](Entity) {
				PTGN_LOG("Finished fading in scene");
			});
		}
	}
};

int main(int, char**) {
	Application app{ "FadeEffectScene: R/T: Scene Fade In/Out, Left/Right: Tree Fade In/Out" };
	app.StartWith<FadeEffectScene>();
}