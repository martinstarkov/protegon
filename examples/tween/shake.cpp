#include "app/application.h"
#include "core/math/easing.h"
#include "core/time/time.h"
#include "platform/input/input_handler.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

struct ShakeEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	void OnEnter() override {
		ctx().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "smile", { -300, -300 });
		sprite2 = CreateSprite(*this, "smile", { -300, 200 });
		sprite3 = CreateSprite(*this, "smile", { 200, -300 });

		Shake(sprite1, 1.0f, milliseconds{ 4000 }, {}, Ease::Linear, false, true);
		Shake(sprite1, -1.0f, milliseconds{ 4000 }, {}, Ease::Linear, false);
		Shake(sprite2, 1.0f, milliseconds{ 4000 }, {}, false, true);
		Shake(sprite3, 0.5f, milliseconds{ -1 }, {}, Ease::Linear, false);
	}

	void OnUpdate() override {
		if (ctx().input.MousePressed(Mouse::Left)) {
			Shake(sprite3, 1.0f, {}, true);
		}
		if (ctx().input.MousePressed(Mouse::Right)) {
			StopShake(sprite3, true);
		}
	}
};

int main(int, char**) {
	Application app{ "ShakeEffectScene: left/right click to start/stop shake" };
	app.StartWith<ShakeEffectScene>();
}