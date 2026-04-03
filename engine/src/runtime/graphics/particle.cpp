#include "runtime/graphics/particle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/math_utils.h"
#include "core/math/rng.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/time/timer.h"
#include "ecs/ecs.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/vertex.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace impl {

static V2_float SampleEmissionPosition(const EmissionShape& shape) {
	return std::visit(
		[]<typename V>(const V& s) {
			if constexpr (std::is_same_v<V, EmissionShape::ArcShape>) {
				Radians arc{ s.arc_angle };

				// Sample angle in arc

				auto angle{ Radians::Random(Radians{ 0.0f }, arc) };

				// Uniform area distribution:
				// r = sqrt(lerp(inner^2, outer^2))
				float r0 = s.inner_radius * s.inner_radius;
				float r1 = s.outer_radius * s.outer_radius;
				auto dist{ RandomFloat(r0, r1) };
				float r = std::sqrt(dist);

				float x = angle.Cos() * r;
				float y = angle.Sin() * r;

				return V2_float{ x, y };
			}

			else if constexpr (std::is_same_v<V, ptgn::Rect>) {
				auto half = s.GetSize() * 0.5f;

				return V2_float::Random(-half, half);
			}
		},
		shape.type
	);
}

static V2_float SampleEmissionDirection(const EmissionShape& shape) {
	return std::visit(
		[]<typename V>(const V& s) {
			if constexpr (std::is_same_v<V, EmissionShape::ArcShape>) {
				Radians arc{ s.arc_angle };

				auto angle{ Radians::Random(Radians{ 0.0f }, arc) };

				V2_float dir{ 1.0f, 0.0f };

				return dir.Rotated(angle);
			}

			else if constexpr (std::is_same_v<V, ptgn::Rect>) {
				// Default: random direction
				auto angle{ Radians::Random() };

				V2_float dir{ 1.0f, 0.0f };

				return dir.Rotated(angle);
			}
		},
		shape.type
	);
}

static Particle* TrySpawnParticle(ParticleEmitterComponent& emitter) {
	if (emitter.live_particle_count >= emitter.config.max_particles) {
		return nullptr;
	}

	Entity particle_entity = emitter.manager.CreateEntity();
	Particle& p			   = particle_entity.Add<Particle>();
	++emitter.live_particle_count;

	const auto& config = emitter.config;

	const V2_float emission_offset = SampleEmissionPosition(config.emission_shape);
	const V2_float dir			   = SampleEmissionDirection(config.emission_shape);
	const float speed			   = Evaluate(config.start_speed);

	p.position = emission_offset;
	p.velocity = dir * speed;
	p.gravity  = Evaluate(config.start_gravity);
	p.size	   = Evaluate(config.start_size);
	p.color	   = Evaluate(config.start_color);

	if (config.start_rotation) {
		p.rotation = Evaluate(*config.start_rotation).ToRad();
	} else if (config.align_to_direction) {
		p.rotation = dir.Angle().ToRad();
	}

	if (config.lifetime) {
		p.lifetime = Evaluate(*config.lifetime);
	} else if (std::holds_alternative<Rate>(config.rate_or_burst)) {
		p.lifetime = std::get<Rate>(config.rate_or_burst).duration;
	} else {
		p.lifetime = milliseconds{ 1000 };
	}

	p.start_size = Evaluate(config.start_size);
	p.size		 = p.start_size;

	if (config.size_over_lifetime) {
		const auto size_value = Evaluate(*config.size_over_lifetime);
		p.end_size			  = size_value;
	} else {
		p.end_size = p.start_size;
	}

	p.start_color = Evaluate(config.start_color);
	p.color		  = p.start_color;

	if (config.color_over_lifetime) {
		p.end_color = Evaluate(*config.color_over_lifetime);
	} else {
		p.end_color = p.start_color;
	}

	return &p;
}

static void UpdateRateEmitter(ParticleEmitterComponent& emitter, milliseconds dt) {
	auto& playback	 = emitter.playback;
	const auto& rate = std::get<Rate>(emitter.config.rate_or_burst);

	if (playback.state != ParticleEmitterState::Playing) {
		return;
	}

	const float dt_seconds		= duration<float>(dt).count();
	playback.spawn_accumulator += rate.rate_over_time * dt_seconds;

	std::size_t to_spawn		= static_cast<std::size_t>(playback.spawn_accumulator);
	playback.spawn_accumulator -= static_cast<float>(to_spawn);

	for (std::size_t i = 0; i < to_spawn; ++i) {
		if (!TrySpawnParticle(emitter)) {
			break;
		}
	}
}

