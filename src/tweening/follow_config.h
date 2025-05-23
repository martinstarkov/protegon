#pragma once

#include <vector>

#include "math/vector2.h"

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
};

} // namespace ptgn