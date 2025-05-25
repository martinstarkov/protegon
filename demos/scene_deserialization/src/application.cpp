#include "core/game.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class DeserializedScene : public Scene {
public:
	void Enter() override {
		json j = LoadJson("resources/animation_scene.json");
		j.get_to(*this);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("DeserializedScene");
	game.scene.Enter<DeserializedScene>("");
	return 0;
}
