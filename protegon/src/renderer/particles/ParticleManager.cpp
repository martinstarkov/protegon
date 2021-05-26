#include "ParticleManager.h"

#include "ecs/systems/LifetimeSystem.h"
#include "math/Math.h"
#include "renderer/Renderer.h"

namespace engine {

ParticleManager::ParticleManager(std::size_t max_particles) : max_particles_{ max_particles } {
	particle_pool_.reserve(max_particles_);
}

ParticleManager::~ParticleManager() {
	for (auto& particle : particle_pool_) {
		delete particle.first.begin_shape;
		delete particle.first.end_shape;
		particle.first.begin_shape = nullptr;
		particle.first.end_shape = nullptr;
	}
}

void ParticleManager::Init(Particle&& template_particle) {
	particle_pool_.resize(max_particles_, { template_particle, {} });
	assert(template_particle.begin_shape != nullptr &&
		   "Cannot create particle manager with template particle with nullptr begin shape");
	assert(template_particle.end_shape != nullptr &&
		   "Cannot create particle manager with template particle with nullptr end shape");
	assert(template_particle.begin_shape->GetType() == template_particle.end_shape->GetType() &&
		   "Cannot create particle manager with template particle with differing shape types");
	for (auto& particle : particle_pool_) {
		particle.first.begin_shape = template_particle.begin_shape->Clone();
		particle.first.end_shape = template_particle.end_shape->Clone();
	}
	delete template_particle.begin_shape;
	delete template_particle.end_shape;
	template_particle.begin_shape = nullptr;
	template_particle.end_shape = nullptr;
}

void ParticleManager::Emit(const ParticleProperties& new_properties) {
	if (active_particles_ < particle_pool_.size()) {
		for (auto& [particle, properties] : particle_pool_) {
			if (!particle.lifetime.IsRunning()) {
				particle.lifetime.Start();
				properties = new_properties;
				++active_particles_;
				break;
			}
		}
	}
}

void ParticleManager::Update() {
	std::size_t active_particles{ 0 };
	for (auto& [particle, properties] : particle_pool_) {
		if (particle.lifetime.IsRunning()) {
			++active_particles;
			properties.body.velocity += properties.body.acceleration;
			properties.body.angular_velocity += properties.body.angular_acceleration;
			properties.transform.position += properties.body.velocity;
			properties.transform.rotation += properties.body.angular_velocity;
		}
	}
	active_particles_ = active_particles;
}

void ParticleManager::Render() {
	for (auto& [particle, properties] : particle_pool_) {
		if (particle.lifetime.IsRunning()) {
			auto type{ particle.begin_shape->GetType() };
			auto percentage_life_elapsed{ particle.lifetime.ElapsedPercentage() };
			//PrintLine("Remaining life: ", percentage_life_remaining);
			auto color{
				math::Lerp(particle.begin_color, particle.end_color, percentage_life_elapsed)
			};
			if (type == ShapeType::CIRCLE) {
				auto radius{
					math::Lerp(particle.begin_shape->CastTo<Circle>().radius, particle.end_shape->CastTo<Circle>().radius, percentage_life_elapsed)
				};
				Renderer::DrawCircle(properties.transform.position, radius, color);
			} else if (type == ShapeType::AABB) {
				auto size{
					math::Lerp(particle.begin_shape->CastTo<AABB>().size, particle.end_shape->CastTo<AABB>().size, percentage_life_elapsed)
				};
				Renderer::DrawRectangle(properties.transform.position, size, color);
			}
		}
	}
}

} // namespace engine