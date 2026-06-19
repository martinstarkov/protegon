#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "renderer/pipeline/viewport.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/edge_detection.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/grayscale.h"
#include "runtime/graphics/fx/inverse_color.h"
#include "runtime/graphics/fx/sharpen.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

class CombinedEffectsSecondScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, { 0, -280 }, "sprite");
	}
};

class CombinedEffectsScene : public Scene {
private:
	SceneCamera second_camera;

	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		second_camera = CreateCamera(*this);

		ctx().camera.SetTag("Left Camera");
		second_camera.SetTag("Right Camera");

		ctx().camera.SetViewport(Viewport{ {}, { 0.5f, 1.0f } }, ViewportSpace::Normalized);
		second_camera.SetViewport(
			Viewport{ { 0.5f, 0.0f }, { 0.5f, 1.0f } }, ViewportSpace::Normalized
		);

		ctx().camera.SetClearColor(color::LightRed.WithAlpha(0.5f));
		second_camera.SetClearColor(color::LightBlue.WithAlpha(0.5f));

		CreateSprite(*this, { -180, 140 }, "sprite");

		auto texture_effect_sprite{ CreateSprite(*this, { -180, -140 }, "sprite") };
		AddEffect<EdgeDetection>(texture_effect_sprite);

		CreateEffect<Sharpen>(*this);

		CreateSprite(*this, { 180, 140 }, "sprite");

		AddEffect<Grayscale>(ctx().camera);

		AddEffect<Blur>(*this);

		AddScreenEffect<InverseColor>(*this);

		ctx().scene.Enter<CombinedEffectsSecondScene>("combined_effects_second_scene");
	}
};

int main(int, char**) {
	Application app{ "CombinedEffectsScene", { 1280, 720 } };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<CombinedEffectsScene>();
}