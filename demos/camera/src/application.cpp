#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/geometry/circle.h"
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
		game.texture.Load("ui_texture2", "resources/ui2.jpg");

		auto ui = CreateSprite(manager, "ui_texture2");
		ui.Add<Transform>();
		ui.Add<Origin>(Origin::TopLeft);

		camera.primary.FadeFrom(color::Black, seconds{ 3 });
		camera.primary.FadeTo(color::Red, seconds{ 3 });
		camera.primary.FadeFrom(color::Red, seconds{ 3 });
	}
};

class CameraExampleScene : public Scene {
public:
	const float pan_speed	   = 200.0f;
	const float rotation_speed = 1.0f;
	const float zoom_speed{ 0.4f };

	ecs::Entity rt;
	ecs::Entity ui;
	ecs::Entity mouse;

	CameraExampleScene() {
		game.scene.Load<CameraUIScene>("ui_scene");
	}

	void Enter() override {
		game.texture.Load("texture", "resources/test1.jpg");

		Rect bounds{ {}, window_size, Origin::TopLeft };

		camera.primary.SetPosition(game.window.GetCenter());
		// camera.primary.SetBounds(bounds);

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

		game.scene.Enter("ui_scene");

		game.texture.Load("ui_texture", "resources/ui.jpg");

		ui = CreateSprite(manager, "ui_texture");
		ui.Add<Transform>(V2_float{ window_size.x, 0 });
		ui.Add<Origin>(Origin::TopRight);
		ui.Get<Visible>() = false;

		rt = manager.CreateEntity();
		rt.Add<RenderTarget>(manager, window_size);
		rt.Add<Transform>();
		rt.Add<Visible>();

		mouse = manager.CreateEntity();
		mouse.Add<Transform>();
		mouse.Add<Circle>();
		mouse.Add<Radius>(20.0f);
		mouse.Add<Tint>(color::Red);
		mouse.Add<Visible>();

		camera.primary.PanTo({ 0, 0 }, seconds{ 3 });
		camera.primary.PanTo({ 800, 0 }, seconds{ 3 });
		camera.primary.PanTo({ 800, 800 }, seconds{ 3 });
		camera.primary.PanTo({ 0, 800 }, seconds{ 3 });
		camera.primary.StartFollow(mouse);

		camera.primary.ZoomTo(0.5f, seconds{ 3 });
		camera.primary.ZoomTo(2.0f, seconds{ 3 });
		camera.primary.ZoomTo(0.25f, seconds{ 3 });
		camera.primary.ZoomTo(1.0f, seconds{ 3 });

		camera.primary.RotateTo(DegToRad(90.0f), seconds{ 3 });
		camera.primary.RotateTo(DegToRad(0.0f), seconds{ 3 });
		camera.primary.RotateTo(DegToRad(-90.0f), seconds{ 3 });
		camera.primary.RotateTo(DegToRad(0.0f), seconds{ 3 });
	}

	void Update() override {
		V2_float center{ game.window.GetCenter() };
		float dt{ game.dt() };

		if (game.input.KeyPressed(Key::W)) {
			camera.primary.Translate({ 0, -pan_speed * dt });
		}
		if (game.input.KeyPressed(Key::S)) {
			camera.primary.Translate({ 0, pan_speed * dt });
		}
		if (game.input.KeyPressed(Key::A)) {
			camera.primary.Translate({ -pan_speed * dt, 0 });
		}
		if (game.input.KeyPressed(Key::D)) {
			camera.primary.Translate({ pan_speed * dt, 0 });
		}

		if (game.input.KeyPressed(Key::Z)) {
			camera.primary.Yaw(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::X)) {
			camera.primary.Yaw(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::C)) {
			camera.primary.Pitch(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::V)) {
			camera.primary.Pitch(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::B)) {
			camera.primary.Roll(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::N)) {
			camera.primary.Roll(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::E)) {
			camera.primary.Zoom(zoom_speed * dt);
		}
		if (game.input.KeyPressed(Key::Q)) {
			camera.primary.Zoom(-zoom_speed * dt);
		}

		if (game.input.KeyDown(Key::R)) {
			camera.primary.SetPosition(center);
			camera.primary.SetZoom(1.0f);
		}

		if (game.input.MouseDown(Mouse::Left)) {
			mouse.Get<Transform>().position =
				camera.primary.TransformToCamera(game.input.GetMousePosition());
			/*camera.primary.PanTo(
				camera.primary.TransformToCamera(game.input.GetMousePosition()), seconds{ 4 },
				TweenEase::InOutSine, false
			);*/
		} else if (game.input.MouseDown(Mouse::Right)) {
			camera.primary.StopFollow();
		}

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