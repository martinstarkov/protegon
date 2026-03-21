
#include <utility>

#include "app/application.h"
#include "app/context.h"
#include "core/event/dispatcher.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "platform/input/key.h"
#include "platform/window/window.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/scaling_mode.h"
#include "renderer/primitives/viewport.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/render_target_component.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"
#include "runtime/ui/interactive.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int game_size{ 320, 180 };
constexpr Viewport camera0_viewport{ {}, { game_size.x / 2.0f, game_size.y } };
constexpr Viewport camera_viewport{ { game_size.x / 2.0f, 0.0f },
									{ game_size.x / 2.0f, game_size.y } };

struct RectDragScript : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<Dragging>([this](auto d) { OnDrag(d.position); });
	}

	void OnDrag(V2_float pos) {
		SetPosition(entity, pos);
		PTGN_LOG("Position: ", pos);
	}
};

struct CircleDragScript : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<Dragging>([this](auto d) { OnDrag(d.position); });
	}

	void OnDrag(V2_float pos) {
		SetPosition(entity, pos);
	}
};

struct ResolutionScene : public Scene {
	Sprite circle;

	Camera camera0;

	void OnEnter() override {
		app().renderer.SetGameSize(game_size);
		app().window.SetBackgroundColor(color::LightPurple);
		app().renderer.SetBackgroundColor(color::LightBlue);
		app().renderer.SetScalingMode(ScalingMode::Letterbox);

		SetBackgroundColor(color::LightGray.WithAlpha(0.8f));

		camera0 = CreateCamera(*this);

		camera0.SetClearColor(color::LightPink.WithAlpha(0.5f));
		camera.SetClearColor(color::LightGold.WithAlpha(0.5f));

		camera0.SetViewport(camera0_viewport);
		camera.SetViewport(camera_viewport);

		input.SetSettings({ .debug_draw_enabled = true, .debug_draw_line_width = 10.0f });

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

	const float rotation_speed{ 1.0f };
	const float zoom_speed{ 0.4f };

	void OnUpdate() override {
		constexpr V2_float speed{ 300.0f, 300.0f };
		float dt{ app().DeltaTime().count() };
		MoveWASD(camera, speed * dt);

		if (input.KeyHeld(Key::Q)) {
			Rotate(camera, rotation_speed * dt);
		}
		if (input.KeyHeld(Key::E)) {
			Rotate(camera, -rotation_speed * dt);
		}
		if (input.KeyHeld(Key::Z)) {
			camera.Zoom(zoom_speed * dt);
		}
		if (input.KeyHeld(Key::C)) {
			camera.Zoom(-zoom_speed * dt);
		}

		RenderTarget scene_target{ GetRenderTarget() };
		MoveArrowKeys(GetRenderTarget(), speed * dt);

		if (input.KeyHeld(Key::R)) {
			Rotate(scene_target, rotation_speed * dt);
		}
		if (input.KeyHeld(Key::T)) {
			Rotate(scene_target, -rotation_speed * dt);
		}
		if (input.KeyHeld(Key::F)) {
			SetScale(scene_target, GetScale(scene_target) + V2_float{ zoom_speed * dt });
		}
		if (input.KeyHeld(Key::G)) {
			SetScale(scene_target, GetScale(scene_target) + V2_float{ -zoom_speed * dt });
		}
	}
};

int main(int, char**) {
	Application app{ "ResolutionScene: WASD/QE/ZC: Move/Rotate/Scale scene camera, Arrows/RT/FG: "
					 "Move/Rotate/Scale scene target",
					 window_size };
	app.StartWith<ResolutionScene>();
}