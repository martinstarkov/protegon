#pragma once

#include <cstdlib> // std::size_t
#include <tuple> // std::pair
#include <vector> // std::vector
#include <algorithm> // std::fill

#include "renderer/particle/Particle.h"
#include "utils/TypeTraits.h"
#include "math/Math.h"
#include "renderer/ScreenRenderer.h"

namespace ptgn {

template <typename TShape, 
	type_traits::is_base_of_e<Shape, TShape> = true>
class ParticleManager {
public:
	ParticleManager() = delete;
	~ParticleManager() = default;

	ParticleManager(std::size_t max_particles) : max_particles_{ max_particles } {
		properties_.resize(max_particles_, {});
		particles_.resize(max_particles_, {});
	}

	void SetAppearance(const ParticleAppearance<TShape>& new_appearance) {
		appearance_ = new_appearance;
	}

	template <typename Duration,
		type_traits::is_duration_e<Duration> = true>
	void SetLifetime(const Duration& lifetime) {
		std::fill(particles_.begin(), particles_.end(), lifetime);
	}

	virtual void Emit(const ParticleProperties& new_properties) {
		if (active_particles_ < max_particles_) {
			assert(particles_.size() == properties_.size() &&
				   "Particle lifetime and property vectors cannot be differing sizes");
			for (auto i{ 0 }; i < particles_.size(); ++i) {
				auto& lifetime{ particles_[i] };
				if (!lifetime.IsRunning()) {
					lifetime.Start();
					properties_[i] = new_properties;
					++active_particles_;
					break;
				}
			}
		}
	}
	
	virtual void Update() {
		std::size_t active_particles{ 0 };
		assert(particles_.size() == properties_.size() &&
			   "Particle lifetime and property vectors cannot be differing sizes");
		for (auto i{ 0 }; i < particles_.size(); ++i) {
			auto& lifetime{ particles_[i] };
			if (lifetime.IsRunning()) {
				++active_particles;
				auto& properties{ properties_[i] };
				properties.body.velocity += properties.body.acceleration;
				properties.body.angular_velocity += properties.body.angular_acceleration;
				properties.transform.position += properties.body.velocity;
				properties.transform.rotation += properties.body.angular_velocity;
			}
		}
		active_particles_ = active_particles;
	}

	virtual void Render() {
		assert(particles_.size() == properties_.size() &&
			   "Particle lifetime and property vectors cannot be differing sizes");
		for (auto i{ 0 }; i < particles_.size(); ++i) {
			auto& lifetime{ particles_[i] };
			if (lifetime.IsRunning()) {
				auto percentage_life_elapsed{ lifetime.ElapsedPercentage() };
				auto color{
					math::Lerp(appearance_.color_begin, appearance_.color_end, percentage_life_elapsed)
				};
				if constexpr (std::is_same_v<TShape, AABB>) {
					auto size{
						math::Lerp(appearance_.shape_begin.size, appearance_.shape_end.size, percentage_life_elapsed)
					};
					ScreenRenderer::DrawRectangle(properties.transform.position, size, color);
				} else if constexpr (std::is_same_v<TShape, Circle>) {
					auto radius{
						math::Lerp(appearance_.shape_begin.radius, appearance_.shape_end.radius, percentage_life_elapsed)
					};
					ScreenRenderer::DrawCircle(properties_[i].transform.position, radius, color);
				} else {
					static_assert(
						false, 
						"ParticleManager does not currently support rendering shapes other than Circle or AABB"
					);
				}
			}
		}
	}
protected:
	std::size_t max_particles_{ 0 };

	std::size_t active_particles_{ 0 };

	ParticleAppearance<TShape> appearance_;

	std::vector<ParticleProperties> properties_;

	std::vector<ParticleLifetime> particles_;
};

} // namespace ptgn