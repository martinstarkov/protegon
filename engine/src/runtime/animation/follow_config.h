#pragma once

#include <optional>

#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

enum class MoveMode {
	Lerp,
	Velocity
};
PTGN_REFLECT_ENUM(MoveMode);

struct FollowConfig {
	MoveMode move_mode{ MoveMode::Lerp };

	/// @brief Follow along the x-axis.
	bool follow_x{ true };

	/// @brief Follow along the y-axis.
	bool follow_y{ true };

	/// @brief Teleport to the target when the following starts.
	bool teleport_on_start{ false };

	/// @brief What is considered close enough to the target, nullopt means that the follow will
	/// never complete.
	std::optional<float> stop_distance;

	/// @brief Value from 0 to 1 which determines how aggressively the move mode interpolates. Only
	/// applicable when move mode is set to lerp.
	V2_float lerp{ 0.9f, 0.9f };

	/// @brief Distance below which the entity snaps to the target position instead of lerping. Only
	/// applicable when move mode is set to lerp.
	float snap_distance{ 0.1f };

	/// @brief Area around target within which no following occurs.
	V2_float deadzone;

	/// @brief Offset from the target position that is followed (if zero, uses target transform).
	V2_float offset;

	/// @brief Only applicable when move mode is set to velocity.
	float max_speed{ 4.0f * 60.0f };
	float max_acceleration{ 20.0f * 60.0f };

	bool operator==(const FollowConfig&) const = default;

	PTGN_REFLECT(
		FollowConfig, move_mode, follow_x, follow_y, teleport_on_start, stop_distance, lerp,
		deadzone, offset, max_speed, max_acceleration
	)
};

struct TargetFollowConfig : public FollowConfig {
	explicit TargetFollowConfig(const FollowConfig& config) : FollowConfig{ config } {}

	using FollowConfig::FollowConfig;

	bool operator==(const TargetFollowConfig&) const = default;
};

struct PathFollowConfig : public FollowConfig {
	bool loop_path{ true };

	PathFollowConfig() : FollowConfig{ .move_mode = MoveMode::Velocity, .stop_distance = 10.0f } {}

	bool operator==(const PathFollowConfig&) const = default;

	PTGN_REFLECT_DERIVED(PathFollowConfig, FollowConfig, loop_path)
};

} // namespace ptgn