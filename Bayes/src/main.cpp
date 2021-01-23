#include <engine/Include.h>

#include "components/Components.h"
#include "factory/Factories.h"
#include "systems/Systems.h"

// https://gamedevelopment.tutsplus.com/tutorials/how-to-create-a-custom-2d-physics-engine-oriented-rigid-bodies--gamedev-8032

#include <cmath>

class Hopper : public engine::Engine {
public:
	void Init() {
		LOG("Initializing hopper systems...");
		scene.manager.AddSystem<WorldRenderSystem>(&scene);
		scene.manager.AddSystem<CameraSystem>(&scene);

		CreateWorld(scene.manager, scene);

		auto hopper = scene.manager.GetEntitiesWith<PlayerController, RigidBodyComponent>()[0];
		auto rb = hopper.GetComponent<RigidBodyComponent>();
		original_vertices = *rb.body->shape->GetVertices();
		original_rotation = rb.body->shape->GetRotationMatrix();
		original_position = rb.body->position;

		LOG("Initialized all game systems successfully");
		// Green inner box (gives some depth perception).
		inner_box = { V2_double{ 0,0 }, Engine::ScreenSize() };
		// If hopper leaves this box, reset the simulation.
		outer_box = { V2_double{ 0,0 } - distance, Engine::ScreenSize() + V2_double{ distance.x * 2.0, distance.y } };
	}

	void Reset() {
		scene.manager.Clear();
		CreateWorld(scene.manager, scene);
		LOG("RESETTING SIMULATION!");
	}

	// How far out of the inner box the outer box is.
	V2_double distance = { 1000, 500 };
	std::vector<V2_double> original_vertices;
	V2_double original_position;
	Matrix<double, 2, 2> original_rotation;
	AABB inner_box;
	AABB outer_box;
	std::vector<Manifold> contacts;

    void Update() {

		static int counter = 0;

		auto hopper = scene.manager.GetEntitiesWith<PlayerController, RigidBodyComponent>()[0];
		auto hb = hopper.GetComponent<RigidBodyComponent>().body;

		// Gravitational acceleration of Hopper (m/s^2).
		auto gravity = V2_double{ 0, 9.81 };
		// Hopper properties.
		hb->mass = 5.0;
		hb->inertia = 0.08;

		auto players = scene.manager.GetComponentTuple<PlayerController, RigidBodyComponent>();
		
		// Thrust of EDF (N).
		auto thrust = 50.0;
		if (engine::InputHandler::KeyPressed(Key::SPACE)) {
			for (auto [entity, player, rb] : players) {
				// Apply varying thrust based on orientation.
				rb.body->force.y += -thrust * abs(std::cos(rb.body->orientation));
				rb.body->force.x += thrust * std::sin(rb.body->orientation);
			}
		}
		
		// Disturbance torque (N*m)
		auto torque = 0.0005;
		if (engine::InputHandler::KeyPressed(Key::RIGHT)) {
			for (auto [entity, player, rb] : players) {
				rb.body->torque += torque;
			}
		} else if (engine::InputHandler::KeyPressed(Key::LEFT)) {
			for (auto [entity, player, rb] : players) {
				rb.body->torque -= torque;
			}
		}


		/*


		for (auto [entity, player, rb] : players) {
			
			// *Add control here*

			// Add forces to the net force.
			rb.force += 0; // Blah blah blah

			// Add forces to the net force.
			rb.torque += 0;// Blah blah blah
		}

		*/


		// Physics.
		for (auto [entity, player, rb] : players) {
			auto& b = *rb.body;

			// Add linear accelerations to velocity.
			b.velocity += b.force / b.mass + gravity;
			// Add angular accelerations to angular velocity.
			b.angular_velocity += b.torque / b.inertia;
			
			// Update linear quantities.
			b.position += b.velocity;
			b.orientation += b.angular_velocity;
			rb.body->SetOrientation(b.orientation); // Orientation must be updated like this as it uses a rotation matrix.

			// Reset net values of torque and force.
			b.torque = 0;
			b.force = {};
		}

		// Collision handling.
		contacts.clear();
		auto entities = scene.manager.GetComponentTuple<RigidBodyComponent>();
		for (auto [A_entity, A_rb] : entities) {
			Body* A = A_rb.body;
			for (auto [B_entity, B_rb] : entities) {
				Body* B = B_rb.body;
				if (A_entity == B_entity)
					continue;
				Manifold m(A, B);
				m.Solve();
				if (m.contact_count) {
					if (m.A->name == 69) {
						m.A->position -= m.normal * m.penetration;
						// Reset velocity and angular velocity when collision occurs.
						m.A->velocity = {};
						m.A->angular_velocity = 0;
						// Reset orientation when Hopper collides with ground.
						m.A->SetOrientation(0);
					}
					contacts.emplace_back(m);
				}
			}
		}

		// Reset when R is pressed / Hopper leaves outer boundaries / Hopper flips 180 degrees.
		if (engine::InputHandler::KeyPressed(Key::R) || !engine::collision::AABBvsAABB(outer_box, AABB{ hb->position, hopper.GetComponent<SizeComponent>().size }) || std::abs(hb->orientation) > engine::math::PI<double>) {
			Reset();
		}

		// Draw additional elements to screen.
		DebugDisplay::rectangles().emplace_back(inner_box, engine::DARK_GREEN);
		DebugDisplay::rectangles().emplace_back(outer_box, engine::DARK_RED);
		// Original Hopper position.
		DebugDisplay::polygons().emplace_back(original_position, original_vertices, original_rotation, engine::GREEN);
		// Line from Hopper's current position to original position.
		DebugDisplay::lines().emplace_back(hb->position, original_position, engine::ORANGE);

		// Keep camera centered on Hopper.
		scene.manager.Update<CameraSystem>();
    }

	void Render() {
		// Draw environment and Hopper to the screen.
		scene.manager.Update<WorldRenderSystem>();
	}
private:
};

int main(int argc, char* args[]) { // sdl main override

	LOG("Starting Hopper Simulation");
	engine::Engine::Start<Hopper>("Hopper Simulation", 1000, 600);

    return 0;
}