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
	void Reset() {
		scene.manager.Clear();
		CreateWorld(scene.manager);
		LOG("RESETTING SIMULATION");
		LOG("RESETTING SIMULATION");
		LOG("RESETTING SIMULATION");
		LOG("RESETTING SIMULATION");
		LOG("RESETTING SIMULATION");
	}
    void Update() {
		static int counter = 0;
		if (engine::InputHandler::KeyDown(Key::R)) {
			Reset();
		} else {
			if (scene.manager.HasSystem<HopperPhysicsSystem>()) {

				auto random_int = engine::math::GetRandomValue<double>(-1, 1);
				auto players = scene.manager.GetComponentTuple<PlayerController, TransformComponent, RigidBodyComponent, StateVectorComponent, EDFComponent, HopperComponent>();
				for (auto [entity, player, transform, rigid, state_vector, edf, hopper] : players) {
					//transform.rotation += random_int;
					auto& rb = rigid.rigid_body;
					if (engine::InputHandler::KeyDown(Key::UP)) {
						edf.thrust_percent += 0.01;
					} else if (engine::InputHandler::KeyDown(Key::DOWN)) {
						edf.thrust_percent -= 0.01;
					}
					if (engine::InputHandler::KeyPressed(Key::SPACE)) {
						edf.thrust_ramp_up += edf.thrust_ramp_up_speed;
						edf.Power();
					} else {
						edf.Deactivate();
					}
					rb.acceleration.y -= edf.thrust_force * abs(std::cos(engine::math::DegreeToRadian(transform.rotation))) / rb.mass;
					rb.acceleration.x += edf.thrust_force * std::sin(engine::math::DegreeToRadian(transform.rotation)) / rb.mass;
					double disturbance_torque = 0;
					double control_torque = 0;
					auto proportional_gain = 0.9;
					if (engine::InputHandler::KeyPressed(Key::RIGHT)) {
						disturbance_torque = 3;
					} else if (engine::InputHandler::KeyPressed(Key::LEFT)) {
						disturbance_torque = -3;
					}
					control_torque = edf.GetTorque(-proportional_gain * transform.rotation);
					hopper.theta_dd = (disturbance_torque + control_torque) / hopper.inertia * 0.005;
					LOG("rot:"<< transform.rotation << ",thrust_f:" << edf.thrust_force << ",theta_dd:"<< hopper.theta_dd << ",control_tq:" << control_torque << ",disturb_torque:" << disturbance_torque);
					if (std::abs(transform.rotation) > 180) {
						Reset();
					}
				}

				scene.manager.Update<HopperPhysicsSystem>();
			}
			scene.manager.Update<HopperCollisionSystem>();
		}
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