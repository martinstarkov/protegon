#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <variant>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/rng.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/drawable.h"
#include "runtime/scripting/script.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class DrawContext;
class ParticleEmitter;

namespace impl {

struct ParticleEmitterComponent;

} // namespace impl

namespace event {

struct ParticleDestroyed;

} // namespace event

template <typename T>
struct Range {
	constexpr Range() = default;

	constexpr Range(T min, T max) : min{ min }, max{ max } {}

	T min{};
	T max{};

	PTGN_SERIALIZE(Range, min, max)
};

template <typename T>
	requires std::is_default_constructible_v<T>
struct ConstantOrRange {
	constexpr ConstantOrRange() = default;

	constexpr ConstantOrRange(const T& v) : value_{ v } {} // NOSONAR

	constexpr ConstantOrRange(const T& min, const T& max) : value_{ Range<T>{ min, max } } {}

	constexpr ConstantOrRange(const Range<T>& range) : value_{ range } {} // NOSONAR

	[[nodiscard]] T Evaluate() const {
		return std::visit(
			[&]<typename V>(const V& v) -> T {
				if constexpr (std::is_same_v<V, T>) {
					return v;
				} else if constexpr (std::is_same_v<V, Range<T>>) {
					float t = Random01();
					return Lerp(v.min, v.max, t);
				}
			},
			value_
		);
	}

	PTGN_SERIALIZE_VALUE(ConstantOrRange, value_)
private:
	std::variant<T, Range<T>> value_{};
};

struct EmissionShapeArc {
	Degrees arc_angle{ 360.0f };
	float outer_radius{ 1.0f };
	V2_float direction{ 1.0f, 0.0f };
	float inner_radius{ 0.0f };

	PTGN_SERIALIZE(EmissionShapeArc, arc_angle, outer_radius, direction, inner_radius)
};

struct EmissionShapeRect {
	ptgn::Rect rect{ V2_float{ 1.0f } };
	V2_float direction{ 0.0f, 1.0f };

	PTGN_SERIALIZE(EmissionShapeRect, rect, direction)
};

using EmissionShapes = std::variant<EmissionShapeArc, EmissionShapeRect>;

/// @brief The shape from which particles are emitted. Determines the initial position of emitted
/// particles.
class EmissionShape {
public:
	struct EmissionSample {
		V2_float position;
		V2_float direction;

		PTGN_SERIALIZE(EmissionSample, position, direction)
	};

	constexpr EmissionShape() = default;

	[[nodiscard]] constexpr static EmissionShape Arc(
		Degrees arc_angle, float outer_radius, V2_float direction = V2_float{ 1.0f, 0.0f },
		float inner_radius = 0.0f
	) {
		EmissionShape s;
		s.type_ = EmissionShapeArc{ arc_angle, outer_radius, direction, inner_radius };
		return s;
	}

	[[nodiscard]] constexpr static EmissionShape Rect(
		V2_float size, V2_float direction = V2_float{ 0.0f, 1.0f }
	) {
		EmissionShape s;
		s.type_ = EmissionShapeRect{ size, direction };
		return s;
	}

	[[nodiscard]] EmissionSample SampleEmission() const;

	PTGN_SERIALIZE_VALUE(EmissionShape, type_)
private:
	EmissionShapes type_{};
};

/// @brief A rate of particle emission over time.
struct ParticleRate {
	/// @brief Duration of a full cycle of the particle emitter. Only applies if loop is true.
	milliseconds duration{ 1000 };

	/// @brief If true, the particle emitter will continuously emit particles in cycles of the given
	/// duration. If false, the particle emitter will only emit particles for one duration cycle and
	/// then stop.
	bool loop{ true };

	/// @brief If true, the particle emitter will immediately emit particles as if one full cycle
	/// has already passed.
	bool prewarm{ false };

	/// @brief The number of particles emitted per second.
	float rate_over_time{ 10.0f };

	PTGN_SERIALIZE(ParticleRate, duration, loop, prewarm, rate_over_time)
};

/// @brief A burst of particles emitted at once.
struct ParticleBurst {
	/// @brief The number of particles to emit in the burst.
	std::size_t particle_count{ 10 };

	/// @brief Number of times the burst should be emitted.
	std::size_t cycles{ 1 };

	/// @brief Time between consecutive cycles.
	milliseconds interval{ 1000 };

	PTGN_SERIALIZE(ParticleBurst, particle_count, cycles, interval)
};

using ParticleType = std::variant<Shape, std::string>;

using ParticleRateOrBurst = std::variant<ParticleRate, ParticleBurst>;

struct ParticleConfig {
	ParticleRateOrBurst rate_or_burst;

	/// @brief Time after which a particle despawns. If nullopt defaults to duration.
	std::optional<ConstantOrRange<milliseconds>> lifetime;

	ConstantOrRange<float> start_speed{ 1.0f };

	ConstantOrRange<float> start_size{ 1.0f };

