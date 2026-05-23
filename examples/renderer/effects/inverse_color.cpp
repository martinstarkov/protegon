#include "runtime/graphics/fx/inverse_color.h"

#include "app/application.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class InverseColorEffectScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite", { -180.0f, 0.0f });
		auto sprite2{ CreateSprite(*this, "sprite", { 180.0f, 0.0f }) };

		AddEffect(sprite2, CreateEffect<InverseColor>(*this));
	}
};

int main(int, char**) {
	Application app{ "InverseColorEffectScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<InverseColorEffectScene>();
}