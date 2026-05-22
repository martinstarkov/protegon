#include "runtime/graphics/fx/particle.h"

#include <ecs/ecs.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/math_utils.h"
#include "core/math/rng.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/resources/texture.h"
#include "renderer/vertex/vertex.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/particle_event.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event.h"

namespace ptgn {

namespace {

struct ParticleDrawInfo {
	Transform transform;
	float size{ 0.0f };
	Color color;
	FillStyle fill_style;
	float depth{ 0.0f };
	BlendMode blend_mode{ BlendMode::Blend };
	Origin origin{ Origin::Center };
};

template <ShapeType T>
void DrawParticleShape(DrawContext& ctx, const T& shape, const ParticleDrawInfo& draw) {
	ctx.WithBlendMode(draw.blend_mode, [&]() {
		constexpr auto entity_id{ -1 };

		if constexpr (std::is_same_v<T, Circle>) {
			Circle circle{ shape.GetRadius() * draw.size * 0.5f };
			ctx.DrawShape(
				circle, draw.transform, draw.depth, draw.color, draw.fill_style, draw.origin,
				entity_id
			);
		} else if constexpr (std::is_same_v<T, Rect>) {
			Rect rect{ shape.GetSize() * V2_float{ draw.size } };

			Transform transform{ draw.transform };
			// We rotate rectangle particle -90 degrees because the default direction of the
			// rectangle shape is down (90 degrees).
			transform.Rotate(-Radians{ kHalfPi });

			ctx.DrawShape(
				rect, transform, draw.depth, draw.color, draw.fill_style, draw.origin, entity_id
			);
		} else {
			ctx.DrawShape(
				shape, draw.transform, draw.depth, draw.color, draw.fill_style, draw.origin,
				entity_id
			);
		}
	});
}

template <typename T>
void DrawParticleType(
	const AssetManager& assets, DrawContext& ctx, const T& particle_type,
	const ParticleDrawInfo& draw
) {
	if constexpr (std::is_same_v<T, std::string>) {
		Texture texture{ assets.Get<Texture>(particle_type) };

		constexpr auto tex_coords{ impl::GetDefaultTextureCoordinates<false>() };
		impl::EffectParams effects{};

		constexpr auto entity_id{ -1 };

		ctx.WithBlendMode(draw.blend_mode, [&]() {
			ctx.DrawTexture(
				texture, draw.transform, draw.depth, V2_float{ draw.size }, draw.origin, draw.color,
				tex_coords, effects, entity_id
			);
		});
	} else if constexpr (std::is_same_v<T, Shape>) {
		particle_type.Visit([&ctx, &draw]<typename S>(const S& shape) {
			DrawParticleShape(ctx, shape, draw);
		});
	} else {
		static_assert(false, "Incomplete visitor");
	}
}

} // namespace

EmissionShape::EmissionSample EmissionShape::SampleEmission() const {
	return std::visit(
		[]<typename V>(const V& s) -> EmissionSample {
			if constexpr (std::is_same_v<V, EmissionShapeArc>) {
				Radians half_arc{ s.arc_angle * 0.5f };
				auto offset{ Radians::Random(-half_arc, half_arc) };

				V2_float direction{ s.direction.IsZero() ? V2_float{ 1.0f, 0.0f } : s.direction };

				V2_float dir = direction.Normalized().Rotated(offset);

				float inner{ std::max(0.0f, std::min(s.inner_radius, s.outer_radius)) };
				float outer{ std::max(inner, s.outer_radius) };

				float r0{ inner * inner };
				float r1{ outer * outer };
				float r{ std::sqrt(RandomFloat(r0, r1)) };

				return { dir * r, dir };
			} else if constexpr (std::is_same_v<V, EmissionShapeRect>) {
				auto half{ s.rect.GetSize() * 0.5f };
				V2_float direction{ s.direction.IsZero() ? V2_float{ 0.0f, 1.0f } : s.direction };
				return { V2_float::Random(-half, half), direction.Normalized() };
			}
		},
		type_
	);
}

namespace impl {

void ParticleEmitterPlayback::Start() {
	elapsed				 = 0ms;
	cycle_elapsed		 = 0ms;
	spawn_accumulator	 = 0.0f;
	burst_elapsed		 = 0ms;
	burst_cycles_emitted = 0;
	initialized			 = true;
}

void ParticleEmitterPlayback::Update(
	ParticleEmitterComponent& emitter, const ParticleRate& rate, secondsf dt
) {
	elapsed		  += dt;
	cycle_elapsed += dt;

	if (auto duration{ duration_cast<secondsf>(rate.duration) }; cycle_elapsed >= duration) {
		if (rate.loop) {
			cycle_elapsed -= duration * static_cast<int>(cycle_elapsed / duration);

		} else {
			state = ParticleEmitterState::Stopped;
			return;
		}
	}

	if (state != ParticleEmitterState::Playing) {
		return;
	}

	spawn_accumulator += rate.rate_over_time * dt.count();

	PTGN_ASSERT(spawn_accumulator >= 0.0f);

	auto to_spawn{ static_cast<std::size_t>(spawn_accumulator) };
	spawn_accumulator -= static_cast<float>(to_spawn);

	spawn_accumulator = std::max(0.0f, spawn_accumulator);

	for (auto i{ 0uz }; i < to_spawn; ++i) {
		if (!emitter.TrySpawnParticle()) {
			// Reached max particles, stop trying to spawn more this frame.
			break;
		}
	}
}

void ParticleEmitterPlayback::Update(
	ParticleEmitterComponent& emitter, const ParticleBurst& burst, secondsf dt
) {
	if (state != ParticleEmitterState::Playing) {
		return;
	}

	elapsed		  += dt;
	burst_elapsed += dt;

	while (burst_cycles_emitted < burst.cycles && burst_elapsed >= burst.interval) {
		burst_elapsed -= burst.interval;

		for (auto i{ 0uz }; i < burst.particle_count; ++i) {
			if (!emitter.TrySpawnParticle()) {
				break;
			}
		}

		++burst_cycles_emitted;
	}

	if (burst_cycles_emitted >= burst.cycles) {
		state = ParticleEmitterState::Stopped;
	}
}

ParticleEmitterComponent::ParticleEmitterComponent(const ParticleConfig& config) :
	config{ config } {}

Entity ParticleEmitterComponent::TrySpawnParticle() {
	if (live_particle_count >= config.max_particles) {
		return {};
	}

	auto particle_entity{ manager.CreateEntity() };
	particle_entity.Add<Particle>(config);
	++live_particle_count;

	return particle_entity;
}

void ParticleEmitterComponent::Start() {
	playback.Start();

	// Starting a new run should not keep old particles around unless that is
	// explicitly the behavior you want.
	manager.Clear();
	live_particle_count = 0;

	if (std::holds_alternative<ParticleBurst>(config.rate_or_burst)) {
		playback.burst_elapsed = std::get<ParticleBurst>(config.rate_or_burst).interval;
	}

	if (!std::holds_alternative<ParticleRate>(config.rate_or_burst)) {
		return;
	}

	const auto& rate{ std::get<ParticleRate>(config.rate_or_burst) };

	if (!rate.prewarm) {
		return;
	}

	// Prewarm means: make the emitter look like it has already been running
	// for one full cycle.
	auto cycle_particles{ rate.rate_over_time * duration_cast<secondsf>(rate.duration).count() };

	PTGN_ASSERT(cycle_particles >= 0.0f);

	auto prewarm_count{ static_cast<std::size_t>(cycle_particles) };

	for (auto i{ 0uz }; i < prewarm_count; ++i) {
		auto particle_entity{ TrySpawnParticle() };

		if (!particle_entity) {
			break;
		}

		PTGN_ASSERT(particle_entity.Has<Particle>());

		auto& particle{ particle_entity.Get<Particle>() };

		particle.Prewarm(config.simulation_speed);
	}
}

void ParticleEmitterComponent::Update(const ParticleEmitter& emitter, secondsf dt) {
	if (playback.state == impl::ParticleEmitterState::Playing) {
		// Update emission (spawn new particles).
		std::visit(
			[&](const auto& rate_or_burst) { playback.Update(*this, rate_or_burst, dt); },
			config.rate_or_burst
		);
	}

	if (playback.state == impl::ParticleEmitterState::Paused) {
		return;
	}

	auto simulated_dt{ dt * config.simulation_speed };

	std::vector<Entity> dead_particles;
	dead_particles.reserve(live_particle_count);

	// Update existing particles (movement, lifetime, etc.).
	for (auto [entity, particle] : manager.EntitiesWith<Particle>()) {
		bool died{ particle.Update(simulated_dt) };
		if (died) {
			dead_particles.emplace_back(entity);
			PushEvent<event::ParticleDestroyed>(emitter, emitter, particle);
		}
	}

	for (Entity entity : dead_particles) {
		entity.Destroy();
		--live_particle_count;
	}

	manager.Refresh();
}

} // namespace impl

Particle::Particle(const ParticleConfig& config) {
	auto sample{ config.emission_shape.SampleEmission() };
	auto speed{ config.start_speed.Evaluate() };

	position	= sample.position;
	velocity	= sample.direction * speed;
	gravity		= config.start_gravity.value_or(V2_float{});
	start_size	= config.start_size.Evaluate();
	size		= start_size;
	start_color = config.start_color.Evaluate();
	color		= start_color;

	if (config.start_rotation) {
		rotation = config.start_rotation->Evaluate().ToRad();
	} else if (config.align_to_direction) {
		rotation = sample.direction.Angle().ToRad();
	}

	if (config.lifetime) {
		lifetime = config.lifetime->Evaluate();
	} else if (std::holds_alternative<ParticleRate>(config.rate_or_burst)) {
		lifetime = std::get<ParticleRate>(config.rate_or_burst).duration;
	}

	const auto optional_range_or = [](const auto& range, const auto& constant) {
		if (range.has_value()) {
			return range->Evaluate();
		} else {
			return constant;
		}
	};

	end_size  = optional_range_or(config.size_over_lifetime, start_size);
	end_color = optional_range_or(config.color_over_lifetime, start_color);
}

float Particle::GetProgress() const {
	if (lifetime.count() <= 0) {
		return 1.0f;
	}
	return Clamp01(age.count() / static_cast<float>(lifetime.count()));
}

void Particle::Lerp(float t) {
	size  = ptgn::Lerp(start_size, end_size, t);
	color = ptgn::Lerp(start_color, end_color, t);
}

bool Particle::Update(secondsf dt) {
	age += dt;

	if (age >= lifetime) {
		return true;
	}

	auto elapsed{ GetProgress() };

	velocity += gravity * dt.count();
	position += velocity * dt.count();

	Lerp(elapsed);

	return false;
}

void Particle::Prewarm(float simulation_speed) {
	if (lifetime.count() <= 0) {
		return;
	}

	age = RandomDuration(0ms, lifetime);

	auto elapsed{ GetProgress() };

	float time{ age.count() * simulation_speed };

	auto initial_velocity{ velocity };

	position += initial_velocity * time + 0.5f * gravity * time * time;
	velocity  = initial_velocity + gravity * time;

	Lerp(elapsed);
}

ParticleEmitter::ParticleEmitter(Entity entity) : Entity{ entity } {}

ParticleEmitter& ParticleEmitter::Start() {
	using enum impl::ParticleEmitterState;

	auto& emitter{ Get<impl::ParticleEmitterComponent>() };

	if (emitter.playback.state == Paused) {
		emitter.playback.state = Playing;
		return *this;
	}

	if (emitter.playback.state == Stopped) {
		emitter.Start();
		emitter.playback.state = Playing;
	}

	return *this;
}

ParticleEmitter& ParticleEmitter::Stop() {
	auto& emitter{ Get<impl::ParticleEmitterComponent>() };
	emitter.playback.state = impl::ParticleEmitterState::Stopped;
	return *this;
}

ParticleEmitter& ParticleEmitter::Pause() {
	if (auto& emitter{ Get<impl::ParticleEmitterComponent>() };
		emitter.playback.state == impl::ParticleEmitterState::Playing) {
		emitter.playback.state = impl::ParticleEmitterState::Paused;
	}
	return *this;
}

ParticleEmitter& ParticleEmitter::Resume() {
	if (auto& emitter{ Get<impl::ParticleEmitterComponent>() };
		emitter.playback.state == impl::ParticleEmitterState::Paused) {
		emitter.playback.state = impl::ParticleEmitterState::Playing;
	}
	return *this;
}

ParticleEmitter& ParticleEmitter::Toggle() {
	switch (const auto& emitter{ Get<impl::ParticleEmitterComponent>() }; emitter.playback.state) {
		using enum impl::ParticleEmitterState;
		case Stopped: Start(); break;
		case Playing: Pause(); break;
		case Paused:  Resume(); break;
	}

	return *this;
}

ParticleEmitter& ParticleEmitter::Reset() {
	auto& emitter{ Get<impl::ParticleEmitterComponent>() };
	emitter.playback = {};
	emitter.manager.Clear();
	emitter.live_particle_count = 0;
	return *this;
}

bool ParticleEmitter::IsPlaying() const {
	return Get<impl::ParticleEmitterComponent>().playback.state ==
		   impl::ParticleEmitterState::Playing;
}

bool ParticleEmitter::IsPaused() const {
	return Get<impl::ParticleEmitterComponent>().playback.state ==
		   impl::ParticleEmitterState::Paused;
}

bool ParticleEmitter::IsStopped() const {
	return Get<impl::ParticleEmitterComponent>().playback.state ==
		   impl::ParticleEmitterState::Stopped;
}

void ParticleEmitter::Draw(DrawContext& ctx, Entity entity) {
	auto depth{ GetDepth(entity) };
	auto blend_mode{ GetBlendMode(entity) };

	const auto& emitter{ entity.Get<impl::ParticleEmitterComponent>() };

	const auto& scene{ entity.GetScene() };
	const auto& assets{ scene.ctx().asset };

	const Transform base_transform{ GetDrawTransform(entity) };

	for (const auto& [particle_entity, particle] :
		 std::as_const(emitter.manager).EntitiesWith<Particle>()) {
		Transform transform{ base_transform };
		transform.Translate(particle.position);
		transform.Rotate(particle.rotation);

		std::visit(
			[&]<typename T>(const T& type) {
				DrawParticleType(
					assets, ctx, type,
					{ .transform  = transform,
					  .size		  = particle.size,
					  .color	  = particle.color,
					  .fill_style = emitter.config.particle_fill_style,
					  .depth	  = depth,
					  .blend_mode = blend_mode }
				);
			},
			emitter.config.particle_type
		);
	}
}

void ParticleEmitter::Update(Scene& scene, secondsf dt) {
	for (auto [entity, emitter] : scene.EntitiesWith<impl::ParticleEmitterComponent>()) {
		emitter.Update(ParticleEmitter{ entity }, dt);
	}
}

ParticleEmitter CreateParticleEmitter(
	Scene& scene, V2_float position, const ParticleConfig& config
) {
	ParticleEmitter particle{ scene.CreateEntity() };
	SetPosition(particle, position);

	SetDraw<ParticleEmitter>(particle);
	particle.Add<impl::ParticleEmitterComponent>(config);

	Show(particle, false);

	return particle;
}

} // namespace ptgn