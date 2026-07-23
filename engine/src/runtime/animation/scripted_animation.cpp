#include "runtime/animation/scripted_animation.h"

#include <algorithm>
#include <ranges>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/math/tolerance.h"
#include "runtime/graphics/tint.h"

namespace ptgn {

namespace {

[[nodiscard]] ReentryMode AnimationReentry(bool force) {
	return force ? ReentryMode::Restart : ReentryMode::Queue;
}

[[nodiscard]] SequenceHandle RunAnimation(
	Entity entity, SequenceChannelKey channel, ScriptSequence sequence, bool force
) {
	if (!entity) {
		return {};
	}
	sequence.Transient();
	return script_runtime::RunInChannel(
		entity, std::move(channel), std::move(sequence), AnimationReentry(force)
	);
}

[[nodiscard]] SequenceHandle BounceImpl(
	Entity entity, V2_float amplitude, milliseconds period,
	std::optional<std::size_t> total_periods, Ease ease,
	V2_float static_offset, bool force, bool symmetrical
) {
	PTGN_ASSERT(period > 0ms, "Bounce period must be positive");

	if (total_periods.has_value() && total_periods.value() == 0) {
		impl::ResetBounceAnimationState(entity);
		return {};
	}

	ScriptSequence sequence{ symmetrical ? "Symmetrical Bounce" : "Bounce" };
	sequence.During(
		static_cast<float>(period.count()),
		BounceScript{ amplitude, static_offset, symmetrical }
	).Ease(ease);

	if (total_periods.has_value()) {
		sequence.Repeat(static_cast<int>(total_periods.value() - 1));
	} else {
		sequence.Infinite();
	}

	return RunAnimation(
		entity, animation_channel::Bounce, std::move(sequence), force
	);
}

} // namespace

SequenceHandle TranslateTo(
	Entity entity, V2_float target_position, milliseconds duration, Ease ease,
	bool force, bool relative
) {
	PTGN_ASSERT(duration > 0ms, "Translation duration must be positive");

	ScriptSequence sequence{ "Translate To" };
	sequence.During(
		static_cast<float>(duration.count()),
		MoveToScript{ target_position, relative }
	).Ease(ease);

	return RunAnimation(
		entity, animation_channel::Position, std::move(sequence), force
	);
}

SequenceHandle RotateTo(
	Entity entity, Degrees target_angle, milliseconds duration, Ease ease,
	bool force, bool shortest_path, bool relative
) {
	PTGN_ASSERT(duration > 0ms, "Rotation duration must be positive");

	ScriptSequence sequence{ "Rotate To" };
	sequence.During(
		static_cast<float>(duration.count()),
		RotateToScript{ target_angle.value, shortest_path, relative }
	).Ease(ease);

	return RunAnimation(
		entity, animation_channel::Rotation, std::move(sequence), force
	);
}

SequenceHandle ScaleTo(
	Entity entity, V2_float target_scale, milliseconds duration, Ease ease,
	bool force, bool relative
) {
	PTGN_ASSERT(duration > 0ms, "Scale duration must be positive");

	ScriptSequence sequence{ "Scale To" };
	sequence.During(
		static_cast<float>(duration.count()),
		ScaleToScript{ target_scale, relative }
	).Ease(ease);

	return RunAnimation(
		entity, animation_channel::Scale, std::move(sequence), force
	);
}

SequenceHandle TintTo(
	Entity entity, Color target_tint, milliseconds duration, Ease ease, bool force
) {
	PTGN_ASSERT(duration > 0ms, "Tint duration must be positive");

	ScriptSequence sequence{ "Tint To" };
	sequence.During(
		static_cast<float>(duration.count()),
		TintToScript{ target_tint }
	).Ease(ease);

	return RunAnimation(
		entity, animation_channel::Tint, std::move(sequence), force
	);
}

SequenceHandle FadeIn(
	Entity entity, milliseconds duration, Ease ease, bool force,
	bool start_transparent
) {
	if (!entity) {
		return {};
	}
	if (start_transparent) {
		entity.Add<Tint>(color::Transparent);
	}
	return TintTo(entity, color::White, duration, ease, force);
}

SequenceHandle FadeOut(
	Entity entity, milliseconds duration, Ease ease, bool force, bool start_opaque
) {
	if (!entity) {
		return {};
	}
	if (start_opaque) {
		entity.Add<Tint>(color::White);
	}
	return TintTo(entity, color::Transparent, duration, ease, force);
}

SequenceHandle Bounce(
	Entity entity, V2_float amplitude, milliseconds period,
	std::optional<std::size_t> total_periods, Ease ease,
	V2_float static_offset, bool force
) {
	return BounceImpl(
		entity, amplitude, period, total_periods, ease, static_offset, force, false
	);
}

SequenceHandle SymmetricalBounce(
	Entity entity, V2_float amplitude, milliseconds period,
	std::optional<std::size_t> total_periods, Ease ease,
	V2_float static_offset, bool force
) {
	return BounceImpl(
		entity, amplitude, period, total_periods, ease, static_offset, force, true
	);
}

void StopBounce(Entity entity, bool force) {
	if (!entity) {
		return;
	}
	script_runtime::StopChannel(
		entity, animation_channel::Bounce,
		force ? SequenceStopMode::All : SequenceStopMode::Current
	);
	if (force) {
		impl::ResetBounceAnimationState(entity);
	}
}

SequenceHandle Shake(
	Entity entity, float intensity, std::optional<milliseconds> duration,
	const ShakeConfig& config, Ease ease, bool force, bool reset_trauma
) {
	PTGN_ASSERT(
		intensity >= -1.0f && intensity <= 1.0f,
		"Shake intensity must be in range [-1, 1]"
	);
	PTGN_ASSERT(
		!duration.has_value() || duration.value() >= 0ms,
		"Shake duration cannot be negative"
	);

	ScriptSequence sequence{ "Shake" };
	if (!duration.has_value()) {
		sequence.Forever(ShakeScript{ intensity, config, false });
	} else {
		if (duration.value() == 0ms) {
			sequence.Then(AddShakeTraumaScript{ intensity, config });
		} else {
			sequence.During(
				static_cast<float>(duration->count()),
				ShakeScript{ intensity, config, false }
			).Ease(ease);
		}

		if (reset_trauma) {
			sequence.Then(ResetShakeScript{});
		} else {
			sequence.UntilComplete(RecoverShakeScript{ config });
		}
	}

	return RunAnimation(
		entity, animation_channel::Shake, std::move(sequence), force
	);
}

SequenceHandle Shake(
	Entity entity, float intensity, std::optional<milliseconds> duration,
	const ShakeConfig& config, bool force, bool reset_trauma
) {
	return Shake(
		entity, intensity, duration, config, Ease::None, force, reset_trauma
	);
}

SequenceHandle Shake(
	Entity entity, float intensity, const ShakeConfig& config, bool force
) {
	return Shake(entity, intensity, 0ms, config, Ease::None, force, false);
}

void StopShake(Entity entity, bool force) {
	if (!entity) {
		return;
	}
	script_runtime::StopChannel(
		entity, animation_channel::Shake,
		force ? SequenceStopMode::All : SequenceStopMode::Current
	);
	if (force) {
		impl::ResetShakeAnimationState(entity);
	}
}

SequenceHandle Follow(
	Entity entity, Entity target, float speed, float stopping_distance, bool force
) {
	ScriptSequence sequence{ "Follow Target" };
	sequence.UntilComplete(FollowTargetScript{ target, speed, stopping_distance });
	return RunAnimation(
		entity, animation_channel::Follow, std::move(sequence), force
	);
}

SequenceHandle StartFollow(
	Entity entity, Entity target, const TargetFollowConfig& config, bool force
) {
	ScriptSequence sequence{ "Follow Entity" };
	sequence.UntilComplete(FollowEntityScript{ target, config });
	return RunAnimation(
		entity, animation_channel::Follow, std::move(sequence), force
	);
}

SequenceHandle StartFollow(
	Entity entity, std::span<const V2_float> waypoints,
	const PathFollowConfig& config, bool force, bool reset_waypoint_index
) {
	PTGN_ASSERT(!waypoints.empty(), "Cannot follow an empty waypoint path");
	PTGN_ASSERT(
		config.stop_distance.has_value() &&
			config.stop_distance.value() >= kEpsilon<float>,
		"Waypoint following requires a positive stopping distance"
	);

	ScriptSequence sequence{ "Follow Path" };
	sequence.UntilComplete(FollowPathScript{
		std::ranges::to<std::vector<V2_float>>(waypoints),
		config,
		reset_waypoint_index
	});
	return RunAnimation(
		entity, animation_channel::Follow, std::move(sequence), force
	);
}

void StopFollow(
	Entity entity, bool force, bool reset_previous_waypoints
) {
	if (!entity) {
		return;
	}
	script_runtime::StopChannel(
		entity, animation_channel::Follow,
		force ? SequenceStopMode::All : SequenceStopMode::Current
	);
	if (force || reset_previous_waypoints) {
		impl::ResetFollowAnimationState(entity, reset_previous_waypoints);
	}
}

} // namespace ptgn
