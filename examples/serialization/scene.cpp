#include "world/scene/scene.h"

#include "core/app/game.h"
#include "world/scene/scene_manager.h"

using namespace ptgn;

// TODO: Add resource manager serialization and deserialization.

class DeserializedScene : public Scene {
public:
	void OnEnter() override {
		LoadResource("anim", "resources/animation.png");
		LoadResource("test", "resources/test1.jpg");
		json j = LoadJson("resources/animation_scene.json");
		j	   = LoadJson("resources/light_scene.json");
		j.at("manager").get_to(*this);
		// TODO: Figure this out: j.get_to(*this);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application app{ "DeserializedScene" };
	app.StartWith<DeserializedScene>();
}
