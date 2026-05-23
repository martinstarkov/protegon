#include "app/application.h"
#include "core/editor.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class PixelSpritesScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/sprite.png");
		ctx().asset.Load("sprite2", "assets/sprite2.png");

		CreateSprite(*this, "sprite", { -50 + 0, 0 });
		CreateSprite(*this, "sprite", { -50 + 0, 100.5 });
		CreateSprite(*this, "sprite", { -50 + 0, 50.5 });
		CreateSprite(*this, "sprite", { -50 + 50, 100 });
		CreateSprite(*this, "sprite", { -50 + 0, -100.5 });
		CreateSprite(*this, "sprite", { -50 + 0, -50.5 });
		CreateSprite(*this, "sprite", { -50 + -50, -100 });

		CreateSprite(*this, "sprite2", { 50 + 0, 0 });
		CreateSprite(*this, "sprite2", { 50 + 0, 100.5 });
		CreateSprite(*this, "sprite2", { 50 + 0, 50.5 });
		CreateSprite(*this, "sprite2", { 50 + 50, 100 });
		CreateSprite(*this, "sprite2", { 50 + 0, -100.5 });
		CreateSprite(*this, "sprite2", { 50 + 0, -50.5 });
		CreateSprite(*this, "sprite2", { 50 + -50, -100 });
	}
};

int main(int, char**) {
	Application app{ "PixelSpritesScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<PixelSpritesScene>();
}