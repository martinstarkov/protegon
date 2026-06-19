#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "renderer/pipeline/viewport.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class CameraTintScene : public Scene {
private:
	SceneCamera second_camera;

	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		second_camera = CreateCamera(*this);

		ctx().camera.SetTag("Left Camera");
		second_camera.SetTag("Right Camera");

		ctx().camera.SetViewport(
			Viewport{ { 0.0f, 0.0f }, { 0.5f, 1.0f } }, ViewportSpace::Normalized
		);
		second_camera.SetViewport(
			Viewport{ { 0.5f, 0.0f }, { 0.5f, 1.0f } }, ViewportSpace::Normalized
		);

		ctx().camera.SetClearColor(color::LightBlue.WithAlpha(0.5f));
		second_camera.SetClearColor(color::LightBlue.WithAlpha(0.5f));

		CreateSprite(*this, { -180, 0 }, "sprite");
		CreateSprite(*this, { 180, 0 }, "sprite");

		SetTint(ctx().camera, color::Red);
	}
};

int main(int, char**) {
	Application app{ "CameraTintScene", { 1280, 720 } };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<CameraTintScene>();
}