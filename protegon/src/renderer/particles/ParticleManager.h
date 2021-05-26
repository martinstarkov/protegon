#pragma once

#include <cstdlib> // std::size_t
#include <tuple> // std::pair
#include <vector> // std::vector

#include "renderer/particles/Particle.h"

namespace engine {

class ParticleManager {
public:
	ParticleManager() = delete;

	ParticleManager(std::size_t max_particles);

	void Init(Particle&& template_particle);

	~ParticleManager();

	virtual void Emit(const ParticleProperties& new_properties);
	
	virtual void Update();
	
	virtual void Render();
private:
	std::size_t max_particles_{ 0 };
	std::size_t active_particles_{ 0 };
	std::vector<std::pair<Particle, ParticleProperties>> particle_pool_;
};

} // namespace engine