	/// @brief Starting rotation of an individual particle.
	std::optional<ConstantOrRange<Degrees>> start_rotation;

	/// @brief If true, will attempt to align particles to their emission direction upon emission.
	/// This is overridden if start_rotation is set.
	bool align_to_direction{ true };

	ConstantOrRange<Color> start_color{ color::White };

	std::optional<V2_float> start_gravity;

	std::size_t max_particles{ 1000 };

	/// @brief Simulation speed multiplier.
	float simulation_speed{ 1.0f };

	ParticleType particle_type{ Rect{ V2_float{ 1.0f } } };

	FillStyle particle_fill_style{ Solid{} };

	EmissionShape emission_shape;

	std::optional<ConstantOrRange<V2_float>> velocity_over_lifetime;

	std::optional<ConstantOrRange<float>> size_over_lifetime;

	std::optional<ConstantOrRange<Color>> color_over_lifetime;

	PTGN_SERIALIZE(
		ParticleConfig, rate_or_burst, lifetime, start_speed, start_size, start_rotation,
		align_to_direction, start_color, start_gravity, max_particles, simulation_speed,
		particle_type, particle_fill_style, emission_shape, velocity_over_lifetime,
		size_over_lifetime, color_over_lifetime
	)
};

namespace impl {

enum class ParticleEmitterState {
	Stopped,
	Playing,
	Paused
};
PTGN_SERIALIZE_ENUM(ParticleEmitterState);

struct ParticleEmitterPlayback {
	ParticleEmitterState state{ ParticleEmitterState::Stopped };

	milliseconds elapsed{ 0 };
	milliseconds cycle_elapsed{ 0 };

	float spawn_accumulator{ 0.0f };

	milliseconds burst_elapsed{ 0 };
	std::size_t burst_cycles_emitted{ 0 };

	bool initialized{ false };

	void Start();

	void Update(ParticleEmitterComponent& emitter, const ParticleBurst& burst, milliseconds dt);
	void Update(ParticleEmitterComponent& emitter, const ParticleRate& rate, milliseconds dt);
};

struct ParticleEmitterComponent {
	ParticleEmitterComponent() = default;

	explicit ParticleEmitterComponent(const ParticleConfig& config);

	ParticleConfig config;
	ParticleEmitterPlayback playback;
	Manager manager;
	std::size_t live_particle_count{ 0 };

	/// @return Null entity if the particle emitter has reached its max particle count.
	Entity TrySpawnParticle();

	void Start();

	void Update(const ParticleEmitter& emitter, secondsf dt);

	PTGN_SERIALIZE(ParticleEmitterComponent, config)
};

} // namespace impl

class ParticleEmitter : public Entity {
public:
	ParticleEmitter() = default;
	explicit ParticleEmitter(Entity entity);

	static void Draw(DrawContext& renderer, Entity entity);

	ParticleEmitter& Start();
	ParticleEmitter& Stop();
	ParticleEmitter& Pause();
	ParticleEmitter& Resume();
	ParticleEmitter& Toggle();
	ParticleEmitter& Reset();

	template <typename F>
	ParticleEmitter& OnParticleDestroy(F&& callback) {
		AddScript<impl::EventScript<event::ParticleDestroyed>>(
			*this, impl::MakeEventCallback<event::ParticleDestroyed>(std::forward<F>(callback))
		);
		return *this;
	}

	[[nodiscard]] bool IsPlaying() const;
	[[nodiscard]] bool IsPaused() const;
	[[nodiscard]] bool IsStopped() const;

private:
	friend class Scene;

	static void Update(Scene& scene);
};

struct Particle {
	Particle() = default;

	explicit Particle(const ParticleConfig& config);

	V2_float position;
	V2_float velocity;
	V2_float gravity;

	Color start_color{ color::White };
	Color end_color{ color::White };
	Color color{ color::White };

	float start_size{ 1.0f };
	float end_size{ 1.0f };
	float size{ 1.0f };

	Radians rotation{ 0.0f };

	milliseconds age{ 0 };
	milliseconds lifetime{ 1000 };

	PTGN_SERIALIZE(
		Particle, position, velocity, gravity, start_color, end_color, color, start_size, end_size,
		size, rotation, age, lifetime
	)
private:
	friend struct impl::ParticleEmitterComponent;

	/// @return True if the particle died during the update, false otherwise.
	[[nodiscard]] bool Update(secondsf dt);

	void Prewarm(float simulation_speed);

	/// @return Progress of the particle's lifetime in the range [0.0, 1.0].
	float GetProgress() const;

	/// @brief Linearly interpolates the particle's properties based on its lifetime progress.
	void Lerp(float t);
};

ParticleEmitter CreateParticleEmitter(
	Scene& scene, V2_float position = {}, const ParticleConfig& config = {}
);

PTGN_REGISTER_DRAWABLE(ParticleEmitter);

} // namespace ptgn