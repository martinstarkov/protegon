
#include <memory>
#include <vector>

#include "app/application.h"
#include "core/log.h"
#include "platform/input/key.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

struct Test {};

struct ComponentHookScene : public Scene {
	std::vector<Entity> list;

	void AddToUpdateList(Entity entity) {
		list.emplace_back(entity);
	}

	void RemoveFromUpdateList(Entity entity) {
		list.erase(std::remove(list.begin(), list.end(), entity), list.end());
	}

	void OnEnter() override {
		OnConstruct<Test>().Connect<&ComponentHookScene::AddToUpdateList>();
		OnDestruct<Test>().Connect<&ComponentHookScene::RemoveFromUpdateList>();
	}

	void OnUpdate() override {
		if (ctx().input.KeyPressed(Key::A)) {
			CreateEntity().Add<Test>();
		}

		if (ctx().input.KeyPressed(Key::C)) {
			for (Entity e : list) {
				e.Destroy();
			}
			list.clear();
		}

		PTGN_LOG("List: ", list.size());
	}
};

int main(int, char**) {
	Application game{ "ComponentHookScene: A: Add Entity, C: Clear Entities" };
	game.StartWith<ComponentHookScene>();
}