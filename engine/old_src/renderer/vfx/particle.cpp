// #include "renderer/vfx/particle.h"
//
// #include <algorithm>
// #include <chrono>
// #include <cmath>
// #include <cstdint>
//
// #include "core/app/application.h"
// #include "core/app/manager.h"
// #include "ecs/components/draw.h"
// #include "ecs/components/effects.h"
// #include "ecs/components/transform.h"
// #include "ecs/entity.h"
// #include "core/util/time.h"
// #include "core/util/timer.h"
// #include "math/geometry/circle.h"
// #include "math/geometry/rect.h"
// #include "math/math_utils.h"
// #include "math/rng.h"
// #include "math/vector2.h"
// #include "renderer/api/color.h"
// #include "renderer/api/origin.h"
// #include "renderer/renderer.h"
// #include "scene/camera.h"
//
// namespace ptgn {
//
// namespace impl {
//
// void ParticleEmitterComponent::Update(const V2_float& start_position) {
//	if (particle_count < info.max_particles && emission.IsRunning() &&
//		emission.Completed(info.emission_delay)) {
//		EmitParticle(start_position);
//		emission.Start();
//	}
//
//	for (auto [e, p] : manager.EntitiesWith<Particle>()) {
//		float elapsed{ p.timer.ElapsedPercentage(p.lifetime) };
//		if (elapsed >= 1.0f) {
//			e.Destroy();
//			particle_count--;
//			continue;
//		}
//		p.color	  = Lerp(p.start_color, p.end_color, elapsed);
//		p.color.a = static_cast<std::uint8_t>(Lerp(255.0f, 0.0f, elapsed));
//		p.radius  = p.start_radius * Lerp(info.start_scale, info.end_scale, elapsed);
//
//		if (!info.use_random_velocities) {
//			// Apply gravity.
//			p.velocity += info.gravity * Application::Get().dt();
//		}
//
//		p.position += p.velocity * Application::Get().dt();
//	}
//	manager.Refresh();
// }
//
// void ParticleEmitterComponent::EmitParticle(const V2_float& start_position) {
//	particle_count++;
//	auto particle{ manager.CreateEntity() };
//	auto& p = particle.Add<Particle>();
//	p.timer.Start();
//	ResetParticle(start_position, p);
//	manager.Refresh();
// }
//
// void ParticleEmitterComponent::ResetParticle(const V2_float& start_position, Particle& p) {
//	p.position = start_position + info.position_variance * V2_float{ rng(), rng() };
//
//	if (info.use_random_velocities) {
//		RNG<float> speed_rng{ info.min_speed, info.max_speed };
//		static RNG<float> heading_rng{ 0.0f, two_pi<float> };
//		float angle{ heading_rng() };
//		V2_float heading{ std::cos(angle), std::sin(angle) };
//		p.velocity = heading * speed_rng();
//	} else {
//		p.velocity = { info.speed + info.speed_variance * rng() *
//										std::cos(info.starting_angle + info.angle_variance * rng()),
//					   info.speed +
//						   info.speed_variance * rng() *
//							   std::sin(info.starting_angle + info.angle_variance * rng()) };
//	}
//
//	p.start_radius = std::max(info.radius + info.radius_variance * rng(), 0.0f);
//	// TODO: Fix multiplications.
//	// TODO: Add clamping of values.
//	p.start_color = info.start_color; // + info.start_color_variance * rng_();
//	p.end_color	  = info.end_color;	  // + info.end_color_variance * rng_();
//	p.lifetime	  = info.lifetime;	  // + info.lifetime_variance * rng_();
// }
//
// } // namespace impl
//
// void ParticleEmitter::Draw(const Entity& entity) {
//	auto depth{ GetDepth(entity) };
//	auto blend_mode{ GetBlendMode(entity) };
//	auto camera{ entity.GetOrParentOrDefault<Camera>() };
//	auto pre_fx{ entity.GetOrDefault<PreFX>() };
//	auto post_fx{ entity.GetOrDefault<PostFX>() };
//
//	auto& i{ entity.Get<impl::ParticleEmitterComponent>() };
//
//	if (i.info.texture_enabled && i.info.texture_key) {
//		Color tint{ color::White };
//
//		for (const auto& [e, p] : i.manager.EntitiesWith<Particle>()) {
//			if (i.info.tint_texture) {
//				tint = p.color;
//			}
//
//			Application::Get().render_.DrawTexture(
//				i.info.texture_key, Transform{ p.position },
//				V2_float{ 2.0f * p.radius, 2.0f * p.radius }, Origin::Center, tint, depth,
//				blend_mode, camera, pre_fx, post_fx
//			);
//		}
//		return;
//	}
//	switch (i.info.particle_shape) {
//		case ParticleShape::Circle: {
//			for (const auto& [e, p] : i.manager.EntitiesWith<Particle>()) {
//				Application::Get().render_.DrawCircle(
//					Transform{ p.position }, Circle{ p.radius }, p.color, i.info.line_width, depth,
//					blend_mode, camera, post_fx
//				);
//			}
//			break;
//		}
//		case ParticleShape::Square: {
//			for (const auto& [e, p] : i.manager.EntitiesWith<Particle>()) {
//				// TODO: Add rotation.
//				Application::Get().render_.DrawRect(
//					Transform{ p.position }, Rect{ V2_float{ 2.0f * p.radius } }, p.color,
//					i.info.line_width, Origin::Center, depth, blend_mode, camera, post_fx
//				);
//			}
//			break;
//		}
//	}
// }
//
// ParticleEmitter& ParticleEmitter::Start() {
//	Get<impl::ParticleEmitterComponent>().emission.Start();
//	return *this;
// }
//
// ParticleEmitter& ParticleEmitter::Stop() {
//	Get<impl::ParticleEmitterComponent>().emission.Stop();
//	return *this;
// }
//
// ParticleEmitter& ParticleEmitter::Toggle() {
//	Get<impl::ParticleEmitterComponent>().emission.Toggle();
//	return *this;
// }
//
// ParticleEmitter& ParticleEmitter::EmitParticle() {
//	Get<impl::ParticleEmitterComponent>().EmitParticle(GetPosition(*this));
//	return *this;
// }
//
// ParticleEmitter& ParticleEmitter::Reset() {
//	Get<impl::ParticleEmitterComponent>().manager.Reset();
//	return *this;
// }
//
// ParticleEmitter& ParticleEmitter::SetGravity(const V2_float& particle_gravity) {
//	auto& info{ Get<impl::ParticleEmitterComponent>().info };
//	if (!particle_gravity.IsZero()) {
//		info.use_random_velocities = false;
//	}
//	info.gravity = particle_gravity;
//	return *this;
// }
//
// ParticleEmitter& ParticleEmitter::UseRandomVelocities(
//	float min_speed, float max_speed, bool use_random_velocities
//) {
//	auto& emitter{ Get<impl::ParticleEmitterComponent>().info };
//	emitter.min_speed			  = min_speed;
//	emitter.max_speed			  = max_speed;
//	emitter.use_random_velocities = use_random_velocities;
//	return *this;
// }
//
// V2_float ParticleEmitter::GetGravity() const {
//	return Get<impl::ParticleEmitterComponent>().info.gravity;
// }
//
// ParticleEmitter& ParticleEmitter::SetMaxParticles(std::size_t max_particles) {
//	Get<impl::ParticleEmitterComponent>().info.max_particles = max_particles;
//	return *this;
// }
//
// std::size_t ParticleEmitter::GetMaxParticles() const {
//	return Get<impl::ParticleEmitterComponent>().info.max_particles;
// }
//
// ParticleEmitter& ParticleEmitter::SetShape(ParticleShape shape) {
//	Get<impl::ParticleEmitterComponent>().info.particle_shape = shape;
//	return *this;
// }
//
// ParticleShape ParticleEmitter::GetShape() const {
//	return Get<impl::ParticleEmitterComponent>().info.particle_shape;
// }
//
// ParticleEmitter& ParticleEmitter::SetRadius(float particle_radius) {
//	Get<impl::ParticleEmitterComponent>().info.radius = particle_radius;
//	return *this;
// }
//
// float ParticleEmitter::GetRadius() const {
//	return Get<impl::ParticleEmitterComponent>().info.radius;
// }
//
// ParticleEmitter& ParticleEmitter::SetStartColor(const Color& start_color) {
//	Get<impl::ParticleEmitterComponent>().info.start_color = start_color;
//	return *this;
// }
//
// Color ParticleEmitter::GetStartColor() const {
//	return Get<impl::ParticleEmitterComponent>().info.start_color;
// }
//
// ParticleEmitter& ParticleEmitter::SetEndColor(const Color& end_color) {
//	Get<impl::ParticleEmitterComponent>().info.end_color = end_color;
//	return *this;
// }
//
// Color ParticleEmitter::GetEndColor() const {
//	return Get<impl::ParticleEmitterComponent>().info.end_color;
// }
//
// ParticleEmitter& ParticleEmitter::SetEmissionDelay(milliseconds emission_delay) {
//	Get<impl::ParticleEmitterComponent>().info.emission_delay = emission_delay;
//	return *this;
// }
//
// milliseconds ParticleEmitter::GetEmissionDelay() const {
//	return Get<impl::ParticleEmitterComponent>().info.emission_delay;
// }
//
// void ParticleEmitter::Update(Manager& manager) {
//	for (auto [entity, particle_manager] : manager.EntitiesWith<impl::ParticleEmitterComponent>()) {
//		auto position{ GetPosition(entity) };
//		particle_manager.Update(position);
//	}
//
//	manager.Refresh();
// }
//
// ParticleEmitter CreateParticleEmitter(Manager& manager, const ParticleInfo& info) {
//	ParticleEmitter emitter{ manager.CreateEntity() };
//
//	SetDraw<ParticleEmitter>(emitter);
//	auto& i{ emitter.Add<impl::ParticleEmitterComponent>() };
//	i.info = info;
//	i.manager.Reserve(i.info.max_particles);
//	Show(emitter);
//	SetPosition(emitter, {});
//
//	return emitter;
// }
//
// } // namespace ptgn