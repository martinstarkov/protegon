#include <engine/Include.h>

#include "components/Components.h"
#include "factory/Factories.h"
#include "systems/Systems.h"

#include <cmath>

class Hopper : public engine::Engine {
public:
	void Init() {

		/*V2_int graph_size = { 400, 400 };
		auto [graph_window, graph_renderer] = GenerateWindow("Graph", { 50, 50 }, graph_size);
		scene.manager.AddSystem<GraphSystem>(graph_renderer, graph_size);*/
		LOG("Initializing hopper systems...");
		scene.manager.AddSystem<HopperRenderSystem>(&scene);
		scene.manager.AddSystem<WorldRenderSystem>(&scene);
		scene.manager.AddSystem<HopperPhysicsSystem>();
		scene.manager.AddSystem<HopperCollisionSystem>();

		CreateWorld(scene.manager);

		LOG("Initialized all game systems successfully");
	}

    void Update() {
		static int counter = 0;
		if (scene.manager.HasSystem<HopperPhysicsSystem>()) {

			auto random_int = engine::math::GetRandomValue<double>(-1, 1);
			auto players = scene.manager.GetComponentTuple<PlayerController, TransformComponent, RigidBodyComponent, StateVectorComponent, EDFComponent, HopperComponent>();
			for (auto [entity, player, transform, rb, state_vector, edf, hopper] : players) {
				//transform.rotation += random_int;
				if (counter % 1 == 0) {
					// engine::math::GetRandomValue<double>(5)
					if (engine::InputHandler::KeyPressed(Key::SPACE)) {
						rb.rigid_body.acceleration.y -= edf.thrust_force * abs(std::cos(engine::math::DegreeToRadian(transform.rotation))) / rb.rigid_body.mass;
						rb.rigid_body.acceleration.x += edf.thrust_force * std::sin(engine::math::DegreeToRadian(transform.rotation)) / rb.rigid_body.mass;
					}
				}
				double torque = 0;
				if (engine::InputHandler::KeyPressed(Key::RIGHT)) {
					torque = 3;
				} else if (engine::InputHandler::KeyPressed(Key::LEFT)) {
					torque = -3;
				}
				hopper.theta_dd = torque / hopper.inertia * 0.005;
			}

			scene.manager.Update<HopperPhysicsSystem>();
		}
		scene.manager.Update<HopperCollisionSystem>();
    }

	void Render() {
		//scene.manager.Update<GraphSystem>();
		scene.manager.Update<HopperRenderSystem>();
		scene.manager.Update<WorldRenderSystem>();
	}
private:
};

int main(int argc, char* args[]) { // sdl main override

	LOG("Starting Hopper Simulation");
	engine::Engine::Start<Hopper>("Hopper Simulation", 1000, 600);

    return 0;
}