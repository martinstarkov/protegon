#define SDL_MAIN_HANDLED

#include "../tests/test_ecs.h"
#include "core/window.h"
#include "protegon/color.h"
#include "protegon/game.h"
#include "protegon/scene.h"
#include "protegon/vector2.h"
#include "renderer/renderer.h"
#include "test_camera.h"
#include "test_events.h"
#include "test_math.h"
#include "test_matrix4.h"
#include "test_renderer.h"
#include "test_rng.h"
#include "test_text.h"
#include "test_tween.h"
#include "test_vector.h"

using namespace ptgn;

class Tests : public Scene {
public:
	Tests() = default;

	void Init() final {
		game.renderer.SetClearColor(color::White);
		game.window.SetSize(V2_int{ 800, 800 });
		game.window.Show();

		// Non-visual tests.

		TestMatrix4();
		TestECS();
		TestMath();
		TestVector2();

		// Visual tests.

		TestTween();
		TestCamera();
		TestRNG();
		TestRenderer();
		TestText();
		TestEvents();

		game.window.SetTitle("Update Loop");
	}

	void Update() final {
		game.Stop();
	}
};

int main() {
	game.Start<Tests>();
	return 0;
}