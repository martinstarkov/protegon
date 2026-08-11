
#include "runtime/graphics/render_target.h"

#include <chrono>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

struct RenderTargetScene : public Scene {
	RenderTarget rt;
	SceneCamera rt_camera;

	void OnEnter() override {
		constexpr V2_float rt_size{ 200 };
		constexpr auto rt_layer{ GetLayer(3) };

		SetBackgroundColor(color::LightBlue);

		CreateRect(*this, { -300, -300 }, { 100, 100 }, color::Blue);

		ctx().camera.SetExcludeMask(rt_layer);

		rt = CreateRenderTarget(*this, { 200, 200 }, rt_size, color::Red);

		rt_camera = CreateCamera(*this, {}, rt.GetSize());
		rt_camera.SetRenderTarget(rt);
		rt_camera.SetIncludeMask(rt_layer);

		auto rect2{ CreateRect(*this, { 0, 0 }, { 100, 100 }, color::Orange) };
		SetMask(rect2, rt_layer);
	}

	void OnUpdate() override {
		float dt{ ctx().dt().count() };
		constexpr V2_float speed{ 300.0f };
		MoveArrowKeys(rt_camera, speed * dt);
		MoveWASD(rt, speed * dt);
	}
};

int main(int, char**) {
	Application app{ "RenderTargetScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<RenderTargetScene>();
}