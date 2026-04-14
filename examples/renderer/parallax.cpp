#include "app/application.h"
#include "core/math/vector2.h"

#include "core/input/key.h"
#include "platform/window.h"
#include "renderer/renderer.h"
#include "runtime/scene/scene.h"

#include "runtime/scene/scene_manager.h"

// TODO: Fix this demo.

using namespace ptgn;

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

	void OnEnter() override {
		ctx().asset.LoadMany({ { "background", "assets/background.png" },
							   { "planet_b", "assets/planet_b.png" },
							   { "planet_s", "assets/planet_s.png" },
							   { "stars", "assets/stars.png" } });

		bg_pos		 = ctx().renderer.GetGameSize() * 0.5f;
		planet_b_pos = ctx().renderer.GetGameSize() * 0.5f - V2_float{ 200, 200 };
		planet_s_pos = ctx().renderer.GetGameSize() * 0.5f + V2_float{ 200, 200 };
		stars_pos	 = ctx().renderer.GetGameSize() * 0.5f;

		size			= ctx().renderer.GetGameSize() * scale;
		background_size = ctx().asset.GetTexture("background")->GetSize();
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

		ctx().renderer.DrawTexture(
			"background", bg_pos, V2_int{ size.x * bg_aspect_ratio, size.y }, Origin::Center
		);
		Translate(ctx().camera, background_cam);
		ctx().renderer.DrawTexture(
			"stars", stars_pos, V2_int{ size.x * bg_aspect_ratio, size.y }, Origin::Center
		);
		Translate(ctx().camera, star_cam);
		ctx().renderer.DrawTexture(
			"planet_b", planet_b_pos, ctx().asset.GetTexture("planet_b")->GetSize() * scale,
			Origin::Center
		);
		ctx().renderer.DrawTexture(
			"planet_s", planet_s_pos, ctx().asset.GetTexture("planet_s")->GetSize() * scale,
			Origin::Center
		);
		Translate(ctx().camera, foreground_cam);
	}
};

int main(int, char**) {
	Application app{ "ParallaxExampleScene" };
	app.StartWith<ParallaxExampleScene>();
}