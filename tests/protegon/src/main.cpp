#define SDL_MAIN_HANDLED

#include "../tests/test_ecs.h"
#include "test_animation.h"
#include "test_camera.h"
#include "test_collision.h"
#include "test_events.h"
#include "test_lighting.h"
#include "test_math.h"
#include "test_matrix4.h"
#include "test_particles.h"
#include "test_pathfinding.h"
#include "test_platforming.h"
#include "test_renderer.h"
#include "test_rng.h"
#include "test_scenes.h"
#include "test_text.h"
#include "test_tween.h"
#include "test_ui.h"
#include "test_vector.h"
#include "test_window.h"

using namespace ptgn;

class Tests : public Scene {
public:
	Tests() = default;

	void Preload() final {
		PTGN_INFO("Preloaded test scene");
	}

	void Init() final {
		game.draw.SetClearColor(color::White);
		V2_int window_size{ 800, 800 };
		game.window.SetSize(window_size);

		// Non-visual tests.

		TestECS();
		TestMatrix4();
		TestMath();
		TestVector2();

		// Visual tests.

		TestCollisions();
		TestRenderer();
		TestParticles();
		TestUI();
		TestPlatforming();
		TestScenes();
		TestLighting();
		TestCamera();
		TestRNG();
		TestAnimations();
		TestPathfinding();
		TestTween();
		TestText();
		TestWindow();
		TestEvents();

		PTGN_INFO("Initialized test scene");
	}

	void Update() final {}

	void Shutdown() final {
		PTGN_INFO("Shutdown test scene");
	}

	void Unload() final {
		PTGN_INFO("Unloaded test scene");
	}
};

int main() {
	game.Start<Tests>();
	return 0;
}