
#include <utility>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/geometry/circle.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "platform/window/window.h"
#include "renderer/primitives/light.h"
#include "renderer/renderer.h"
#include "renderer/resources/render_target.h"
#include "runtime/ecs/components/camera_component.h"
#include "runtime/ecs/components/sprite.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/input/interactive.h"
#include "runtime/input/movement.h"
#include "runtime/input/scene_input.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scripting/script.h"

using namespace ptgn;

struct CircleDragScript : public Script<CircleDragScript, DragScript> {
	void OnDrag() override {
		SetPosition(
			entity, entity.GetScene().input.GetMousePosition() + entity.Get<Draggable>().GetOffset()
		);
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

		camera.CenterOnViewport({ 600, 300 });

		input.SetDrawInteractives(true);
		input.SetDrawInteractivesLineWidth(10.0f);

		V2_float camera_center{ GetTransform(camera).GetPosition() };

		CreateRect(*this, camera_center - V2_float{ 100, 0 }, { 100, 100 }, color::Green);

		float intensity{ 0.5f };
		float falloff{ 2.0f };

		CreatePointLight(
			*this, camera_center + V2_float{ 100, 0 }, 50.0f, color::Red, intensity, falloff
		);

		float radius{ 50.0f };
		circle = CreateEntity();
		SetPosition(circle, camera_center);
		auto child{ CreateEntity(*this) };
		child.Add<Circle>(radius);
		AddInteractable(circle, std::move(child));
		circle.Add<Draggable>();
		AddScript<CircleDragScript>(circle);
	}

	const float rotation_speed{ 1.0f };
	const float zoom_speed{ 0.4f };

	void OnUpdate() override {
		MoveWASD(camera, { 3.0f, 3.0f });

		auto dt{ app().DeltaTime() };

		if (input.KeyPressed(Key::Q)) {
			Rotate(camera, rotation_speed * dt);
		}
		if (input.KeyPressed(Key::E)) {
			Rotate(camera, -rotation_speed * dt);
		}
		if (input.KeyPressed(Key::Z)) {
			camera.Zoom(zoom_speed * dt);
		}
		if (input.KeyPressed(Key::C)) {
			camera.Zoom(-zoom_speed * dt);
		}

		RenderTarget scene_target{ GetRenderTarget() };
		MoveArrowKeys(GetRenderTarget(), { 3.0f, 3.0f });

		if (input.KeyPressed(Key::R)) {
			Rotate(scene_target, rotation_speed * dt);
		}
		if (input.KeyPressed(Key::T)) {
			Rotate(scene_target, -rotation_speed * dt);
		}
		if (input.KeyPressed(Key::F)) {
			SetScale(scene_target, GetScale(scene_target) + V2_float{ zoom_speed * dt });
		}
		if (input.KeyPressed(Key::G)) {
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