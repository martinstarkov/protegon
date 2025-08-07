#include "core/game.h"
#include "core/window.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "math/vector2.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

class ParallaxExampleScene : public Scene {
public:
	V2_float bg_pos;
	V2_float planet_b_pos;
	V2_float planet_s_pos;
	V2_float stars_pos;

	// Window size
	V2_float size;

	V2_float star_cam;
	V2_float background_cam;
	V2_float foreground_cam;

	float scale{ 3.0f };
	V2_float background_size;
	float bg_aspect_ratio{ 0.0f };

	void Enter() override {
		LoadResources({ { "background", "resources/background.png" },
						{ "planet_b", "resources/planet_b.png" },
						{ "planet_s", "resources/planet_s.png" },
						{ "stars", "resources/stars.png" } });

		bg_pos		 = game.window.GetCenter();
		planet_b_pos = game.window.GetCenter() - V2_float{ 200, 200 };
		planet_s_pos = game.window.GetCenter() + V2_float{ 200, 200 };
		stars_pos	 = game.window.GetCenter();

		size			= game.window.GetSize() * scale;
		background_size = game.texture.GetSize("background");
		bg_aspect_ratio = background_size.x / background_size.y;

		ResetPositions();
	}

	void ResetPositions() {
		background_cam = {};
		star_cam	   = {};
		foreground_cam = {};
	}

	void Update() override {
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

		if (game.input.KeyDown(Key::R)) {
			ResetPositions();
		}

		background_cam += velocity / 10.0f;
		star_cam	   += velocity / 6.0f;
		foreground_cam += velocity / 2.0f;

		// TODO: Fix.
		DrawDebugTexture("background", bg_pos, { size.x * bg_aspect_ratio, size.y });
		camera.primary.Translate(background_cam);
		DrawDebugTexture("stars", stars_pos, { size.x * bg_aspect_ratio, size.y });
		camera.primary.Translate(star_cam);
		DrawDebugTexture("planet_b", planet_b_pos, game.texture.GetSize("planet_b") * scale);
		DrawDebugTexture("planet_s", planet_s_pos, game.texture.GetSize("planet_s") * scale);
		camera.primary.Translate(foreground_cam);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ParallaxExampleScene", window_size);
	game.scene.Enter<ParallaxExampleScene>("");
	return 0;
}