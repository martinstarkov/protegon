#include "core/ecs/components/sprite.h"
#include "core/app/game.h"
#include "core/utils/time.h"
#include "core/input/input_handler.h"
#include "math/easing.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"
#include "tweens/tween_effects.h"

using namespace ptgn;

struct ShakeEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void Enter() override {
		LoadResource("smile", "resources/smile.png");

		sprite1 = CreateSprite(*this, "smile", { -300, -300 });
		sprite2 = CreateSprite(*this, "smile", { -300, 200 });
		sprite3 = CreateSprite(*this, "smile", { 200, -300 });

		Shake(sprite1, 1.0f, milliseconds{ 4000 }, {}, SymmetricalEase::Linear, false, true);
		Shake(sprite1, -1.0f, milliseconds{ 4000 }, {}, SymmetricalEase::Linear, false);
		Shake(sprite2, 1.0f, milliseconds{ 4000 }, {}, false, true);
		Shake(sprite3, 0.5f, milliseconds{ -1 }, {}, SymmetricalEase::Linear, false);
	}

	void Update() override {
		if (input.MouseDown(Mouse::Left)) {
			Shake(sprite3, 1.0f, {}, true);
		}
		if (input.MouseDown(Mouse::Right)) {
			StopShake(sprite3, true);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ShakeEffectScene: left/right click to start/stop shake");
	game.scene.Enter<ShakeEffectScene>("");
	return 0;
}