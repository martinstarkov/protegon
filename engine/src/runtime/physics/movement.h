#pragma once

#include <string>

#include "core/input/key.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/physics/move_direction.h"
#include "runtime/physics/platformer_grounding.h"
#include "runtime/physics/platformer_jump.h"
#include "runtime/physics/rigid_body.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class Physics;

void MoveWASD(
	const Scene& scene, V2_float& vel, V2_float amount, bool cancel_velocity_if_unpressed = true
);

void MoveArrowKeys(
	const Scene& scene, V2_float& vel, V2_float amount, bool cancel_velocity_if_unpressed = true
);

void MoveWASD(Entity entity, V2_float speed);
void MoveArrowKeys(Entity entity, V2_float speed);

struct TopDownMovement {
	float max_speed{ 4.0f * 60.0f };
	float max_acceleration{ 20.0f * 60.0f };
	float max_deceleration{ 20.0f * 60.0f };
	float max_turn_speed{ 60.0f * 60.0f };
	float friction{ 0.0f };

	/// @brief  If false, velocity will be immediately set to desired velocity. Otherwise
	/// integration is used.
	bool use_acceleration{ true };
	bool flip_vertically{ false };
	bool keys_enabled{ true };
	bool only_orthogonal_movement{ true };

	Key up_key{ Key::W };
	Key left_key{ Key::A };
	Key down_key{ Key::S };
	Key right_key{ Key::D };

	void Update(Entity entity, Transform& transform, RigidBody& rb, secondsf dt);

	void Move(MoveDirection direction);
	void Move(V2_float direction);

	[[nodiscard]] bool IsMoving(MoveDirection direction) const;
	[[nodiscard]] bool WasMoving(MoveDirection direction) const;
	[[nodiscard]] MoveDirection GetDirection() const;
	[[nodiscard]] MoveDirection GetPreviousDirection() const;

	V2_float facing_direction{};

	PTGN_REFLECT(
		TopDownMovement, max_speed, max_acceleration, max_deceleration, max_turn_speed, friction,
		use_acceleration, flip_vertically, keys_enabled, only_orthogonal_movement, up_key, left_key,
		down_key, right_key
	)

	PTGN_REFLECT_READONLY(
		TopDownMovement, facing_direction, dir, prev_dir, up_input, down_input, left_input,
		right_input
	)

private:
	void RunWithAcceleration(V2_float desired_velocity, RigidBody& rb, secondsf dt) const;

	static bool GetMovingState(V2_float d, MoveDirection direction);
	static MoveDirection GetDirectionState(V2_float d);

	void InvokeCallbacks(Entity entity) const;

	bool up_input{ false };
	bool down_input{ false };
	bool left_input{ false };
	bool right_input{ false };

	V2_float dir{};
	V2_float prev_dir{};
};

struct PlatformerMovement {
	float max_speed{ 4.0f * 60.0f };
	float max_acceleration{ 20.0f * 60.0f };
	float max_deceleration{ 20.0f * 60.0f };
	float max_turn_speed{ 60.0f * 60.0f };
	float max_air_acceleration{ 40.0f * 60.0f };
	float max_air_deceleration{ 40.0f * 60.0f };
	float max_air_turn_speed{ 60.0f * 60.0f };

	/// @brief  If false, velocity will be immediately set to desired velocity. Otherwise
	/// integration is used.
	bool use_acceleration{ true };
	float friction{ 0.0f };

	Key left_key{ Key::A };
	Key right_key{ Key::D };

	PlatformerGrounding grounding{};

	/// @brief Empty means no jump controller.
	std::string jump_controller{};

	void Update(const Scene& scene, Transform& transform, RigidBody& rb, secondsf dt) const;

	[[nodiscard]] bool IsGrounded() const;
	[[nodiscard]] bool WasGrounded() const;
	[[nodiscard]] Entity GetGroundEntity() const;
	[[nodiscard]] V2_float GetGroundNormal() const;

	PTGN_REFLECT(
		PlatformerMovement, max_speed, max_acceleration, max_deceleration, max_turn_speed,
		max_air_acceleration, max_air_deceleration, max_air_turn_speed, use_acceleration, friction,
		left_key, right_key, grounding, jump_controller
	)

private:
	friend class Physics;

	void RunWithAcceleration(
		const Scene& scene, V2_float desired_velocity, float dir_x, RigidBody& rb, secondsf dt
	) const;

	PlatformerGroundingState grounding_state_{};
};

} // namespace ptgn
