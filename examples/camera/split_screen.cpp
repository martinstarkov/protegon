
#include <chrono>
#include <utility>

#include "app/application.h"
#include "core/editor.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "platform/window.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/shape.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/draggable_event.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int game_size{ 320, 180 };
constexpr Viewport camera0_viewport{ {}, { game_size.x / 2.0f, game_size.y } };
constexpr Viewport camera_viewport{ { game_size.x / 2.0f, 0.0f },
									{ game_size.x / 2.0f, game_size.y } };

struct RectDragScript : public Script {
	void OnEvent(Event d) override {
		d.Dispatch<event::Drag>(&RectDragScript::OnDrag, this);
	}

	void OnDrag(V2_float pos) const {
		SetPosition(entity, pos);
		PTGN_LOG("Position: ", pos);
	}
};

struct CircleDragScript : public Script {
	void OnEvent(Event d) override {
		d.Dispatch<event::Drag>(&CircleDragScript::OnDrag, this);
	}

	void OnDrag(V2_float pos) const {
		SetPosition(entity, pos);
	}
};

struct ResolutionScene : public Scene {
	SceneCamera camera0;

	void OnEnter() override {
		ctx().renderer.SetGameSize(game_size);
		ctx().window.SetBackgroundColor(color::LightPurple);
		ctx().renderer.SetBackgroundColor(color::LightBlue);
		ctx().renderer.SetScalingMode(ScalingMode::Letterbox);

		SetBackgroundColor(color::LightGray.WithAlpha(0.8f));

		camera0 = CreateCamera(*this);

		camera0.SetClearColor(color::LightPink.WithAlpha(0.5f));
		ctx().camera.SetClearColor(color::LightGold.WithAlpha(0.5f));

		camera0.SetViewport(camera0_viewport);
		ctx().camera.SetViewport(camera_viewport);

		ctx().interaction.SetDebugSettings({ .draw_enabled = true, .draw_line_width = 10.0f });

		V2_int rect_size{ 100, 100 };
		auto rect = CreateRect(*this, { 0, 0 }, rect_size, color::Green);
		auto child0{ CreateEntity() };
		child0.Add<Rect>(rect_size);
		AddInteractiveShape(rect, GameObject{ std::move(child0) });
		SetDraggable(rect);
		AddScript<RectDragScript>(rect);

		// TODO: Fix point light.
		/*
		float intensity{ 0.5f };
		float falloff{ 2.0f };

		CreatePointLight(
			*this, camera_center + V2_float{ 100, 0 }, 50.0f, color::Red, intensity, falloff
		);*/

		/*float radius{ 50.0f };
		circle = Sprite{ CreateEntity() };
		auto child{ CreateEntity() };
		child.Add<Circle>(radius);
		AddInteractiveShape(circle, GameObject{ std::move(child) });
		SetDraggable(circle);
		AddScript<CircleDragScript>(circle);*/
	}

	const float rotation_speed{ 100.0f };
	const float zoom_speed{ 0.4f };

	void OnUpdate() override {
		constexpr V2_float speed{ 300.0f, 300.0f };
		float dt{ ctx().dt().count() };
		MoveWASD(ctx().camera, speed * dt);

		if (ctx().input.KeyHeld(Key::Q)) {
			Rotate(ctx().camera, rotation_speed * dt);
		}
		if (ctx().input.KeyHeld(Key::E)) {
			Rotate(ctx().camera, -rotation_speed * dt);
		}
		if (ctx().input.KeyHeld(Key::Z)) {
			ctx().camera.Zoom(zoom_speed * dt);
		}
		if (ctx().input.KeyHeld(Key::C)) {
			ctx().camera.Zoom(-zoom_speed * dt);
		}

		RenderTarget scene_target{ GetRenderTarget() };
		MoveArrowKeys(GetRenderTarget(), speed * dt);

		if (ctx().input.KeyHeld(Key::R)) {
			Rotate(scene_target, rotation_speed * dt);
		}
		if (ctx().input.KeyHeld(Key::T)) {
			Rotate(scene_target, -rotation_speed * dt);
		}
		if (ctx().input.KeyHeld(Key::F)) {
			SetScale(scene_target, GetScale(scene_target) + V2_float{ zoom_speed * dt });
		}
		if (ctx().input.KeyHeld(Key::G)) {
			SetScale(scene_target, GetScale(scene_target) + V2_float{ -zoom_speed * dt });
		}
	}
};

int main(int, char**) {
	Application app{ "ResolutionScene: WASD/QE/ZC: Move/Rotate/Scale scene camera, Arrows/RT/FG: "
					 "Move/Rotate/Scale scene target",
					 window_size };
	PTGN_WITH_EDITOR(app);
	app.StartWith<ResolutionScene>();
}