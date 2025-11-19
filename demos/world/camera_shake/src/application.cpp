#include <functional>
#include <string_view>

#include "ecs/components/draw.h"
#include "ecs/components/movement.h"
#include "ecs/components/transform.h"
#include "ecs/entity.h"
#include "core/app/application.h"
#include "core/app/window.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "ecs/components/origin.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "world/tile/grid.h"
#include "tween/tween_effect.h"
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
		Application::Get().window_.SetResizable();

		auto res{ Application::Get().render_.GetGameSize() };

		CreateRect(*this, -res * 0.5f + V2_float{ 500, 250 }, { 200, 50 }, color::Green);
		player = CreateRect(*this, -res * 0.5f + V2_float{ 400, 150 }, { 50, 50 }, color::Red);

		StartFollow(camera, player);

		grid.Set({ 0, 0 }, CreateButton("Stop Shake", [&]() { StopShake(camera); }));
		grid.Set({ 0, 1 }, CreateButton("Induce 0.10 Shake", [&]() { Shake(camera, 0.1f); }));
		grid.Set({ 0, 2 }, CreateButton("Induce 0.25 Shake", [&]() { Shake(camera, 0.25f); }));
		grid.Set({ 0, 3 }, CreateButton("Induce 0.75 Shake", [&]() { Shake(camera, 0.5f); }));
		grid.Set({ 0, 4 }, CreateButton("Induce 1.00 Shake", [&]() { Shake(camera, 1.0f); }));

		V2_float screen_offset{ 30, 30 };
		V2_float offset{ 6, 6 };
		V2_float size{ 200, 50 };

		grid.ForEach([&](auto coord, Button& b) {
			if (!b) {
				return;
			}
			SetPosition(b, -res * 0.5f + screen_offset + (offset + size) * coord);
			b.SetSize(size);
			SetDrawOrigin(b, Origin::TopLeft);
			b.Add<Camera>(fixed_camera);
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
	Application::Get().Init("CameraShakeScene: WASD: Move");
	Application::Get().scene_.Enter<CameraShakeScene>("");
	return 0;
}
