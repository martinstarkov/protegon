
#include <utility>

#include "components/interactive.h"
#include "components/movement.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "core/window.h"
#include "input/key.h"
#include "math/geometry/circle.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/render_data.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_input.h"
#include "scene/scene_manager.h"

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
		SetBackgroundColor(color::LightBlue);
		game.window.SetSetting(WindowSetting::Resizable);
		game.renderer.SetLogicalResolutionMode(LogicalResolutionMode::Letterbox);

		RenderTarget rt{ GetRenderTarget() };

		// SetRotation(rt, DegToRad(45.0f));
		// SetScale(rt, 0.5f);
		// SetPosition(rt, V2_float{ 600, 0 });

		SetBackgroundColor(color::LightGray);

		camera.CenterOnViewport({ 600, 300 });

		input.SetDrawInteractives(true);
		input.SetDrawInteractivesLineWidth(10.0f);

		V2_float camera_center{ GetTransform(camera).GetPosition() };

		CreateRect(*this, camera_center - V2_float{ 100, 0 }, { 50, 50 }, color::Green);

		float radius{ 40.0f };
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

		auto dt{ game.dt() };

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
	game.Init(
		"ResolutionScene: WASD/QE/ZC: Move/Rotate/Scale scene camera, Arrows/RT/FG: "
		"Move/Rotate/Scale scene target",
		{ 1200, 800 }
	);
	game.scene.Enter<ResolutionScene>("");
	return 0;
}