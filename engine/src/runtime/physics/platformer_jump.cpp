#include "runtime/physics/platformer_jump.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "core/assert.h"
#include "core/math/tolerance.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/platformer_event.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event.h"
#include "runtime/scene/scene_input.h"

namespace ptgn {

void RegisterBuiltInPlatformerJumpControllers() {
	static const AutoPlatformerJumpControllerRegistration<PlatformerJump> registration{
		"standard",
		"Standard",
		"Built In",
		"Built-in configurable platformer jump controller"
	};
	(void)registration;
}

void PlatformerJump::Update(PlatformerJumpContext& ctx) {
	coyote_timer_.Update(ctx.dt);
	jump_buffer_timer_.Update(ctx.dt);

	bool grounded{ ctx.platformer.IsGrounded() };
	bool was_grounded{ ctx.platformer.WasGrounded() };

	if (grounded) {
		jumping_ = false;
		air_jumps_used_ = 0;
		coyote_timer_.Stop();
	} else if (was_grounded && coyote_time.has_value() && !jumping_) {
		coyote_timer_.Start();
	}

	CalculateGravity(ctx);

	bool pressed_jump{ ctx.scene.ctx().input.KeyPressed(jump_key) };
	if (pressed_jump) {
		if (auto type{ GetAvailableJump(ctx) }) {
			Jump(ctx, type.value());
			return;
		}

		if (jump_buffer.has_value()) {
			jump_buffer_timer_.Start();
		}
	}

	if (!jump_buffer.has_value() || !jump_buffer_timer_.IsRunning() ||
		jump_buffer_timer_.Completed(jump_buffer->duration)) {
		return;
	}

	if (auto type{ GetAvailableJump(ctx) }) {
		Jump(ctx, type.value());
	}
}

std::optional<event::PlayerJumpType> PlatformerJump::GetAvailableJump(
	const PlatformerJumpContext& ctx
) const {
	if (ctx.platformer.IsGrounded()) {
		return event::PlayerJumpType::Ground;
	}

	if (coyote_time.has_value() && coyote_timer_.IsRunning() &&
		!coyote_timer_.Completed(coyote_time->duration)) {
		return event::PlayerJumpType::Coyote;
	}

	if (air_jumps.has_value() && air_jumps_used_ < air_jumps->count) {
		return event::PlayerJumpType::Air;
	}

	return std::nullopt;
}

void PlatformerJump::Jump(PlatformerJumpContext& ctx, event::PlayerJumpType type) {
	jumping_ = true;
	jump_buffer_timer_.Stop();
	coyote_timer_.Stop();

	if (type == event::PlayerJumpType::Air) {
		++air_jumps_used_;
	}

	float gravity_y{ ctx.gravity.y * ctx.rigid_body.gravity };
	if (NearlyEqual(gravity_y, 0.0f)) {
		return;
	}

	float jump_speed{ std::sqrt(std::abs(2.0f * gravity_y * jump_height)) };
	float jump_direction{ gravity_y > 0.0f ? -1.0f : 1.0f };
	ctx.rigid_body.velocity.y = jump_direction * jump_speed;
	PushEvent<event::PlayerJump>(ctx.entity, type);
}

void PlatformerJump::CalculateGravity(PlatformerJumpContext& ctx) const {
	const auto& input{ ctx.scene.ctx().input };
	bool grounded{ ctx.platformer.IsGrounded() };

	float gravity_multiplier{ 0.0f };
	float gravity_sign{ ctx.gravity.y >= 0.0f ? 1.0f : -1.0f };
	float velocity_along_gravity{ ctx.rigid_body.velocity.y * gravity_sign };

	if (grounded) {
		gravity_multiplier = default_gravity_scale;
	} else if (fast_fall.has_value() && input.KeyHeld(fast_fall->key)) {
		gravity_multiplier = fast_fall->gravity_multiplier;
	} else if (velocity_along_gravity < -0.01f) {
		if (!variable_jump_height.has_value() || (input.KeyHeld(jump_key) && jumping_)) {
			gravity_multiplier = upward_gravity_multiplier;
		} else {
			gravity_multiplier = variable_jump_height->gravity_multiplier;
		}
	} else if (velocity_along_gravity > 0.01f) {
		gravity_multiplier = downward_gravity_multiplier;
	} else {
		gravity_multiplier = default_gravity_scale;
	}

	if (velocity_along_gravity > 0.0f) {
		ctx.rigid_body.velocity.y =
			std::clamp(velocity_along_gravity, 0.0f, terminal_velocity) * gravity_sign;
	}

	if (NearlyEqual(ctx.gravity.y, 0.0f)) {
		ctx.rigid_body.gravity = 0.0f;
		return;
	}

	PTGN_ASSERT(time_to_jump_apex > 0.0f);
	ctx.rigid_body.gravity = gravity_multiplier * 2.0f * jump_height /
							  (time_to_jump_apex * time_to_jump_apex * std::abs(ctx.gravity.y));
	PTGN_ASSERT(!std::isinf(ctx.rigid_body.gravity));
}

} // namespace ptgn
