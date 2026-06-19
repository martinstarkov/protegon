#include "runtime/graphics/fx/blur.h"

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

class BlurEffectScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, { -180, 0 }, "sprite");

		CreateEffect<Blur>(*this);

		auto sprite2{ CreateSprite(*this, { 180, 0 }, "sprite") };

		AddEffect<Blur>(sprite2);

		CreateSprite(*this, { 0, -250 }, "sprite");

		auto sprite4{ CreateSprite(*this, { 0, 250 }, "sprite") };

		AddEffect<Blur>(sprite4).Add<EffectMargin>(5);
	}
};

int main(int, char**) {
	Application app{ "BlurEffectScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<BlurEffectScene>();
}