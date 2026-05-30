#include "runtime/graphics/fx/gaussian_blur.h"

#include "app/application.h"
#include "core/editor.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class GaussianBlurEffectScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite", { 0.0f, 0.0f });

		CreateEffect<GaussianBlur>(*this);
	}
};

int main(int, char**) {
	Application app{ "GaussianBlurEffectScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<GaussianBlurEffectScene>();
}