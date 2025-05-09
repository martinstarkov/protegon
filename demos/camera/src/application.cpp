#include "components/draw.h"
#include "components/input.h"
#include "core/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/key.h"

#include "math/geometry.h"
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
constexpr V2_int deadzone_size{ 150, 150 };

class CameraUIScene : public Scene {
public:
	void Enter() override {
		game.texture.Load("ui_texture2", "resources/ui2.jpg");

		auto ui = CreateSprite(manager, "ui_texture2");
		ui.Add<Transform>();
		ui.Add<Origin>(Origin::TopLeft);

		auto camera_center = manager.CreateEntity();
		camera_center.Add<Circle>(3.0f);
		camera_center.Add<Transform>(game.window.GetCenter());
		camera_center.Add<Tint>(color::Black);
		camera_center.Add<Visible>();

		auto deadzone = manager.CreateEntity();
		deadzone.Add<Rect>(deadzone_size, Origin::Center);
		deadzone.Add<Transform>(game.window.GetCenter());
		deadzone.Add<LineWidth>(2.0f);
		deadzone.Add<Origin>();
		deadzone.Add<Tint>(color::DarkGreen);
		deadzone.Add<Visible>();

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

		camera.primary.SetPosition(game.window.GetCenter());
		// camera.primary.SetBounds({}, window_size);

		auto texture = CreateSprite(manager, "texture");
		texture.Add<Transform>(game.window.GetCenter());
		texture.Add<Interactive>();
		texture.Add<callback::KeyDown>([](auto key) {
			if (key == Key::W) {
				PTGN_LOG("Key down W");
			}
		});
		texture.Add<callback::KeyPressed>([](auto key) {
			if (key == Key::W) {
				PTGN_LOG("Key pressed W");
			}
		});
		texture.Add<callback::KeyUp>([](auto key) {
			if (key == Key::W) {
				PTGN_LOG("Key up W");
			}
		});
		texture.Add<callback::MouseMove>([](auto mouse) { PTGN_LOG("Mouse move: ", mouse); });
		texture.Add<callback::MouseDown>([](auto mouse) { PTGN_LOG("Mouse down: ", mouse); });
		texture.Add<callback::MouseUp>([](auto mouse) { PTGN_LOG("Mouse up: ", mouse); });
		texture.Add<callback::MousePressed>([](auto mouse) { PTGN_LOG("Mouse pressed: ", mouse); });
		texture.Add<callback::MouseScroll>([](auto scroll) { PTGN_LOG("Mouse scroll: ", scroll); });

		auto b = manager.CreateEntity();
		b.Add<Rect>(window_size, Origin::TopLeft);
		b.Add<Transform>(V2_float{});
		b.Add<LineWidth>(3.0f);
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
		mouse.Add<Circle>(20.0f);
		mouse.Add<Tint>(color::Red);
		mouse.Add<Visible>();

		camera.primary.PanTo({ 0, 0 }, seconds{ 3 });
		camera.primary.PanTo({ 800, 0 }, seconds{ 3 });
		camera.primary.PanTo({ 800, 800 }, seconds{ 3 });
		camera.primary.PanTo({ 0, 800 }, seconds{ 3 });
		camera.primary.StartFollow(mouse);
		camera.primary.SetLerp(V2_float{ 0.9f });
		// camera.primary.SetOffset(V2_float{ -75, -75 });
		camera.primary.SetDeadzone(deadzone_size);

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