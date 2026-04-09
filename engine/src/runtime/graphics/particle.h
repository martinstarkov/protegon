#pragma once

#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "core/event/event.h"
#include "core/math/angle.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/math_utils.h"
#include "core/math/rng.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/event/event_dispatcher.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/scripting/script.h"

namespace ptgn {

class Scene;
class DrawContext;
class ParticleEmitter;

namespace impl {

struct ParticleEmitterComponent;

} // namespace impl

template <typename T>
struct Range {
	constexpr Range() = delete;

	constexpr Range(T min, T max) : min{ min }, max{ max } {}

	T min;
	T max;
};

template <typename T>
struct ConstantOrRange {
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

private:
	std::variant<T, Range<T>> value_;
};

/// @brief The shape from which particles are emitted. Determines the initial position of emitted
/// particles.
class EmissionShape {
public:
	struct EmissionSample {
		V2_float position;
		V2_float direction;
	};

	constexpr EmissionShape() = default;

	[[nodiscard]] constexpr static EmissionShape Arc(
		Degrees arc_angle, float outer_radius, V2_float direction = V2_float{ 1.0f, 0.0f },
		float inner_radius = 0.0f
	) {
		EmissionShape s;
		s.type_ = ArcShape{ arc_angle, outer_radius, direction, inner_radius };
		return s;
	}

	[[nodiscard]] constexpr static EmissionShape Rect(
		V2_float size, V2_float direction = V2_float{ 0.0f, 1.0f }
	) {
		EmissionShape s;
		s.type_ = RectShape{ size, direction };
		return s;
	}

	[[nodiscard]] EmissionSample SampleEmission() const;

private:
	struct ArcShape {
		Degrees arc_angle{ 360.0f };
		float outer_radius{ 1.0f };
		V2_float direction{ 1.0f, 0.0f };
		float inner_radius{ 0.0f };
	};

	struct RectShape {
		ptgn::Rect rect{ V2_float{ 1.0f } };
		V2_float direction{ 0.0f, 1.0f };
	};

	std::variant<ArcShape, RectShape> type_{};
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
};

/// @brief A burst of particles emitted at once.
struct ParticleBurst {
	/// @brief The number of particles to emit in the burst.
	std::size_t particle_count{ 10 };

	/// @brief Number of times the burst should be emitted.
	std::size_t cycles{ 1 };

	/// @brief Time between consecutive cycles.
	milliseconds interval{ 1000 };
};

struct ParticleConfig {
	std::variant<ParticleRate, ParticleBurst> rate_or_burst;

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

	std::variant<Shape, std::string> particle_type{ Rect{ V2_float{ 1.0f } } };

	FillStyle particle_fill_style{ Solid{} };

	EmissionShape emission_shape;

	std::optional<ConstantOrRange<V2_float>> velocity_over_lifetime;

	std::optional<ConstantOrRange<float>> size_over_lifetime;

	std::optional<ConstantOrRange<Color>> color_over_lifetime;
};

namespace impl {

enum class ParticleEmitterState {
	Stopped,
	Playing,
	Paused
};

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

	std::optional<Entity> TrySpawnParticle();

	void Start();

	void Update(const ParticleEmitter& emitter, secondsf dt);
};

} // namespace impl

namespace event {

struct ParticleDestroyed;

} // namespace event

class ParticleEmitter : public Entity {
public:
	ParticleEmitter() = default;
	explicit ParticleEmitter(Entity entity);

	static void Draw(DrawContext& renderer, Entity entity, Camera camera);

	ParticleEmitter& Start();
	ParticleEmitter& Stop();
	ParticleEmitter& Pause();
	ParticleEmitter& Resume();
	ParticleEmitter& Toggle();
	ParticleEmitter& Reset();

	using DestroyCallback =
		std::variant<std::function<void()>, std::function<void(event::ParticleDestroyed)>>;

	ParticleEmitter& OnParticleDestroy(const DestroyCallback& callback);

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

namespace event {

/// @brief Triggered when a particle is destroyed after reaching the end of its lifetime.
struct ParticleDestroyed : public Event<ParticleDestroyed> {
	ParticleDestroyed() = default;

	ParticleDestroyed(const ParticleEmitter& emitter, const Particle& particle);

	ParticleEmitter emitter;
	Particle particle;
};

} // namespace event

namespace impl {

struct ParticleDestroyScript : public Script {
	ParticleDestroyScript() = default;

	explicit ParticleDestroyScript(const ParticleEmitter::DestroyCallback& callback);

	void OnEvent(EventDispatcher dispatcher) override;

private:
	ParticleEmitter::DestroyCallback callback_;
};

} // namespace impl

ParticleEmitter CreateParticleEmitter(
	Scene& scene, V2_float position = {}, const ParticleConfig& config = {}
);

PTGN_REGISTER_DRAWABLE(ParticleEmitter);

} // namespace ptgn