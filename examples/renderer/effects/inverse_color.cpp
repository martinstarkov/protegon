#include "runtime/graphics/fx/inverse_color.h"

#include "app/application.h"
#include "app/editor.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class InverseColorEffectScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, { -180, 0 }, "sprite");
		auto sprite2{ CreateSprite(*this, { 180, 0 }, "sprite") };

		AddEffect<InverseColor>(sprite2);
	}
};

int main(int, char**) {
	Application app{ "InverseColorEffectScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<InverseColorEffectScene>();
}