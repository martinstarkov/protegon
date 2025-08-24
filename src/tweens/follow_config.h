#pragma once

#include <vector>

#include "math/vector2.h"
#include "serialization/enum.h"
#include "serialization/serializable.h"

namespace ptgn {

enum class MoveMode {
	Lerp,
	Velocity
};

class TargetFollowConfig {
public:
	MoveMode move_mode{ MoveMode::Lerp };

	bool follow_x{ true };
	bool follow_y{ true };

	bool teleport_on_start{ false };

	bool loop_path{ false };

	float stop_distance{ -1.0f }; // Never stop following the target.

	V2_float lerp_factor{ 1.0f, 1.0f };

	V2_float deadzone;

	V2_float offset;

	float max_speed{ 4.0f * 60.0f };
	float max_acceleration{ 20.0f * 60.0f };

	bool operator==(const TargetFollowConfig&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		TargetFollowConfig, move_mode, follow_x, follow_y, teleport_on_start, loop_path,
		stop_distance, lerp_factor, deadzone, offset, max_speed, max_acceleration
	)
};

class PathFollowConfig {
public:
	MoveMode move_mode{ MoveMode::Lerp };

	bool follow_x{ true };
	bool follow_y{ true };

	bool teleport_on_start{ false };

	bool loop_path{ false };

	// What is considered close enough to the target.
	float stop_distance{ 10.0f };

	V2_float lerp_factor{ 1.0f, 1.0f };

	V2_float deadzone;

	V2_float offset;

	float max_speed{ 4.0f * 60.0f };
	float max_acceleration{ 20.0f * 60.0f };

	bool operator==(const PathFollowConfig&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		PathFollowConfig, move_mode, follow_x, follow_y, teleport_on_start, loop_path,
		stop_distance, lerp_factor, deadzone, offset, max_speed, max_acceleration
	)
};

PTGN_SERIALIZER_REGISTER_ENUM(
	MoveMode, { { MoveMode::Lerp, "lerp" }, { MoveMode::Velocity, "velocity" } }
);

} // namespace ptgn