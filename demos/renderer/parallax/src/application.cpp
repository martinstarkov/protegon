#include "core/app/application.h"
#include "core/app/window.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/vector2.h"
#include "renderer/renderer.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

// TODO: Fix this demo.

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
		LoadResource({ { "background", "resources/background.png" },
					   { "planet_b", "resources/planet_b.png" },
					   { "planet_s", "resources/planet_s.png" },
					   { "stars", "resources/stars.png" } });

		bg_pos		 = game.renderer.GetGameSize() * 0.5f;
		planet_b_pos = game.renderer.GetGameSize() * 0.5f - V2_float{ 200, 200 };
		planet_s_pos = game.renderer.GetGameSize() * 0.5f + V2_float{ 200, 200 };
		stars_pos	 = game.renderer.GetGameSize() * 0.5f;

		size			= game.renderer.GetGameSize() * scale;
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
		float speed = 10.0f * game.dt();

		V2_float velocity;

		if (input.KeyPressed(Key::W)) {
			velocity.y = -speed;
		}
		if (input.KeyPressed(Key::S)) {
			velocity.y = +speed;
		}
		if (input.KeyPressed(Key::A)) {
			velocity.x = -speed;
		}
		if (input.KeyPressed(Key::D)) {
			velocity.x = +speed;
		}

		if (input.KeyDown(Key::R)) {
			ResetPositions();
		}

		background_cam += velocity / 10.0f;
		star_cam	   += velocity / 6.0f;
		foreground_cam += velocity / 2.0f;

		// TODO: Fix by implementing SetScrollFactor().

		game.renderer.DrawTexture(
			"background", bg_pos, V2_int{ size.x * bg_aspect_ratio, size.y }, Origin::Center
		);
		Translate(camera, background_cam);
		game.renderer.DrawTexture(
			"stars", stars_pos, V2_int{ size.x * bg_aspect_ratio, size.y }, Origin::Center
		);
		Translate(camera, star_cam);
		game.renderer.DrawTexture(
			"planet_b", planet_b_pos, game.texture.GetSize("planet_b") * scale, Origin::Center
		);
		game.renderer.DrawTexture(
			"planet_s", planet_s_pos, game.texture.GetSize("planet_s") * scale, Origin::Center
		);
		Translate(camera, foreground_cam);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ParallaxExampleScene", window_size);
	game.scene.Enter<ParallaxExampleScene>("");
	return 0;
}