#include "core/ecs/components/draw.h"
#include "core/ecs/components/sprite.h"
#include "core/ecs/entity.h"
#include "core/app/game.h"
#include "core/utils/time.h"
#include "debug/core/log.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "math/easing.h"
#include "renderer/api/color.h"
#include "renderer/renderer.h"
#include "world/scene/scene.h"
#include "world/scene/scene_input.h"
#include "world/scene/scene_manager.h"
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