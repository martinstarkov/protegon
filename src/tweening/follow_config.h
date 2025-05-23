#pragma once

#include <vector>

#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

enum class MoveMode {
	Snap,
	Lerp,
	Velocity
};

enum class FollowMode {
	Target,
	Path
};

class FollowConfig {
public:
	MoveMode move_mode{ MoveMode::Snap };
	FollowMode follow_mode{ FollowMode::Target };

	bool follow_x{ true };
	bool follow_y{ true };

	bool teleport_on_start{ false };

	std::vector<V2_float> waypoints;

	bool loop_path{ false };

	float stop_distance{ -1.0f }; // Never stop following the target.

	V2_float lerp_factor{ 1.0f, 1.0f };

	V2_float deadzone;

	V2_float offset;

	float max_speed{ 4.0f * 60.0f };
	float max_acceleration{ 20.0f * 60.0f };

	PTGN_SERIALIZER_REGISTER(
		FollowConfig, move_mode, follow_mode, follow_x, follow_y, teleport_on_start, waypoints,
		loop_path, stop_distance, lerp_factor, deadzone, offset, max_speed, max_acceleration
	)
};

} // namespace ptgn