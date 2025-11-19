
#include <utility>

#include "ecs/components/interactive.h"
#include "ecs/components/movement.h"
#include "ecs/components/sprite.h"
#include "ecs/components/transform.h"
#include "ecs/entity.h"
#include "core/app/application.h"
#include "core/scripting/script.h"
#include "core/scripting/script_interfaces.h"
#include "core/app/window.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/geometry/circle.h"
#include "math/math_utils.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/render_data.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/vfx/light.h"
#include "world/scene/camera.h"
#include "world/scene/scene.h"
#include "world/scene/scene_input.h"
#include "world/scene/scene_manager.h"

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

	void Enter() override {
		Application::Get().render_.SetBackgroundColor(color::LightBlue);
		Application::Get().window_.SetResizable();
		Application::Get().render_.SetScalingMode(ScalingMode::Letterbox);

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

	void Update() override {
		MoveWASD(camera, { 3.0f, 3.0f });

		auto dt{ Application::Get().dt() };

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

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init(
		"ResolutionScene: WASD/QE/ZC: Move/Rotate/Scale scene camera, Arrows/RT/FG: "
		"Move/Rotate/Scale scene target",
		{ 1200, 800 }
	);
	Application::Get().scene_.Enter<ResolutionScene>("");
	return 0;
}