#pragma once

#include "System.h"

#include "physics/Physics.h"

#include <cstdint>
#include <vector>

#define LOWEST_VELOCITY 1

#define TEMP_GRAVITY V2_double{ 0.0, 0.0 }

static Body* Add(std::vector<Body*>& bodies, Shape* shape, const V2_double& position) {
	assert(shape != nullptr && "Shape could not be created");
	auto* b = new Body(shape, position);
	assert(b != nullptr);
	bodies.push_back(b);
	return b;
}

static void IntegrateForces(Body* b, const V2_double& gravity, double dt) {
	if (b->inverse_mass == 0.0)
		return;
	b->velocity += (b->force * b->inverse_mass + gravity) * (dt / 2.0);
	b->angular_velocity += b->torque * b->inverse_inertia * (dt / 2.0);
}

static void IntegrateVelocity(Body* b, const V2_double& gravity, double dt) {
	if (b->inverse_mass == 0.0)
		return;

	b->position += b->velocity * dt;
	b->orientation += b->angular_velocity * dt;
	b->SetOrientation(b->orientation);
	IntegrateForces(b, gravity, dt);
}

static void Step(std::vector<Manifold>& contacts, std::vector<Body*>& bodies, std::uint32_t m_iterations, const V2_double& gravity) {
	auto dt = 1.0;
	// Generate new collision info
	contacts.clear();
	for (auto i = 0; i < bodies.size(); ++i) {
		Body* A = bodies[i];
		for (auto j = i + 1; j < bodies.size(); ++j) {
			Body* B = bodies[j];
			if (A->inverse_mass == 0 && B->inverse_mass == 0)
				continue;
			Manifold m(A, B);
			m.Solve();
			if (m.contact_count)
				contacts.emplace_back(m);
		}
	}

	// Integrate forces
	for (std::uint32_t i = 0; i < bodies.size(); ++i)
		IntegrateForces(bodies[i], TEMP_GRAVITY, dt);

	// Initialize collision
	for (std::uint32_t i = 0; i < contacts.size(); ++i)
		contacts[i].Initialize(gravity, dt);

	// Solve collisions
	for (std::uint32_t j = 0; j < m_iterations; ++j)
		for (std::uint32_t i = 0; i < contacts.size(); ++i)
			contacts[i].ApplyImpulse();

	// Integrate velocities
	for (std::uint32_t i = 0; i < bodies.size(); ++i)
		IntegrateVelocity(bodies[i], gravity, dt);

	// Correct positions
	for (std::uint32_t i = 0; i < contacts.size(); ++i)
		contacts[i].PositionalCorrection();

	// Clear all forces
	for (std::uint32_t i = 0; i < bodies.size(); ++i) {
		Body* b = bodies[i];
		b->force = { 0, 0 };
		b->torque = 0;
	}
}

static void RenderBodies(std::vector<Body*>& bodies) {
	for (std::uint32_t i = 0; i < bodies.size(); ++i) {
		Body* b = bodies[i];
		b->shape->Draw();
	}
}

class PhysicsSystem : public ecs::System<TransformComponent, RigidBodyComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, transform, rigid_body] : entities) {
			/*
			auto& rb = rigid_body.rigid_body;
			// Gravity.
			rb.acceleration += rb.gravity;
			// Motion.
			rb.velocity += rb.acceleration;
			// Drag.
			rb.velocity *= V2_double{ 1.0, 1.0 } - rb.drag;

			// Terminal motion.
			if (std::abs(rb.velocity.x) > rb.terminal_velocity.x) {
				rb.velocity.x = engine::math::Sign(rb.velocity.x) * rb.terminal_velocity.x;
			} else if (std::abs(rb.velocity.x) < LOWEST_VELOCITY) {
				rb.velocity.x = 0.0;
			}
			if (std::abs(rb.velocity.y) > rb.terminal_velocity.y) {
				rb.velocity.y = engine::math::Sign(rb.velocity.y) * rb.terminal_velocity.y;
			} else if (std::abs(rb.velocity.y) < LOWEST_VELOCITY) {
				rb.velocity.y = 0.0;
			}
			*/
		}
	}
};