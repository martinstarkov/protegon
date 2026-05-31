
#include <chrono>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/grayscale.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

constexpr V2_int kWindowSize{ 800, 800 };

struct RenderTargetEffectScene : public Scene {
	RenderTarget rt;
	SceneCamera rt_camera;

	void OnEnter() override {
		constexpr V2_float rt_size{ 200, 200 };
		constexpr auto rt_layer{ GetLayer(3) };

		SetBackgroundColor(color::LightBlue);

		CreateRect(*this, { -300, -300 }, { 100, 100 }, color::Blue);

		ctx().camera.SetExcludeMask(rt_layer);

		rt = CreateRenderTarget(*this, rt_size, color::Red);
		SetPosition(rt, { 200, 200 });

		rt_camera = CreateCamera(*this, rt.GetSize());
		rt_camera.SetParentRenderTarget(rt);
		rt_camera.SetIncludeMask(rt_layer);

		auto rect2{ CreateRect(*this, V2_float{ 0, 0 }, { 100, 100 }, color::Orange) };
		SetMask(rect2, rt_layer);

		AddEffect<Grayscale>(rt);
	}

	void OnUpdate() override {
		float dt{ ctx().dt().count() };
		constexpr V2_float speed{ 300.0f };
		MoveArrowKeys(rt_camera, speed * dt);
		MoveWASD(rt, speed * dt);
	}
};

int main(int, char**) {
	Application app{ "RenderTargetEffectScene", kWindowSize };
	app.StartWith<RenderTargetEffectScene>();
}