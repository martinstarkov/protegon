#pragma once

#include <cstdint>

#include "core/game.h"
#include "ecs/ecs.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "utility/time.h"
#include "utility/timer.h"

namespace ptgn {

enum class ParticleShape {
	Circle,
	Square
};

struct Particle {
	V2_float position;
	V2_float velocity;
	Color color;
	Color start_color;
	Color end_color;
	Timer timer;
	milliseconds lifetime;
	float start_radius{ 0.0f };
	float radius{ 0.0f };
};

struct ParticleInfo {
	ParticleInfo() = default;

	Texture texture;
	bool texture_enabled{ false };
	bool tint_texture{ true };

	V2_float starting_position;

	std::size_t total_particles{ 200 };

	milliseconds emission_frequency{ 60 };
	milliseconds lifetime{ 2000 };

	float speed{ 10.0f };
	float starting_angle{ DegToRad(0.0f) };

	float line_thickness{ -1.0f };
	ParticleShape particle_shape{ ParticleShape::Circle };

	Color start_color{ color::Red };
	Color end_color{ color::Red };

	float radius{ 5.0f };
	float radius_variance{ 4.0f };

	float start_scale{ 1.0f };
	float end_scale{ 0.0f };

	milliseconds lifetime_variance{ 400 };

	float speed_variance{ 5.0f };
	float angle_variance{ DegToRad(5.0f) };
	V2_float position_variance{ 5.0f };
	// -1.0f means solid. Only applies if texture_enabled == false.
	V2_float gravity;

	// TODO: Implement.
	Color start_color_variance{ color::Red };
	Color end_color_variance{ color::Orange };
	BlendMode blend_mode{ BlendMode::Add };
	V2_float radial_acceleration;
	V2_float radial_acceleration_variance;
	V2_float tangential_acceleration;
	V2_float tangential_acceleration_variance;
};

class ParticleManager {
public:
	ParticleManager() = default;

	ParticleInfo info;

	explicit ParticleManager(const ParticleInfo& info) : info{ info } {
		manager_.Reserve(info.total_particles);
	}

	void Update() {
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

	void Draw() {
		// TOOD: Add blend mode
		if (info.texture_enabled) {
			TextureInfo i;
			i.source.origin = Origin::Center;
			PTGN_ASSERT(info.texture.IsValid());
			for (const auto& [e, p] : manager_.EntitiesWith<Particle>()) {
				if (info.tint_texture) {
					i.tint = p.color;
				} else {
					i.tint = color::White;
				}
				game.draw.Texture(info.texture, p.position, { p.radius, p.radius }, i);
			}
			return;
		}
		switch (info.particle_shape) {
			case ParticleShape::Circle: {
				for (const auto& [e, p] : manager_.EntitiesWith<Particle>()) {
					game.draw.Circle(p.position, p.radius, p.color, info.line_thickness);
				}
				break;
			}
			case ParticleShape::Square: {
				for (const auto& [e, p] : manager_.EntitiesWith<Particle>()) {
					// TODO: Add rect rotation.
					game.draw.Rect(
						p.position, { p.radius, p.radius }, p.color, Origin::Center,
						info.line_thickness
					);
				}
				break;
			}
		}
	}

	void Start() {
		emission_.Start();
	}

	void Stop() {
		emission_.Stop();
	}

	void Toggle() {
		emission_.Toggle();
	}

	void EmitParticle() {
		particle_count_++;
		auto e{ manager_.CreateEntity() };
		auto& p = e.Add<Particle>();
		p.timer.Start();
		ResetParticle(p);
		manager_.Refresh();
	}

	void Clear() {
		manager_.Clear();
	}

	void Reset() {
		manager_.Reset();
	}

private:
	std::size_t particle_count_{ 0 };
	Timer emission_;

	void ResetParticle(Particle& p) {
		p.position = info.starting_position + info.position_variance * V2_float{ rng_(), rng_() };
		p.velocity = {
			info.speed + info.speed_variance * rng_() *
							 std::cos(info.starting_angle + info.angle_variance * rng_()),
			info.speed + info.speed_variance * rng_() *
							 std::sin(info.starting_angle + info.angle_variance * rng_())
		};
		p.start_radius = std::max(info.radius + info.radius_variance * rng_(), 0.0f);
		// TODO: Fix multiplications.
		// TODO: Add clamping of values.
		p.start_color = info.start_color; // + info.start_color_variance * rng_();
		p.end_color	  = info.end_color;	  // + info.end_color_variance * rng_();
		p.lifetime	  = info.lifetime;	  // + info.lifetime_variance * rng_();
	}

	Gaussian<float> rng_{ -1.0f, 1.0f };
	ecs::Manager manager_;
};

namespace impl {

// TODO: Add particle manager_.

} // namespace impl

} // namespace ptgn