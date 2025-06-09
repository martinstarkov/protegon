#pragma once

#include <string_view>

#include "components/draw.h"
#include "components/drawable.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/timer.h"
#include "math/math.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/resources/texture.h"

namespace ptgn {

class Scene;
class Manager;

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

	PTGN_SERIALIZER_REGISTER(
		Particle, position, velocity, color, start_color, end_color, timer, lifetime, start_radius,
		radius
	)
};

struct ParticleInfo {
	ParticleInfo() = default;

	TextureHandle texture_key;
	bool texture_enabled{ false };
	bool tint_texture{ true };

	std::size_t total_particles{ 200 };

	milliseconds emission_delay{ 60 };
	milliseconds lifetime{ 2000 };

	float speed{ 10.0f };
	float starting_angle{ DegToRad(0.0f) };

	// -1.0f means shape is solid. Only applies if texture_enabled == false.
	float line_width{ -1.0f };

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

	// TODO: Implement functionality.
	Color start_color_variance{ color::Red };
	Color end_color_variance{ color::Orange };
	V2_float radial_acceleration;
	V2_float radial_acceleration_variance;
	V2_float tangential_acceleration;
	V2_float tangential_acceleration_variance;

	PTGN_SERIALIZER_REGISTER(
		ParticleInfo, texture_key, texture_enabled, tint_texture, total_particles, emission_delay,
		lifetime, speed, starting_angle, line_width, particle_shape, start_color, end_color, radius,
		radius_variance, start_scale, end_scale, lifetime_variance, speed_variance, angle_variance,
		position_variance, gravity, start_color_variance, end_color_variance, radial_acceleration,
		radial_acceleration_variance, tangential_acceleration, tangential_acceleration_variance
	)
};

namespace impl {

class RenderData;

struct ParticleEmitterComponent {
	ParticleInfo info;
	std::size_t particle_count{ 0 };
	Timer emission;
	Gaussian<float> rng{ -1.0f, 1.0f };
	Manager manager;

	void Update(const V2_float& start_position);

	void EmitParticle(const V2_float& start_position);

	void ResetParticle(const V2_float& start_position, Particle& p);

	PTGN_SERIALIZER_REGISTER(ParticleEmitterComponent, info, particle_count, emission, rng, manager)
};

} // namespace impl

class ParticleEmitter : public Sprite, public Drawable<ParticleEmitter> {
public:
	ParticleEmitter() = default;

	using Sprite::Sprite;

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	// Starts emitting particles.
	ParticleEmitter& Start();

	// Stops emitting particles.
	ParticleEmitter& Stop();

	// Toggle particle emission.
	ParticleEmitter& Toggle();

	ParticleEmitter& EmitParticle();

	ParticleEmitter& Reset();

	ParticleEmitter& SetGravity(const V2_float& particle_gravity);
	[[nodiscard]] V2_float GetGravity() const;

	ParticleEmitter& SetMaxParticles(std::size_t max_particles);
	[[nodiscard]] std::size_t GetMaxParticles() const;

	ParticleEmitter& SetShape(ParticleShape shape);
	[[nodiscard]] ParticleShape GetShape() const;

	ParticleEmitter& SetRadius(float particle_radius);
	[[nodiscard]] float GetRadius() const;

	ParticleEmitter& SetStartColor(const Color& start_color);
	[[nodiscard]] Color GetStartColor() const;

	ParticleEmitter& SetEndColor(const Color& end_color);
	[[nodiscard]] Color GetEndColor() const;

	ParticleEmitter& SetEmissionDelay(milliseconds emission_delay);
	[[nodiscard]] milliseconds GetEmissionDelay() const;

private:
	friend class Scene;

	static void Update(Manager& manager);
};

[[nodiscard]] ParticleEmitter CreateParticleEmitter(
	Manager& manager, const ParticleInfo& info = {}
);

} // namespace ptgn