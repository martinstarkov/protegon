#include "runtime/graphics/sprite.h"

#include "app/application.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"


using namespace ptgn;

class SpriteScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/sprite.png");
		ctx().asset.Load("sprite2", "assets/sprite2.png");

		auto sprite1 = CreateSprite(*this, "sprite", { -50 + 0, 0 });
		auto sprite2 = CreateSprite(*this, "sprite", { -50 + 0, 100.5 });
		auto sprite3 = CreateSprite(*this, "sprite", { -50 + 0, 50.5 });
		auto sprite4 = CreateSprite(*this, "sprite", { -50 + 50, 100 });
		auto sprite5 = CreateSprite(*this, "sprite", { -50 + 0, -100.5 });
		auto sprite6 = CreateSprite(*this, "sprite", { -50 + 0, -50.5 });
		auto sprite7 = CreateSprite(*this, "sprite", { -50 + -50, -100 });

		auto sprite21 = CreateSprite(*this, "sprite2", { 50 + 0, 0 });
		auto sprite22 = CreateSprite(*this, "sprite2", { 50 + 0, 100.5 });
		auto sprite23 = CreateSprite(*this, "sprite2", { 50 + 0, 50.5 });
		auto sprite24 = CreateSprite(*this, "sprite2", { 50 + 50, 100 });
		auto sprite25 = CreateSprite(*this, "sprite2", { 50 + 0, -100.5 });
		auto sprite26 = CreateSprite(*this, "sprite2", { 50 + 0, -50.5 });
		auto sprite27 = CreateSprite(*this, "sprite2", { 50 + -50, -100 });
	}
};

int main(int, char**) {
	Application app{ "SpriteScene" };
	app.StartWith<SpriteScene>();
}