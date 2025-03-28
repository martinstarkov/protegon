#pragma once

#include <string_view>

#include "core/transform.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/math.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "renderer/blend_mode.h"
#include "renderer/color.h"
#include "utility/time.h"
#include "utility/timer.h"

namespace ptgn {

class Scene;

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

	std::string_view texture_key{};
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
};

namespace impl {

class RenderData;

struct ParticleEmitterComponent {
	ParticleInfo info;
	std::size_t particle_count{ 0 };
	Timer emission;
	Gaussian<float> rng{ -1.0f, 1.0f };
	ecs::Manager manager;

	void Update(const V2_float& start_position);

	void EmitParticle(const V2_float& start_position);

	void ResetParticle(const V2_float& start_position, Particle& p);
};

} // namespace impl

class ParticleEmitter : public GameObject {
public:
	ParticleEmitter() = delete;
	explicit ParticleEmitter(ecs::Manager& manager);
	explicit ParticleEmitter(ecs::Manager& manager, const ParticleInfo& info);

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
	friend class impl::RenderData;

	static void Draw(
		const ecs::Entity& e, impl::RenderData& r, const Depth& depth, BlendMode blend_mode
	);
};

} // namespace ptgn