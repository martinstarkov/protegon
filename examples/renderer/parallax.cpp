#include <chrono>

#include "app/application.h"
#include "core/editor.h"
#include "core/input/key.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"

// TODO: Fix this demo.

using namespace ptgn;

class ParallaxExampleScene : public Scene {
public:
	V2_float bg_pos;
	V2_float planet_b_pos;
	V2_float planet_s_pos;
	V2_float stars_pos;

	V2_float star_cam;
	V2_float background_cam;
	V2_float foreground_cam;

	float scale{ 3.0f };
	V2_float background_size;
	float bg_aspect_ratio{ 0.0f };

	void OnEnter() override {
		ctx().asset.LoadMany(
			{ { "background", "assets/background.png" },
			  { "planet_b", "assets/planet_b.png" },
			  { "planet_s", "assets/planet_s.png" },
			  { "stars", "assets/stars.png" } }
		);

		bg_pos		 = {};
		planet_b_pos = { -200, -200 };
		planet_s_pos = { 200, 200 };
		stars_pos	 = {};

		background_size = ctx().asset.GetTextureSize("background");
		bg_aspect_ratio = background_size.x / background_size.y;

		ResetPositions();
	}

	void ResetPositions() {
		background_cam = {};
		star_cam	   = {};
		foreground_cam = {};
	}

	void OnUpdate() override {
		float dt{ ctx().dt().count() };
		float speed = 10.0f * dt;

		V2_float velocity;

		if (ctx().input.KeyHeld(Key::W)) {
			velocity.y = -speed;
		}
		if (ctx().input.KeyHeld(Key::S)) {
			velocity.y = +speed;
		}
		if (ctx().input.KeyHeld(Key::A)) {
			velocity.x = -speed;
		}
		if (ctx().input.KeyHeld(Key::D)) {
			velocity.x = +speed;
		}

		if (ctx().input.KeyPressed(Key::R)) {
			ResetPositions();
		}

		background_cam += velocity / 10.0f;
		star_cam	   += velocity / 6.0f;
		foreground_cam += velocity / 2.0f;

		// TODO: Fix by implementing SetScrollFactor().

		auto ws{ ctx().renderer.GetLogicalSize() * scale };

		ctx().render_queue.DrawTexture(
			bg_pos, "background", { .size = V2_float{ ws.x * bg_aspect_ratio, ws.y } }
		);
		Translate(ctx().camera, background_cam);
		ctx().render_queue.DrawTexture(
			stars_pos, "stars", { .size = V2_float{ ws.x * bg_aspect_ratio, ws.y } }
		);
		Translate(ctx().camera, star_cam);
		ctx().render_queue.DrawTexture(
			planet_b_pos, "planet_b", { .size = ctx().asset.GetTextureSize("planet_b") * scale }
		);
		ctx().render_queue.DrawTexture(
			planet_s_pos, "planet_s", { .size = ctx().asset.GetTextureSize("planet_s") * scale }
		);
		Translate(ctx().camera, foreground_cam);
	}
};

int main(int, char**) {
	Application app{ "ParallaxExampleScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ParallaxExampleScene>();
}