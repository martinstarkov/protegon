#include "app/application.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/grayscale.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

class ScreenEffectSecondScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite", { 60.0f, 0.0f });
	}
};

class ScreenEffectFirstScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite", { -60.0f, 0.0f });

		AddScreenEffect<Grayscale>(*this);

		ctx().scene.Enter<ScreenEffectSecondScene>("second_scene");
	}
};

int main(int, char**) {
	Application app{ "ScreenEffectScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<ScreenEffectFirstScene>();
}