static void UpdateBurstEmitter(ParticleEmitterComponent& emitter, milliseconds dt) {
	auto& playback	  = emitter.playback;
	const auto& burst = std::get<Burst>(emitter.config.rate_or_burst);

	if (playback.state != ParticleEmitterState::Playing) {
		return;
	}

	playback.elapsed	   += dt;
	playback.burst_elapsed += dt;

	while (playback.burst_cycles_emitted < burst.cycles && playback.burst_elapsed >= burst.interval
	) {
		playback.burst_elapsed -= burst.interval;

		for (std::size_t i = 0; i < burst.particle_count; ++i) {
			if (!TrySpawnParticle(emitter)) {
				break;
			}
		}

		++playback.burst_cycles_emitted;
	}

	if (playback.burst_cycles_emitted >= burst.cycles) {
		playback.state = ParticleEmitterState::Stopped;
	}
}

static void InitializeEmitterRun(ParticleEmitterComponent& emitter) {
	auto& playback	   = emitter.playback;
	const auto& config = emitter.config;

	playback.elapsed			  = milliseconds{ 0 };
	playback.cycle_elapsed		  = milliseconds{ 0 };
	playback.spawn_accumulator	  = 0.0f;
	playback.burst_elapsed		  = milliseconds{ 0 };
	playback.burst_cycles_emitted = 0;
	playback.initialized		  = true;

	// Starting a new run should not keep old particles around unless that is
	// explicitly the behavior you want.
	emitter.manager.Clear();
	emitter.live_particle_count = 0;

	if (!std::holds_alternative<Rate>(config.rate_or_burst)) {
		return;
	}

	const auto& rate = std::get<Rate>(config.rate_or_burst);
	if (!rate.prewarm) {
		return;
	}
	// Prewarm means: make the emitter look like it has already been running
	// for one full cycle.
	const float duration_seconds = std::chrono::duration<float>(rate.duration).count();

	const auto prewarm_count = static_cast<std::size_t>(rate.rate_over_time * duration_seconds);

	for (std::size_t i = 0; i < prewarm_count; ++i) {
		Particle* p = TrySpawnParticle(emitter);
		if (!p) {
			break;
		}

		if (p->lifetime.count() > 0) {
			const auto max_age = static_cast<float>(p->lifetime.count());
			p->age = milliseconds{ static_cast<milliseconds::rep>(RandomFloat(0.0f, max_age)) };

			const float dt = std::chrono::duration<float>(p->age).count() * config.simulation_speed;

			p->velocity += p->gravity * dt;
			p->position += p->velocity * dt;
		}
	}
}

static void UpdateParticles(ParticleEmitterComponent& emitter, milliseconds dt) {
	const auto& config = emitter.config;
	const float sim_dt = std::chrono::duration<float>(dt).count() * config.simulation_speed;

	std::vector<Entity> dead_particles;

	for (auto [entity, p] : emitter.manager.EntitiesWith<Particle>()) {
		p.age += dt;

		if (p.age >= p.lifetime) {
			dead_particles.push_back(entity);
			return;
		}

		const float t =
			(p.lifetime.count() > 0)
				? std::clamp(
					  static_cast<float>(p.age.count()) / static_cast<float>(p.lifetime.count()),
					  0.0f, 1.0f
				  )
				: 1.0f;

		p.velocity += p.gravity * sim_dt;
		p.position += p.velocity * sim_dt;

		p.size	= Lerp(p.start_size, p.end_size, t);
		p.color = Lerp(p.start_color, p.end_color, t);
	}

	for (Entity entity : dead_particles) {
		entity.Destroy();
		--emitter.live_particle_count;
	}

	emitter.manager.Refresh();
}

static void UpdateEmitterPlayback(ParticleEmitterComponent& emitter, milliseconds dt) {
	if (emitter.playback.state != ParticleEmitterState::Playing) {
		return;
	}

	if (std::holds_alternative<Rate>(emitter.config.rate_or_burst)) {
		auto& playback	 = emitter.playback;
		const auto& rate = std::get<Rate>(emitter.config.rate_or_burst);

		playback.elapsed	   += dt;
		playback.cycle_elapsed += dt;

		if (playback.cycle_elapsed >= rate.duration) {
			if (rate.loop) {
				playback.cycle_elapsed %= rate.duration;
			} else {
				playback.state = ParticleEmitterState::Stopped;
				return;
			}
		}

		UpdateRateEmitter(emitter, dt);
	} else {
		UpdateBurstEmitter(emitter, dt);
	}
}

} // namespace impl

ParticleEmitter::ParticleEmitter(Entity entity) : Entity{ entity } {}

ParticleEmitter& ParticleEmitter::Start() {
	auto& emitter = Get<impl::ParticleEmitterComponent>();

	if (emitter.playback.state == impl::ParticleEmitterState::Paused) {
		emitter.playback.state = impl::ParticleEmitterState::Playing;
		return *this;
	}

	if (emitter.playback.state == impl::ParticleEmitterState::Stopped) {
		InitializeEmitterRun(emitter);
		emitter.playback.state = impl::ParticleEmitterState::Playing;
	}

	return *this;
}

