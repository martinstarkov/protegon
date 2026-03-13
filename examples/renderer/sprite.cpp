#include "runtime/graphics/sprite.h"

#include "app/application.h"
#include "app/context.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class SpriteScene : public Scene {
	void OnEnter() override {
		app().asset.Load("sprite", "assets/sprite.png");

		// app().renderer.SetGameSize(V2_int{ 400, 400 }, ScalingMode::IntegerScale);

		auto sprite1 = CreateSprite(*this, "sprite", { 0, 0 });
		auto sprite2 = CreateSprite(*this, "sprite", { 0, 100.5 });
		auto sprite3 = CreateSprite(*this, "sprite", { 0, 50.5 });
		auto sprite4 = CreateSprite(*this, "sprite", { 50, 100 });
		auto sprite5 = CreateSprite(*this, "sprite", { 0, -100.5 });
		auto sprite6 = CreateSprite(*this, "sprite", { 0, -50.5 });
		auto sprite7 = CreateSprite(*this, "sprite", { -50, -100 });
	}
};

int main(int, char**) {
	Application app{ "SpriteScene" };
	app.StartWith<SpriteScene>();
}