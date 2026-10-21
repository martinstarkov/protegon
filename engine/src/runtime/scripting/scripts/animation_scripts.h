#pragma once

#include "runtime/scripting/script_common.h"

namespace ptgn {

struct FollowTargetScript : public Script {
	UUID target{};
	float speed{ 120.0f };
	float stopping_distance{ 2.0f };
	FollowTargetScript() = default;

	FollowTargetScript(
		Entity follow_target, float follow_speed, float follow_stopping_distance = 2.0f
	) :
		target{ follow_target.Get<UUID>() },
		speed{ follow_speed },
		stopping_distance{ follow_stopping_distance } {}

	[[nodiscard]] ScriptStatus OnUpdate() override;
	PTGN_REFLECT(FollowTargetScript, target, speed, stopping_distance)
};

struct MoveToScript : public Script {
	V2_float destination{ 0.0f, 64.0f };
	bool relative{ true };
	MoveToScript() = default;

	MoveToScript(V2_float move_destination, bool relative_movement = true) :
		destination{ move_destination }, relative{ relative_movement } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnRepeat() override;
	PTGN_REFLECT(MoveToScript, destination, relative)

private:
	V2_float start_{};
	V2_float end_{};
};

struct RotateToScript : public Script {
	float degrees{ 90.0f };
	bool shortest_path{ true };
	bool relative{ false };
	RotateToScript() = default;

	RotateToScript(
		float rotate_degrees, bool shortest_rotation_path = true, bool relative_rotation = false
	) :
		degrees{ rotate_degrees },
		shortest_path{ shortest_rotation_path },
		relative{ relative_rotation } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnRepeat() override;
	PTGN_REFLECT(RotateToScript, degrees, shortest_path, relative)

private:
	float start_degrees_{ 0.0f };
	float delta_degrees_{ 0.0f };
};

struct ScaleToScript : public Script {
	V2_float scale{ 1.0f, 1.0f };
	bool relative{ false };
	ScaleToScript() = default;

	ScaleToScript(V2_float target_scale, bool relative_scaling = false) :
		scale{ target_scale }, relative{ relative_scaling } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnRepeat() override;
	PTGN_REFLECT(ScaleToScript, scale, relative)

private:
	V2_float start_{};
	V2_float end_{};
};

struct TintToScript : public Script {
	Color tint{ color::White };
	TintToScript() = default;

	explicit TintToScript(Color target_tint) : tint{ target_tint } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnRepeat() override;
	PTGN_REFLECT(TintToScript, tint)

private:
	Color start_{ color::White };
};

struct FadeInScript : public Script {
	bool start_transparent{ false };
	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnRepeat() override;
	PTGN_REFLECT(FadeInScript, start_transparent)

private:
	Color start_{ color::White };
};

struct FadeOutScript : public Script {
	bool start_opaque{ false };
	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnRepeat() override;
	PTGN_REFLECT(FadeOutScript, start_opaque)

private:
	Color start_{ color::White };
};

struct BounceScript : public Script {
	V2_float amplitude{ 0.0f, -16.0f };
	V2_float static_offset{};
	bool symmetrical{ false };
	BounceScript() = default;

	BounceScript(
		V2_float bounce_amplitude, V2_float bounce_static_offset = {},
		bool symmetrical_bounce = false
	) :
		amplitude{ bounce_amplitude },
		static_offset{ bounce_static_offset },
		symmetrical{ symmetrical_bounce } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnComplete() override;
	void OnCancel(SequenceCancelReason) override;
	PTGN_REFLECT(BounceScript, amplitude, static_offset, symmetrical)
};

struct StartBounceScript : public Script {
	V2_float amplitude{ 0.0f, -16.0f };
	millisecondsf period{ 500.0f };
	Ease ease{ Ease::Linear };
	V2_float static_offset{};
	bool symmetrical{ false };
	bool force{ true };
	void OnStart() override;
	PTGN_REFLECT(StartBounceScript, amplitude, period, ease, static_offset, symmetrical, force)
};

struct StopBounceScript : public Script {
	bool force{ true };
	void OnStart() override;
	PTGN_REFLECT(StopBounceScript, force)
};

struct ShakeScript : public Script {
	float intensity{ 1.0f };
	ShakeConfig config{};
	bool reset_on_complete{ true };
	ShakeScript() = default;

	ShakeScript(
		float shake_intensity, ShakeConfig shake_config = {}, bool reset_shake_on_complete = true,
		bool background_recovery_on_complete = true
	) :
		intensity{ shake_intensity },
		config{ std::move(shake_config) },
		reset_on_complete{ reset_shake_on_complete },
		background_recovery_on_complete_{ background_recovery_on_complete } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnRepeat() override;
	void OnComplete() override;
	void OnCancel(SequenceCancelReason) override;
	PTGN_REFLECT(ShakeScript, intensity, config, reset_on_complete)

private:
	float start_trauma_{ 0.0f };
	float target_trauma_{ 0.0f };
	bool background_recovery_on_complete_{ true };
};

struct ShakeActionScript : public Script {
	float intensity{ 1.0f };
	ShakeConfig config{};
	bool force{ true };
	void OnStart() override;
	PTGN_REFLECT(ShakeActionScript, intensity, config, force)
};

struct AddShakeTraumaScript : public Script {
	float intensity{ 1.0f };
	AddShakeTraumaScript() = default;

	explicit AddShakeTraumaScript(float shake_intensity) : intensity{ shake_intensity } {}

