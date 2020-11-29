#include <engine/Include.h>

#include "components/Components.h"
#include "event/Events.h"
#include "factory/Factories.h"
#include "systems/Systems.h"

#include <cmath>

class MyGame : public engine::Engine {
public:
	std::vector<engine::Image> images;
	void Init() {

		LOG("Initializing game systems...");
		scene.manager.AddSystem<RenderSystem>(&scene);
		scene.manager.AddSystem<PhysicsSystem>();
		scene.manager.AddSystem<LifetimeSystem>();
		scene.manager.AddSystem<AnimationSystem>();
		scene.manager.AddSystem<CollisionSystem>();
		scene.manager.AddSystem<InputSystem>();
		//scene.manager.AddSystem<StateMachineSystem>();
		scene.manager.AddSystem<DirectionSystem>();
		scene.manager.AddSystem<CameraSystem>(&scene);
		scene.ui_manager.AddSystem<RenderSystem>();
		scene.ui_manager.AddSystem<UIButtonListener>();
		scene.ui_manager.AddSystem<UIButtonRenderer>();
		scene.ui_manager.AddSystem<UITextRenderer>();

		title_screen = scene.event_manager.CreateEntity();
		engine::EventHandler::Register<TitleScreenEvent>(title_screen);
		auto& title = title_screen.AddComponent<TitleScreenComponent>();
		pause_screen = scene.event_manager.CreateEntity();
		engine::EventHandler::Register<PauseScreenEvent>(pause_screen);
		auto& pause = pause_screen.AddComponent<PauseScreenComponent>();
		engine::EventHandler::Invoke(title_screen, scene.manager, scene.ui_manager);
		title.open = true;
		pause.open = false;
		LOG("Initialized all game systems successfully");
	}

    void Update() {
		auto& pause = pause_screen.GetComponent<PauseScreenComponent>();
		scene.manager.Update<InputSystem>();
		if (!pause.open) {
			scene.ui_manager.Update<UIButtonListener>();
			if (scene.manager.HasSystem<PhysicsSystem>()) {
				scene.manager.Update<PhysicsSystem>();
			}
			scene.manager.Update<CollisionSystem>();
			//scene.manager.Update<StateMachineSystem>();
			scene.manager.Update<DirectionSystem>();
			scene.manager.Update<LifetimeSystem>();
			scene.manager.Update<CameraSystem>();
		}
		//AllocationMetrics::printMemoryUsage();
		auto& title = title_screen.GetComponent<TitleScreenComponent>();
		if (engine::InputHandler::KeyPressed(Key::R)) {
			engine::EventHandler::Invoke(title_screen, scene.manager, scene.ui_manager);
			title.open = true;
			pause.open = false;
		} else if (title.open) {
			if (scene.ui_manager.GetEntitiesWith<TitleScreenComponent>().size() == 0) {
				title.open = false;
			}
		}
		if (engine::InputHandler::KeyPressed(Key::ESCAPE) && pause.toggleable && !title.open) {
			pause.toggleable = false;
			engine::EventHandler::Invoke(pause_screen, pause_screen, scene.manager, scene.ui_manager);
		} else if (engine::InputHandler::KeyReleased(Key::ESCAPE)) {
			if (!pause.toggleable) {
				pause.release_time += 1;
			}
			if (pause.release_time >= 5) {
				pause.toggleable = true;
				pause.release_time = 0;
			}
		}
    }

	void Render() {
		auto& pause = pause_screen.GetComponent<PauseScreenComponent>();
		if (!pause.open) {
			scene.manager.Update<AnimationSystem>();
		}
		scene.manager.Update<RenderSystem>();
		scene.ui_manager.Update<UIButtonRenderer>();
		scene.ui_manager.Update<UITextRenderer>();
	}
private:
	ecs::Entity title_screen;
	ecs::Entity pause_screen;
};

int main(int argc, char* args[]) { // sdl main override

	LOG("Starting Protegon");
	engine::Engine::Start<MyGame>("Protegon", 1200, 900);

    return 0;
}