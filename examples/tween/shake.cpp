#include <optional>

#include "app/application.h"
#include "core/input/mouse.h"
#include "core/math/easing.h"
#include "core/util/time.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

struct ShakeEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	milliseconds shake_duration{ 3000 };

	void OnEnter() override {
		ctx().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, "smile", { -300, -300 });
		sprite2 = CreateSprite(*this, "smile", { -300, 200 });
		sprite3 = CreateSprite(*this, "smile", { 200, -300 });

		Shake(sprite1, 1.0f, shake_duration, {}, Ease::Linear, false, true);
		Shake(sprite1, -1.0f, shake_duration, {}, Ease::Linear, false);
		Shake(sprite2, 1.0f, shake_duration, {}, false, true);
		Shake(sprite3, 0.5f, std::nullopt, {}, Ease::Linear, false);
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