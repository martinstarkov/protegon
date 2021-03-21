#include <engine/Include.h>

#include "components/Components.h"
#include "event/Events.h"
#include "factory/Factories.h"
#include "systems/Systems.h"
#include "core/SurfaceWorld.h"

class Game : public engine::Engine {
public:
	std::vector<engine::Image> images;
	void Init() {
		using namespace engine;
		auto& scene{ Scene::Get() };

		scene.world = new SurfaceWorld();
		auto& world{ *scene.world };
		auto& world_manager{ world.GetManager() };

		LOG("Initializing game systems...");
		world_manager.AddSystem<RenderSystem>();
		world_manager.AddSystem<HitboxRenderSystem>();
		world_manager.AddSystem<PhysicsSystem>();
		world_manager.AddSystem<TargetSystem>();
		world_manager.AddSystem<LifetimeSystem>();
		world_manager.AddSystem<AnimationSystem>();
		world_manager.AddSystem<CollisionSystem>();
		world_manager.AddSystem<InputSystem>();
		world_manager.AddSystem<StateMachineSystem>();
		world_manager.AddSystem<DirectionSystem>();
		world_manager.AddSystem<CameraSystem>();
		world_manager.AddSystem<InventorySystem>();
		scene.ui_manager.AddSystem<RenderSystem>();
		scene.ui_manager.AddSystem<StateMachineSystem>();
		scene.ui_manager.AddSystem<UIRenderer>();


		title_screen = scene.event_manager.CreateEntity();
		scene.event_manager.Refresh();
		engine::EventHandler::Register<TitleScreenEvent>(title_screen);
		auto& title{ title_screen.AddComponent<TitleScreenComponent>() };
		title_screen.AddComponent<EventComponent>(scene);
		engine::EventHandler::Invoke(title_screen);
		title.open = true;
		LOG("Initialized all game systems successfully");
	}

    void Update() {
		using namespace engine;
		auto& scene{ Scene::Get() };
		auto& world{ *scene.world };
		auto& world_manager{ world.GetManager() };

		world_manager.UpdateSystem<InputSystem>();
		auto& title{ title_screen.GetComponent<TitleScreenComponent>() };
		if (engine::InputHandler::KeyDown(Key::R)) {
			engine::EventHandler::Invoke(title_screen);
			title.open = true;
			world.Reset();
		}
		world_manager.UpdateSystem<PhysicsSystem>();
		world_manager.UpdateSystem<TargetSystem>();

		world_manager.UpdateSystem<StateMachineSystem>();
		scene.ui_manager.UpdateSystem<StateMachineSystem>();
		world_manager.UpdateSystem<DirectionSystem>();
		world_manager.UpdateSystem<CameraSystem>();

		auto& title_{ title_screen.GetComponent<TitleScreenComponent>() };
		if (title_.open && scene.ui_manager.GetEntitiesWith<TitleScreenComponent>().size() == 0) {
			title_.open = false;
		}

		auto camera{ scene.GetCamera() };
		if (camera && !title.open) {
			world.Update();
		} else {
			world.Clear();
		}

    }

	void Render() {
		using namespace engine;
		auto& scene{ Scene::Get() };
		auto& world{ *scene.world };
		auto& world_manager{ world.GetManager() };

		world_manager.UpdateSystem<AnimationSystem>();

		world.Render();

		world_manager.UpdateSystem<RenderSystem>();
		world_manager.UpdateSystem<HitboxRenderSystem>();

		scene.ui_manager.UpdateSystem<UIRenderer>();

		world_manager.UpdateSystem<InventorySystem>();
	}
private:
	ecs::Entity title_screen;
};

int main(int argc, char* args[]) { // sdl main override

	engine::Engine::Start<Game>("Protegon", 512 * 2, 600, 60);

    return 0;
}