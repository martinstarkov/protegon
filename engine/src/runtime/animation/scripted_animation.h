#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/angle.h"
#include "core/math/easing.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/time.h"
#include "runtime/animation/follow_config.h"
#include "runtime/animation/shake_config.h"
#include "runtime/ecs/entity.h"
#include "runtime/scripting/builtin_scripts.h"

namespace ptgn {

namespace animation_channel {

inline const SequenceChannelKey Position{ "transform.position" };
inline const SequenceChannelKey Rotation{ "transform.rotation" };
inline const SequenceChannelKey Scale{ "transform.scale" };
inline const SequenceChannelKey Tint{ "graphics.tint" };
inline const SequenceChannelKey Bounce{ "transform.bounce" };
inline const SequenceChannelKey Shake{ "transform.shake" };
inline const SequenceChannelKey Follow{ "movement.follow" };

} // namespace animation_channel

namespace impl {

template <EntityType E, typename F>
[[nodiscard]] std::vector<SequenceHandle> CreateAnimations(
	std::span<const E> entities, F&& create_animation
) {
	std::vector<SequenceHandle> handles;
	handles.reserve(entities.size());
	for (const auto& entity : entities) {
		handles.push_back(std::invoke(create_animation, entity));
	}
	return handles;
}

template <EntityType E, typename T, typename F>
[[nodiscard]] std::vector<SequenceHandle> CreateAnimations(
	std::span<const E> entities, std::span<const T> targets, F&& create_animation
) {
	PTGN_ASSERT(targets.size() == entities.size(), "Target count must match entity count");

	std::vector<SequenceHandle> handles;
	handles.reserve(entities.size());
	for (std::size_t i{ 0 }; i < entities.size(); ++i) {
		handles.push_back(
			std::invoke(create_animation, entities[i], targets[i])
		);
	}
	return handles;
}

} // namespace impl

SequenceHandle TranslateTo(
	Entity entity, V2_float target_position, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true, bool relative = false
);

SequenceHandle RotateTo(
	Entity entity, Degrees target_angle, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true, bool shortest_path = true, bool relative = false
);

SequenceHandle ScaleTo(
	Entity entity, V2_float target_scale, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true, bool relative = false
);

SequenceHandle TintTo(
	Entity entity, Color target_tint, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true
);

template <EntityType E>
std::vector<SequenceHandle> TranslateTo(
	std::span<const E> entities, V2_float target_position, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true, bool relative = false
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return TranslateTo(entity, target_position, duration, ease, force, relative);
	});
}

template <EntityType E>
std::vector<SequenceHandle> TranslateTo(
	std::span<const E> entities, std::span<const V2_float> target_positions,
	milliseconds duration, Ease ease = Ease::Linear, bool force = true,
	bool relative = false
) {
	return impl::CreateAnimations(
		entities, target_positions,
		[&](Entity entity, const V2_float& target) {
			return TranslateTo(entity, target, duration, ease, force, relative);
		}
	);
}

template <EntityType E>
std::vector<SequenceHandle> RotateTo(
	std::span<const E> entities, Degrees target_angle, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true, bool shortest_path = true,
	bool relative = false
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return RotateTo(
			entity, target_angle, duration, ease, force, shortest_path, relative
		);
	});
}

template <EntityType E>
std::vector<SequenceHandle> RotateTo(
	std::span<const E> entities, std::span<const Degrees> target_angles,
	milliseconds duration, Ease ease = Ease::Linear, bool force = true,
	bool shortest_path = true, bool relative = false
) {
	return impl::CreateAnimations(
		entities, target_angles,
		[&](Entity entity, const Degrees& target) {
			return RotateTo(
				entity, target, duration, ease, force, shortest_path, relative
			);
		}
	);
}

template <EntityType E>
std::vector<SequenceHandle> ScaleTo(
	std::span<const E> entities, V2_float target_scale, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true, bool relative = false
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return ScaleTo(entity, target_scale, duration, ease, force, relative);
	});
}

template <EntityType E>
std::vector<SequenceHandle> ScaleTo(
	std::span<const E> entities, std::span<const V2_float> target_scales,
	milliseconds duration, Ease ease = Ease::Linear, bool force = true,
	bool relative = false
) {
	return impl::CreateAnimations(
		entities, target_scales,
		[&](Entity entity, const V2_float& target) {
			return ScaleTo(entity, target, duration, ease, force, relative);
		}
	);
}

template <EntityType E>
std::vector<SequenceHandle> TintTo(
	std::span<const E> entities, Color target_tint, milliseconds duration,
	Ease ease = Ease::Linear, bool force = true
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return TintTo(entity, target_tint, duration, ease, force);
	});
}

template <EntityType E>
std::vector<SequenceHandle> TintTo(
	std::span<const E> entities, std::span<const Color> target_tints,
	milliseconds duration, Ease ease = Ease::Linear, bool force = true
) {
	return impl::CreateAnimations(
		entities, target_tints,
		[&](Entity entity, const Color& target) {
			return TintTo(entity, target, duration, ease, force);
		}
	);
}

SequenceHandle FadeIn(
	Entity entity, milliseconds duration, Ease ease = Ease::Linear, bool force = true,
	bool start_transparent = false
);

SequenceHandle FadeOut(
	Entity entity, milliseconds duration, Ease ease = Ease::Linear, bool force = true,
	bool start_opaque = false
);

template <EntityType E>
std::vector<SequenceHandle> FadeIn(
	std::span<const E> entities, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true, bool start_transparent = false
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return FadeIn(entity, duration, ease, force, start_transparent);
	});
}

