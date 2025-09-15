#include "core/game.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

void LoadScene(const path& scene_file) {
	json j{ LoadJson(scene_file) };
	PTGN_LOG(j.dump(4));
}

// --------------------------------------------

class SceneTemplateExample : public Scene {
public:
	SceneTemplateExample() {
		LoadResource({ { "bg1", "resources/bg1.png" },
					   { "bg2", "resources/bg2.png" },
					   { "bg3", "resources/bg3.png" } });

		LoadScene("resources/scenes.json");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SceneTemplateExample", resolution);
	game.scene.Enter<SceneTemplateExample>("");
	return 0;
}