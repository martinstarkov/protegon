#include "vfx/particle.h"

#include <cmath>
#include <cstdint>
#include <utility>

#include "components/common.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/game_object.h"
#include "core/manager.h"
#include "core/transform.h"
#include "math/math.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/blend_mode.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/render_data.h"
#include "utility/time.h"
#include "utility/timer.h"

namespace ptgn {

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

ParticleEmitter::ParticleEmitter(Manager& manager) : GameObject{ manager } {
	Add<impl::ParticleEmitterComponent>();
	SetVisible(true);
	SetEnabled(true);
	Add<Transform>();
}

ParticleEmitter::ParticleEmitter(Manager& manager, const ParticleInfo& info) :
	ParticleEmitter{ manager } {
	auto& i{ Get<impl::ParticleEmitterComponent>() };
	i.info = info;
	i.manager.Reserve(i.info.total_particles);
}

void ParticleEmitter::Draw(
	const Entity& e, impl::RenderData& r, const Depth& depth, BlendMode blend_mode
) {
	auto& i{ e.Get<impl::ParticleEmitterComponent>() };
	if (i.info.texture_enabled && i.info.texture_key != "") {
		V4_float tint{ color::White.Normalized() };
		for (const auto& [entity, p] : i.manager.EntitiesWith<Particle>()) {
			if (i.info.tint_texture) {
				tint = p.color.Normalized();
			}
			// TODO: Add texture rotaiton.
			r.AddTexture(
				{}, game.texture.Get(i.info.texture_key), p.position,
				{ 2.0f * p.radius, 2.0f * p.radius }, Origin::Center, depth, blend_mode, tint, 0.0f,
				false, false
			);
		}
		return;
	}
	switch (i.info.particle_shape) {
		case ParticleShape::Circle: {
			for (const auto& [entity, p] : i.manager.EntitiesWith<Particle>()) {
				r.AddEllipse(
					p.position, V2_float{ p.radius }, i.info.line_width, depth, blend_mode,
					p.color.Normalized(), 0.0f, false
				);
			}
			break;
		}
		case ParticleShape::Square: {
			for (const auto& [entity, p] : i.manager.EntitiesWith<Particle>()) {
				// TODO: Add rect rotation
				r.AddQuad(
					p.position, { 2.0f * p.radius, 2.0f * p.radius }, Origin::Center,
					i.info.line_width, depth, blend_mode, p.color.Normalized(), 0.0f, false
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

} // namespace ptgn