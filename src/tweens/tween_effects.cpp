#include "tweens/tween_effects.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/movement.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/time.h"
#include "core/timer.h"
#include "debug/log.h"
#include "math/easing.h"
#include "math/math.h"
#include "math/noise.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "renderer/api/color.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "tweens/follow_config.h"

namespace ptgn {

namespace impl {

ShakeEffectInfo::ShakeEffectInfo(
	float start_intensity, float target_intensity, milliseconds shake_duration,
	const Ease& shake_ease, const ShakeConfig& shake_config, std::int32_t shake_seed
) :
	EffectInfo<float>{ start_intensity, target_intensity, shake_duration, shake_ease } {
	PTGN_ASSERT(
		start_intensity >= 0.0f && start_intensity <= 1.0f,
		"Shake effect intensity must be in range [0.0, 1.0]"
	);
	PTGN_ASSERT(
		target_intensity >= 0.0f && target_intensity <= 1.0f,
		"Shake effect intensity must be in range [0.0, 1.0]"
	);
	config = shake_config;
	seed   = shake_seed;
}

template <typename T>
auto GetTaskValue(T& task) {
	PTGN_ASSERT(task.timer.IsRunning());

	float t{ 1.0f };

	if (task.duration >= milliseconds{ 0 }) {
		t = task.timer.template ElapsedPercentage<milliseconds, float>(task.duration);
	}

	float eased_t{ ApplyEase(t, task.ease) };

	return Lerp(task.start_value, task.target_value, eased_t);
}

template <typename T, typename F>
void UpdateTask(T& effect, F& task) {
	if (!task.timer.Completed(task.duration)) {
		return;
	}

	// Task completed.
	effect.tasks.pop_front();

	if (effect.tasks.empty()) {
		return;
	}

	// Start next task.
	effect.tasks.front().timer.Start(true);
}

void TranslateEffectSystem::Update(Scene& scene) const {
	for (auto [entity, effect] : scene.EntitiesWith<TranslateEffect>()) {
		if (effect.tasks.empty()) {
			entity.template Remove<TranslateEffect>();
			return;
		}

		auto& task{ effect.tasks.front() };

		SetPosition(entity, GetTaskValue(task));

		UpdateTask(effect, task);
	}
}

void RotateEffectSystem::Update(Scene& scene) const {
	for (auto [entity, effect] : scene.EntitiesWith<RotateEffect>()) {
		if (effect.tasks.empty()) {
			entity.template Remove<RotateEffect>();
			return;
		}

		auto& task{ effect.tasks.front() };

		SetRotation(entity, GetTaskValue(task));

		UpdateTask(effect, task);
	}
}

void ScaleEffectSystem::Update(Scene& scene) const {
	for (auto [entity, effect] : scene.EntitiesWith<ScaleEffect>()) {
		if (effect.tasks.empty()) {
			entity.template Remove<ScaleEffect>();
			return;
		}

		auto& task{ effect.tasks.front() };

		SetScale(entity, GetTaskValue(task));

		UpdateTask(effect, task);
	}
}

void TintEffectSystem::Update(Scene& scene) const {
	for (auto [entity, effect] : scene.EntitiesWith<TintEffect>()) {
		if (effect.tasks.empty()) {
			entity.template Remove<TintEffect>();
			return;
		}

		auto& task{ effect.tasks.front() };

		SetTint(entity, GetTaskValue(task));

		UpdateTask(effect, task);
	}
}

BounceEffectInfo::BounceEffectInfo(
	const V2_float& shake_amplitude, milliseconds shake_duration, const Ease& shake_ease,
	const V2_float& shake_static_offset, std::int64_t shake_total_periods, bool shake_symmetrical
) :
	amplitude{ shake_amplitude },
	duration{ shake_duration },
	ease{ shake_ease },
	static_offset{ shake_static_offset },
	total_periods{ shake_total_periods },
	symmetrical{ shake_symmetrical } {
	PTGN_ASSERT(
		total_periods == -1 || total_periods > 0,
		"Invalid number of total periods for bounce effect"
	);
}

void BounceEffectSystem::Update(Scene& scene) const {
	for (auto [entity, effect, offsets] : scene.EntitiesWith<BounceEffect, Offsets>()) {
		if (effect.tasks.empty()) {
			offsets.bounce = {};
			entity.template Remove<BounceEffect>();
			continue;
		}

		auto& task{ effect.tasks.front() };

		PTGN_ASSERT(task.timer.IsRunning());

		float t{ task.timer.template ElapsedPercentage<milliseconds, float>(task.duration) };

		float eased_t{ ApplyEase(t, task.symmetrical, task.ease) };

		offsets.bounce.SetPosition(task.static_offset + task.amplitude * eased_t);

		if (!task.timer.Completed(task.duration)) {
			continue;
		}

		// Bounce timer completed.

		task.periods_completed++;

		if (task.total_periods == -1 || task.periods_completed < task.total_periods) {
			task.timer.Start(true);
			continue;
		}

		// Bounce repeats completed.

		effect.tasks.pop_front();

		if (effect.tasks.empty()) {
			continue;
		}

		// New bounce effect.

		effect.tasks.front().timer.Start(true);
	}
}

float BounceEffectSystem::ApplyEase(float t, bool symmetrical, const Ease& ease) {
	if (!symmetrical) {
		// Standard up-down bounce.

		// Triangle wave with y=1.0 peak at t=0.5.
		float triangle_t{ TriangleWave(t, 2.0f, 0.25f) };
		float eased_t{ ptgn::ApplyEase(triangle_t, ease) };
		return eased_t;
	}

	// Symmetrical bounce.

	PTGN_ASSERT(
		std::holds_alternative<SymmetricalEase>(ease),
		"Symmetrical bounces only support symmetrical easing functions"
	);

	// In essence this is a piece wise triangle wave function which rises from 0.5 to 1.0 in the
	// domain [0, 0.25], falls from 1.0 to 0.0 in the domain [0.25, 0.75] and rises again from 0.0
	// to 0.5 in the domain [0.75, 1.0].
	float triangle_t{ 0.0f };
	if (t < 0.25f) {
		triangle_t = 1.0f + (2.0f * t - 0.5f);
	} else if (t > 0.75f) {
		triangle_t = -1.0f + (2.0f * t - 0.5f);
	} else {
		triangle_t = 1.0f - (2.0f * t - 0.5f);
	}

	float eased_t{ ptgn::ApplyEase(triangle_t, ease) };
	// Transform to -1 to 1 range for symmetrical amplitudes.
	return 2.0f * eased_t - 1.0f;
}

void ShakeEffectSystem::Update(Scene& scene, float time, float dt) const {
	for (auto [entity, effect, offsets] : scene.EntitiesWith<ShakeEffect, Offsets>()) {
		if (effect.tasks.empty()) {
			offsets.shake = {};
			entity.template Remove<ShakeEffect>();
			continue;
		}

		auto& task{ effect.tasks.front() };

		bool completed{ task.timer.Completed(task.duration) };

		if (completed) {
			// Shake effect has finished but trauma needs to be decayed (organically).
			task.trauma = std::clamp(task.trauma - task.config.recovery_speed * dt, 0.0f, 1.0f);
		} else {
			// Shake effect is ongoing, update trauma value with intensity.

			float intensity{ GetTaskValue(task) };

			PTGN_ASSERT(intensity >= 0.0f && intensity <= 1.0f);

			task.trauma = intensity;
		}

		// Shake algorithm based on: https://roystan.net/articles/camera-shake/

		// Taking trauma to an exponent allows the ability to smoothen
		// out the transition from shaking to being static.
		float shake{ std::pow(task.trauma, task.config.trauma_exponent) };

		float x{ time * task.config.frequency };

		V2_float position_noise{ PerlinNoise::GetValue(x, 0.0f, task.seed + 0) * 2.0f - 1.0f,
								 PerlinNoise::GetValue(x, 0.0f, task.seed + 1) * 2.0f - 1.0f };

		float rotation_noise{ PerlinNoise::GetValue(x, 0.0f, task.seed + 3) * 2.0f - 1.0f };

		offsets.shake.SetPosition(shake * task.config.maximum_translation * position_noise);
		offsets.shake.SetRotation(shake * task.config.maximum_rotation * rotation_noise);

		if (!completed) {
			// Shake effect has not finished yet.
			continue;
		}

		if (task.duration == milliseconds{ -1 }) {
			// Shake effect is infinite, restart the timer.
			task.timer.Start(true);
			continue;
		}

		if (task.trauma >= 0.0f && effect.tasks.size() == 1) {
			// Shake effect has finished and is the only queued effect, but there is some trauma
			// left to decay.
			continue;
		}

		// Shake effect has finished and all trauma has been decayed (or another queued shake
		// starts).
		effect.tasks.pop_front();

		if (effect.tasks.empty()) {
			// No more shake effects.
			continue;
		}

		// Start next shake effect.
		effect.tasks.front().timer.Start(true);
	}
}

void BounceImpl(
	Entity& entity, const V2_float& amplitude, milliseconds duration, std::int64_t total_periods,
	const Ease& ease, const V2_float& static_offset, bool force, bool symmetrical
) {
	auto& bounce{ entity.TryAdd<impl::BounceEffect>() };
	entity.TryAdd<impl::Offsets>();

	bool first_task{ force || bounce.tasks.empty() };

	if (first_task) {
		bounce.tasks.clear();
	}

	auto& task{ bounce.tasks.emplace_back(
		amplitude, duration, ease, static_offset, total_periods, symmetrical
	) };

	if (first_task) {
		task.timer.Start(true);
	}
}

FollowEffectInfo::FollowEffectInfo(Entity follow_target, const FollowConfig& follow_config) :
	target{ follow_target }, config{ follow_config } {}

void FollowEffectSystem::Update(Scene& scene) const {
	for (auto [entity, effect] : scene.EntitiesWith<FollowEffect>()) {
		if (effect.tasks.empty()) {
			entity.template Remove<FollowEffect>();
			entity.template Remove<TopDownMovement>();
			entity.template Remove<RigidBody>();
			continue;
		}

		auto& task{ effect.tasks.front() };

		if (!task.config.follow_x && !task.config.follow_y) {
			continue;
		}

		// TODO: Clean up repeated code.

		if (!task.target || !task.target.IsAlive()) {
			effect.tasks.pop_front();
			if (!effect.tasks.empty()) {
				auto front{ effect.tasks.front() };
				if (front.config.teleport_on_start) {
					SetPosition(entity, GetPosition(front.target));
				}
				if (front.config.move_mode == MoveMode::Velocity) {
					entity.TryAdd<RigidBody>();
					entity.TryAdd<Transform>();
					auto& movement{ entity.TryAdd<TopDownMovement>() };
					movement.max_acceleration		  = front.config.max_acceleration;
					movement.max_deceleration		  = front.config.max_acceleration;
					movement.max_speed				  = front.config.max_speed;
					movement.keys_enabled			  = false;
					movement.only_orthogonal_movement = false;
				} else {
					entity.template Remove<TopDownMovement>();
					entity.template Remove<RigidBody>();
				}
			}
			continue;
		}

		auto pos{ GetAbsolutePosition(entity) };

		V2_float target_pos;

		if (task.config.follow_mode == FollowMode::Target) {
			target_pos = GetAbsolutePosition(task.target) + task.config.offset;
		} else if (task.config.follow_mode == FollowMode::Path) {
			PTGN_ASSERT(
				!task.config.waypoints.empty(),
				"Cannot set FollowMode::Path without providing at least one waypoint"
			);
			PTGN_ASSERT(
				task.config.stop_distance >= epsilon<float>,
				"Stopping distance cannot be negative or 0 when following waypoints"
			);

			PTGN_ASSERT(task.current_waypoint < task.config.waypoints.size());

			target_pos = task.config.waypoints[task.current_waypoint] + task.config.offset;

			auto dir{ target_pos - pos };

			if (dir.MagnitudeSquared() < task.config.stop_distance * task.config.stop_distance) {
				if (task.current_waypoint + 1 < task.config.waypoints.size()) {
					task.current_waypoint++;
				} else if (task.config.loop_path) {
					task.current_waypoint = 0;
				} else {
					effect.tasks.pop_front();
					if (!effect.tasks.empty()) {
						auto front{ effect.tasks.front() };
						if (front.config.teleport_on_start) {
							SetPosition(entity, GetPosition(front.target));
						}
						if (front.config.move_mode == MoveMode::Velocity) {
							entity.TryAdd<RigidBody>();
							entity.TryAdd<Transform>();
							auto& movement{ entity.TryAdd<TopDownMovement>() };
							movement.max_acceleration		  = front.config.max_acceleration;
							movement.max_deceleration		  = front.config.max_acceleration;
							movement.max_speed				  = front.config.max_speed;
							movement.keys_enabled			  = false;
							movement.only_orthogonal_movement = false;
						} else {
							entity.template Remove<TopDownMovement>();
							entity.template Remove<RigidBody>();
						}
					}
					continue;
				}
			}
		}

		if (task.config.move_mode == MoveMode::Velocity) {
			PTGN_ASSERT(
				entity.Has<TopDownMovement>(),
				"Entity with MoveMode::Velocity must have a TopDownMovement component"
			);
			auto& movement{ entity.Get<TopDownMovement>() };
			auto dir{ target_pos - pos };

			auto dist2{ dir.MagnitudeSquared() };

			if (task.config.stop_distance >= epsilon<float> &&
				dist2 < task.config.stop_distance * task.config.stop_distance) {
				effect.tasks.pop_front();
				if (!effect.tasks.empty()) {
					auto front{ effect.tasks.front() };
					if (front.config.teleport_on_start) {
						SetPosition(entity, GetPosition(front.target));
					}
					if (front.config.move_mode != MoveMode::Velocity) {
						entity.template Remove<TopDownMovement>();
						entity.template Remove<RigidBody>();
					}
				}
			} else {
				if (!NearlyEqual(dist2, 0.0f)) {
					auto norm_dir{ dir / std::sqrt(dist2) };
					if (!task.config.follow_x) {
						norm_dir = { 0.0f, Sign(norm_dir.y) };
					}
					if (!task.config.follow_y) {
						norm_dir = { Sign(norm_dir.x), 0.0f };
					}
					movement.Move(norm_dir);
				}
			}
		} else if (task.config.move_mode == MoveMode::Lerp) {
			PTGN_ASSERT(task.config.lerp_factor.x >= 0.0f && task.config.lerp_factor.x <= 1.0f);
			PTGN_ASSERT(task.config.lerp_factor.y >= 0.0f && task.config.lerp_factor.y <= 1.0f);

			V2_float lerp_dt{ 1.0f - std::pow(1.0f - task.config.lerp_factor.x, game.dt()),
							  1.0f - std::pow(1.0f - task.config.lerp_factor.y, game.dt()) };

			V2_float new_pos{ pos };

			if (task.config.deadzone.IsZero()) {
				new_pos = Lerp(pos, target_pos, lerp_dt);
			} else {
				// TODO: Consider adding a custom deadzone origin in the future.
				V2_float deadzone_half{ task.config.deadzone * 0.5f };

				V2_float min{ target_pos - deadzone_half };
				V2_float max{ target_pos + deadzone_half };

				if (pos.x < min.x) {
					new_pos.x = Lerp(pos.x, pos.x - (min.x - target_pos.x), lerp_dt.x);
				} else if (pos.x > max.x) {
					new_pos.x = Lerp(pos.x, pos.x + (target_pos.x - max.x), lerp_dt.x);
				}
				if (pos.y < min.y) {
					new_pos.y = Lerp(pos.y, pos.y - (min.y - target_pos.y), lerp_dt.y);
				} else if (pos.y > max.y) {
					new_pos.y = Lerp(pos.y, pos.y + (target_pos.y - max.y), lerp_dt.y);
				}
			}

			if (!task.config.follow_x) {
				new_pos.x = pos.x;
			}
			if (!task.config.follow_y) {
				new_pos.y = pos.y;
			}

			SetPosition(entity, new_pos);

			if (task.config.stop_distance >= epsilon<float>) {
				auto dir{ target_pos - new_pos };
				if (auto dist2{ dir.MagnitudeSquared() };
					dist2 < task.config.stop_distance * task.config.stop_distance) {
					effect.tasks.pop_front();
					if (!effect.tasks.empty()) {
						auto front{ effect.tasks.front() };
						if (front.config.teleport_on_start) {
							SetPosition(entity, GetPosition(front.target));
						}
						if (front.config.move_mode == MoveMode::Velocity) {
							entity.TryAdd<RigidBody>();
							entity.TryAdd<Transform>();
							auto& movement					  = entity.TryAdd<TopDownMovement>();
							movement.max_acceleration		  = front.config.max_acceleration;
							movement.max_deceleration		  = front.config.max_acceleration;
							movement.max_speed				  = front.config.max_speed;
							movement.keys_enabled			  = false;
							movement.only_orthogonal_movement = false;
						} else {
							entity.template Remove<TopDownMovement>();
							entity.template Remove<RigidBody>();
						}
					}
				}
			}
		} else {
			PTGN_ERROR("Unrecognized move mode")
		}
	}
}

} // namespace impl

void TranslateTo(
	Entity& entity, const V2_float& target_position, milliseconds duration, const Ease& ease,
	bool force
) {
	impl::AddTweenEffect<impl::TranslateEffect>(
		entity, target_position, duration, ease, force, GetPosition(entity)
	);
}

void RotateTo(
	Entity& entity, float target_angle, milliseconds duration, const Ease& ease, bool force
) {
	impl::AddTweenEffect<impl::RotateEffect>(
		entity, target_angle, duration, ease, force, GetRotation(entity)
	);
}

void ScaleTo(
	Entity& entity, const V2_float& target_scale, milliseconds duration, const Ease& ease,
	bool force
) {
	impl::AddTweenEffect<impl::ScaleEffect>(
		entity, target_scale, duration, ease, force, GetScale(entity)
	);
}

void TintTo(
	Entity& entity, const Color& target_tint, milliseconds duration, const Ease& ease, bool force
) {
	impl::AddTweenEffect<impl::TintEffect>(
		entity, target_tint, duration, ease, force, GetTint(entity)
	);
}

void FadeIn(Entity& entity, milliseconds duration, const Ease& ease, bool force) {
	return TintTo(entity, color::White, duration, ease, force);
}

void FadeOut(Entity& entity, milliseconds duration, const Ease& ease, bool force) {
	return TintTo(entity, color::Transparent, duration, ease, force);
}

void Bounce(
	Entity& entity, const V2_float& amplitude, milliseconds duration, std::int64_t total_periods,
	const Ease& ease, const V2_float& static_offset, bool force
) {
	impl::BounceImpl(entity, amplitude, duration, total_periods, ease, static_offset, force, false);
}

void SymmetricalBounce(
	Entity& entity, const V2_float& amplitude, milliseconds duration, std::int64_t total_periods,
	SymmetricalEase ease, const V2_float& static_offset, bool force
) {
	impl::BounceImpl(entity, amplitude, duration, total_periods, ease, static_offset, force, true);
}

void StopBounce(Entity& entity, bool force) {
	if (!entity.Has<impl::BounceEffect>()) {
		return;
	}

	auto& bounce{ entity.Get<impl::BounceEffect>() };
	auto& offsets{ entity.Get<impl::Offsets>() };
	offsets.bounce = {}; // or reset to bounce.static_offset

	if (force) {
		bounce.tasks.clear();
	} else if (!bounce.tasks.empty()) {
		bounce.tasks.pop_front();
		if (!bounce.tasks.empty()) {
			bounce.tasks.front().timer.Start(true);
		}
	}
}

void Shake(
	Entity& entity, float intensity, milliseconds duration, const ShakeConfig& config,
	const Ease& ease, bool force
) {
	PTGN_ASSERT(
		intensity >= -1.0f && intensity <= 1.0f, "Shake intensity must be in range [-1, 1]"
	);

	auto& comp{ entity.TryAdd<impl::ShakeEffect>() };
	entity.TryAdd<impl::Offsets>();

	float start_intensity{ 0.0f };

	bool first_task{ force || comp.tasks.empty() };

	if (first_task) {
		comp.tasks.clear();
	} else {
		start_intensity = comp.tasks.back().target_value;
	}

	RNG<std::int32_t> rng{ std::numeric_limits<std::int32_t>::min(),
						   std::numeric_limits<std::int32_t>::max() };
	std::int32_t seed{ rng() };

	auto& task = comp.tasks.emplace_back(
		start_intensity, std::clamp(start_intensity + intensity, 0.0f, 1.0f), duration, ease,
		config, seed
	);

	task.trauma = start_intensity;

	if (first_task) {
		task.timer.Start(true);
	}
}

void Shake(Entity& entity, float intensity, const ShakeConfig& config, bool force) {
	auto& comp{ entity.TryAdd<impl::ShakeEffect>() };
	entity.TryAdd<impl::Offsets>();
	PTGN_ASSERT(
		intensity >= -1.0f && intensity <= 1.0f, "Shake intensity must be in range [-1, 1]"
	);

	float start_intensity{ 0.0f };

	bool first_task{ force || comp.tasks.empty() };

	if (first_task) {
		comp.tasks.clear();
	} else {
		start_intensity = comp.tasks.back().target_value;
	}

	// If a previous instantenous shake exists with 0 duration, add to its trauma instead of
	// queueing a new shake effect.
	if (!comp.tasks.empty()) {
		if (auto& back_task{ comp.tasks.back() }; back_task.duration == milliseconds{ 0 }) {
			back_task.trauma = std::clamp(back_task.trauma + intensity, 0.0f, 1.0f);
			return;
		}
	}

	RNG<std::int32_t> rng{ std::numeric_limits<std::int32_t>::min(),
						   std::numeric_limits<std::int32_t>::max() };
	std::int32_t seed{ rng() };

	auto& task = comp.tasks.emplace_back(
		start_intensity, std::clamp(start_intensity + intensity, 0.0f, 1.0f), milliseconds{ 0 },
		SymmetricalEase::None, config, seed
	);

	task.trauma = intensity;

	if (first_task) {
		task.timer.Start(true);
	}
}

void StopShake(Entity& entity, bool force) {
	if (!entity.Has<impl::ShakeEffect>()) {
		return;
	}

	auto& shake{ entity.Get<impl::ShakeEffect>() };
	auto& offsets{ entity.Get<impl::Offsets>() };
	offsets.shake = {};

	if (entity.Has<impl::CameraInfo>()) {
		auto& camera_info{ entity.Get<impl::CameraInfo>() };
		camera_info.SetViewDirty();
	}

	if (force) {
		shake.tasks.clear();
	} else if (!shake.tasks.empty()) {
		shake.tasks.pop_front();
		if (!shake.tasks.empty()) {
			shake.tasks.front().timer.Start(true);
		}
	}
}

void StartFollow(Entity entity, Entity target, FollowConfig config, bool force) {
	PTGN_ASSERT(config.lerp_factor.x >= 0.0f && config.lerp_factor.x <= 1.0f);
	PTGN_ASSERT(config.lerp_factor.y >= 0.0f && config.lerp_factor.y <= 1.0f);

	auto& comp{ entity.TryAdd<impl::FollowEffect>() };

	bool first_task{ force || comp.tasks.empty() };

	if (first_task) {
		comp.tasks.clear();
	}

	if (config.teleport_on_start) {
		SetPosition(entity, GetPosition(target));
	}

	if (config.move_mode == MoveMode::Velocity) {
		// TODO: Consider making the movement system not require enabling an entity.
		entity.TryAdd<RigidBody>();
		entity.TryAdd<Transform>();
		auto& movement{ entity.TryAdd<TopDownMovement>() };
		movement.max_acceleration		  = config.max_acceleration;
		movement.max_deceleration		  = config.max_acceleration;
		movement.max_speed				  = config.max_speed;
		movement.keys_enabled			  = false;
		movement.only_orthogonal_movement = false;
	}

	comp.tasks.emplace_back(target, config);
}

void StopFollow(Entity entity, bool force) {
	if (!entity.Has<impl::FollowEffect>()) {
		return;
	}

	auto& follow{ entity.Get<impl::FollowEffect>() };

	if (force) {
		follow.tasks.clear();
	} else if (!follow.tasks.empty()) {
		follow.tasks.pop_front();
		if (!follow.tasks.empty()) {
			auto front{ follow.tasks.front() };
			if (front.config.teleport_on_start) {
				SetPosition(entity, GetPosition(front.target));
			}
			if (front.config.move_mode == MoveMode::Velocity) {
				entity.TryAdd<RigidBody>();
				entity.TryAdd<Transform>();
				auto& movement					  = entity.TryAdd<TopDownMovement>();
				movement.max_acceleration		  = front.config.max_acceleration;
				movement.max_deceleration		  = front.config.max_acceleration;
				movement.max_speed				  = front.config.max_speed;
				movement.keys_enabled			  = false;
				movement.only_orthogonal_movement = false;
			} else {
				entity.template Remove<TopDownMovement>();
				entity.template Remove<RigidBody>();
			}
		}
	}
}

} // namespace ptgn