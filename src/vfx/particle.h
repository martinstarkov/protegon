#pragma once

#include <cmath>
#include <cstdint>
#include <utility>

#include "ecs/ecs.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/texture.h"
#include "utility/debug.h"
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

	// -1.0f means shape is solid. Only applies if texture_enabled == false.
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

	void Update();

	void Draw() {
		// TOOD: Add blend mode
		if (info.texture_enabled) {
			TextureInfo i;
			PTGN_ASSERT(info.texture.IsValid());
			for (const auto& [e, p] : manager_.EntitiesWith<Particle>()) {
				if (info.tint_texture) {
					i.tint = p.color;
				} else {
					i.tint = color::White;
				}
				info.texture.Draw(
					Rect{ p.position, { 2.0f * p.radius, 2.0f * p.radius }, Origin::Center }, i
				);
			}
			return;
		}
		switch (info.particle_shape) {
			case ParticleShape::Circle: {
				for (const auto& [e, p] : manager_.EntitiesWith<Particle>()) {
					Circle{ p.position, p.radius }.Draw(p.color, info.line_thickness);
				}
				break;
			}
			case ParticleShape::Square: {
				for (const auto& [e, p] : manager_.EntitiesWith<Particle>()) {
					// TODO: Add rect rotation.
					Rect{ p.position, { 2.0f * p.radius, 2.0f * p.radius }, Origin::Center }.Draw(
						p.color, info.line_thickness
					);
				}
				break;
			}
		}
	}

	// Starts emitting particles.
	void Start() {
		emission_.Start();
	}

	// Stops emitting particles.
	void Stop() {
		emission_.Stop();
	}

	// Toggle particle emission.
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

} // namespace ptgn