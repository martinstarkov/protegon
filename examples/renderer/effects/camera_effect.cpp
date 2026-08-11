#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "renderer/pipeline/viewport.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/grayscale.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class CameraEffectScene : public Scene {
private:
	SceneCamera second_camera;

	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		second_camera = CreateCamera(*this);

		ctx().camera.Add<Tag>("Left Camera");
		second_camera.Add<Tag>("Right Camera");

		ctx().camera.SetViewport(Viewport{ {}, { 0.5f, 1.0f } }, ViewportSpace::Normalized);
		second_camera.SetViewport(
			Viewport{ { 0.5f, 0.0f }, { 0.5f, 1.0f } }, ViewportSpace::Normalized
		);

		ctx().camera.SetClearColor(color::LightRed.WithAlpha(0.5f));
		second_camera.SetClearColor(color::LightBlue.WithAlpha(0.5f));

		CreateSprite(*this, {}, "sprite");

		AddEffect<Grayscale>(ctx().camera);
	}
};

int main(int, char**) {
	Application app{ "CameraEffectScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<CameraEffectScene>();
}