#include "components/sprite.h"
#include "core/game.h"
#include "core/time.h"
#include "input/input_handler.h"
#include "math/easing.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tweens/tween_effects.h"

using namespace ptgn;

struct ShakeEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void Enter() override {
		LoadResource("smile", "resources/smile.png");

		sprite1 = CreateSprite(*this, "smile", { 100, 100 });
		sprite2 = CreateSprite(*this, "smile", { 100, 600 });
		sprite3 = CreateSprite(*this, "smile", { 600, 100 });

		// TODO: Implement shake effects.

		Shake(sprite1, 1.0f, milliseconds{ 8000 }, {}, SymmetricalEase::Linear, false);
		Shake(sprite1, -1.0f, milliseconds{ 8000 }, {}, SymmetricalEase::Linear, false);
		Shake(sprite2, 1.0f, milliseconds{ 4000 }, {}, SymmetricalEase::None, false);
		Shake(sprite3, 0.5f, milliseconds{ -1 }, {}, SymmetricalEase::Linear, false);
	}

	void Update() override {
		if (input.MouseDown(Mouse::Left)) {
			Shake(sprite2, 0.25f, {}, false);
		}
		if (input.MouseDown(Mouse::Right)) {
			StopShake(sprite2, true);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ShakeEffectScene: left/right click to start/stop shake");
	game.scene.Enter<ShakeEffectScene>("");
	return 0;
}