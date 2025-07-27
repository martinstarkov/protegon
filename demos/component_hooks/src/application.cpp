
#include <memory>
#include <vector>

#include "components/common.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "debug/log.h"
#include "ecs/ecs.h"
#include "events/input_handler.h"
#include "events/key.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct ComponentHookScene : public Scene {
	std::vector<Entity> update_list;

	void AddToUpdateList(Entity entity) {
		update_list.emplace_back(entity);
	}

	void RemoveFromUpdateList(Entity entity) {
		update_list.erase(
			std::remove(update_list.begin(), update_list.end(), entity), update_list.end()
		);
	}

	void Enter() override {
		OnConstruct<Enabled>().Connect<ComponentHookScene, &ComponentHookScene::AddToUpdateList>(
			this
		);
		OnDestruct<Enabled>()
			.Connect<ComponentHookScene, &ComponentHookScene::RemoveFromUpdateList>(this);
	}

	void Update() override {
		if (game.input.KeyDown(Key::E)) {
			CreateEntity().Enable();
		}

		if (game.input.KeyDown(Key::R)) {
			for (Entity e : update_list) {
				e.Destroy();
			}
			update_list.clear();
		}

		PTGN_LOG("Update list: ", update_list.size());
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ComponentHookScene", window_size);
	game.scene.Enter<ComponentHookScene>("");
	return 0;
}