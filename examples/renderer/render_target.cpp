
#include "app/application.h"
#include "core/math/vector2.h"
#include "platform/window/window.h"
#include "renderer/primitives/shader.h"
#include "renderer/renderer.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

// TODO: Fix this demo.

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

struct RenderTargetScene : public Scene {
	RenderTarget rt1;
	RenderTarget rt2;

	void OnEnter() override {
		SetBackgroundColor(color::LightGray);

		ctx().renderer.SetGameSize(game_size);

		CreateRect(
			*this, V2_float{ 200, -200 }, { 200, 200 }, color::Gray, Solid{}, Origin::Center
		);

		rt1 = CreateRenderTarget(*this, { 400, 400 }, color::Red);
		SetDrawOrigin(rt1, Origin::TopLeft);
		SetPosition(rt1, -game_size * 0.5f);

		auto rect1 = CreateRect(
			*this, V2_float{ 0, 0 }, { 100, 100 }, color::Orange, Solid{}, Origin::Center
		);

		// rt1.AddToDisplayList(rect1);

		rt2 = CreateRenderTarget(*this, { 400, 400 }, color::Cyan);
		SetDrawOrigin(rt2, Origin::TopLeft);
		SetPosition(rt2, -game_size * 0.5f + V2_float{ 400, 400 });

		// Rect2 position is relative to rt position (0, 0 is center of rt).
		auto rect2 = CreateRect(
			*this, V2_float{ 0, 0 }, { 100, 100 }, color::White, Solid{}, Origin::Center
		);

		// rt2.AddToDisplayList(rect2);
	}

	void OnUpdate() override {
		float dt{ ctx().dt().count() };
		constexpr V2_float speed{ 300.0f };
		// TODO: Fix.
		// MoveArrowKeys(camera1, speed * dt);
		// MoveWASD(camera2, speed * dt);
	}
};

int main(int, char**) {
	Application app{ "RenderTargetScene", game_size };
	app.StartWith<RenderTargetScene>();
}