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

		auto [entity, p, rb, hopper, size] = scene.manager.GetComponentTuple<PlayerController, RigidBodyComponent, HopperComponent, SizeComponent>()[0];
		assert(rb.body != nullptr);
		auto& b = *rb.body;
		original_vertices = *b.shape->GetVertices();
		original_rotation = b.shape->GetRotationMatrix();
		original_position = b.position;

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
	V2_double distance = { 3000, 2000 };
	std::vector<V2_double> original_vertices;
	V2_double original_position;
	Matrix<double, 2, 2> original_rotation;
	AABB inner_box;
	AABB outer_box;
	std::vector<Manifold> contacts;

	engine::Particle air_particle{ {}, {}, {}, 0, engine::SILVER, engine::WHITE, 4, 1, 0, 0.3 };
	engine::ParticleManager particles{ 1000 };
	int particles_per_frame = 20;

    void Update() {

		static int counter = 0;

		auto [entity, p, rb, hopper, size] = scene.manager.GetComponentTuple<PlayerController, RigidBodyComponent, HopperComponent, SizeComponent>()[0];
		assert(rb.body != nullptr);
		auto& b = *rb.body;

		// Gravitational acceleration of Hopper (m/s^2).
		auto gravity = V2_double{ 0, 9.81 };
		// Hopper properties.
		b.mass = 5.5;
		b.inertia = 0.08;
		
		//if (engine::InputHandler::KeyPressed(Key::SPACE)) {
		//	// Apply varying thrust based on orientation.
		//	b.force.y += -hopper.thrust * abs(std::cos(b.orientation));
		//	b.force.x += hopper.thrust * std::sin(b.orientation);
		//}
		
		// Disturbance torque (N*m)
		auto torque = 3.0;
		if (engine::InputHandler::KeyPressed(Key::RIGHT)) {
			b.torque += torque;
		} else if (engine::InputHandler::KeyPressed(Key::LEFT)) {
			b.torque -= torque;
		}

		// Control

		hopper.Update(original_position, b);
		//LOG("Thrust: " << hopper.thrust << ", thrust_vector: " << hopper.thrust_vector << ", force: " << b.force << ", control_angle: " << engine::math::RadiansToDegrees(hopper.control_angle));
		//LOG(b.position << " vs. og." << original_position);
		
		// Physics.

		// Add linear accelerations to velocity.
		b.velocity += (b.force / b.mass + gravity) / 60.0;
		// Add angular accelerations to angular velocity.
		b.angular_velocity += (b.torque / b.inertia) / 60.0;
			
		// Update linear quantities.
		b.position += b.velocity / 60.0;
		b.orientation += b.angular_velocity / 60.0;
		b.SetOrientation(b.orientation); // Orientation must be updated like this as it uses a rotation matrix.




		// Air particles out the back.
		auto highest_y = -engine::math::Infinity<double>();
		for (const auto& vertex : *b.shape->GetVertices()) {
			highest_y = std::max(highest_y, vertex.y);
		}
		air_particle.position = { b.position.x, b.position.y + highest_y - air_particle.velocity.y };
		//air_particle.position = rb.body->position;
		V2_double scale = { 0.1, 1.0 };
		for (auto i = 0; i < particles_per_frame; ++i) {
			air_particle.velocity = scale * V2_double{ -b.velocity.x, 1.0 };
			air_particle.velocity += V2_double::Random(-4.0, 4.0, 0.0, 5.0);
			air_particle.acceleration = b.force / b.mass + gravity;
			if (b.velocity.y < 0)
				particles.Emit(air_particle);
		}




		// Reset net values of torque and force.
		b.torque = 0;
		b.force = {};

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
		if (engine::InputHandler::KeyPressed(Key::R)
			)//|| !engine::collision::AABBvsAABB(outer_box, AABB{ b.position, size.size })
			//|| std::abs(b.orientation) > engine::math::PI<double>) 
		{
			Reset();
		}

		// Draw additional elements to screen.
		DebugDisplay::rectangles().emplace_back(inner_box, engine::DARK_GREEN);
		DebugDisplay::rectangles().emplace_back(outer_box, engine::DARK_RED);
		// Original Hopper position.
		DebugDisplay::polygons().emplace_back(original_position, original_vertices, original_rotation, engine::GREEN);
		// Line from Hopper's current position to original position.
		DebugDisplay::lines().emplace_back(b.position, original_position, engine::ORANGE);

		// Keep camera centered on Hopper.
		scene.manager.Update<CameraSystem>();

		particles.Update();
    }

	void Render() {
		// Draw environment and Hopper to the screen.
		scene.manager.Update<WorldRenderSystem>();
		auto camera = scene.GetCamera();
		if (camera) {
			particles.Render(*camera);
		}
	}
private:
};

int main(int argc, char* args[]) { // sdl main override

	LOG("Starting Hopper Simulation");
	engine::Engine::Start<Hopper>("Hopper Simulation", 1000, 600);

    return 0;
}