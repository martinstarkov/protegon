#pragma once

#include <vector>

#include "math/easing.h"
#include "math/vector2.h"
#include "serialization/enum.h"
#include "serialization/serializable.h"

namespace ptgn {

enum class MoveMode {
	Ease,
	SmoothLerp,
	Velocity
};

namespace impl {

struct FollowConfig {
	MoveMode move_mode{ MoveMode::Ease };

	bool follow_x{ true };
	bool follow_y{ true };

	bool teleport_on_start{ false };

	// What is considered close enough to the target.
	float stop_distance{ 10.0f };

	// Applies when MoveMode::Ease.
	Ease ease{ SymmetricalEase::Linear };

	// Applies when MoveMode::SmoothLerp.
	V2_float smooth_lerp_factor{ 1.0f, 1.0f };

	V2_float deadzone;

	V2_float offset;

	float max_speed{ 4.0f * 60.0f };
	float max_acceleration{ 20.0f * 60.0f };

	bool operator==(const FollowConfig&) const = default;

	PTGN_SERIALIZER_REGISTER(
		FollowConfig, move_mode, follow_x, follow_y, teleport_on_start, stop_distance, ease,
		smooth_lerp_factor, deadzone, offset, max_speed, max_acceleration
	)
};

} // namespace impl

struct TargetFollowConfig : public impl::FollowConfig {
	bool operator==(const TargetFollowConfig&) const = default;
};

struct PathFollowConfig : public impl::FollowConfig {
	bool loop_path{ false };

	bool operator==(const PathFollowConfig&) const = default;

	friend void to_json(json& j, const PathFollowConfig& config) {
		to_json(j, static_cast<const impl::FollowConfig&>(config));
		j["loop_path"] = config.loop_path;
	}

	friend void from_json(const json& j, PathFollowConfig& config) {
		from_json(j, static_cast<impl::FollowConfig&>(config));
		j.at("loop_path").get_to(config.loop_path);
	}
};

PTGN_SERIALIZER_REGISTER_ENUM(
	MoveMode, { { MoveMode::Ease, "ease" },
				{ MoveMode::SmoothLerp, "smooth_lerp" },
				{ MoveMode::Velocity, "velocity" } }
);

} // namespace ptgn