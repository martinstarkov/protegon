#include "runtime/scene/scene.h"

#include "app/application.h"

#include "runtime/scene/scene_manager.h"

using namespace ptgn;

// TODO: Add resource manager serialization and deserialization.

class DeserializedScene : public Scene {
public:
	void OnEnter() override {
		ctx().asset.Load("anim", "assets/animation.png");
		ctx().asset.Load("test", "assets/test1.jpg");
		json j = LoadJson("assets/animation_scene.json");
		j	   = LoadJson("assets/light_scene.json");
		j.at("manager").get_to(*this);
		// TODO: Figure this out: j.get_to(*this);
	}
};

int main(int, char**) {
	Application app{ "DeserializedScene" };
	app.StartWith<DeserializedScene>();
}
