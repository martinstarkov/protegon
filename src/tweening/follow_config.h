#pragma once

#include <vector>

#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

enum class MoveMode {
	Lerp,
	Velocity
};

enum class FollowMode {
	Target,
	Path
};

class FollowConfig {
public:
	MoveMode move_mode{ MoveMode::Lerp };
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

	friend bool operator==(const FollowConfig& a, const FollowConfig& b) {
		return a.move_mode == b.move_mode && a.follow_mode == b.follow_mode &&
			   a.follow_x == b.follow_x && a.follow_y == b.follow_y &&
			   a.teleport_on_start == b.teleport_on_start && a.loop_path == b.loop_path &&
			   NearlyEqual(a.stop_distance, b.stop_distance) && a.lerp_factor == b.lerp_factor &&
			   a.deadzone == b.deadzone && a.offset == b.offset &&
			   NearlyEqual(a.max_speed, b.max_speed) &&
			   NearlyEqual(a.max_acceleration, b.max_acceleration) && a.waypoints == b.waypoints;
	}

	friend bool operator!=(const FollowConfig& a, const FollowConfig& b) {
		return !(a == b);
	}

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		FollowConfig, move_mode, follow_mode, follow_x, follow_y, teleport_on_start, waypoints,
		loop_path, stop_distance, lerp_factor, deadzone, offset, max_speed, max_acceleration
	)
};

} // namespace ptgn