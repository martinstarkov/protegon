#include <functional>
#include <string_view>

#include "app/application.h"
#include "app/context.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "platform/window/window.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/ui/button.h"
#include "runtime/world/grid.h"

using namespace ptgn;

class CameraShakeScene : public Scene {
public:
	Entity player;

	Grid<Button> grid{ { 1, 5 } };

	Button CreateButton(std::string_view content, const std::function<void()>& on_activate) {
		Button b{ CreateTextButton(*this, content, color::Black) };
		b.SetBackgroundColor(color::Gold);
		b.SetBackgroundColor(color::Gray, ButtonState::Hover);
		b.SetBackgroundColor(color::DarkGray, ButtonState::Pressed);
		b.SetBorderColor(color::LightGray);
		b.SetBorderWidth(3.0f);
		b.OnActivate(on_activate);
		return b;
	}

	void OnEnter() override {
		input.SetInteractiveSettings({ .enabled = true });

		input.SetTopOnly(true);
		auto res{ app().renderer.GetGameSize() };

		CreateRect(*this, -res * 0.5f + V2_float{ 500, 250 }, { 200, 50 }, color::Green);
		player = CreateRect(*this, -res * 0.5f + V2_float{ 400, 150 }, { 50, 50 }, color::Red);

		StartFollow(camera, player);
		// TranslateTo(camera, GetPosition(player), 1000ms);

		grid.Set({ 0, 0 }, CreateButton("Stop Shake", [&]() { StopShake(camera); }));
		grid.Set({ 0, 1 }, CreateButton("Induce 0.10 Shake", [&]() { Shake(camera, 0.1f); }));
		grid.Set({ 0, 2 }, CreateButton("Induce 0.25 Shake", [&]() { Shake(camera, 0.25f); }));
		grid.Set({ 0, 3 }, CreateButton("Induce 0.75 Shake", [&]() { Shake(camera, 0.5f); }));
		grid.Set({ 0, 4 }, CreateButton("Induce 1.00 Shake", [&]() { Shake(camera, 1.0f); }));

		V2_float screen_offset{ 30, 30 };
		V2_float offset{ 6, 6 };
		V2_float size{ 200, 50 };

		grid.ForEach([&](V2_int coord, Button& b) {
			if (!b) {
				return;
			}
			SetPosition(b, -res * 0.5f + screen_offset + (offset + size) * coord);
			b.SetSize(size);
			SetDrawOrigin(b, Origin::TopLeft);
			if (coord == V2_int{}) {
				SetUI(b, false);
			}
		});
	}

	void OnUpdate() override {
		constexpr V2_float speed{ 300.0f };
		V2_float pos{ GetPosition(player) };
		float dt{ app().DeltaTime().count() };
		MoveWASD(*this, pos, speed * dt, false);
		SetPosition(player, pos);
	}
};

int main(int, char**) {
	Application game{ "CameraShakeScene: WASD: Move" };
	game.StartWith<CameraShakeScene>();
}
