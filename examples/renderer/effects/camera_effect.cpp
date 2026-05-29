#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/grayscale.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

inline constexpr V2_int kWindowSize{ 1280, 720 };

inline constexpr Viewport kLeftViewport{ { 0.0f, 0.0f }, { kWindowSize.x / 2.0f, kWindowSize.y } };

inline constexpr Viewport kRightViewport{ { kWindowSize.x / 2.0f, 0.0f },
										  { kWindowSize.x / 2.0f, kWindowSize.y } };

class CameraEffectScene : public Scene {
public:
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

		CreateSprite(*this, "sprite", { -180.0f, 0.0f });
		CreateSprite(*this, "sprite", { 180.0f, 0.0f });

		AddEffect(ctx().camera, CreateEffect<Grayscale>(*this));
	}
};

int main(int, char**) {
	Application app{ "CameraEffectScene", kWindowSize };
	PTGN_WITH_EDITOR(app);
	app.StartWith<CameraEffectScene>();
}