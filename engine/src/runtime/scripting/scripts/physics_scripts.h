#pragma once

#include "runtime/scripting/script_common.h"

namespace ptgn {

struct RigidBodySetVelocityScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(RigidBodySetVelocityScript, vector)
};

struct RigidBodyAddVelocityScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(RigidBodyAddVelocityScript, vector)
};

struct RigidBodyAddImpulseScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(RigidBodyAddImpulseScript, vector)
};

struct RigidBodyAddAccelerationScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	V2_float vector{};

	PTGN_REFLECT(RigidBodyAddAccelerationScript, number, vector)
};

struct RigidBodyStopMovementScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(RigidBodyStopMovementScript)
};

struct RigidBodySetGravityScaleScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(RigidBodySetGravityScaleScript, number)
};

struct RigidBodySetDragScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(RigidBodySetDragScript, number)
};

struct RigidBodySetAngularVelocityScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(RigidBodySetAngularVelocityScript, number)
};

struct RigidBodyAddAngularImpulseScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(RigidBodyAddAngularImpulseScript, number)
};

struct RigidBodySetMaximumSpeedScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	bool option{ true };

	PTGN_REFLECT(RigidBodySetMaximumSpeedScript, number, option)
};

struct RigidBodySetImmovableScript : public Script {
	void OnStart() override;
	bool option{ true };

	PTGN_REFLECT(RigidBodySetImmovableScript, option)
};

struct ColliderSetCollisionModeScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(ColliderSetCollisionModeScript, text)
};

struct ColliderSetColliderShapeScript : public Script {
	void OnStart() override;
	ColliderShape collider_shape{};

	PTGN_REFLECT(ColliderSetColliderShapeScript, collider_shape)
};

struct ColliderSetCollisionResponseScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(ColliderSetCollisionResponseScript, text)
};

struct ColliderSetCollisionFilterScript : public Script {
	void OnStart() override;
	EntityFilter entity_filter{};

	PTGN_REFLECT(ColliderSetCollisionFilterScript, entity_filter)
};

struct ColliderSetOverlapModeScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ColliderSetOverlapModeScript)
};

struct TopDownSetMaximumSpeedScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(TopDownSetMaximumSpeedScript, number)
};

struct TopDownMoveDirectionScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(TopDownMoveDirectionScript, vector)
};

struct TopDownSetAccelerationScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(TopDownSetAccelerationScript, number)
};

struct PlatformerSetMaximumSpeedScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(PlatformerSetMaximumSpeedScript, number)
};

struct PlatformerSetJumpControllerScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(PlatformerSetJumpControllerScript, text)
};

struct JumpJumpScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(JumpJumpScript)
};

struct JumpSetJumpHeightScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(JumpSetJumpHeightScript, number)
};

struct JumpSetGravityMultipliersScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	float number2{ 1.0f };

	PTGN_REFLECT(JumpSetGravityMultipliersScript, number, number2)
};

struct JumpSetAirJumpsScript : public Script {
	void OnStart() override;
	int index{};

	PTGN_REFLECT(JumpSetAirJumpsScript, index)
};

struct PhysicsSetGravityScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(PhysicsSetGravityScript, vector)
};

struct PhysicsEnablePhysicsScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(PhysicsEnablePhysicsScript)
};

struct PhysicsDisablePhysicsScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(PhysicsDisablePhysicsScript)
};

struct PhysicsSetBoundsScript : public Script {
	void OnStart() override;
	std::string text{};
	V2_float vector{};
	V2_float vector2{};

	PTGN_REFLECT(PhysicsSetBoundsScript, text, vector, vector2)
};

struct PhysicsClearBoundsScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(PhysicsClearBoundsScript)
};

} // namespace ptgn
