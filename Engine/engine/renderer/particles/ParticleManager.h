#pragma once

#include <cstdlib>

#include "ecs/ECS.h"
#include "ecs/systems/LifetimeSystem.h"
#include "physics/Physics.h"
#include "core/Scene.h"

namespace engine {

class ParticleManager {
public:
	ParticleManager(std::size_t max_particles) : max_particles_{ max_particles } {
		particle_pool_.Reserve(max_particles_);
		particle_pool_.AddSystem<LifetimeSystem>();
	}
	~ParticleManager() = default;
	void Reset() {
		particle_pool_.Clear();
	}
	void Refresh() {
		particle_pool_.Refresh();
	}
	virtual void Emit(const Particle& properties) {
		if (particle_pool_.GetEntityCount() < max_particles_) {
			auto entity = particle_pool_.CreateEntity();
			entity.AddComponent<ParticleComponent>(properties);
			entity.AddComponent<LifetimeComponent>(properties.lifetime);
			entity.AddComponent<RenderComponent>(properties.start_color);
			Circle circle(properties.start_radius);
			auto body = new Body(&circle, properties.position);
			body->velocity = properties.velocity;
			body->force = properties.acceleration;
			body->angular_velocity = properties.angular_velocity;
			body->SetOrientation(properties.rotation);
			entity.AddComponent<RigidBodyComponent>(body);
		}
	}
	virtual void Update() {
		auto particles = particle_pool_.GetEntityComponents<ParticleComponent, LifetimeComponent, RigidBodyComponent, RenderComponent>();
		for (auto [entity, particle, life, rb, render] : particles) {
			// Assume 1-to-1 ratio between force and acceleration for point-like particles.
			rb.body->velocity += rb.body->force;
			rb.body->position += rb.body->velocity;
			rb.body->orientation += rb.body->angular_velocity;
			rb.body->SetOrientation(rb.body->orientation);
			auto percentage_life_left = (1.0 - life.lifetime / life.original_lifetime);
			assert(percentage_life_left >= 0.0 && percentage_life_left <= 1.0);
			auto radius = math::Lerp(particle.properties.start_radius, particle.properties.end_radius, percentage_life_left);
			rb.body->shape->SetRadius(radius);
			render.color = Lerp(particle.properties.start_color, particle.properties.end_color, percentage_life_left);
		}
		particle_pool_.UpdateSystem<LifetimeSystem>();
	}
	virtual void Render() {
		auto& scene = Scene::Get();
		auto particles = particle_pool_.GetEntityComponents<RigidBodyComponent, RenderComponent>();
		for (auto [entity, rb, render] : particles) {
			//TextureManager::DrawSolidCircle(scene.WorldToScreen(rb.body->position), scene.ScaleX(rb.body->shape->GetRadius()), render.color);
			auto radius{ rb.body->shape->GetRadius() };
			V2_double half_size{ radius, radius };
			TextureManager::DrawSolidRectangle(scene.WorldToScreen(rb.body->position - half_size), scene.Scale(half_size * 2), render.color);
		}
	}
private:
	std::size_t max_particles_{ 10 };
	ecs::Manager particle_pool_;
};

} // namespace engine