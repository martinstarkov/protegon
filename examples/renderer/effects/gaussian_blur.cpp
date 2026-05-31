#include "runtime/graphics/fx/gaussian_blur.h"

#include "app/application.h"
#include "core/editor.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class GaussianBlurEffectScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite", { -180.0f, 0.0f });

		CreateEffect<GaussianBlur>(*this);

		auto sprite2{ CreateSprite(*this, "sprite", { 180.0f, 0.0f }) };

		AddEffect<GaussianBlur>(sprite2);

		CreateSprite(*this, "sprite", { 0.0f, -250.0f });

		auto sprite4{ CreateSprite(*this, "sprite", { 0.0f, 250.0f }) };

		AddEffect<GaussianBlur>(sprite4).Add<EffectMargin>(5);
	}
};

int main(int, char**) {
	Application app{ "GaussianBlurEffectScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<GaussianBlurEffectScene>();
}