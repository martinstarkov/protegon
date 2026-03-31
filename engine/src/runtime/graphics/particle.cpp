#include "runtime/graphics/particle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <ostream>
#include <utility>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
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

void ParticleEmitterComponent::Update(V2_float start_position, secondsf dt) {
	if (particle_count < info.max_particles && emission.IsRunning() &&
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
		p.color	  = Lerp(p.start_color, p.end_color, elapsed);
		p.color.a = static_cast<std::uint8_t>(Lerp(255.0f, 0.0f, elapsed));
		p.radius  = p.start_radius * Lerp(info.start_scale, info.end_scale, elapsed);

		if (!info.use_random_velocities) {
			// Apply gravity.
			p.velocity += info.gravity * dt.count();
		}

		p.position += p.velocity * dt.count();
	}
	manager.Refresh();
}

void ParticleEmitterComponent::EmitParticle(V2_float start_position) {
	particle_count++;
	auto particle{ manager.CreateEntity() };
	auto& p = particle.Add<Particle>();
	p.timer.Start();
	ResetParticle(start_position, p);
	manager.Refresh();
}

void ParticleEmitterComponent::ResetParticle(V2_float start_position, Particle& p) {
	p.position = start_position + info.position_variance * V2_float{ rng(), rng() };

	if (info.use_random_velocities) {
		RNG<float> speed_rng{ info.min_speed, info.max_speed };
		float angle{ RandomAngleRadians() };
		V2_float heading{ std::cos(angle), std::sin(angle) };
		p.velocity = heading * speed_rng();
	} else {
		V2_float heading{ std::cos(info.starting_angle + info.angle_variance * rng()),
						  std::sin(info.starting_angle + info.angle_variance * rng()) };
		V2_float velocity{ info.speed + info.speed_variance * rng() * heading.x,
						   info.speed + info.speed_variance * rng() * heading.y };
		p.velocity = velocity;
	}

	p.start_radius = std::max(info.radius + info.radius_variance * rng(), 0.0f);
	// TODO: Fix multiplications.
	// TODO: Add clamping of values.
	p.start_color = info.start_color; // + info.start_color_variance * rng();
	p.end_color	  = info.end_color;	  // + info.end_color_variance * rng();
	p.lifetime	  = info.lifetime;	  // + info.lifetime_variance * rng();
}

} // namespace impl

ParticleEmitter::ParticleEmitter(Entity entity) : Entity{ entity } {}

void ParticleEmitter::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	auto depth{ GetDepth(entity) };
	auto blend_mode{ GetBlendMode(entity) };

	auto& i{ entity.Get<impl::ParticleEmitterComponent>() };

	if (i.info.texture_key.has_value()) {
		const auto& scene{ entity.GetScene() };
		PTGN_ASSERT(
			scene.ctx().asset.HasTexture(*i.info.texture_key),
			"Texture key must be loaded in the asset manager before creating a particle with it"
		);
		Texture texture{ *scene.ctx().asset.GetTexture(*i.info.texture_key) };

		Color tint{ color::White };
		auto origin{ Origin::Center };

		auto tex_coords{ impl::GetDefaultTextureCoordinates<false>() };

		for (const auto& [e, p] : std::as_const(i.manager).EntitiesWith<Particle>()) {
			if (i.info.tint_texture) {
				tint = p.color;
			}
			Transform transform{ p.position };
			V2_float size{ 2.0f * p.radius };
			renderer.DrawTexture(
				texture, transform, size, origin, tint, depth, tex_coords, blend_mode
			);
		}
		return;
	}
	switch (i.info.particle_shape) {
		case ParticleShape::Circle: {
			auto origin{ Origin::Center };
			for (const auto& [e, p] : std::as_const(i.manager).EntitiesWith<Particle>()) {
				Circle circle{ p.radius };
				Transform transform{ p.position };
				renderer.DrawShape(
					circle, transform, p.color, i.info.fill_style, origin, depth, blend_mode
				);
			}
			break;
		}
		case ParticleShape::Square: {
			auto origin{ Origin::Center };
			for (const auto& [e, p] : std::as_const(i.manager).EntitiesWith<Particle>()) {
				// TODO: Add rotation.
				Rect rect{ V2_float{ 2.0f * p.radius } };
				Transform transform{ p.position };
				renderer.DrawShape(
					rect, transform, p.color, i.info.fill_style, origin, depth, blend_mode
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
	Get<impl::ParticleEmitterComponent>().EmitParticle(GetPosition(*this));
	return *this;
}

ParticleEmitter& ParticleEmitter::Reset() {
	Get<impl::ParticleEmitterComponent>().manager.Reset();
	return *this;
}

ParticleEmitter& ParticleEmitter::SetGravity(V2_float particle_gravity) {
	auto& info{ Get<impl::ParticleEmitterComponent>().info };
	if (!particle_gravity.IsZero()) {
		info.use_random_velocities = false;
	}
	info.gravity = particle_gravity;
	return *this;
}

ParticleEmitter& ParticleEmitter::UseRandomVelocities(
	float min_speed, float max_speed, bool use_random_velocities
) {
	auto& emitter{ Get<impl::ParticleEmitterComponent>().info };
	emitter.min_speed			  = min_speed;
	emitter.max_speed			  = max_speed;
	emitter.use_random_velocities = use_random_velocities;
	return *this;
}

V2_float ParticleEmitter::GetGravity() const {
	return Get<impl::ParticleEmitterComponent>().info.gravity;
}

ParticleEmitter& ParticleEmitter::SetMaxParticles(std::size_t max_particles) {
	Get<impl::ParticleEmitterComponent>().info.max_particles = max_particles;
	return *this;
}

std::size_t ParticleEmitter::GetMaxParticles() const {
	return Get<impl::ParticleEmitterComponent>().info.max_particles;
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
	for (auto [entity, particle_manager] : scene.EntitiesWith<impl::ParticleEmitterComponent>()) {
		auto position{ GetPosition(entity) };
		particle_manager.Update(position, scene.ctx().dt());
	}

	scene.Refresh();
}

ParticleEmitter CreateParticleEmitter(Scene& scene, V2_float position, const ParticleInfo& info) {
	ParticleEmitter emitter{ scene.CreateEntity() };

	SetDraw<ParticleEmitter>(emitter);
	auto& i{ emitter.Add<impl::ParticleEmitterComponent>() };
	i.info = info;
	i.manager.Reserve(i.info.max_particles);
	Show(emitter, false);
	SetPosition(emitter, position);

	return emitter;
}

std::ostream& operator<<(std::ostream& os, ParticleShape shape) {
	switch (shape) {
		using enum ParticleShape;
		case Circle: return os << "Circle";
		case Square: return os << "Square";
		default:	 PTGN_ERROR("Unknown ParticleShape: ", std::to_underlying(shape));
	}
}

} // namespace ptgn