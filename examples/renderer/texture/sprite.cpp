#include "runtime/graphics/sprite.h"

#include "app/application.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class SpriteScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite");
	}
};

int main(int, char**) {
	Application app{ "SpriteScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<SpriteScene>();
}