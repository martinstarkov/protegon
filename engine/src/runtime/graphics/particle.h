#pragma once

#include <optional>
#include <ostream>
#include <string_view>

#include "core/math/math_utils.h"
#include "core/math/rng.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/time/timer.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "serialization/json/enum.h"
#include "serialization/json/serialize.h"

namespace ptgn {

class Scene;
class DrawContext;

enum class ParticleShape {
	Circle,
	Square
};

std::ostream& operator<<(std::ostream& o, ParticleShape shape);

PTGN_SERIALIZE_ENUM(
	ParticleShape, { { ParticleShape::Circle, "circle" }, { ParticleShape::Square, "square" } }
);

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

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		Particle, position, velocity, color, start_color, end_color, timer, lifetime, start_radius,
		radius
	)
};

struct ParticleInfo {
	ParticleInfo() = default;

	std::optional<std::string_view> texture_key;
	bool tint_texture{ true };

	std::size_t max_particles{ 200 };

	milliseconds emission_delay{ 60 };
	milliseconds lifetime{ 2000 };

	float speed{ 10.0f };
	float starting_angle{ DegToRad(0.0f) };

	/// @brief Only applies if texture_key == nullopt.
	FillStyle fill_style{ FillStyle::Solid() };

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

	float min_speed{ 0.0f };
	float max_speed{ 10.0f };
	bool use_random_velocities{ true };

	// TODO: Implement functionality.
	Color start_color_variance{ color::Red };
	Color end_color_variance{ color::Orange };
	V2_float radial_acceleration;
	V2_float radial_acceleration_variance;
	V2_float tangential_acceleration;
	V2_float tangential_acceleration_variance;

	// TODO: Fix serialization (add fill_style and texture).
	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		ParticleInfo, tint_texture, max_particles, emission_delay, lifetime, speed, starting_angle,
		particle_shape, start_color, end_color, radius, radius_variance, start_scale, end_scale,
		lifetime_variance, speed_variance, angle_variance, position_variance, gravity,
		start_color_variance, end_color_variance, radial_acceleration, radial_acceleration_variance,
		tangential_acceleration, tangential_acceleration_variance
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

	void Update(V2_float start_position, secondsf dt);

	void EmitParticle(V2_float start_position);

	void ResetParticle(V2_float start_position, Particle& p);

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		ParticleEmitterComponent, info, particle_count, emission, rng, manager
	)
};

} // namespace impl

class ParticleEmitter : public Entity {
public:
	ParticleEmitter() = default;
	explicit ParticleEmitter(Entity entity);

	static void Draw(DrawContext& renderer, Entity entity);

	// Starts emitting particles.
	ParticleEmitter& Start();

	// Stops emitting particles.
	ParticleEmitter& Stop();

	// Toggle particle emission.
	ParticleEmitter& Toggle();

	ParticleEmitter& EmitParticle();

	ParticleEmitter& Reset();

	ParticleEmitter& SetGravity(V2_float particle_gravity);
	[[nodiscard]] V2_float GetGravity() const;

	// Will make the emitter use random velocities instead of gravity.
	ParticleEmitter& UseRandomVelocities(
		float min_speed, float max_speed, bool use_random_velocities = true
	);

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

	static void Update(Scene& scene);
};

ParticleEmitter CreateParticleEmitter(Scene& scene, const ParticleInfo& info = {});

PTGN_REGISTER_DRAWABLE(ParticleEmitter);

} // namespace ptgn