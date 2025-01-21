#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

class CameraExampleScene : public Scene {
public:
	Texture texture{ "resources/test1.jpg" };
	Texture ui_texture{ "resources/ui.jpg" };

	const float pan_speed	   = 200.0f;
	const float rotation_speed = 1.0f;
	const float zoom_speed{ 0.4f };

	void Enter() override {
		Rect bounds{ {}, window_size, Origin::TopLeft };

		auto& camera{ game.camera.Load("cam1") };
		auto& camera2{ game.camera.Load("cam2") };
		camera.SetPosition({ 0, 0 });
		camera.SetBounds(bounds);
		camera2.SetPosition({ 200, 200 });
		camera2.SetZoom(2.0f);
		camera2.SetBounds(bounds);
		chosen_cam = "cam1";
	}

	std::string_view chosen_cam;

	void Update() override {
		V2_float center{ game.window.GetCenter() };
		float dt{ game.dt() };

		if (game.input.KeyDown(Key::K_1)) {
			chosen_cam = "cam1";
		} else if (game.input.KeyDown(Key::K_2)) {
			chosen_cam = "cam2";
		}

		game.input.GetMousePosition().Draw(color::Red, 8.0f);

		auto& camera{ game.camera.Get(chosen_cam) };

		if (game.input.KeyPressed(Key::W)) {
			camera.Translate({ 0, -pan_speed * dt });
		}
		if (game.input.KeyPressed(Key::S)) {
			camera.Translate({ 0, pan_speed * dt });
		}
		if (game.input.KeyPressed(Key::A)) {
			camera.Translate({ -pan_speed * dt, 0 });
		}
		if (game.input.KeyPressed(Key::D)) {
			camera.Translate({ pan_speed * dt, 0 });
		}

		if (game.input.KeyPressed(Key::Z)) {
			camera.Yaw(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::X)) {
			camera.Yaw(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::C)) {
			camera.Pitch(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::V)) {
			camera.Pitch(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::B)) {
			camera.Roll(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::N)) {
			camera.Roll(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::E)) {
			camera.Zoom(zoom_speed * dt);
		}
		if (game.input.KeyPressed(Key::Q)) {
			camera.Zoom(-zoom_speed * dt);
		}

		if (game.input.KeyDown(Key::R)) {
			camera.SetPosition(center);
			camera.SetZoom(1.0f);
		}

		game.camera.SetPrimary(chosen_cam);

		texture.Draw({ center, texture.GetSize() });

		camera.GetBounds().Draw(color::Red, 3.0f);

		game.renderer.Flush();

		RenderTarget ui{ color::Transparent };
		game.renderer.SetTemporaryRenderTarget(ui, [&]() {
			game.camera.SetPrimary({});
			ui_texture.Draw({ { 0, 0 }, ui_texture.GetSize(), Origin::TopLeft }, {});
			game.input.GetMousePosition().Draw(color::Blue, 4.0f);
		});
		ui.Draw();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Camera: WASD move, Q/E zoom, R reset, 1/2 swap cameras", window_size);
	game.scene.Enter<CameraExampleScene>("camera_example_scene");
	return 0;
}