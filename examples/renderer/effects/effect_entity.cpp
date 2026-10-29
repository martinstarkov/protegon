#include "app/application.h"
#include "app/editor.h"
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

		CreateSprite(*this, { -180, 0 }, "sprite");
		CreateSprite(*this, { 180, 0 }, "sprite");

		CreateEffect<Grayscale>(*this);

		CreateSprite(*this, { 0, -250 }, "sprite");
	}
};

int main(int, char**) {
	Application app{ "EffectEntityScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<EffectEntityScene>();
}