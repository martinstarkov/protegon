#include "core/game.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

struct GraphicsObjectScene : public Scene {
	void Enter() override {
		// TODO: Implement.
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("GraphicsObjectScene");
	game.scene.Enter<GraphicsObjectScene>("");
	return 0;
}