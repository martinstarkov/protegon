#include "app/application.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/grayscale.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class EffectEntityScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite", { -60.0f, 0.0f });

		CreateEffect<Grayscale>(*this);

		CreateSprite(*this, "sprite", { 60.0f, 0.0f });
	}
};

int main(int, char**) {
	Application app{ "EffectEntityScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<EffectEntityScene>();
}