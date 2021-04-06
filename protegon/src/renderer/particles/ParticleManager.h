#pragma once

#include <cstdlib>

#include "ecs/ECS.h"
#include "renderer/particles/Particle.h"

namespace engine {

class ParticleManager {
public:
	ParticleManager(std::size_t max_particles);
	~ParticleManager() = default;
	void Reset() {
		particle_pool_.Clear();
	}
	void Refresh() {
		particle_pool_.Refresh();
	}
	virtual void Emit(const Particle& properties);
	virtual void Render();
	virtual void Update();
private:
	std::size_t max_particles_{ 10 };
	ecs::Manager particle_pool_;
};

} // namespace engine