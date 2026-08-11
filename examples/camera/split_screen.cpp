
#include <chrono>
#include <utility>

#include "app/application.h"
#include "app/editor.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "platform/window.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/light.h"
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
#include "tools/debug/debug_system.h"

using namespace ptgn;

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

struct SplitScreenScene : public Scene {
	SceneCamera second_camera;

	void OnEnter() override {
		ctx().renderer.SetLogicalSize(V2_int{ 320, 180 });
		ctx().window.SetBackgroundColor(color::LightPurple);
		ctx().renderer.SetBackgroundColor(color::LightBlue);
		ctx().renderer.SetScalingMode(ScalingMode::Letterbox);

		SetBackgroundColor(color::LightGray.WithAlpha(0.8f));

		second_camera = CreateCamera(*this);

		ctx().camera.Add<Tag>("Left Camera");
		second_camera.Add<Tag>("Right Camera");

		ctx().camera.SetViewport(Viewport{ {}, { 0.5f, 1.0f } }, ViewportSpace::Normalized);
		second_camera.SetViewport(
			Viewport{ { 0.5f, 0.0f }, { 0.5f, 1.0f } }, ViewportSpace::Normalized
		);

		ctx().camera.SetClearColor(color::LightGold.WithAlpha(0.5f));
		second_camera.SetClearColor(color::LightPink.WithAlpha(0.5f));

		ctx().debug.settings.interaction.draw_enabled	= true;
		ctx().debug.settings.interaction.draw_line_width = 10.0f;

		V2_int rect_size{ 100, 100 };
		auto rect = CreateRect(*this, { 0, 0 }, rect_size, color::Green);
		auto child0{ CreateEntity() };
		child0.Add<Rect>(rect_size);
		AddInteractiveShape(rect, child0);
		SetDraggable(rect);
		AddScript<RectDragScript>(rect);

		float intensity{ 1.0f };
		float falloff{ 2.0f };
		auto light = CreateLight(
			*this, { 100, 0 },
			{ .color = color::Red, .radius = 50.0f, .intensity = intensity, .falloff = falloff }
		);
		float radius{ 50.0f };
		auto child{ CreateEntity() };
		child.Add<Circle>(radius);
		AddInteractiveShape(light, child);
		SetDraggable(light);
		AddScript<CircleDragScript>(light);
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
	Application app{ "SplitScreenScene: WASD/QE/ZC: Move/Rotate/Scale scene camera, Arrows/RT/FG: "
					 "Move/Rotate/Scale scene target",
					 { 1280, 720 } };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<SplitScreenScene>();
}