template <EntityType E>
std::vector<SequenceHandle> FadeOut(
	std::span<const E> entities, milliseconds duration, Ease ease = Ease::Linear,
	bool force = true, bool start_opaque = false
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return FadeOut(entity, duration, ease, force, start_opaque);
	});
}

/// Runs one complete bounce cycle per period.
SequenceHandle Bounce(
	Entity entity, V2_float amplitude, milliseconds period,
	std::optional<std::size_t> total_periods = std::nullopt,
	Ease ease = Ease::Linear, V2_float static_offset = {}, bool force = true
);

/// Runs a positive and negative bounce within each period.
SequenceHandle SymmetricalBounce(
	Entity entity, V2_float amplitude, milliseconds period,
	std::optional<std::size_t> total_periods = std::nullopt,
	Ease ease = Ease::Linear, V2_float static_offset = {}, bool force = true
);

template <EntityType E>
std::vector<SequenceHandle> Bounce(
	std::span<const E> entities, V2_float amplitude, milliseconds period,
	std::optional<std::size_t> total_periods = std::nullopt,
	Ease ease = Ease::Linear, V2_float static_offset = {}, bool force = true
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return Bounce(
			entity, amplitude, period, total_periods, ease, static_offset, force
		);
	});
}

template <EntityType E>
std::vector<SequenceHandle> SymmetricalBounce(
	std::span<const E> entities, V2_float amplitude, milliseconds period,
	std::optional<std::size_t> total_periods = std::nullopt,
	Ease ease = Ease::Linear, V2_float static_offset = {}, bool force = true
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return SymmetricalBounce(
			entity, amplitude, period, total_periods, ease, static_offset, force
		);
	});
}

void StopBounce(Entity entity, bool force = true);

template <EntityType E>
void StopBounce(std::span<const E> entities, bool force = true) {
	std::ranges::for_each(entities, [force](const auto& entity) {
		StopBounce(entity, force);
	});
}

/// Adds intensity to the entity's current shake trauma.
///
/// A null duration holds the resulting trauma until StopShake is called. A finite duration ramps to
/// the target and then either resets immediately or recovers using ShakeConfig::recovery_speed.
SequenceHandle Shake(
	Entity entity, float intensity, std::optional<milliseconds> duration,
	const ShakeConfig& config = {}, Ease ease = Ease::None, bool force = true,
	bool reset_trauma = false
);

SequenceHandle Shake(
	Entity entity, float intensity, std::optional<milliseconds> duration,
	const ShakeConfig& config, bool force, bool reset_trauma = false
);

/// Adds trauma immediately and then recovers naturally.
SequenceHandle Shake(
	Entity entity, float intensity, const ShakeConfig& config = {}, bool force = true
);

template <EntityType E>
std::vector<SequenceHandle> Shake(
	std::span<const E> entities, float intensity, milliseconds duration,
	const ShakeConfig& config = {}, Ease ease = Ease::None, bool force = true,
	bool reset_trauma = false
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return Shake(
			entity, intensity, duration, config, ease, force, reset_trauma
		);
	});
}

template <EntityType E>
std::vector<SequenceHandle> Shake(
	std::span<const E> entities, float intensity, milliseconds duration,
	const ShakeConfig& config, bool force, bool reset_trauma = false
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return Shake(
			entity, intensity, duration, config, Ease::None, force, reset_trauma
		);
	});
}

template <EntityType E>
std::vector<SequenceHandle> Shake(
	std::span<const E> entities, float intensity, const ShakeConfig& config = {},
	bool force = true
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return Shake(entity, intensity, config, force);
	});
}

void StopShake(Entity entity, bool force = true);

template <EntityType E>
void StopShake(std::span<const E> entities, bool force = true) {
	std::ranges::for_each(entities, [force](const auto& entity) {
		StopShake(entity, force);
	});
}

/// Simple constant speed follow behavior from the script sequencing demo.
SequenceHandle Follow(
	Entity entity, Entity target, float speed, float stopping_distance = 2.0f,
	bool force = true
);

template <EntityType E>
std::vector<SequenceHandle> Follow(
	std::span<const E> entities, Entity target, float speed,
	float stopping_distance = 2.0f, bool force = true
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return Follow(entity, target, speed, stopping_distance, force);
	});
}

SequenceHandle StartFollow(
	Entity entity, Entity target, const TargetFollowConfig& config = {},
	bool force = true
);

SequenceHandle StartFollow(
	Entity entity, std::span<const V2_float> waypoints,
	const PathFollowConfig& config = {}, bool force = true,
	bool reset_waypoint_index = false
);

template <EntityType E>
std::vector<SequenceHandle> StartFollow(
	std::span<const E> entities, Entity target,
	const TargetFollowConfig& config = {}, bool force = true
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return StartFollow(entity, target, config, force);
	});
}

template <EntityType E>
std::vector<SequenceHandle> StartFollow(
	std::span<const E> entities, std::span<const V2_float> waypoints,
	const PathFollowConfig& config = {}, bool force = true,
	bool reset_waypoint_index = false
) {
	return impl::CreateAnimations(entities, [&](Entity entity) {
		return StartFollow(
			entity, waypoints, config, force, reset_waypoint_index
		);
	});
}

void StopFollow(
	Entity entity, bool force = true, bool reset_previous_waypoints = false
);

template <EntityType E>
void StopFollow(
	std::span<const E> entities, bool force = true,
	bool reset_previous_waypoints = false
) {
	std::ranges::for_each(entities, [&](const auto& entity) {
		StopFollow(entity, force, reset_previous_waypoints);
	});
}

} // namespace ptgn