	void OnStart() override;
	PTGN_REFLECT(AddShakeTraumaScript, intensity)
};

struct SetShakeTraumaScript : public Script {
	float intensity{ 1.0f };
	float baseline_trauma{ 0.0f };
	ShakeConfig config{};
	SetShakeTraumaScript() = default;

	SetShakeTraumaScript(float shake_intensity, ShakeConfig shake_config = {}) :
		SetShakeTraumaScript{ shake_intensity, 0.0f, std::move(shake_config) } {}

	SetShakeTraumaScript(
		float shake_intensity, float shake_baseline_trauma, ShakeConfig shake_config = {}
	) :
		intensity{ shake_intensity },
		baseline_trauma{ shake_baseline_trauma },
		config{ std::move(shake_config) } {}

	void OnStart() override;
	PTGN_REFLECT(SetShakeTraumaScript, intensity, baseline_trauma, config)
};

struct MaintainShakeScript : public Script {
	ShakeConfig config{};
	MaintainShakeScript() = default;

	explicit MaintainShakeScript(ShakeConfig shake_config) : config{ std::move(shake_config) } {}

	[[nodiscard]] ScriptStatus OnUpdate() override;
	PTGN_REFLECT(MaintainShakeScript, config)
};

struct RecoverShakeScript : public Script {
	ShakeConfig config{};
	RecoverShakeScript() = default;

	explicit RecoverShakeScript(ShakeConfig shake_config) : config{ std::move(shake_config) } {}

	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnComplete() override;
	void OnCancel(SequenceCancelReason) override;
	PTGN_REFLECT(RecoverShakeScript, config)
};

struct ResetShakeScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ResetShakeScript)
};

struct StartShakeScript : public Script {
	float intensity{ 1.0f };
	ShakeConfig config{};
	bool force{ true };
	void OnStart() override;
	PTGN_REFLECT(StartShakeScript, intensity, config, force)
};

struct StopShakeScript : public Script {
	bool force{ true };
	void OnStart() override;
	PTGN_REFLECT(StopShakeScript, force)
};

struct FollowEntityScript : public Script {
	UUID target{};
	TargetFollowConfig config{};
	FollowEntityScript() = default;

	FollowEntityScript(Entity follow_target, TargetFollowConfig follow_config = {}) :
		target{ follow_target.Get<UUID>() }, config{ std::move(follow_config) } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnComplete() override;
	void OnCancel(SequenceCancelReason) override;
	PTGN_REFLECT(FollowEntityScript, target, config)
};

struct FollowPathScript : public Script {
	std::vector<V2_float> waypoints{};
	PathFollowConfig config{};
	bool reset_waypoint_index{ false };
	FollowPathScript() = default;

	FollowPathScript(
		std::vector<V2_float> follow_waypoints, PathFollowConfig follow_config = {},
		bool reset_follow_waypoint_index = false
	) :
		waypoints{ std::move(follow_waypoints) },
		config{ std::move(follow_config) },
		reset_waypoint_index{ reset_follow_waypoint_index } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnComplete() override;
	void OnCancel(SequenceCancelReason) override;
	PTGN_REFLECT(FollowPathScript, waypoints, config, reset_waypoint_index)
};

struct StartFollowEntityScript : public Script {
	UUID target{};
	TargetFollowConfig config{};
	bool force{ true };
	void OnStart() override;
	PTGN_REFLECT(StartFollowEntityScript, target, config, force)
};

struct StartFollowPathScript : public Script {
	std::vector<V2_float> waypoints{};
	PathFollowConfig config{};
	bool force{ true };
	bool reset_waypoint_index{ false };
	void OnStart() override;
	PTGN_REFLECT(StartFollowPathScript, waypoints, config, force, reset_waypoint_index)
};

struct StopFollowScript : public Script {
	bool force{ true };
	bool reset_previous_waypoints{ false };
	void OnStart() override;
	PTGN_REFLECT(StopFollowScript, force, reset_previous_waypoints)
};

struct AnimationStartScript : public Script {
	void OnStart() override;
	std::string animation_key{};
	bool force{ true };
	PTGN_REFLECT(AnimationStartScript, animation_key, force)
};

struct AnimationStopScript : public Script {
	void OnStart() override;
	std::string animation_key{};
	bool reset_on_stop{ false };
	PTGN_REFLECT(AnimationStopScript, animation_key, reset_on_stop)
};

struct AnimationResetScript : public Script {
	void OnStart() override;
	std::string animation_key{};
	PTGN_REFLECT(AnimationResetScript, animation_key)
};

struct AnimationPauseScript : public Script {
	void OnStart() override;
	std::string animation_key{};
	PTGN_REFLECT(AnimationPauseScript, animation_key)
};

struct AnimationResumeScript : public Script {
	void OnStart() override;
	std::string animation_key{};
	PTGN_REFLECT(AnimationResumeScript, animation_key)
};

struct AnimationTogglePlayingScript : public Script {
	void OnStart() override;
	std::string animation_key{};
	PTGN_REFLECT(AnimationTogglePlayingScript, animation_key)
};

struct AnimationSetFrameScript : public Script {
	void OnStart() override;
	std::string animation_key{};
	std::size_t frame{};
	PTGN_REFLECT(AnimationSetFrameScript, animation_key, frame)
};

struct AnimationNextFrameScript : public Script {
	void OnStart() override;
	std::string animation_key{};
	PTGN_REFLECT(AnimationNextFrameScript, animation_key)
};

struct AnimationPreviousFrameScript : public Script {
	void OnStart() override;
	std::string animation_key{};
	PTGN_REFLECT(AnimationPreviousFrameScript, animation_key)
};

} // namespace ptgn
