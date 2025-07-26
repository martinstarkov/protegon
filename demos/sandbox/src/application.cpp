
#include "core/game.h"
#include "events/input_handler.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct SandboxScene : public Scene {
	void Enter() override {}

	std::vector<Entity> update_list;

	void AddToUpdateList(Entity entity) {
		update_list.emplace_back(entity);
	}

	void RemoveFromUpdateList(Entity entity) {
		update_list.erase(
			std::remove(update_list.begin(), update_list.end(), entity), update_list.end()
		);
	}

	SandboxScene() {
		OnConstruct<Enabled>().Connect<SandboxScene, &SandboxScene::AddToUpdateList>(this);
		OnDestruct<Enabled>().Connect<SandboxScene, &SandboxScene::RemoveFromUpdateList>(this);
	}

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