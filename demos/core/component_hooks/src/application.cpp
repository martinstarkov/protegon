
#include <memory>
#include <vector>

#include "core/ecs/entity.h"
#include "core/app/game.h"
#include "core/app/manager.h"
#include "debug/core/log.h"
#include "ecs/ecs.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/vector2.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

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
		if (input.KeyDown(Key::A)) {
			CreateEntity().Add<Test>();
		}

		if (input.KeyDown(Key::C)) {
			for (Entity e : list) {
				e.Destroy();
			}
			list.clear();
		}

		PTGN_LOG("List: ", list.size());
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ComponentHookScene: A: Add Entity, C: Clear Entities", window_size);
	game.scene.Enter<ComponentHookScene>("");
	return 0;
}