#pragma once

#include <cstdint>
#include <optional>

#include "core/input/key.h"
#include "core/util/time.h"
#include "core/util/timer.h"
#include "runtime/physics/platformer_event.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "serialization/serialize.h"

namespace ptgn {

struct JumpBuffer {
	milliseconds duration{ 150 };

	PTGN_REFLECT(JumpBuffer, duration)
};

struct CoyoteTime {
	milliseconds duration{ 150 };

	PTGN_REFLECT(CoyoteTime, duration)
};

struct AirJumps {
	std::uint32_t count{ 1 };

	PTGN_REFLECT(AirJumps, count)
};

struct VariableJumpHeight {
	float gravity_multiplier{ 12.0f };

	PTGN_REFLECT(VariableJumpHeight, gravity_multiplier)
};

struct FastFall {
	Key key{ Key::S };
	float gravity_multiplier{ 12.0f };

	PTGN_REFLECT(FastFall, key, gravity_multiplier)
};

void RegisterBuiltInPlatformerJumpControllers();

struct PlatformerJump {
	Key jump_key{ Key::W };

	float default_gravity_scale{ 5.0f };
	float upward_gravity_multiplier{ 5.0f };
	float downward_gravity_multiplier{ 6.0f };
	float terminal_velocity{ 36000.0f };
	float jump_height{ 150.0f };
	float time_to_jump_apex{ 1.0f };

	std::optional<CoyoteTime> coyote_time{ CoyoteTime{} };
	std::optional<JumpBuffer> jump_buffer{ JumpBuffer{} };
	std::optional<AirJumps> air_jumps;
	std::optional<VariableJumpHeight> variable_jump_height{ VariableJumpHeight{} };
	std::optional<FastFall> fast_fall{ FastFall{} };

	void Update(PlatformerJumpContext& ctx);

	PTGN_REFLECT(
		PlatformerJump, jump_key, default_gravity_scale, upward_gravity_multiplier,
		downward_gravity_multiplier, terminal_velocity, jump_height, time_to_jump_apex,
		coyote_time, jump_buffer, air_jumps, variable_jump_height, fast_fall
	)

private:
	bool jumping_{ false };
	std::uint32_t air_jumps_used_{ 0 };
	ManualTimer jump_buffer_timer_;
	ManualTimer coyote_timer_;

	void Jump(PlatformerJumpContext& ctx, event::PlayerJumpType type);
	void CalculateGravity(PlatformerJumpContext& ctx) const;

	[[nodiscard]] std::optional<event::PlayerJumpType> GetAvailableJump(
		const PlatformerJumpContext& ctx
	) const;
};

} // namespace ptgn
