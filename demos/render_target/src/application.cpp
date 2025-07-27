
#include "core/game.h"
#include "events/input_handler.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct RenderTargetScene : public Scene {
	void Enter() override {}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("RenderTargetScene", window_size);
	game.scene.Enter<RenderTargetScene>("");
	return 0;
}