ParticleEmitter& ParticleEmitter::Stop() {
	auto& emitter		   = Get<impl::ParticleEmitterComponent>();
	emitter.playback.state = impl::ParticleEmitterState::Stopped;
	return *this;
}

ParticleEmitter& ParticleEmitter::Pause() {
	auto& emitter = Get<impl::ParticleEmitterComponent>();
	if (emitter.playback.state == impl::ParticleEmitterState::Playing) {
		emitter.playback.state = impl::ParticleEmitterState::Paused;
	}
	return *this;
}

ParticleEmitter& ParticleEmitter::Resume() {
	auto& emitter = Get<impl::ParticleEmitterComponent>();
	if (emitter.playback.state == impl::ParticleEmitterState::Paused) {
		emitter.playback.state = impl::ParticleEmitterState::Playing;
	}
	return *this;
}

ParticleEmitter& ParticleEmitter::Toggle() {
	auto& emitter = Get<impl::ParticleEmitterComponent>();

	switch (emitter.playback.state) {
		case impl::ParticleEmitterState::Stopped: Start(); break;
		case impl::ParticleEmitterState::Playing: Pause(); break;
		case impl::ParticleEmitterState::Paused:  Resume(); break;
	}

	return *this;
}

ParticleEmitter& ParticleEmitter::Reset() {
	auto& emitter	 = Get<impl::ParticleEmitterComponent>();
	emitter.playback = {};
	emitter.manager.Clear();
	emitter.live_particle_count = 0;
	return *this;
}

void ParticleEmitter::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	auto depth{ GetDepth(entity) };
	auto blend_mode{ GetBlendMode(entity) };

	const auto& emitter = entity.Get<impl::ParticleEmitterComponent>();
	const auto& config	= emitter.config;

	const Transform base_transform{ GetWorldTransform(entity) };

	for (const auto& [particle_entity, p] :
		 std::as_const(emitter.manager).EntitiesWith<impl::Particle>()) {
		Transform transform = base_transform;
		transform.Translate(p.position);
		transform.Rotate(p.rotation);

		if (std::holds_alternative<std::string>(config.particle_type)) {
			const auto& texture_key = std::get<std::string>(config.particle_type);

			const auto& scene = entity.GetScene();
			PTGN_ASSERT(
				scene.ctx().asset.HasTexture(texture_key),
				"Texture key must be loaded in the asset manager before drawing a particle"
			);

			Texture texture{ *scene.ctx().asset.GetTexture(texture_key) };

			auto origin{ Origin::Center };
			auto tex_coords{ impl::GetDefaultTextureCoordinates<false>() };

			renderer.DrawTexture(
				texture, transform, V2_float{ p.size }, origin, p.color, depth, tex_coords,
				blend_mode
			);
		} else {
			const auto& shape = std::get<Shape>(config.particle_type);
			auto origin{ Origin::Center };

			shape.Visit([&]<typename S>(const S& s) {
				if constexpr (std::is_same_v<S, Circle>) {
					Circle circle{ p.size * 0.5f };
					renderer.DrawShape(
						circle, transform, p.color, config.particle_fill_style, origin, depth,
						blend_mode
					);
				} else if constexpr (std::is_same_v<S, Rect>) {
					Rect rect{ V2_float{ p.size } };
					renderer.DrawShape(
						rect, transform, p.color, config.particle_fill_style, origin, depth,
						blend_mode
					);
				} else {
					// Fallback: draw the stored shape as-is if it is already scaled elsewhere.
					renderer.DrawShape(
						s, transform, p.color, config.particle_fill_style, origin, depth, blend_mode
					);
				}
			});
		}
	}
}

void ParticleEmitter::Update(Scene& scene) {
	auto dt{ duration_cast<milliseconds>(scene.ctx().dt()) };
	for (auto [entity, emitter] : scene.EntitiesWith<impl::ParticleEmitterComponent>()) {
		// 1. Update emission (spawn new particles)
		UpdateEmitterPlayback(emitter, dt);

		// 2. Update existing particles (movement, lifetime, etc.)
		if (emitter.playback.state != impl::ParticleEmitterState::Paused) {
			UpdateParticles(emitter, dt);
		}
	}

	scene.Refresh();
}

ParticleEmitter CreateParticleEmitter(
	Scene& scene, V2_float position, const ParticleConfig& config
) {
	ParticleEmitter particle{ scene.CreateEntity() };
	SetPosition(particle, position);

	SetDraw<ParticleEmitter>(particle);
	auto& emitter{ particle.Add<impl::ParticleEmitterComponent>() };

	emitter.config = config;

	Show(particle, false);

	return particle;
}

} // namespace ptgn