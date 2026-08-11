#include "runtime/graphics/sprite.h"

#include "app/application.h"
#include "app/editor.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class SpriteScene : public Scene {
	void OnNew() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, {}, "sprite");
	}
};

PTGN_REGISTER_SCENE(SpriteScene, "Sprite Scene");

int main(int, char**) {
	Application app{ "SpriteScene" };
	PTGN_WITH_EDITOR(app, true);
	app.StartProject<SpriteScene>(
		"SpriteSceneProject/SpriteScene.ptgnproj"
	);
}