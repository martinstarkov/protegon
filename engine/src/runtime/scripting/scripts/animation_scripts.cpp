#include "runtime/scripting/scripts/animation_scripts.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "app/application_context.h"
#include "app/project.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/math_utils.h"
#include "core/math/noise.h"
#include "core/math/rng.h"
#include "core/math/tolerance.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/platformer_jump.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_transitions.h"
#include "runtime/scripting/script_common.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "runtime/world/tilemap.h"

namespace ptgn {

namespace impl {
struct ShakeAnimationState {
	float trauma{ 0.0f };
	float baseline_trauma{ 0.0f };
	std::int32_t seed{ 0 };
};

struct PathFollowAnimationState {
	std::size_t current_waypoint{ 0 };
	std::vector<V2_float> waypoints{};
};

} // namespace impl

namespace {

const SequenceChannelKey kShakeRecoveryChannel{ "transform.shake.recovery" };

[[nodiscard]] Animation ResolveAnimation(Entity owner, std::string_view animation_key) {
	if (owner.Has<impl::AnimationMapData>()) {
		AnimationMap map{ owner };
		if (animation_key.empty()) {
			return map.GetActive().value_or(Animation{});
		}

		auto animation{ map.GetAnimation(animation_key) };
		if (!animation) {
			return {};
		}

		map.SetActive(animation_key);

		return animation;
	}

	return owner.Has<impl::AnimationData>() ? Animation{ owner } : Animation{};
}

[[nodiscard]] float BounceWave(float progress, bool symmetrical) {
	float phase{ (symmetrical ? 2.0f : 1.0f) * std::numbers::pi_v<float> *
				 std::clamp(progress, 0.0f, 1.0f) };

	return std::sin(phase);
}

void ClearShakeOffset(Entity entity) {
	if (auto* offsets{ entity.TryGet<impl::Offsets>() }) {
		offsets->shake = {};
	}
}

void StopBackgroundShakeRecovery(Entity entity) {
	if (!entity) {
		return;
	}

	script_runtime::StopChannel(entity, kShakeRecoveryChannel, SequenceStopMode::All);
}

void StartBackgroundShakeRecovery(Entity entity, const ShakeConfig& config) {
	if (!entity) {
		return;
	}

	ScriptSequence sequence{ "Shake Recovery" };
	sequence.UntilComplete(RecoverShakeScript{ config });
	sequence.Transient();
	script_runtime::RunInChannel(
		entity, kShakeRecoveryChannel, std::move(sequence), ReentryMode::Restart
	);
}

void RecoverShakeTrauma(impl::ShakeAnimationState& state, const ShakeConfig& config, secondsf dt) {
	float recovery{ std::max(0.0f, config.recovery_speed) *
					std::max(secondsf::zero(), dt).count() };
	if (state.trauma > state.baseline_trauma) {
		state.trauma = std::max(state.baseline_trauma, state.trauma - recovery);
	} else if (state.trauma < state.baseline_trauma) {
		state.trauma = std::min(state.baseline_trauma, state.trauma + recovery);
	}
}

void ApplyShake(
	millisecondsf time, impl::Offsets& offsets, float trauma, const ShakeConfig& config,
	std::int32_t seed
) {
	float shake_value{ std::pow(std::clamp(trauma, 0.0f, 1.0f), config.trauma_exponent) };
	float phase{ time.count() * config.frequency };
	V2_float position_noise{ PerlinNoise::GetValue(phase, 0.0f, seed + 0) * 2.0f - 1.0f,
							 PerlinNoise::GetValue(phase, 0.0f, seed + 1) * 2.0f - 1.0f };
	float rotation_noise{ PerlinNoise::GetValue(phase, 0.0f, seed + 3) * 2.0f - 1.0f };

	offsets.shake.position = shake_value * config.maximum_translation * position_noise;
	offsets.shake.rotation = Radians{ shake_value * config.maximum_rotation * rotation_noise };
}

[[nodiscard]] V2_float GetFollowPosition(
	secondsf dt, const FollowConfig& config, V2_float position, V2_float target_position
) {
	PTGN_ASSERT(config.lerp.x >= 0.0f && config.lerp.x <= 1.0f);
	PTGN_ASSERT(config.lerp.y >= 0.0f && config.lerp.y <= 1.0f);
	V2_float lerp{ 1.0f - std::pow(1.0f - config.lerp.x, dt.count()),
				   1.0f - std::pow(1.0f - config.lerp.y, dt.count()) };

	V2_float new_position{ position };
	if (config.deadzone.IsZero()) {
		new_position = Lerp(position, target_position, lerp);
	} else {
		V2_float half_deadzone{ config.deadzone * 0.5f };
		V2_float minimum{ target_position - half_deadzone };
		V2_float maximum{ target_position + half_deadzone };
		if (position.x < minimum.x) {
			new_position.x = Lerp(position.x, position.x - (minimum.x - target_position.x), lerp.x);
		} else if (position.x > maximum.x) {
			new_position.x = Lerp(position.x, position.x + (target_position.x - maximum.x), lerp.x);
		}

		if (position.y < minimum.y) {
			new_position.y = Lerp(position.y, position.y - (minimum.y - target_position.y), lerp.y);
		} else if (position.y > maximum.y) {
			new_position.y = Lerp(position.y, position.y + (target_position.y - maximum.y), lerp.y);
		}
	}

	if (!config.follow_x) {
		new_position.x = position.x;
	}

	if (!config.follow_y) {
		new_position.y = position.y;
	}

	if (config.snap_distance > 0.0f) {
		if (config.follow_x &&
			std::abs(target_position.x - new_position.x) <= config.snap_distance) {
			new_position.x = target_position.x;
		}

		if (config.follow_y &&
			std::abs(target_position.y - new_position.y) <= config.snap_distance) {
			new_position.y = target_position.y;
		}
	}

	return new_position;
}

void StopFollowMovement(Entity entity) {
	entity.Remove<TopDownMovement>();
	entity.Remove<RigidBody>();
}

void StartFollowMovement(Entity entity, const FollowConfig& config) {
	if (config.move_mode != MoveMode::Velocity) {
		StopFollowMovement(entity);

		return;
	}

	entity.TryAdd<RigidBody>();
	if (!entity.Has<Transform>()) {
		SetPosition(entity, {});
	}

	auto& movement{ entity.TryAdd<TopDownMovement>() };
	movement.max_acceleration		  = config.max_acceleration;
	movement.max_deceleration		  = config.max_acceleration;
	movement.max_speed				  = config.max_speed;
	movement.keys_enabled			  = false;
	movement.only_orthogonal_movement = false;
}

void MoveUsingVelocity(const FollowConfig& config, Entity entity, V2_float direction) {
	if (!entity.Has<TopDownMovement>()) {
		PTGN_WARN("Entity with MoveMode::Velocity should have a TopDownMovement component");

		return;
	}

	float distance_squared{ direction.MagnitudeSquared() };
	if (config.stop_distance.has_value() && config.stop_distance.value() >= kEpsilon<float> &&
		distance_squared < config.stop_distance.value() * config.stop_distance.value()) {
		return;
	}

	if (NearlyEqual(distance_squared, 0.0f)) {
		return;
	}

	V2_float normalized{ direction / std::sqrt(distance_squared) };
	if (!config.follow_x) {
		normalized = { 0.0f, Sign(normalized.y) };
	}

	if (!config.follow_y) {
		normalized = { Sign(normalized.x), 0.0f };
	}

	entity.Get<TopDownMovement>().Move(normalized);
}

[[nodiscard]] bool ReachedStopDistance(const FollowConfig& config, const V2_float& direction) {
	return config.stop_distance.has_value() && config.stop_distance.value() >= kEpsilon<float> &&
		   direction.MagnitudeSquared() <
			   config.stop_distance.value() * config.stop_distance.value();
}

} // namespace

namespace impl {
void ResetBounceAnimationState(Entity entity) {
	if (auto* offsets{ entity.TryGet<Offsets>() }) {
		offsets->bounce = {};
	}
}

void ResetShakeAnimationState(Entity entity) {
	ClearShakeOffset(entity);
	if (entity.Has<ShakeAnimationState>()) {
		entity.Remove<ShakeAnimationState>();
	}
}

void ResetFollowAnimationState(Entity entity, bool reset_waypoints) {
	StopFollowMovement(entity);
	if (reset_waypoints && entity.Has<PathFollowAnimationState>()) {
		entity.Remove<PathFollowAnimationState>();
	}
}

} // namespace impl

void MoveToScript::OnStart() {
	start_ = Target().Get<Transform>().position;
	end_   = relative ? start_ + destination : destination;
}

ScriptStatus MoveToScript::OnUpdate() {
	Target().Get<Transform>().position = start_ + (end_ - start_) * Progress();

	return ScriptStatus::Running;
}

void MoveToScript::OnRepeat() {
	if (!IsReversed()) {
		OnStart();
	}
}

void RotateToScript::OnStart() {
	start_degrees_ = Target().Get<Transform>().rotation.ToDeg().value;
	float end_degrees{ relative ? start_degrees_ + degrees : degrees };
	delta_degrees_ = end_degrees - start_degrees_;
	if (shortest_path) {
		delta_degrees_ = std::remainder(delta_degrees_, 360.0f);
	}
}

ScriptStatus RotateToScript::OnUpdate() {
	float value{ start_degrees_ + delta_degrees_ * Progress() };
	Target().Get<Transform>().rotation = Degrees{ value }.ToRad();

	return ScriptStatus::Running;
}

void RotateToScript::OnRepeat() {
	if (!IsReversed()) {
		OnStart();
	}
}

void ScaleToScript::OnStart() {
	start_ = Target().Get<Transform>().scale;
	end_   = relative ? start_ * scale : scale;
}

ScriptStatus ScaleToScript::OnUpdate() {
	Target().Get<Transform>().scale = start_ + (end_ - start_) * Progress();

	return ScriptStatus::Running;
}

void ScaleToScript::OnRepeat() {
	if (!IsReversed()) {
		OnStart();
	}
}

void TintToScript::OnStart() {
	start_ = Target().GetOrDefault<Tint>();
}

ScriptStatus TintToScript::OnUpdate() {
	Target().Add<Tint>(Lerp(start_, tint, Progress()));

	return ScriptStatus::Running;
}

void TintToScript::OnRepeat() {
	if (!IsReversed()) {
		OnStart();
	}
}

void FadeInScript::OnStart() {
	if (start_transparent) {
		Target().Add<Tint>(color::Transparent);
	}

	start_ = Target().GetOrDefault<Tint>();
}

ScriptStatus FadeInScript::OnUpdate() {
	Target().Add<Tint>(Lerp(start_, color::White, Progress()));

	return ScriptStatus::Running;
}

void FadeInScript::OnRepeat() {
	if (!IsReversed()) {
		OnStart();
	}
}

void FadeOutScript::OnStart() {
	if (start_opaque) {
		Target().Add<Tint>(color::White);
	}

	start_ = Target().GetOrDefault<Tint>();
}

ScriptStatus FadeOutScript::OnUpdate() {
	Target().Add<Tint>(Lerp(start_, color::Transparent, Progress()));

	return ScriptStatus::Running;
}

void FadeOutScript::OnRepeat() {
	if (!IsReversed()) {
		OnStart();
	}
}

void BounceScript::OnStart() {
	Target().TryAdd<impl::Offsets>();
	impl::ResetBounceAnimationState(Target());
}

ScriptStatus BounceScript::OnUpdate() {
	auto& offsets{ Target().TryAdd<impl::Offsets>() };
	float bounce_value{ TimingEase() == Ease::None ? 1.0f : BounceWave(Progress(), symmetrical) };
	offsets.bounce.position = static_offset + amplitude * bounce_value;

	return ScriptStatus::Running;
}

void BounceScript::OnComplete() {
	impl::ResetBounceAnimationState(Target());
}

void BounceScript::OnCancel(SequenceCancelReason) {
	impl::ResetBounceAnimationState(Target());
}

void StartBounceScript::OnStart() {
	if (period <= 0ms) {
		PTGN_WARN("Start Bounce requires a positive period");
		return;
	}

	if (symmetrical) {
		SymmetricalBounce(Target(), amplitude, period, std::nullopt, ease, static_offset, force);
	} else {
		Bounce(Target(), amplitude, period, std::nullopt, ease, static_offset, force);
	}
}

void StopBounceScript::OnStart() {
	ptgn::StopBounce(Target(), force);
}

void ShakeScript::OnStart() {
	StopBackgroundShakeRecovery(Target());
	PTGN_ASSERT(intensity >= 0.0f && intensity <= 1.0f, "Shake intensity must be in range [0, 1]");
	auto& state{ Target().TryAdd<impl::ShakeAnimationState>() };
	start_trauma_  = Clamp01(state.trauma);
	target_trauma_ = Clamp01(intensity);

	// Finite Tween shakes recover to zero after the timed portion completes.
	state.baseline_trauma = 0.0f;
	state.seed			  = RandomNumber<std::int32_t>();
	Target().TryAdd<impl::Offsets>();
}

ScriptStatus ShakeScript::OnUpdate() {
	auto& state{ Target().TryAdd<impl::ShakeAnimationState>() };

	state.trauma = Clamp01(Lerp(start_trauma_, target_trauma_, Progress()));
	auto& offsets{ Target().TryAdd<impl::Offsets>() };
	ApplyShake(
		GetScene().ctx().GameTime<millisecondsf>(), offsets, state.trauma, config, state.seed
	);
	return ScriptStatus::Running;
}

void ShakeScript::OnRepeat() {
	OnStart();
}

void ShakeScript::OnComplete() {
	if (reset_on_complete) {
		impl::ResetShakeAnimationState(Target());
	} else if (background_recovery_on_complete_) {
		StartBackgroundShakeRecovery(Target(), config);
	}
}

void ShakeScript::OnCancel(SequenceCancelReason) {
	if (reset_on_complete) {
		impl::ResetShakeAnimationState(Target());
	} else if (background_recovery_on_complete_) {
		StartBackgroundShakeRecovery(Target(), config);
	} else {
		ClearShakeOffset(Target());
	}
}

void ShakeActionScript::OnStart() {
	Shake(Target(), intensity, config, force);
	MoveOn();
}

void AddShakeTraumaScript::OnStart() {
	PTGN_ASSERT(
		intensity >= -1.0f && intensity <= 1.0f, "Shake intensity must be in range [-1, 1]"
	);
	if (auto* state{ Target().TryGet<impl::ShakeAnimationState>() }) {
		state->trauma = Clamp01(state->trauma + intensity);
	}

	MoveOn();
}

void SetShakeTraumaScript::OnStart() {
	PTGN_ASSERT(intensity >= 0.0f && intensity <= 1.0f, "Shake intensity must be in range [0, 1]");
	PTGN_ASSERT(
		baseline_trauma >= 0.0f && baseline_trauma <= 1.0f,
		"Shake baseline trauma must be in range [0, 1]"
	);
	auto& state{ Target().TryAdd<impl::ShakeAnimationState>() };
	state.trauma		  = Clamp01(intensity);
	state.baseline_trauma = Clamp01(baseline_trauma);
	state.seed			  = RandomNumber<std::int32_t>();
	auto& offsets{ Target().TryAdd<impl::Offsets>() };
	ApplyShake(
		GetScene().ctx().GameTime<millisecondsf>(), offsets, state.trauma, config, state.seed
	);
	MoveOn();
}

ScriptStatus MaintainShakeScript::OnUpdate() {
	auto& state{ Target().TryAdd<impl::ShakeAnimationState>() };

	RecoverShakeTrauma(state, config, dt());

	auto& offsets{ Target().TryAdd<impl::Offsets>() };

	ApplyShake(
		GetScene().ctx().GameTime<millisecondsf>(), offsets, state.trauma, config, state.seed
	);

	return ScriptStatus::Running;
}

ScriptStatus RecoverShakeScript::OnUpdate() {
	auto* state{ Target().TryGet<impl::ShakeAnimationState>() };
	if (!state) {
		return ScriptStatus::Complete;
	}

	RecoverShakeTrauma(*state, config, dt());

	if (NearlyEqual(state->trauma, state->baseline_trauma)) {
		return ScriptStatus::Complete;
	}

	auto& offsets{ Target().TryAdd<impl::Offsets>() };

	ApplyShake(
		GetScene().ctx().GameTime<millisecondsf>(), offsets, state->trauma, config, state->seed
	);

	return ScriptStatus::Running;
}

void RecoverShakeScript::OnComplete() {
	impl::ResetShakeAnimationState(Target());
}

void RecoverShakeScript::OnCancel(SequenceCancelReason) {
	ClearShakeOffset(Target());
}

void ResetShakeScript::OnStart() {
	impl::ResetShakeAnimationState(Target());
	MoveOn();
}

void StartShakeScript::OnStart() {
	Shake(Target(), intensity, std::nullopt, config, Ease::None, force, false);
	MoveOn();
}

void StopShakeScript::OnStart() {
	ptgn::StopShake(Target(), force);
	MoveOn();
}

void FollowEntityScript::OnStart() {
	const auto& scene{ GetScene() };
	auto target_entity{ scene.GetEntity(target) };
	if (!target_entity || target_entity == Target()) {
		return;
	}

	if (config.teleport_on_start) {
		SetPosition(Target(), GetPosition(target_entity) + config.offset);
	}

	StartFollowMovement(Target(), config);
}

ScriptStatus FollowEntityScript::OnUpdate() {
	if (!config.follow_x && !config.follow_y) {
		return ScriptStatus::Running;
	}

	const auto& scene{ GetScene() };
	auto target_entity{ scene.GetEntity(target) };
	if (!target_entity || target_entity == Target()) {
		return ScriptStatus::Complete;
	}

	V2_float current{ GetWorldPosition(Target()) };
	V2_float target_position{ GetWorldPosition(target_entity) + config.offset };
	V2_float direction{ target_position - current };
	if (config.move_mode == MoveMode::Velocity) {
		MoveUsingVelocity(config, Target(), direction);
	} else {
		V2_float next{ GetFollowPosition(dt(), config, current, target_position) };

		SetPosition(Target(), next);
		direction = target_position - next;
	}

	return ReachedStopDistance(config, direction) ? ScriptStatus::Complete : ScriptStatus::Running;
}

void FollowEntityScript::OnComplete() {
	StopFollowMovement(Target());
}

void FollowEntityScript::OnCancel(SequenceCancelReason) {
	StopFollowMovement(Target());
}

void FollowPathScript::OnStart() {
	auto& state{ Target().TryAdd<impl::PathFollowAnimationState>() };
	bool path_changed{ !std::ranges::equal(state.waypoints, waypoints) };
	state.waypoints = waypoints;
	if (reset_waypoint_index || state.current_waypoint >= waypoints.size() || path_changed) {
		state.current_waypoint = 0;
	}

	if (config.teleport_on_start && !waypoints.empty()) {
		SetPosition(Target(), waypoints.back() + config.offset);
	}

	StartFollowMovement(Target(), config);
}

ScriptStatus FollowPathScript::OnUpdate() {
	if (waypoints.empty()) {
		return ScriptStatus::Complete;
	}

	if (!config.follow_x && !config.follow_y) {
		return ScriptStatus::Running;
	}

	if (!config.stop_distance.has_value() || config.stop_distance.value() < kEpsilon<float>) {
		return ScriptStatus::Complete;
	}

	auto& state{ Target().TryAdd<impl::PathFollowAnimationState>() };
	state.current_waypoint = std::min(state.current_waypoint, waypoints.size() - 1);
	V2_float current{ GetWorldPosition(Target()) };
	V2_float target_position{ waypoints[state.current_waypoint] + config.offset };

	V2_float direction{ target_position - current };
	if (ReachedStopDistance(config, direction)) {
		if (state.current_waypoint + 1 < waypoints.size()) {
			++state.current_waypoint;
		} else if (config.loop_path) {
			state.current_waypoint = 0;
		} else {
			state.current_waypoint = waypoints.size();

			return ScriptStatus::Complete;
		}

		target_position = waypoints[state.current_waypoint] + config.offset;
		direction		= target_position - current;
	}

	if (config.move_mode == MoveMode::Velocity) {
		MoveUsingVelocity(config, Target(), direction);
	} else {
		SetPosition(Target(), GetFollowPosition(dt(), config, current, target_position));
	}

	return ScriptStatus::Running;
}

void FollowPathScript::OnComplete() {
	StopFollowMovement(Target());
}

void FollowPathScript::OnCancel(SequenceCancelReason) {
	StopFollowMovement(Target());
}

void StartFollowEntityScript::OnStart() {
	Entity follow_target{ GetScene().GetEntity(target) };
	if (!follow_target || follow_target == Target()) {
		PTGN_WARN("Start Follow Entity requires a valid target entity different from the owner");

		return;
	}

	StartFollow(Target(), follow_target, config, force);
}

void StartFollowPathScript::OnStart() {
	if (waypoints.empty()) {
		PTGN_WARN("Start Follow Path requires at least one waypoint");

		return;
	}

	StartFollow(
		Target(), std::span<const V2_float>{ waypoints.data(), waypoints.size() }, config, force,
		reset_waypoint_index
	);
}

void StopFollowScript::OnStart() {
	ptgn::StopFollow(Target(), force, reset_previous_waypoints);
}

void AnimationStartScript::OnStart() {
	Animation animation{ ResolveAnimation(Target(), animation_key) };
	if (!animation) {
		PTGN_WARN("Animation action requires an Animation target");
		return;
	}
	animation.Start(force);
}

void AnimationStopScript::OnStart() {
	Animation animation{ ResolveAnimation(Target(), animation_key) };
	if (!animation) {
		PTGN_WARN("Animation action requires an Animation target");
		return;
	}
	animation.Stop(reset_on_stop);
}

void AnimationResetScript::OnStart() {
	Animation animation{ ResolveAnimation(Target(), animation_key) };
	if (!animation) {
		PTGN_WARN("Animation action requires an Animation target");
		return;
	}
	animation.Reset();
}

void AnimationPauseScript::OnStart() {
	Animation animation{ ResolveAnimation(Target(), animation_key) };
	if (!animation) {
		PTGN_WARN("Animation action requires an Animation target");
		return;
	}
	animation.Pause();
}

void AnimationResumeScript::OnStart() {
	Animation animation{ ResolveAnimation(Target(), animation_key) };
	if (!animation) {
		PTGN_WARN("Animation action requires an Animation target");
		return;
	}
	animation.Resume();
}

void AnimationTogglePlayingScript::OnStart() {
	Animation animation{ ResolveAnimation(Target(), animation_key) };
	if (!animation) {
		PTGN_WARN("Animation action requires an Animation target");
		return;
	}
	animation.Toggle();
}

void AnimationSetFrameScript::OnStart() {
	Animation animation{ ResolveAnimation(Target(), animation_key) };
	if (!animation) {
		PTGN_WARN("Animation action requires an Animation target");
		return;
	}
	animation.SetCurrentFrame(frame);
}

void AnimationNextFrameScript::OnStart() {
	Animation animation{ ResolveAnimation(Target(), animation_key) };

	if (!animation) {
		PTGN_WARN("Animation action requires an Animation target");
		return;
	}

	animation.IncrementFrame();
}

void AnimationPreviousFrameScript::OnStart() {
	Animation animation{ ResolveAnimation(Target(), animation_key) };

	if (!animation) {
		PTGN_WARN("Animation action requires an Animation target");
		return;
	}

	std::size_t count{ animation.GetFrameCount() };

	if (count > 0) {
		animation.SetCurrentFrame((animation.GetCurrentFrame() + count - 1) % count);
	}
}

ScriptStatus FollowTargetScript::OnUpdate() {
	const auto& scene{ GetScene() };

	auto target_entity{ scene.GetEntity(target) };

	if (!target_entity || !target_entity.Has<Transform>() || !Target().Has<Transform>()) {
		return ScriptStatus::Complete;
	}

	auto& position{ Target().Get<Transform>().position };

	V2_float offset{ target_entity.Get<Transform>().position - position };

	float distance{ std::sqrt(offset.x * offset.x + offset.y * offset.y) };

	if (distance <= std::max(0.0f, stopping_distance)) {
		return ScriptStatus::Complete;
	}

	float step{ std::max(0.0f, speed) * std::max(secondsf::zero(), dt()).count() };

	if (step >= distance) {
		position += offset;

		return ScriptStatus::Complete;
	}

	position += offset * (step / distance);

	return ScriptStatus::Running;
}

} // namespace ptgn
