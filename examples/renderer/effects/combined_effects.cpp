#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
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

		CreateSprite(*this, "sprite", { 0.0f, -280.0f });
	}
};

class CombinedEffectsScene : public Scene {
public:
	static constexpr V2_int kWindowSize{ 1280, 720 };

	static constexpr Viewport kLeftViewport{ { 0.0f, 0.0f },
											 { kWindowSize.x / 2.0f, kWindowSize.y } };

	static constexpr Viewport kRightViewport{ { kWindowSize.x / 2.0f, 0.0f },
											  { kWindowSize.x / 2.0f, kWindowSize.y } };

private:
	SceneCamera left_camera;

	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		left_camera = CreateCamera(*this);

		left_camera.SetTag("Left Camera");
		ctx().camera.SetTag("Right Camera");

		left_camera.SetViewport(kLeftViewport);
		ctx().camera.SetViewport(kRightViewport);

		left_camera.SetClearColor(color::LightBlue.WithAlpha(0.5f));
		ctx().camera.SetClearColor(color::LightRed.WithAlpha(0.5f));

		CreateSprite(*this, "sprite", { -180.0f, 140.0f });

		auto texture_effect_sprite{ CreateSprite(*this, "sprite", { -180.0f, -140.0f }) };
		AddEffect<EdgeDetection>(texture_effect_sprite);

		CreateEffect<Sharpen>(*this);

		CreateSprite(*this, "sprite", { 180.0f, 140.0f });

		AddEffect<Grayscale>(ctx().camera);

		AddEffect<Blur>(*this);

		AddScreenEffect<InverseColor>(*this);

		ctx().scene.Enter<CombinedEffectsSecondScene>("combined_effects_second_scene");
	}
};

int main(int, char**) {
	Application app{ "CombinedEffectsScene", CombinedEffectsScene::kWindowSize };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<CombinedEffectsScene>();
}