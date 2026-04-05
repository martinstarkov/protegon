#include "runtime/graphics/particle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/event/dispatcher.h"
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
#include "runtime/scripting/scripts.h"

namespace ptgn {

EmissionShape::EmissionSample EmissionShape::SampleEmission() const {
	return std::visit(
		[]<typename V>(const V& s) -> EmissionSample {
			if constexpr (std::is_same_v<V, EmissionShape::ArcShape>) {
				Radians half_arc = Radians{ s.arc_angle } * 0.5f;
				Radians offset	 = Radians::Random(-half_arc, half_arc);

				V2_float direction{ s.direction.IsZero() ? V2_float{ 1.0f, 0.0f } : s.direction };

				V2_float dir = direction.Normalized().Rotated(offset);

				float inner = std::max(0.0f, std::min(s.inner_radius, s.outer_radius));
				float outer = std::max(inner, s.outer_radius);

				float r0 = inner * inner;
				float r1 = outer * outer;
				float r	 = std::sqrt(RandomFloat(r0, r1));

				return { dir * r, dir };
			} else if constexpr (std::is_same_v<V, EmissionShape::RectShape>) {
				auto half = s.rect.GetSize() * 0.5f;
				V2_float direction{ s.direction.IsZero() ? V2_float{ 0.0f, 1.0f } : s.direction };
				return { V2_float::Random(-half, half), direction.Normalized() };
			}
		},
		type_
	);
}

namespace impl {

static std::optional<Entity> TrySpawnParticle(ParticleEmitterComponent& emitter) {
	if (emitter.live_particle_count >= emitter.config.max_particles) {
		return std::nullopt;
	}

	Entity particle_entity = emitter.manager.CreateEntity();
	Particle& p			   = particle_entity.Add<Particle>();
	++emitter.live_particle_count;

	const auto& config = emitter.config;

	auto sample = config.emission_shape.SampleEmission();
	float speed = config.start_speed.Evaluate();

	p.position = sample.position;
	p.velocity = sample.direction * speed;
	p.gravity  = config.start_gravity.value_or(V2_float{});
	p.size	   = config.start_size.Evaluate();
	p.color	   = config.start_color.Evaluate();

	float start_size  = config.start_size.Evaluate();
	Color start_color = config.start_color.Evaluate();

	p.start_size = start_size;
	p.size		 = start_size;

	p.start_color = start_color;
	p.color		  = start_color;

	if (config.start_rotation) {
		p.rotation = config.start_rotation->Evaluate().ToRad();
	} else if (config.align_to_direction) {
		p.rotation = sample.direction.Angle().ToRad();
	}

	if (config.lifetime) {
		p.lifetime = config.lifetime->Evaluate();
	} else if (std::holds_alternative<Rate>(config.rate_or_burst)) {
		p.lifetime = std::get<Rate>(config.rate_or_burst).duration;
	}

	if (config.size_over_lifetime) {
		auto size_value = config.size_over_lifetime->Evaluate();
		p.end_size		= size_value;
	} else {
		p.end_size = p.start_size;
	}

	if (config.color_over_lifetime) {
		p.end_color = config.color_over_lifetime->Evaluate();
	} else {
		p.end_color = p.start_color;
	}

	return particle_entity;
}

static void UpdateRateEmitter(ParticleEmitterComponent& emitter, milliseconds dt) {
	auto& playback	 = emitter.playback;
	const auto& rate = std::get<Rate>(emitter.config.rate_or_burst);

	if (playback.state != ParticleEmitterState::Playing) {
		return;
	}

	float dt_seconds			= duration<float>(dt).count();
	playback.spawn_accumulator += static_cast<float>(rate.rate_over_time) * dt_seconds;

	auto to_spawn				= static_cast<std::size_t>(playback.spawn_accumulator);
	playback.spawn_accumulator -= static_cast<float>(to_spawn);

	for (std::size_t i = 0; i < to_spawn; ++i) {
		if (!TrySpawnParticle(emitter).has_value()) {
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
			if (!TrySpawnParticle(emitter).has_value()) {
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

	if (std::holds_alternative<Burst>(config.rate_or_burst)) {
		playback.burst_elapsed = std::get<Burst>(config.rate_or_burst).interval;
	}

	if (!std::holds_alternative<Rate>(config.rate_or_burst)) {
		return;
	}

	const auto& rate = std::get<Rate>(config.rate_or_burst);
	if (!rate.prewarm) {
		return;
	}
	// Prewarm means: make the emitter look like it has already been running
	// for one full cycle.
	float duration_seconds = std::chrono::duration<float>(rate.duration).count();

	auto prewarm_count =
		static_cast<std::size_t>(static_cast<float>(rate.rate_over_time) * duration_seconds);

	for (std::size_t i = 0; i < prewarm_count; ++i) {
		auto particle_entity = TrySpawnParticle(emitter);

		if (!particle_entity.has_value()) {
			break;
		}

		PTGN_ASSERT(particle_entity->Has<Particle>());

		Particle& p{ particle_entity->Get<Particle>() };

		if (p.lifetime.count() > 0) {
			auto max_age = static_cast<float>(p.lifetime.count());
			p.age = milliseconds{ static_cast<milliseconds::rep>(RandomFloat(0.0f, max_age)) };

			float t = std::clamp(
				static_cast<float>(p.age.count()) / static_cast<float>(p.lifetime.count()), 0.0f,
				1.0f
			);

			float dt = std::chrono::duration<float>(p.age).count() * config.simulation_speed;

			V2_float initial_velocity = p.velocity;

			p.position += initial_velocity * dt + 0.5f * p.gravity * dt * dt;
			p.velocity	= initial_velocity + p.gravity * dt;

			p.size	= Lerp(p.start_size, p.end_size, t);
			p.color = Lerp(p.start_color, p.end_color, t);
		}
	}
}

static void UpdateParticles(
	ParticleEmitter emitter_entity, ParticleEmitterComponent& emitter, milliseconds dt
) {
	const auto& config = emitter.config;
	float sim_dt	   = std::chrono::duration<float>(dt).count() * config.simulation_speed;

	std::vector<Entity> dead_particles;
	dead_particles.reserve(emitter.live_particle_count);

	for (auto [entity, p] : emitter.manager.EntitiesWith<Particle>()) {
		p.age += duration_cast<milliseconds>(dt * config.simulation_speed);

		if (p.age >= p.lifetime) {
			dead_particles.emplace_back(entity);
			if (auto scripts{ emitter_entity.TryGet<Scripts>() }) {
				ParticleDestroyed event;
				event.emitter  = emitter_entity;
				event.particle = p;
				scripts->Emit(event);
			}
			continue;
		}

		float t{ 1.0f };

		if (p.lifetime.count() > 0) {
			t = std::clamp(
				static_cast<float>(p.age.count()) / static_cast<float>(p.lifetime.count()), 0.0f,
				1.0f
			);
		}

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

ParticleDestroyScript::ParticleDestroyScript(const ParticleEmitter::DestroyCallback& callback) :
	callback_{ callback } {}

void ParticleDestroyScript::OnEvent(EventDispatcher d) {
	d.Dispatch<ParticleDestroyed>([this](const ParticleDestroyed& event) {
		std::visit(
			[this, &event]<typename TCallback>(const TCallback& callback) {
				if constexpr (std::is_same_v<TCallback, std::function<void()>>) {
					callback();
				} else if constexpr (std::is_same_v<
										 TCallback, std::function<void(ParticleDestroyed)>>) {
					callback(event);
				} else {
					static_assert(false, "Incomplete visitor");
				}
			},
			callback_
		);
	});
}

} // namespace impl

ParticleEmitter::ParticleEmitter(Entity entity) : Entity{ entity } {}

ParticleEmitter& ParticleEmitter::Start() {
	using enum impl::ParticleEmitterState;

	auto& emitter = Get<impl::ParticleEmitterComponent>();

	if (emitter.playback.state == Paused) {
		emitter.playback.state = Playing;
		return *this;
	}

	if (emitter.playback.state == Stopped) {
		InitializeEmitterRun(emitter);
		emitter.playback.state = Playing;
	}

	return *this;
}

ParticleEmitter& ParticleEmitter::Stop() {
	auto& emitter		   = Get<impl::ParticleEmitterComponent>();
	emitter.playback.state = impl::ParticleEmitterState::Stopped;
	return *this;
}

ParticleEmitter& ParticleEmitter::Pause() {
	if (auto& emitter = Get<impl::ParticleEmitterComponent>();
		emitter.playback.state == impl::ParticleEmitterState::Playing) {
		emitter.playback.state = impl::ParticleEmitterState::Paused;
	}
	return *this;
}

ParticleEmitter& ParticleEmitter::Resume() {
	if (auto& emitter = Get<impl::ParticleEmitterComponent>();
		emitter.playback.state == impl::ParticleEmitterState::Paused) {
		emitter.playback.state = impl::ParticleEmitterState::Playing;
	}
	return *this;
}

ParticleEmitter& ParticleEmitter::Toggle() {
	switch (const auto& emitter = Get<impl::ParticleEmitterComponent>(); emitter.playback.state) {
		using enum impl::ParticleEmitterState;
		case Stopped: Start(); break;
		case Playing: Pause(); break;
		case Paused:  Resume(); break;
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

ParticleEmitter& ParticleEmitter::OnParticleDestroy(const ParticleEmitter::DestroyCallback& callback
) {
	AddScript<impl::ParticleDestroyScript>(*this, callback);
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

void ParticleEmitter::Draw(DrawContext& renderer, Entity entity, Camera) {
	auto depth{ GetDepth(entity) };
	auto blend_mode{ GetBlendMode(entity) };

	const auto& emitter = entity.Get<impl::ParticleEmitterComponent>();
	const auto& config	= emitter.config;

	const Transform base_transform{ GetDrawTransform(entity) };

	for (const auto& [particle_entity, p] :
		 std::as_const(emitter.manager).EntitiesWith<Particle>()) {
		Transform transform{ base_transform };
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
					Circle circle{ s.GetRadius() * p.size * 0.5f };
					renderer.DrawShape(
						circle, transform, p.color, config.particle_fill_style, origin, depth,
						blend_mode
					);
				} else if constexpr (std::is_same_v<S, Rect>) {
					Rect rect{ s.GetSize() * V2_float{ p.size } };
					transform.Rotate(Radians{ kHalfPi });
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
		// Update emission (spawn new particles)
		UpdateEmitterPlayback(emitter, dt);

		// Update existing particles (movement, lifetime, etc.)
		if (emitter.playback.state != impl::ParticleEmitterState::Paused) {
			UpdateParticles(ParticleEmitter{ entity }, emitter, dt);
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