
#include <utility>

#include "app/application.h"
#include "core/event/dispatcher.h"
#include "core/math/geometry/circle.h"
#include "core/math/vector2.h"
#include "platform/input/key.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/scaling_mode.h"
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

struct CircleDragScript : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<Dragging>([this](auto d) { OnDrag(d.offset); });
	}

	void OnDrag(V2_float offset) {
		SetPosition(entity, entity.GetScene().input.GetMousePosition() + offset);
	}
};

struct ResolutionScene : public Scene {
	Sprite circle;

	void OnEnter() override {
		app().renderer.SetBackgroundColor(color::LightBlue);
		app().renderer.SetScalingMode(ScalingMode::Letterbox);

		RenderTarget rt{ GetRenderTarget() };

		// SetRotation(rt, DegToRad(45.0f));
		// SetScale(rt, 0.5f);
		// SetPosition(rt, V2_float{ 600, 0 });

		SetBackgroundColor(color::LightGray);

		camera.SetViewport({ {}, { 600, 300 } });

		input.SetInteractiveDebugDraw({ .enabled = true, .line_width = 10.0f });

		V2_float camera_center{ GetTransform(camera).GetPosition() };

		CreateRect(*this, camera_center - V2_float{ 100, 0 }, { 100, 100 }, color::Green);

		// TODO: Fix point light.
		/*
		float intensity{ 0.5f };
		float falloff{ 2.0f };

		CreatePointLight(
			*this, camera_center + V2_float{ 100, 0 }, 50.0f, color::Red, intensity, falloff
		);*/

		float radius{ 50.0f };
		circle = Sprite{ CreateEntity() };
		SetPosition(circle, camera_center);
		auto child{ CreateEntity() };
		child.Add<Circle>(radius);
		AddInteractiveShape(circle, GameObject{ std::move(child) });
		SetDraggable(circle);
		AddScript<CircleDragScript>(circle);
	}

	const float rotation_speed{ 1.0f };
	const float zoom_speed{ 0.4f };

	void OnUpdate() override {
		MoveWASD(camera, { 3.0f, 3.0f });

		float dt{ app().DeltaTime().count() };

		PTGN_LOG("Dt: ", dt);

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
		MoveArrowKeys(GetRenderTarget(), { 3.0f, 3.0f });

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
					 { 1200, 800 } };
	app.StartWith<ResolutionScene>();
}