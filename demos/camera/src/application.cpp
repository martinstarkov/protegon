#include "components/sprite.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/time.h"
#include "events/input_handler.h"
#include "events/key.h"
#include "events/mouse.h"
#include "math/easing.h"
#include "math/math.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tweening/follow_config.h"
#include "tweening/tween_effects.h"

using namespace ptgn;

/*
class CameraUIScene : public Scene {
constexpr V2_int deadzone_size{ 150, 150 };
public:
	void Enter() override {
		game.texture.Load("ui_texture2", "resources/ui2.jpg");

		auto ui = CreateSprite(*this, "ui_texture2");
		ui.SetPosition({});
		ui.SetOrigin(Origin::TopLeft);

		auto camera_center = manager.CreateEntity();
		camera_center.Add<Circle>(3.0f);
		camera_center.SetPosition(game.window.GetCenter());
		camera_center.SetTint(color::Black);
		camera_center.Show();

		auto deadzone = manager.CreateEntity();
		deadzone.Add<Rect>(deadzone_size, Origin::Center);
		deadzone.SetPosition(game.window.GetCenter());
		deadzone.Add<LineWidth>(2.0f);
		deadzone.SetOrigin;
		deadzone.SetTint(color::DarkGreen);
		deadzone.Show();

		camera.primary.FadeFrom(color::Black, seconds{ 3 });
		camera.primary.FadeTo(color::Red, seconds{ 3 });
		camera.primary.FadeFrom(color::Red, seconds{ 3 });
	}
};
*/

/*
class CameraExampleScene : public Scene {
public:
	const float pan_speed	   = 200.0f;
	const float rotation_speed = 1.0f;
	const float zoom_speed{ 0.4f };

	Entity rt;
	Entity ui;
	Entity mouse;

	CameraExampleScene() {
		game.scene.Load<CameraUIScene>("ui_scene");
	}

	void Enter() override {
		game.texture.Load("texture", "resources/test1.jpg");

		camera.primary.SetPosition(game.window.GetCenter());
		// camera.primary.SetBounds({}, window_size);

		auto texture = CreateSprite(*this, "texture");
		texture.SetPosition(game.window.GetCenter());
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
		b.SetPosition({});
		b.Add<LineWidth>(3.0f);
		b.SetTint(color::Red);
		b.Show();

		game.scene.Enter("ui_scene");

		game.texture.Load("ui_texture", "resources/ui.jpg");

		ui = CreateSprite(*this, "ui_texture");
		ui.SetPosition(V2_float{ window_size.x, 0 });
		ui.SetOrigin(Origin::TopRight);
		ui.Hide();

		rt = manager.CreateEntity();
		rt.Add<RenderTarget>(manager, window_size);
		rt.SetPosition({});
		rt.Show();

		mouse = manager.CreateEntity();
		mouse.SetPosition({});
		mouse.Add<Circle>(20.0f);
		mouse.SetTint(color::Red);
		mouse.Show();

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
			mouse.SetPosition( =
				camera.primary.TransformToCamera(game.input.GetMousePosition());
			//camera.primary.PanTo(camera.primary.TransformToCamera(game.input.GetMousePosition()),
seconds{ 4 },SymmetricalEase::InOutSine, false); } else if (game.input.MouseDown(Mouse::Right)) {
			camera.primary.StopFollow();
		}

		const auto& r{ rt.Get<RenderTarget>() };
		r.Bind();
		r.Clear();

		r.Draw(ui);
	}
};
*/

class CameraScene : public Scene {
public:
	const float pan_speed{ 200.0f };
	const float rotation_speed{ 1.0f };
	const float zoom_speed{ 0.4f };

	Entity mouse;
	FollowConfig follow_config;

	void Enter() override {
		LoadResource("tree", "resources/test1.jpg");

		mouse = CreateEntity();
		mouse.SetPosition({});

		CreateSprite(*this, "tree").SetPosition({ 200, 400 });
		CreateSprite(*this, "tree").SetPosition({ 600, 400 });

		follow_config.move_mode	  = MoveMode::Lerp;
		follow_config.lerp_factor = { 0.5f, 0.5f };
		follow_config.deadzone	  = { 300, 300 };

		// camera.primary.Shake(0.5f, seconds{ 5 });
		// camera.primary.RotateTo(DegToRad(360.0f), seconds{ 5 });
		//  camera.primary.Shake(1, seconds{ 5 }, {}, SymmetricalEase::Linear, false);
		//  camera.primary.Shake(0, seconds{ 5 }, {}, SymmetricalEase::Linear, false);
		// camera.primary.FadeTo(color::Black, seconds{ 5 });
		camera.primary.StartFollow(mouse, follow_config);
	}

	void Update() override {
		float dt{ game.dt() };

		mouse.SetPosition(input.GetMousePosition());

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
			camera.primary.Rotate(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::X)) {
			camera.primary.Rotate(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::E)) {
			camera.primary.Zoom(zoom_speed * dt);
		}
		if (game.input.KeyPressed(Key::Q)) {
			camera.primary.Zoom(-zoom_speed * dt);
		}

		if (game.input.MouseDown(Mouse::Left)) {
			camera.primary.StopFollow();
		} else if (game.input.MouseDown(Mouse::Right)) {
			camera.primary.StartFollow(mouse, follow_config);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Camera: WASD move, Q/E zoom");
	game.scene.Enter<CameraScene>("");
	return 0;
}