#include "rendering/graphics/vfx/particle.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <utility>

#include "components/common.h"
#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/timer.h"
#include "math/geometry.h"
#include "math/math.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/batching/render_data.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"

namespace ptgn {

ParticleEmitter CreateParticleEmitter(Scene& scene, const ParticleInfo& info) {
	ParticleEmitter emitter{ scene.CreateEntity() };

	emitter.SetDraw<ParticleEmitter>();
	auto& i{ emitter.Add<impl::ParticleEmitterComponent>() };
	i.info = info;
	i.manager.Reserve(i.info.total_particles);
	emitter.Show();
	emitter.Enable();
	emitter.Add<Transform>();

	return emitter;
}

namespace impl {

void ParticleEmitterComponent::Update(const V2_float& start_position) {
	if (particle_count < info.total_particles && emission.IsRunning() &&
		emission.Completed(info.emission_delay)) {
		EmitParticle(start_position);
		emission.Start();
	}

	for (auto [e, p] : manager.EntitiesWith<Particle>()) {
		float elapsed{ p.timer.ElapsedPercentage(p.lifetime) };
		if (elapsed >= 1.0f) {
			e.Destroy();
			particle_count--;
			continue;
		}
		p.color		= Lerp(p.start_color, p.end_color, elapsed);
		p.color.a	= static_cast<std::uint8_t>(Lerp(255.0f, 0.0f, elapsed));
		p.radius	= p.start_radius * Lerp(info.start_scale, info.end_scale, elapsed);
		p.velocity += info.gravity * game.dt();
		p.position += p.velocity * game.dt();
	}
	manager.Refresh();
}

void ParticleEmitterComponent::EmitParticle(const V2_float& start_position) {
	particle_count++;
	auto particle{ manager.CreateEntity() };
	auto& p = particle.Add<Particle>();
	p.timer.Start();
	ResetParticle(start_position, p);
	manager.Refresh();
}

void ParticleEmitterComponent::ResetParticle(const V2_float& start_position, Particle& p) {
	p.position	   = start_position + info.position_variance * V2_float{ rng(), rng() };
	p.velocity	   = { info.speed + info.speed_variance * rng() *
										std::cos(info.starting_angle + info.angle_variance * rng()),
					   info.speed + info.speed_variance * rng() *
										std::sin(info.starting_angle + info.angle_variance * rng()) };
	p.start_radius = std::max(info.radius + info.radius_variance * rng(), 0.0f);
	// TODO: Fix multiplications.
	// TODO: Add clamping of values.
	p.start_color = info.start_color; // + info.start_color_variance * rng_();
	p.end_color	  = info.end_color;	  // + info.end_color_variance * rng_();
	p.lifetime	  = info.lifetime;	  // + info.lifetime_variance * rng_();
}

} // namespace impl

void ParticleEmitter::Draw(impl::RenderData& ctx, const Entity& entity) {
	auto blend_mode{ entity.GetBlendMode() };
	auto depth{ entity.GetDepth() };
	auto camera{ entity.GetOrDefault<Camera>() };

	auto& i{ entity.Get<impl::ParticleEmitterComponent>() };
	if (i.info.texture_enabled && i.info.texture_key) {
		V4_float tint{ color::White.Normalized() };
		for (const auto& [e, p] : i.manager.EntitiesWith<Particle>()) {
			if (i.info.tint_texture) {
				tint = p.color.Normalized();
			}

			// TODO: Add texture rotation.
			Transform t{ p.position };
			ctx.AddTexturedQuad(
				t, { 2.0f * p.radius, 2.0f * p.radius }, Origin::Center,
				impl::GetDefaultTextureCoordinates(), game.texture.Get(i.info.texture_key), depth,
				camera, blend_mode, tint, false
			);
		}
		return;
	}
	switch (i.info.particle_shape) {
		case ParticleShape::Circle: {
			for (const auto& [e, p] : i.manager.EntitiesWith<Particle>()) {
				ctx.AddEllipse(
					p.position, V2_float{ p.radius }, i.info.line_width, depth, camera, blend_mode,
					p.color.Normalized(), 0.0f, false
				);
			}
			break;
		}
		case ParticleShape::Square: {
			for (const auto& [e, p] : i.manager.EntitiesWith<Particle>()) {
				// TODO: Add rect rotation.
				ctx.AddQuad(
					p.position, { 2.0f * p.radius, 2.0f * p.radius }, Origin::Center,
					i.info.line_width, depth, camera, blend_mode, p.color.Normalized(), 0.0f, false
				);
			}
			break;
		}
	}
}

ParticleEmitter& ParticleEmitter::Start() {
	Get<impl::ParticleEmitterComponent>().emission.Start();
	return *this;
}

ParticleEmitter& ParticleEmitter::Stop() {
	Get<impl::ParticleEmitterComponent>().emission.Stop();
	return *this;
}

ParticleEmitter& ParticleEmitter::Toggle() {
	Get<impl::ParticleEmitterComponent>().emission.Toggle();
	return *this;
}

ParticleEmitter& ParticleEmitter::EmitParticle() {
	Get<impl::ParticleEmitterComponent>().EmitParticle(GetPosition());
	return *this;
}

ParticleEmitter& ParticleEmitter::Reset() {
	Get<impl::ParticleEmitterComponent>().manager.Reset();
	return *this;
}

ParticleEmitter& ParticleEmitter::SetGravity(const V2_float& particle_gravity) {
	Get<impl::ParticleEmitterComponent>().info.gravity = particle_gravity;
	return *this;
}

V2_float ParticleEmitter::GetGravity() const {
	return Get<impl::ParticleEmitterComponent>().info.gravity;
}

ParticleEmitter& ParticleEmitter::SetMaxParticles(std::size_t max_particles) {
	Get<impl::ParticleEmitterComponent>().info.total_particles = max_particles;
	return *this;
}

std::size_t ParticleEmitter::GetMaxParticles() const {
	return Get<impl::ParticleEmitterComponent>().info.total_particles;
}

ParticleEmitter& ParticleEmitter::SetShape(ParticleShape shape) {
	Get<impl::ParticleEmitterComponent>().info.particle_shape = shape;
	return *this;
}

ParticleShape ParticleEmitter::GetShape() const {
	return Get<impl::ParticleEmitterComponent>().info.particle_shape;
}

ParticleEmitter& ParticleEmitter::SetRadius(float particle_radius) {
	Get<impl::ParticleEmitterComponent>().info.radius = particle_radius;
	return *this;
}

float ParticleEmitter::GetRadius() const {
	return Get<impl::ParticleEmitterComponent>().info.radius;
}

ParticleEmitter& ParticleEmitter::SetStartColor(const Color& start_color) {
	Get<impl::ParticleEmitterComponent>().info.start_color = start_color;
	return *this;
}

Color ParticleEmitter::GetStartColor() const {
	return Get<impl::ParticleEmitterComponent>().info.start_color;
}

ParticleEmitter& ParticleEmitter::SetEndColor(const Color& end_color) {
	Get<impl::ParticleEmitterComponent>().info.end_color = end_color;
	return *this;
}

Color ParticleEmitter::GetEndColor() const {
	return Get<impl::ParticleEmitterComponent>().info.end_color;
}

ParticleEmitter& ParticleEmitter::SetEmissionDelay(milliseconds emission_delay) {
	Get<impl::ParticleEmitterComponent>().info.emission_delay = emission_delay;
	return *this;
}

milliseconds ParticleEmitter::GetEmissionDelay() const {
	return Get<impl::ParticleEmitterComponent>().info.emission_delay;
}

void ParticleEmitter::Update(Scene& scene) {
	for (auto [entity, enabled, particle_manager] :
		 scene.EntitiesWith<Enabled, impl::ParticleEmitterComponent>()) {
		particle_manager.Update(entity.GetPosition());
	}

	scene.Refresh();
}

} // namespace ptgn