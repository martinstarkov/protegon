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

namespace impl {

struct FollowConfig {
	MoveMode move_mode{ MoveMode::Lerp };

	// Follow along the x-axis.
	bool follow_x{ true };

	// Follow along the y-axis.
	bool follow_y{ true };

	// Teleport to the target when the following starts.
	bool teleport_on_start{ false };

	// What is considered close enough to the target, -1 means that the follow will never complete.
	float stop_distance{ -1.0f };

	// Value from 0 to 1 which determines how aggressively the move mode interpolates. Only
	// applicable when move mode is set to lerp.
	V2_float lerp{ 0.9f, 0.9f };

	// Area around target within which no following occurs.
	V2_float deadzone;

	// Offset from the target position that is followed (if zero, uses target transform).
	V2_float offset;

	// Only applicable when move mode is set to velocity.
	float max_speed{ 4.0f * 60.0f };
	float max_acceleration{ 20.0f * 60.0f };

	bool operator==(const FollowConfig&) const = default;

	PTGN_SERIALIZER_REGISTER(
		FollowConfig, move_mode, follow_x, follow_y, teleport_on_start, stop_distance, lerp,
		deadzone, offset, max_speed, max_acceleration
	)
};

} // namespace impl

struct TargetFollowConfig : public impl::FollowConfig {
	bool operator==(const TargetFollowConfig&) const = default;
};

struct PathFollowConfig : public impl::FollowConfig {
	bool loop_path{ true };

	PathFollowConfig() :
		impl::FollowConfig{ .move_mode = MoveMode::Velocity, .stop_distance = 10.0f } {}

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
	MoveMode, { { MoveMode::Lerp, "lerp" }, { MoveMode::Velocity, "velocity" } }
);

} // namespace ptgn