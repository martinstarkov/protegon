#include "core/game.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

struct ScriptScene : public Scene {
	void Enter() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ScriptScene");
	game.scene.Enter<ScriptScene>("");
	return 0;
}