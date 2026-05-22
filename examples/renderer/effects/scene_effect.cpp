#include "app/application.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/grayscale.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

class SceneEffectSecondScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite", { 60.0f, 0.0f });
	}
};

class SceneEffectFirstScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite", { -60.0f, 0.0f });

		AddEffect(*this, CreateEffect<Grayscale>(*this));

		ctx().scene.Enter<SceneEffectSecondScene>("second_scene");
	}
};

int main(int, char**) {
	Application app{ "SceneEffectScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<SceneEffectFirstScene>();
}