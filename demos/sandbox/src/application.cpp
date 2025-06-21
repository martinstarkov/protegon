
#include "core/game.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct SandboxScene : public Scene {
	void Enter() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SandboxScene", window_size);
	game.scene.Enter<SandboxScene>("");
	return 0;
}