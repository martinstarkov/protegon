#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

class TemplateScene : public Scene {
public:
	Texture texture{ "resources/test1.jpg" };
	Texture ui_texture{ "resources/ui.jpg" };

	const float pan_speed	   = 200.0f;
	const float rotation_speed = 1.0f;
	const float zoom_speed{ 0.4f };

	void Init() override {
		auto camera2{ game.camera.GetPrimary() };
		camera2.SetPosition({ 0, 0 });

		auto camera{ game.camera.GetPrimary() };

		Rect bounds{ {}, resolution, Origin::TopLeft };

		camera.SetBounds(bounds);
	}

	void Update() override {
		V2_float center{ game.window.GetCenter() };
		float dt{ game.dt() };

		auto camera{ game.camera.GetPrimary() };

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
			camera.SetPosition({ center.x, center.y, 0.0f });
		}

		// camera.PrintInfo();

		texture.Draw({ center, texture.GetSize() });
		ui_texture.Draw({ { 0, 0 }, ui_texture.GetSize(), Origin::TopLeft }, {}, { 1 });
		ui_texture.Draw({ { 0, 0 }, 3 * ui_texture.GetSize(), Origin::Center }, {}, { 2 });

		auto camera{ game.camera.GetPrimary() };
		const auto& bounds{ camera.GetBounds() };
		bounds.Draw(color::Red, 3.0f);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TemplateTitle", resolution);
	game.scene.LoadActive<TemplateScene>("template_scene");
	return 0;
}