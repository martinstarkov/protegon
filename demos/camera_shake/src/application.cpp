#include <functional>
#include <string_view>

#include "components/draw.h"
#include "components/movement.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tile/grid.h"
#include "tweens/tween_effects.h"
#include "ui/button.h"

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

	void Enter() override {
		CreateRect(*this, V2_float{ 300.0f, 300.0f }, { 150.0f, 50.0f }, color::Green);
		player = CreateRect(*this, V2_float{ 400.0f, 150.0f }, { 50.0f, 50.0f }, color::Red);
		auto transform1{ GetTransform(player) };
		auto abs_transform1{ GetAbsoluteTransform(player) };
		auto transform2{ GetTransform(camera.primary) };
		auto abs_transform2{ GetAbsoluteTransform(camera.primary) };
		camera.primary.StartFollow(player);
		auto transform3{ GetTransform(player) };
		auto abs_transform3{ GetAbsoluteTransform(player) };
		auto transform4{ GetTransform(camera.primary) };
		auto abs_transform4{ GetAbsoluteTransform(camera.primary) };

		grid.Set({ 0, 0 }, CreateButton("Stop Shake", [&]() { StopShake(camera.primary); }));
		grid.Set({ 0, 1 }, CreateButton("Induce 0.10 Shake", [&]() {
					 Shake(camera.primary, 0.1f, {}, false);
				 }));
		grid.Set({ 0, 2 }, CreateButton("Induce 0.25 Shake", [&]() {
					 Shake(camera.primary, 0.25f, {}, false);
				 }));
		grid.Set({ 0, 3 }, CreateButton("Induce 0.75 Shake", [&]() {
					 Shake(camera.primary, 0.5f, {}, false);
				 }));
		grid.Set({ 0, 4 }, CreateButton("Induce 1.00 Shake", [&]() {
					 Shake(camera.primary, 1.0f, {}, false);
				 }));

		V2_float screen_offset{ 10, 30 };
		V2_float offset{ 6, 6 };
		V2_float size{ 200, 50 };

		grid.ForEach([&](auto coord, Button& b) {
			SetPosition(b, screen_offset + (offset + size) * coord);
			b.SetSize(size);
			SetDrawOrigin(b, Origin::TopLeft);
		});
	}

	void Update() override {
		constexpr V2_float speed{ 3.0f, 3.0f };
		V2_float pos{ GetPosition(player) };
		MoveWASD(pos, speed, false);
		SetPosition(player, pos);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("CameraShakeScene: WASD: Move");
	game.scene.Enter<CameraShakeScene>("");
	return 0;
}
