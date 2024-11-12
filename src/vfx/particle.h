#pragma once

#include <cstdint>

#include "ecs/ecs.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/texture.h"
#include "utility/time.h"

namespace ptgn {

struct Particle {
	V2_float position;
	V2_float velocity;
	Color start_color;
	Color end_color;
	float radius{ 0.0f };
};

struct ParticleInfo {
	float start_scale{ 1.0f };
	float end_scale{ 0.0f };
	Texture texture;
	milliseconds emission_frequency{ 100 };
	milliseconds lifetime{ 2000 };
	milliseconds lifetime_variance{ 100 };
	V2_float radial_acceleration;
	V2_float radial_acceleration_variance;
	V2_float tangential_acceleration;
	V2_float tangential_acceleration_variance;

	float speed_variance{ 0.0f };
	float speed{ 0.0f };
	float angle_variance{ 0.0f };
	float starting_angle{ 0.0f };
	V2_float position_variance;
	V2_float starting_position;
	bool texture_enabled{ true };
	V2_float gravity;
	BlendMode blend_mode{ BlendMode::Add };
	float radius{ 10.0f };
	float radius_variance{ 10.0f };
	Color start_color{ color::Red };
	Color start_color_variance{ color::Red };
	Color end_color{ color::Red };
	Color end_color_variance{ color::Red };
	std::size_t total_particles{ 100 };
};

class ParticleManager {
public:
	ParticleInfo info;

	ParticleManager() {
		manager.Reserve(info.total_particles);
		for (std::size_t i{ 0 }; i < info.total_particles; ++i) {
			EmitParticle();
		}
		manager.Refresh();
	}

	void Update() {
		// TODO: Emit particles using emission_frequency.
		// TODO: Destroy particles if their lifetime expires.

		// TODO: Scale down start_scale -> end_scale
		for (auto [e, p] : manager.EntitiesWith<Particle>()) {
			p.position += p.velocity * game.dt();
			// TODO: gravity
		}
	}

	void Draw() {
		// TODO: Implement.
		if (info.texture_enabled) {
			// TOOD: blend_mode
		} else {
			// TOOD: blend_mode
		}
	}

	void EmitParticle() {
		// TODO: Instead of creating new entities, recycle old ones.
		auto e{ manager.CreateEntity() };
		auto& p = e.Add<Particle>();
		ResetParticle(p);
	}

	void Clear() {
		manager.Clear();
	}

	void Reset() {
		manager.Reset();
	}

private:
	void ResetParticle(Particle& p) {
		p.position = info.starting_position + info.position_variance * V2_float{ rng(), rng() };
		p.velocity = { info.speed + info.speed_variance * rng() *
										std::cos(info.starting_angle + info.angle_variance * rng()),
					   info.speed +
						   info.speed_variance * rng() *
							   std::sin(info.starting_angle + info.angle_variance * rng()) };
		p.radius   = info.radius + info.radius_variance * rng();
		// TODO: Fix multiplication.
		p.start_color = info.start_color + info.start_color_variance * rng();
		p.end_color	  = info.end_color + info.end_color_variance * rng();
	}

	RNG<float> rng;
	ecs::Manager manager;
};

namespace impl {

// TODO: Add particle manager.

} // namespace impl

} // namespace ptgn