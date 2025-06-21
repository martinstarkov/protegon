
#include "core/game.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/graphics/circle.h"
#include "rendering/graphics/rect.h"
#include "rendering/graphics/vfx/light.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct SandboxScene : public Scene {
	static constexpr int X = 100;									// Number of random quads
	static constexpr int Y = 100;									// Number of random circles
	static constexpr int Z = 10;									// Number of random lights

	RNG<float> pos_rngx{ 0.0f, static_cast<float>(window_size.x) }; // Position range
	RNG<float> pos_rngy{ 0.0f, static_cast<float>(window_size.y) }; // Position
	RNG<float> size_rng{ 10.0f, 70.0f };							// Size range
	RNG<float> light_radius_rng{ 10.0f, 200.0f };					// Light radius range
	RNG<float> intensity_rng{ 0.0f, 10.0f };						// Intensity range

	void Enter() override {
		for (int i = 0; i < X; ++i) {
			CreateRect(
				*this, { pos_rngx(), pos_rngy() }, { size_rng(), size_rng() },
				Color::RandomTransparent(), -1.0f
			);
		}

		for (int i = 0; i < Y; ++i) {
			CreateCircle(
				*this, { pos_rngx(), pos_rngy() }, size_rng(), Color::RandomTransparent(), -1.0f
			);
		}

		for (int i = 0; i < Z; ++i) {
			CreatePointLight(
				*this, { pos_rngx(), pos_rngy() }, light_radius_rng(), color::Blue, intensity_rng(),
				2.0f
			);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SandboxScene", window_size);
	game.scene.Enter<SandboxScene>("");
	return 0;
}