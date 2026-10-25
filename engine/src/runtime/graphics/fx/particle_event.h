#pragma once

#include "runtime/graphics/fx/particle.h"

namespace ptgn::event {

struct ParticleSpawned {
	ParticleEmitter emitter;
	Particle particle;
};

/// @brief Triggered when a particle is destroyed after reaching the end of its lifetime.
struct ParticleDestroyed {
	ParticleEmitter emitter;
	Particle particle;
};

} // namespace ptgn::event