
#include <memory>
#include <vector>

#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "debug/log.h"
#include "ecs/ecs.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct Test {};

struct ComponentHookScene : public Scene {
	std::vector<Entity> list;

	void AddToUpdateList(Entity entity) {
		list.emplace_back(entity);
	}

	void RemoveFromUpdateList(Entity entity) {
		list.erase(std::remove(list.begin(), list.end(), entity), list.end());
	}

	void Enter() override {
		OnConstruct<Test>().Connect<ComponentHookScene, &ComponentHookScene::AddToUpdateList>(this);
		OnDestruct<Test>().Connect<ComponentHookScene, &ComponentHookScene::RemoveFromUpdateList>(
			this
		);
	}

	void Update() override {
		if (game.input.KeyDown(Key::E)) {
			CreateEntity().Add<Test>();
		}

		if (game.input.KeyDown(Key::R)) {
			for (Entity e : list) {
				e.Destroy();
			}
			list.clear();
		}

		PTGN_LOG("List: ", list.size());
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ComponentHookScene", window_size);
	game.scene.Enter<ComponentHookScene>("");
	return 0;
}