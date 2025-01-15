#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

class ParallaxExampleScene : public Scene {
public:
	V2_float bg_pos;
	V2_float planet_b_pos;
	V2_float planet_s_pos;
	V2_float stars_pos;

	Texture background{ "resources/background.png" };
	Texture planet_b{ "resources/planet_b.png" };
	Texture planet_s{ "resources/planet_s.png" };
	Texture stars{ "resources/stars.png" };

	// Window size
	V2_float size;

	V2_float star_cam;
	V2_float foreground_cam;

	float scale{ 3.0f };
	V2_float background_size;
	float bg_aspect_ratio{ 0.0f };

	void Init() override {
		bg_pos		 = game.window.GetCenter();
		planet_b_pos = game.window.GetCenter() - V2_float{ 200, 200 };
		planet_s_pos = game.window.GetCenter() + V2_float{ 200, 200 };
		stars_pos	 = game.window.GetCenter();

		size			= game.window.GetSize() * scale;
		background_size = background.GetSize();
		bg_aspect_ratio = background_size.x / background_size.y;
	}

	void Update() override {
		auto camera{ game.camera.GetPrimary() };

		float speed = 200.5f * game.dt();

		V2_float velocity;

		if (game.input.KeyPressed(Key::W)) {
			velocity.y = -speed;
		}
		if (game.input.KeyPressed(Key::S)) {
			velocity.y = +speed;
		}
		if (game.input.KeyPressed(Key::A)) {
			velocity.x = -speed;
		}
		if (game.input.KeyPressed(Key::D)) {
			velocity.x = +speed;
		}
		camera.Translate(velocity);

		if (game.input.KeyDown(Key::R)) {
			camera.SetPosition(game.window.GetCenter());
		}

		star_cam	   += velocity / 6.0f;
		foreground_cam += velocity / 2.0f;

		background.Draw({ bg_pos, { size.x * bg_aspect_ratio, size.y } });

		RenderTarget star_target{ color::Transparent };
		stars.Draw({ stars_pos, { size.x * bg_aspect_ratio, size.y } }, {}, star_target);
		star_target.GetCamera().GetPrimary().Translate(star_cam);
		star_target.Draw();

		RenderTarget foreground{ color::Transparent };
		planet_b.Draw({ planet_b_pos, planet_b.GetSize() * scale }, {}, foreground);
		planet_s.Draw({ planet_s_pos, planet_s.GetSize() * scale }, {}, foreground);
		foreground.GetCamera().GetPrimary().Translate(foreground_cam);
		foreground.Draw();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ParallaxExampleScene", resolution);
	game.scene.LoadActive<ParallaxExampleScene>("parallax_example");
	return 0;
}