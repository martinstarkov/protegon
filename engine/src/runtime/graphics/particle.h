#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

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
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class Scene;
class DrawContext;

template <typename T>
struct Range {
	T min;
	T max;
};

template <typename T>
using ConstantOrRange = std::variant<T, ptgn::Range<T>>;

template <typename T>
T Evaluate(const ConstantOrRange<T>& value) {
	return std::visit(
		[&]<typename V>(const V& v) -> T {
			if constexpr (std::is_same_v<V, T>) {
				return v;
			} else if constexpr (std::is_same_v<V, Range<T>>) {
				float t = Random01();
				return Lerp(v.min, v.max, t);
			}
		},
		value
	);
}

/// @brief The shape from which particles are emitted. Determines the initial position of emitted
/// particles.
class EmissionShape {
public:
	EmissionShape() = default;

	static EmissionShape Arc(Degrees arc_angle, float outer_radius, float inner_radius = 0.0f) {
		EmissionShape s;
		s.type = ArcShape{ arc_angle, outer_radius, inner_radius };
		return s;
	}

	static EmissionShape Rect(V2_float size) {
		EmissionShape s;
		s.type = ptgn::Rect{ size };
		return s;
	}

	// TODO: Move to private.
	struct ArcShape {
		Degrees arc_angle{ 360.0f };
		float outer_radius{ 1.0f };
		float inner_radius{ 0.0f };
	};

	std::variant<ArcShape, ptgn::Rect> type{};
};

/// @brief A rate of particle emission over time.
struct Rate {
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
	std::size_t rate_over_time{ 10 };
};

/// @brief A burst of particles emitted at once.
struct Burst {
	/// @brief The number of particles to emit in the burst.
	std::size_t particle_count{ 10 };

	/// @brief Number of times the burst should be emitted.
	std::size_t cycles{ 1 };

	/// @brief Time between consecutive cycles.
	milliseconds interval{ 1000 };
};

struct ParticleConfig {
	std::variant<Rate, Burst> rate_or_burst;

	/// @brief Time after which a particle despawns. If nullopt defaults to duration.
	std::optional<ConstantOrRange<milliseconds>> lifetime;

	ConstantOrRange<float> start_speed{ 1.0f };

	ConstantOrRange<float> start_size{ 1.0f };

	/// @brief Starting rotation of an individual particle.
	std::optional<ConstantOrRange<Degrees>> start_rotation;

	/// @brief If true, will attempt to align particles to their emission direction upon emission.
	/// This is overridden if start_rotation is set.
	bool align_to_direction{ false };

	ConstantOrRange<Color> start_color{ color::White };

	ConstantOrRange<V2_float> start_gravity{ V2_float{} };

	std::size_t max_particles{ 1000 };

	float simulation_speed{ 1.0f };

	std::variant<Shape, std::string> particle_type{ Rect{ V2_float{ 1.0f } } };

	FillStyle particle_fill_style{ FillStyle::Solid() };

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
};

struct Particle {
	V2_float position{};
	V2_float velocity{};
	V2_float gravity{};

	Color start_color{ color::White };
	Color end_color{ color::White };
	Color color{ color::White };

	float start_size{ 1.0f };
	float end_size{ 1.0f };
	float size{ 1.0f };

	Radians rotation{ 0.0f };

	milliseconds age{ 0 };
	milliseconds lifetime{ 1000 };
};

struct ParticleEmitterComponent {
	ParticleConfig config;
	ParticleEmitterPlayback playback;
	Manager manager;
	std::size_t live_particle_count{ 0 };
};

} // namespace impl

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

	[[nodiscard]] bool IsPlaying() const;
	[[nodiscard]] bool IsPaused() const;
	[[nodiscard]] bool IsStopped() const;

private:
	friend class Scene;

	static void Update(Scene& scene);
};

ParticleEmitter CreateParticleEmitter(
	Scene& scene, V2_float position = {}, const ParticleConfig& config = {}
);

PTGN_REGISTER_DRAWABLE(ParticleEmitter);

} // namespace ptgn