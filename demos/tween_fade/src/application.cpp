#include "components/draw.h"
#include "components/sprite.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/time.h"
#include "debug/log.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "input/mouse.h"
#include "math/easing.h"
#include "renderer/api/color.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_input.h"
#include "scene/scene_manager.h"
#include "tweens/tween.h"
#include "tweens/tween_effects.h"

using namespace ptgn;

struct FadeEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;

	void Enter() override {
		SetBackgroundColor(color::LightBlue);

		LoadResource("tree", "resources/tree.jpg");
		LoadResource("smile", "resources/smile.png");

		sprite1 = CreateSprite(*this, "tree", { -200, -200 });
		sprite2 = CreateSprite(*this, "smile", { 200, 200 });

		SetTint(sprite1, color::Transparent);

		FadeIn(sprite1, milliseconds{ 4000 }, SymmetricalEase::Linear);
		FadeOut(sprite1, milliseconds{ 4000 }, SymmetricalEase::Linear, false);
		FadeOut(sprite2, milliseconds{ 4000 }, AsymmetricalEase::InSine);
		FadeIn(sprite2, milliseconds{ 4000 }, AsymmetricalEase::InSine, false);
	}

	void Update() override {
		if (input.MouseDown(Mouse::Left)) {
			FadeIn(sprite1, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
		if (input.MouseDown(Mouse::Right)) {
			FadeOut(sprite1, milliseconds{ 4000 }, SymmetricalEase::Linear, true);
		}
		if (input.KeyDown(Key::T)) {
			FadeOut(GetRenderTarget(), milliseconds{ 3000 }).OnComplete([](Entity) {
				PTGN_LOG("Finished fading out scene");
			});
		}
		if (input.KeyDown(Key::R)) {
			FadeIn(GetRenderTarget(), milliseconds{ 3000 }).OnComplete([](Entity) {
				PTGN_LOG("Finished fading in scene");
			});
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init(
		"FadeEffectScene: R/T: Scene Fade In/Out, Left/Right: Tree Fade In/Out", { 800, 800 }
	);
	game.scene.Enter<FadeEffectScene>("");
	return 0;
}