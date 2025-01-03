#include "vfx/particle.h"

#include <cstdint>

#include "core/game.h"
#include "ecs/ecs.h"
#include "math/math.h"
#include "renderer/color.h"
#include "utility/timer.h"

namespace ptgn {

void ParticleManager::Update() {
	if (particle_count_ < info.total_particles && emission_.IsRunning() &&
		emission_.Completed(info.emission_frequency)) {
		EmitParticle();
		emission_.Start();
	}

	for (auto [e, p] : manager_.EntitiesWith<Particle>()) {
		float elapsed{ p.timer.ElapsedPercentage(p.lifetime) };
		if (elapsed >= 1.0f) {
			e.Destroy();
			particle_count_--;
			continue;
		}
		p.color		= Lerp(p.start_color, p.end_color, elapsed);
		p.color.a	= static_cast<std::uint8_t>(Lerp(255.0f, 0.0f, elapsed));
		p.radius	= p.start_radius * Lerp(info.start_scale, info.end_scale, elapsed);
		p.velocity += info.gravity * game.dt();
		p.position += p.velocity * game.dt();
	}
	manager_.Refresh();
}

} // namespace ptgn