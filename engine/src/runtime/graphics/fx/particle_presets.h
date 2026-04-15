#pragma once

#include <chrono>
#include <utility>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "runtime/graphics/fx/particle.h"
#include "serialization/serialize.h"

namespace ptgn {

enum class ParticlePreset {
	Smoke1,
	Fire1,
	FireExplosion1,
	Rain1,
	Snow1,
};
PTGN_REFLECT_ENUM(ParticlePreset);

namespace impl {

struct PresetParticleConfigEntry {
	ParticlePreset effect;
	ParticleConfig config;
};

static constexpr std::array kParticlePresets{
	PresetParticleConfigEntry{
		ParticlePreset::Smoke1,
		ParticleConfig{ .rate_or_burst		 = ParticleRate{ .duration		 = 1s,
															 .loop			 = true,
															 .prewarm		 = false,
															 .rate_over_time = 80 },
						.lifetime			 = ConstantOrRange<milliseconds>{ 1500ms, 3s },
						.start_speed		 = { 10.0f, 30.0f },
						.start_size			 = { 12.0f, 24.0f },
						.start_rotation		 = ConstantOrRange<Degrees>{ 0.0f, 360.0f },
						.start_color		 = Color{ 120, 120, 120, 180 },
						.start_gravity		 = V2_float{ 0.0f, -10.0f },
						.max_particles		 = 1000,
						.simulation_speed	 = 1.0f,
						.particle_type		 = Circle{ 0.5f },
						.particle_fill_style = Solid{},
						.emission_shape		 = EmissionShape::Arc(360.0f, 12.0f, {}, 0.0f),
						.size_over_lifetime	 = 40.0f,
						.color_over_lifetime = Color{ 60, 60, 60, 0 } } },
	PresetParticleConfigEntry{
		ParticlePreset::Fire1,
		ParticleConfig{ .rate_or_burst		 = ParticleRate{ .duration		 = 1s,
															 .loop			 = true,
															 .prewarm		 = false,
															 .rate_over_time = 180 },
						.lifetime			 = ConstantOrRange{ 800ms, 1800ms },
						.start_speed		 = { 30.0f, 80.0f },
						.start_size			 = { 8.0f, 18.0f },
						.start_rotation		 = ConstantOrRange<Degrees>{ 0.0f, 360.0f },
						.start_color		 = color::Yellow,
						.start_gravity		 = V2_float{ 0.0f, -80.0f },
						.max_particles		 = 1000,
						.simulation_speed	 = 1.0f,
						.particle_type		 = Circle{ 0.5f },
						.particle_fill_style = Solid{},
						.emission_shape = EmissionShape::Arc(50.0f, 8.0f, { 0.0f, -1.0f }, 0.0f),
						.size_over_lifetime	 = 2.0f,
						.color_over_lifetime = Color{ 180, 20, 20, 0 } } },
	PresetParticleConfigEntry{
		ParticlePreset::FireExplosion1,
		ParticleConfig{
			.rate_or_burst	= ParticleBurst{ .particle_count = 180, .cycles = 1, .interval = 1ms },
			.lifetime		= ConstantOrRange{ 500ms, 1200ms },
			.start_speed	= { 80.0f, 220.0f },
			.start_size		= { 6.0f, 14.0f },
			.start_rotation = ConstantOrRange<Degrees>{ 0.0f, 360.0f },
			.start_color	= color::Orange,
			.start_gravity	= V2_float{ 0.0f, 120.0f },
			.max_particles	= 1000,
			.simulation_speed	 = 1.0f,
			.particle_type		 = Circle{ 0.5f },
			.particle_fill_style = Solid{},
			.emission_shape		 = EmissionShape::Arc(360.0f, 6.0f, {}, 0.0f),
			.size_over_lifetime	 = 0.0f,
			.color_over_lifetime = Color{ 80, 80, 80, 0 },
		} },
	PresetParticleConfigEntry{
		ParticlePreset::Rain1,
		ParticleConfig{
			.rate_or_burst =
				ParticleRate{
					.duration = 1s, .loop = true, .prewarm = false, .rate_over_time = 250 },
			.lifetime			 = ConstantOrRange{ 2000ms, 2500ms },
			.start_speed		 = { 260.0f, 420.0f },
			.start_size			 = 6.0f,
			.align_to_direction	 = true,
			.start_color		 = Color{ 120, 170, 255, 220 },
			.start_gravity		 = V2_float{ 0.0f, 300.0f },
			.max_particles		 = 2000,
			.simulation_speed	 = 1.0f,
			.particle_type		 = Rect{ 0.25f, 1.0f },
			.particle_fill_style = Solid{},
			.emission_shape		 = EmissionShape::Rect({ 500.0f, 20.0f }, { 0.0f, 1.0f }),
			.size_over_lifetime	 = 3.0f,
			.color_over_lifetime = Color{ 120, 170, 255, 40 },
		} },
	PresetParticleConfigEntry{
		ParticlePreset::Snow1,
		ParticleConfig{
			.rate_or_burst =
				ParticleRate{ .duration = 8s, .loop = true, .prewarm = true, .rate_over_time = 70 },
			.lifetime			 = ConstantOrRange<milliseconds>{ 6s, 8s },
			.start_speed		 = { 15.0f, 40.0f },
			.start_size			 = { 6.0f, 12.0f },
			.start_rotation		 = ConstantOrRange<Degrees>{ 0.0f, 360.0f },
			.start_color		 = Color{ 245, 245, 255, 230 },
			.start_gravity		 = V2_float{ 0.0f, 18.0f },
			.max_particles		 = 1000,
			.simulation_speed	 = 1.0f,
			.particle_type		 = Circle{ 0.5f },
			.particle_fill_style = Solid{},
			.emission_shape		 = EmissionShape::Rect({ 500.0f, 20.0f }, { 0.0f, 1.0f }),
			.size_over_lifetime	 = 4.0f,
			.color_over_lifetime = Color{ 245, 245, 255, 100 },
		} },
};

} // namespace impl

[[nodiscard]] constexpr ParticleConfig GetParticleConfig(ParticlePreset preset) {
	for (const auto& entry : impl::kParticlePresets) {
		if (entry.effect == preset) {
			return entry.config;
		}
	}
	PTGN_ERROR("Unknown particle preset: ", std::to_underlying(preset));
}

} // namespace ptgn