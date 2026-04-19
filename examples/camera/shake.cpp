#include <chrono>
#include <functional>
#include <string_view>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/resources/shader.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/custom_shader.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/shape.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/world/grid.h"

using namespace ptgn;

class CameraShakeScene : public Scene {
public:
	Entity player;

	Grid<Button> grid{ { 1, 5 } };

	Button CreateButton(std::string_view content, const std::function<void()>& on_press) {
		Button b{ ptgn::CreateButton(*this) };
		b.SetText(content, color::Black);
		b.SetBackgroundColor(color::Gold);
		b.SetBackgroundColor(color::Gray, ButtonState::Hover);
		b.SetBackgroundColor(color::DarkGray, ButtonState::Press);
		b.SetBorderColor(color::LightGray);
		b.SetBorderWidth(3.0f);
		b.OnPress(on_press);
		return b;
	}

	void OnEnter() override {
		ctx().asset.LoadShader("whirlpool", "assets/shader.glsl", "whirlpool");
		ctx().asset.LoadTexture("noise", "assets/noise.png");

		ctx().interaction.SetDebugSettings({ .draw_enabled = true });

		ctx().interaction.SetTopOnly(true);
		auto res{ ctx().renderer.GetGameSize() };

		CreateRect(*this, -res * 0.5f + V2_float{ 500, 250 }, { 200, 50 }, color::Green);

		V2_float player_pos{ 0, 0 };
		// V2_float player_pos{ -res * 0.5f + V2_float{ 400, 150 } };
		player = CreateRect(*this, player_pos, { 50, 50 }, color::Red);

		auto shader_entity = CreateCustomShader(
			*this, "whirlpool", "noise", V2_float{}, V2_float{ 200.0f },
			[this](auto, auto s) mutable {
				float timescale{ 1.0f };
				float scale{ 0.5f };
				float opacity{ 0.5f };

				float time{ static_cast<float>(ctx().TimeSinceStart().count()) };
				s.SetUniform("u_Time", time / 1000.0f * timescale);
				s.SetUniform("u_Scale", scale);
				s.SetUniform("u_Opacity", opacity);
			},
			Origin::Center
		);

		StartFollow(ctx().camera, player);
		// TranslateTo(camera, GetPosition(player), 1000ms);

		grid.Set({ 0, 0 }, CreateButton("Stop Shake", [&]() { StopShake(ctx().camera); }));
		grid.Set({ 0, 1 }, CreateButton("Induce 0.10 Shake", [&]() { Shake(ctx().camera, 0.1f); }));
		grid.Set({ 0, 2 }, CreateButton("Induce 0.25 Shake", [&]() {
					 Shake(ctx().camera, 0.25f);
				 }));
		grid.Set({ 0, 3 }, CreateButton("Induce 0.75 Shake", [&]() { Shake(ctx().camera, 0.5f); }));
		grid.Set({ 0, 4 }, CreateButton("Induce 1.00 Shake", [&]() { Shake(ctx().camera, 1.0f); }));

		V2_float screen_offset{ 30, 30 };
		V2_float offset{ 6, 6 };
		V2_float size{ 200, 50 };

		grid.ForEach([&](V2_int coord, Button& b) {
			if (!b) {
				return;
			}
			SetPosition(b, -res * 0.5f + screen_offset + (offset + size) * coord);
			b.SetShape(size);
			SetDrawOrigin(b, Origin::TopLeft);
			if (coord == V2_int{}) {
				SetUI(b, false);
			}
		});
	}

	void OnUpdate() override {
		constexpr V2_float speed{ 300.0f };
		V2_float pos{ GetPosition(player) };
		float dt{ ctx().dt().count() };
		MoveWASD(*this, pos, speed * dt, false);
		SetPosition(player, pos);
	}
};

int main(int, char**) {
	Application game{ "CameraShakeScene: WASD: Move" };
	game.StartWith<CameraShakeScene>();
}
