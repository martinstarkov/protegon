#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/blur.h"
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

		// This sprite belongs to a different scene.
		// It should not receive the first scene's scene effect,
		// but it should receive the final screen effect.
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

		// ---------------------------------------------------------------------
		// 1. Texture/entity-local effect.
		//
		// These two sprites use the same texture. The left one is untouched.
		// The right one has a local grayscale effect.
		// ---------------------------------------------------------------------

		// CreateSprite(*this, "sprite", { -180.0f, 140.0f });
		// auto local_effect_sprite{ CreateSprite(*this, "sprite", { 180.0f, 140.0f }) };

		// AddEffect(local_effect_sprite, CreateEffect<Sharpen>(*this));

		// ---------------------------------------------------------------------
		// 2. Effect entity.
		//
		// This relies on creation order. The first sprite is created before the
		// effect entity, so it should be affected. The second sprite is created
		// after the effect entity, so it should not be affected by this specific
		// effect entity.
		// ---------------------------------------------------------------------

		// CreateSprite(*this, "sprite", { -180.0f, -140.0f });

		// TODO: Remove.
		CreateSprite(*this, "sprite", { 0.0f, 0.0f });

		CreateEffect<Grayscale>(*this);

		// CreateSprite(*this, "sprite", { 180.0f, -140.0f });

		// ---------------------------------------------------------------------
		// 3. Camera effect.
		//
		// The right split-screen camera receives blur. The left split-screen
		// camera is the comparison view.
		// ---------------------------------------------------------------------

		// AddEffect(ctx().camera, CreateEffect<InverseColor>(*this));

		// ---------------------------------------------------------------------
		// 4. Scene effect.
		//
		// This applies only to this scene's render target.
		// CombinedEffectsSecondScene should not receive this scene effect.
		// ---------------------------------------------------------------------

		// AddEffect(*this, CreateEffect<Blur>(*this));

		// ---------------------------------------------------------------------
		// 5. Screen effect.
		//
		// This applies after all active scenes are composed, so it should affect
		// both this scene and CombinedEffectsSecondScene.
		// ---------------------------------------------------------------------

		// AddScreenEffect<Grayscale>(*this);

		// Enter a second scene so the screen effect can be distinguished from
		// the scene effect.
		ctx().scene.Enter<CombinedEffectsSecondScene>("combined_effects_second_scene");
	}
};

int main(int, char**) {
	Application app{ "CombinedEffectsScene", CombinedEffectsScene::kWindowSize };
	PTGN_WITH_EDITOR(app);
	app.StartWith<CombinedEffectsScene>();
}