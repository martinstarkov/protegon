#include <optional>

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

struct ShakeEffectScene : public Scene {
	Sprite sprite1;
	Sprite sprite2;
	Sprite sprite3;

	milliseconds shake_duration{ 3000 };

	void OnEnter() override {
		ctx().asset.Load("smile", "assets/smile.png");

		sprite1 = CreateSprite(*this, { -300, -300 }, "smile");
		sprite2 = CreateSprite(*this, { -300, 200 }, "smile");
		sprite3 = CreateSprite(*this, { 200, -300 }, "smile");

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
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ShakeEffectScene>();
}