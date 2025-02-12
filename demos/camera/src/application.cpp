#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/render_target.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

class CameraUIScene : public Scene {
public:
	void Enter() override {
		game.texture.Load("ui_texture", "resources/ui.jpg");

		auto ui = CreateSprite(manager, "texture");
		ui.Add<Transform>();
		ui.Add<Origin>(Origin::TopLeft);
	}
};

class CameraExampleScene : public Scene {
public:
	const float pan_speed	   = 200.0f;
	const float rotation_speed = 1.0f;
	const float zoom_speed{ 0.4f };

	Camera camera1;
	Camera camera2;

	ecs::Entity rt;
	ecs::Entity ui;

	void Enter() override {
		game.texture.Load("texture", "resources/test1.jpg");

		Rect bounds{ {}, window_size, Origin::TopLeft };

		camera1.SetPosition({ 0, 0 });
		camera1.SetBounds(bounds);
		camera2.SetPosition({ 200, 200 });
		camera2.SetZoom(2.0f);
		camera2.SetBounds(bounds);
		cam = &camera1;

		auto texture = CreateSprite(manager, "texture");
		texture.Add<Transform>(game.window.GetCenter());

		auto b = manager.CreateEntity();
		b.Add<Rect>();
		b.Add<Transform>(bounds.position);
		b.Add<LineWidth>(3.0f);
		b.Add<Size>(bounds.size);
		b.Add<Origin>(bounds.origin);
		b.Add<Tint>(color::Red);
		b.Add<Visible>();

		game.scene.Enter<CameraUIScene>("ui_scene");

		game.texture.Load("ui_texture", "resources/ui.jpg");

		ui = CreateSprite(manager, "ui_texture");
		ui.Add<Transform>(V2_float{ window_size.x, 0 });
		ui.Add<Origin>(Origin::TopRight);
		ui.Get<Visible>() = false;

		rt = manager.CreateEntity();
		rt.Add<RenderTarget>(window_size);
		rt.Add<Transform>();
		rt.Add<Visible>();
	}

	Camera* cam{ nullptr };

	void Update() override {
		V2_float center{ game.window.GetCenter() };
		float dt{ game.dt() };

		if (game.input.KeyDown(Key::K_1)) {
			cam = &camera1;
		} else if (game.input.KeyDown(Key::K_2)) {
			cam = &camera2;
		}

		if (game.input.KeyPressed(Key::W)) {
			cam->Translate({ 0, -pan_speed * dt });
		}
		if (game.input.KeyPressed(Key::S)) {
			cam->Translate({ 0, pan_speed * dt });
		}
		if (game.input.KeyPressed(Key::A)) {
			cam->Translate({ -pan_speed * dt, 0 });
		}
		if (game.input.KeyPressed(Key::D)) {
			cam->Translate({ pan_speed * dt, 0 });
		}

		if (game.input.KeyPressed(Key::Z)) {
			cam->Yaw(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::X)) {
			cam->Yaw(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::C)) {
			cam->Pitch(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::V)) {
			cam->Pitch(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::B)) {
			cam->Roll(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::N)) {
			cam->Roll(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::E)) {
			cam->Zoom(zoom_speed * dt);
		}
		if (game.input.KeyPressed(Key::Q)) {
			cam->Zoom(-zoom_speed * dt);
		}

		if (game.input.KeyDown(Key::R)) {
			cam->SetPosition(center);
			cam->SetZoom(1.0f);
		}

		camera.primary = *cam;

		const auto& r{ rt.Get<RenderTarget>() };
		r.Bind();
		r.Clear();

		r.Draw(ui);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Camera: WASD move, Q/E zoom, R reset, 1/2 swap cameras", window_size, color::White);
	game.scene.Enter<CameraExampleScene>("camera_example_scene");
	return 0;
}