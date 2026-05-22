#include "runtime/graphics/fx/sharpen.h"

#include "app/application.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class SharpenEffectScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		auto sprite1{ CreateSprite(*this, "sprite", { -60.0f, 0.0f }) };
		auto sprite2{ CreateSprite(*this, "sprite", { 60.0f, 0.0f }) };

		AddEffect(sprite2, CreateEffect<Sharpen>(*this));
	}
};

int main(int, char**) {
	Application app{ "SharpenEffectScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<SharpenEffectScene>();
}