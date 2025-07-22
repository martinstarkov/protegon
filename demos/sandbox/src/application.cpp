
#include "core/game.h"
#include "events/input_handler.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct SandboxScene : public Scene {
	void Enter() override {}

	void Update() override {
		if (game.input.KeyDown(Key::E)) {
			CreateEntity().Enable();
		}

		if (game.input.KeyDown(Key::R)) {
			for (auto e : update_list) {
				e.Destroy();
			}
			update_list.clear();
		}

		PTGN_LOG("Update list: ", update_list.size());
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SandboxScene", window_size);
	game.scene.Enter<SandboxScene>("");
	return 0;
}