#include <engine/Include.h>

#include "components/Components.h"
#include "event/Events.h"
#include "factory/Factories.h"
#include "systems/Systems.h"

#include <cmath>

class Hopper : public engine::Engine {
public:
	void Init() {

		V2_int graph_size = { 400, 400 };
		auto [graph_window, graph_renderer] = GenerateWindow("Graph", { 50, 50 }, graph_size);
		scene.manager.AddSystem<GraphSystem>(graph_renderer, graph_size);
		LOG("Initializing game systems...");
		scene.manager.AddSystem<RenderSystem>(&scene);
		scene.manager.AddSystem<HopperPhysicsSystem>();
		scene.manager.AddSystem<CollisionSystem>();
		//scene.manager.AddSystem<StateMachineSystem>();
		scene.manager.AddSystem<DirectionSystem>();
		scene.ui_manager.AddSystem<RenderSystem>();
		scene.ui_manager.AddSystem<UIListener>();
		scene.ui_manager.AddSystem<UIRenderer>();
		scene.ui_manager.AddSystem<UIButtonListener>();
		scene.ui_manager.AddSystem<UIButtonRenderer>();

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
		static int counter = 0;
		if (!pause.open) {
			scene.ui_manager.Update<UIListener>();
			scene.ui_manager.Update<UIButtonListener>();
			if (scene.manager.HasSystem<HopperPhysicsSystem>()) {

				auto random_int = engine::math::GetRandomValue<double>(-1, 1);
				auto players = scene.manager.GetComponentTuple<PlayerController, TransformComponent, RigidBodyComponent, StateVectorComponent, EDFComponent>();
				for (auto [entity, player, transform, rb, state_vector, edf] : players) {
					//transform.rotation += random_int;
					transform.rotation = std::fmod(transform.rotation, 360.0);
					if (counter % 1 == 0) {
						// engine::math::GetRandomValue<double>(5)
						if (engine::InputHandler::KeyPressed(Key::SPACE)) {
							rb.rigid_body.acceleration.y -= edf.thrust_force * abs(std::cos(engine::math::DegreeToRadian(transform.rotation))) / rb.rigid_body.mass;
							rb.rigid_body.acceleration.x += edf.thrust_force * std::sin(engine::math::DegreeToRadian(transform.rotation)) / rb.rigid_body.mass;
						}
					}
					if (engine::InputHandler::KeyPressed(Key::RIGHT) && engine::InputHandler::KeyReleased(Key::LEFT)) {
						transform.rotation += 0.1;
					} else if (engine::InputHandler::KeyPressed(Key::LEFT) && engine::InputHandler::KeyReleased(Key::RIGHT)) {
						transform.rotation -= 0.1;
					}
					LOG(transform.rotation);
				}

				scene.manager.Update<HopperPhysicsSystem>();
			}
			scene.manager.Update<CollisionSystem>();
			scene.manager.Update<DirectionSystem>();
		}
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
		++counter;
    }

	void Render() {
		auto& pause = pause_screen.GetComponent<PauseScreenComponent>();
		scene.manager.Update<GraphSystem>();
		scene.manager.Update<RenderSystem>();
		scene.ui_manager.Update<UIRenderer>();
		scene.ui_manager.Update<UIButtonRenderer>();
	}
private:
	ecs::Entity title_screen;
	ecs::Entity pause_screen;
};

int main(int argc, char* args[]) { // sdl main override

	LOG("Starting Hopper Simulation");
	engine::Engine::Start<Hopper>("Hopper Simulation", 1200, 900);

    return 0;
}