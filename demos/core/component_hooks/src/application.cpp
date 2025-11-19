
#include "core/app/application.h"

#include <memory>
#include <vector>

#include "core/app/manager.h"
#include "ecs/entity.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "core/log.h"
#include "ecs/ecs.h"
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
	Application::Get().Init("ComponentHookScene: A: Add Entity, C: Clear Entities", window_size);
	Application::Get().scene_.Enter<ComponentHookScene>("");
	return 0;
}