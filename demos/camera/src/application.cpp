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
#include "renderer/render_data.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

class CameraExampleScene : public Scene {
public:
	const float pan_speed	   = 200.0f;
	const float rotation_speed = 1.0f;
	const float zoom_speed{ 0.4f };

	Camera camera1;
	Camera camera2;

	ecs::Entity mouse;

	void Enter() override {
		game.texture.Load("texture", "resources/test1.jpg");
		game.texture.Load("ui_texture", "resources/ui.jpg");

		mouse = manager.CreateEntity();
		mouse.Add<Point>();
		mouse.Add<Transform>();
		mouse.Add<Tint>(color::Red);
		mouse.Add<Visible>();

		Rect bounds{ {}, window_size, Origin::TopLeft };

		camera1.SetPosition({ 0, 0 });
		camera1.SetBounds(bounds);
		camera2.SetPosition({ 200, 200 });
		camera2.SetZoom(2.0f);
		camera2.SetBounds(bounds);
		cam = &camera1;

		auto texture = manager.CreateEntity();
		texture.Add<Transform>(game.window.GetCenter());
		texture.Add<Sprite>("texture");
		texture.Add<Visible>();

		auto b = manager.CreateEntity();
		b.Add<Rect>();
		b.Add<Transform>(bounds.position);
		b.Add<LineWidth>(3.0f);
		b.Add<Size>(bounds.size);
		b.Add<Origin>(bounds.origin);
		b.Add<Tint>(color::Red);
		b.Add<Visible>();

		// TODO: Move to own scene.
		/*RenderTarget ui{ color::Transparent };

		game.renderer.SetRenderTarget(ui);

		ui_texture.Draw({ { 0, 0 }, ui_texture.GetSize(), Origin::TopLeft }, {});
		game.input.GetMousePosition().Draw(color::Blue, 4.0f);

		game.renderer.SetRenderTarget({});

		ui.Draw();*/
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

		mouse.Get<Transform>() = game.input.GetMousePosition();

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
		PTGN_LOG(*cam);
		camera.primary = *cam;
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Camera: WASD move, Q/E zoom, R reset, 1/2 swap cameras", window_size, color::White);
	game.scene.Enter<CameraExampleScene>("camera_example_scene");
	return 0;
}