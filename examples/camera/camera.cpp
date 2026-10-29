#include <chrono>
#include <string>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "runtime/animation/follow_config.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

class CameraScene : public Scene {
public:
	const float pan_speed{ 200.0f };
	const float rotation_speed{ 1.0f };
	const float zoom_speed{ 0.4f };

	Entity mouse;
	TargetFollowConfig follow_config;

	std::string content{ "The quick brown fox jumps over the lazy dog" };
	Color color{ color::White };
	float font_size{ 20.0f };
	V2_int center{ 0, 0 };

	void OnEnter() override {
		ctx().asset.Load("tree", "assets/jpg.jpg");

		mouse = CreateEntity();
		SetPosition(mouse, {});

		auto logical_size{ ctx().renderer.GetLogicalSize() };
		auto s1{ CreateSprite(*this, -logical_size * 0.5f + V2_float{ 100, 400 }, "tree") };
		auto s2{
			CreateSprite(*this, -logical_size * 0.5f + V2_float{ 700, 400 }, "tree")
		}; // AddPostFX(s2, grayscale);

		follow_config.move_mode = MoveMode::Lerp;
		follow_config.lerp		= { 0.5f, 0.5f };
		follow_config.deadzone	= { 300, 300 };
	}

	void OnUpdate() override {
		auto dt{ ctx().dt().count() };

		SetPosition(mouse, ctx().input.GetMousePosition());

		auto& camera{ ctx().camera };

		if (ctx().input.KeyHeld(Key::W)) {
			Translate(camera, { 0, -pan_speed * dt });
		}
		if (ctx().input.KeyHeld(Key::S)) {
			Translate(camera, { 0, pan_speed * dt });
		}
		if (ctx().input.KeyHeld(Key::A)) {
			Translate(camera, { -pan_speed * dt, 0 });
		}
		if (ctx().input.KeyHeld(Key::D)) {
			Translate(camera, { pan_speed * dt, 0 });
		}

		if (ctx().input.KeyHeld(Key::Z)) {
			Rotate(camera, rotation_speed * dt);
		}

		if (ctx().input.KeyHeld(Key::X)) {
			Rotate(camera, -rotation_speed * dt);
		}

		if (ctx().input.KeyHeld(Key::E)) {
			camera.Zoom(zoom_speed * dt);
		}
		if (ctx().input.KeyHeld(Key::Q)) {
			camera.Zoom(-zoom_speed * dt);
		}

		if (ctx().input.MousePressed(Mouse::Left)) {
			StopFollow(camera);
		} else if (ctx().input.MousePressed(Mouse::Right)) {
			StartFollow(camera, mouse, follow_config);
		}
	}
};

int main(int, char**) {
	Application app{ "Camera: WASD move, Q/E zoom" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<CameraScene>();
}