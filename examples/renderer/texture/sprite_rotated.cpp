#include "app/application.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class SpriteRotatedScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		auto sprite{ CreateSprite(*this, "sprite") };

		SetRotation(sprite, 45.0f);
	}
};

int main(int, char**) {
	Application app{ "SpriteRotatedScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<SpriteRotatedScene>();
}