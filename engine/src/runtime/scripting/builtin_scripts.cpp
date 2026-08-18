#include "runtime/scripting/builtin_scripts.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>
#include <type_traits>
#include <utility>

#include "app/application_context.h"
#include "app/project.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/math_utils.h"
#include "core/math/noise.h"
#include "core/math/rng.h"
#include "core/math/tolerance.h"
#include "runtime/audio/audio_system.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_transitions.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_file.h"

namespace ptgn {

namespace impl {

struct ShakeAnimationState {
	float trauma{ 0.0f };
	float target{ 0.0f };
	std::int32_t seed{ 0 };
};

struct PathFollowAnimationState {
	std::size_t current_waypoint{ 0 };
	std::vector<V2_float> waypoints;
};

} // namespace impl

namespace {

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

[[nodiscard]] milliseconds MillisecondsFromFloat(float value) {
	using Rep = milliseconds::rep;
	return milliseconds{ static_cast<Rep>(std::max(0.0f, value)) };
}

struct SceneTransitionPointers {
	std::unique_ptr<SceneTransition> out;
	std::unique_ptr<SceneTransition> in;
};

[[nodiscard]] SceneTransitionPointers MakeSceneTransitions(const SceneChangeScript& script) {
	SceneTransitionPointers transitions;
	if (script.transition == SceneTransitionStyle::None) {
		return transitions;
	}

	const milliseconds duration{ MillisecondsFromFloat(script.duration_ms) };
	const milliseconds delay{ MillisecondsFromFloat(script.delay_ms) };
	const V2_float direction{ script.direction.IsZero() ? V2_float{ 1.0f, 0.0f } : script.direction };
	const bool has_out{
		script.action == SceneChangeAction::Exit || script.action == SceneChangeAction::Switch ||
		script.action == SceneChangeAction::ReEnter
	};
	const bool has_in{
		script.action == SceneChangeAction::Enter || script.action == SceneChangeAction::Switch ||
		script.action == SceneChangeAction::ReEnter
	};

	switch (script.transition) {
		case SceneTransitionStyle::None:
			break;
		case SceneTransitionStyle::Fade: {
			if (has_out) {
				transitions.out =
					std::make_unique<FadeOutTransition>(duration, delay, script.ease);
			}
			if (has_in) {
				const milliseconds in_delay{ has_out ? delay + duration : delay };
				transitions.in =
					std::make_unique<FadeInTransition>(duration, in_delay, script.ease);
			}
			break;
		}
		case SceneTransitionStyle::CrossFade:
			if (has_out) {
				transitions.out =
					std::make_unique<FadeOutTransition>(duration, delay, script.ease);
			}
			if (has_in) {
				transitions.in =
					std::make_unique<FadeInTransition>(duration, delay, script.ease);
			}
			break;
		case SceneTransitionStyle::Slide:
			if (has_out) {
				transitions.out = std::make_unique<SlideOutTransition>(
					duration, direction, delay, script.ease
				);
			}
			if (has_in) {
				transitions.in = std::make_unique<SlideInTransition>(
					duration, -direction, delay, script.ease
				);
			}
			break;
	}

	return transitions;
}

[[nodiscard]] float BounceWave(float progress, bool symmetrical) {
	const float phase{
		(symmetrical ? 2.0f : 1.0f) * std::numbers::pi_v<float> *
		std::clamp(progress, 0.0f, 1.0f)
	};
	return std::sin(phase);
}

void ClearShakeOffset(Entity entity) {
	if (auto* offsets{ entity.TryGet<impl::Offsets>() }) {
		offsets->shake = {};
	}
}

void ApplyShake(
	milliseconds time, impl::Offsets& offsets, float trauma, const ShakeConfig& config,
	std::int32_t seed
) {
	const float shake_value{ std::pow(std::clamp(trauma, 0.0f, 1.0f), config.trauma_exponent) };
	const float x{ static_cast<float>(time.count()) * config.frequency };

	const V2_float position_noise{
		PerlinNoise::GetValue(x, 0.0f, seed + 0) * 2.0f - 1.0f,
		PerlinNoise::GetValue(x, 0.0f, seed + 1) * 2.0f - 1.0f
	};
	const float rotation_noise{
		PerlinNoise::GetValue(x, 0.0f, seed + 3) * 2.0f - 1.0f
	};

	offsets.shake.position = shake_value * config.maximum_translation * position_noise;
	offsets.shake.rotation =
		Radians{ shake_value * config.maximum_rotation * rotation_noise };
}

[[nodiscard]] V2_float GetFollowPosition(
	secondsf dt, const FollowConfig& config, V2_float position, V2_float target_position
) {
	PTGN_ASSERT(config.lerp.x >= 0.0f && config.lerp.x <= 1.0f);
	PTGN_ASSERT(config.lerp.y >= 0.0f && config.lerp.y <= 1.0f);

	const V2_float lerp{
		1.0f - std::pow(1.0f - config.lerp.x, dt.count()),
		1.0f - std::pow(1.0f - config.lerp.y, dt.count())
	};

	V2_float new_position{ position };
	if (config.deadzone.IsZero()) {
		new_position = Lerp(position, target_position, lerp);
	} else {
		const V2_float half_deadzone{ config.deadzone * 0.5f };
		const V2_float minimum{ target_position - half_deadzone };
		const V2_float maximum{ target_position + half_deadzone };

		if (position.x < minimum.x) {
			new_position.x =
				Lerp(position.x, position.x - (minimum.x - target_position.x), lerp.x);
		} else if (position.x > maximum.x) {
			new_position.x =
				Lerp(position.x, position.x + (target_position.x - maximum.x), lerp.x);
		}

		if (position.y < minimum.y) {
			new_position.y =
				Lerp(position.y, position.y - (minimum.y - target_position.y), lerp.y);
		} else if (position.y > maximum.y) {
			new_position.y =
				Lerp(position.y, position.y + (target_position.y - maximum.y), lerp.y);
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
	movement.max_acceleration = config.max_acceleration;
	movement.max_deceleration = config.max_acceleration;
	movement.max_speed = config.max_speed;
	movement.keys_enabled = false;
	movement.only_orthogonal_movement = false;
}

void MoveUsingVelocity(const FollowConfig& config, Entity entity, V2_float direction) {
	if (!entity.Has<TopDownMovement>()) {
		PTGN_WARN("Entity with MoveMode::Velocity should have a TopDownMovement component");
		return;
	}

	const float distance_squared{ direction.MagnitudeSquared() };
	if (config.stop_distance.has_value() &&
		config.stop_distance.value() >= kEpsilon<float> &&
		distance_squared <
			config.stop_distance.value() * config.stop_distance.value()) {
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

[[nodiscard]] bool ReachedStopDistance(
	const FollowConfig& config, const V2_float& direction
) {
	return config.stop_distance.has_value() &&
		config.stop_distance.value() >= kEpsilon<float> &&
		direction.MagnitudeSquared() <
			config.stop_distance.value() * config.stop_distance.value();
}

} // namespace

ComponentDefinition MakeComponentDefinition(const RegisteredComponent& component) {
	auto default_value{ component.MakeDefaultJson() };
	if (!default_value) {
		return {};
	}

	json value{ std::move(*default_value) };
	if (value.is_null()) {
		value = json::object();
	}

	const std::string component_name{ component.name };
	return ComponentDefinition{
		.type_hash = static_cast<TypeHashValue>(component.type_id),
		.type = component_name,
		.value = std::move(value),
		.apply_live = [component_name](Entity entity) {
			if (const auto* registration{ ComponentRegistry::Find(component_name) };
				registration && registration->default_constructible) {
				registration->AddDefault(entity);
			}
		},
	};
}

ComponentDefinition MakeComponentDefinition(std::string_view name) {
	const auto* component{ ComponentRegistry::Find(name) };
	return component ? MakeComponentDefinition(*component) : ComponentDefinition{};
}

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
	end_ = relative ? start_ + destination : destination;
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
	const float end_degrees{ relative ? start_degrees_ + degrees : degrees };
	delta_degrees_ = end_degrees - start_degrees_;
	if (shortest_path) {
		delta_degrees_ = std::remainder(delta_degrees_, 360.0f);
	}
}

ScriptStatus RotateToScript::OnUpdate() {
	const float value{ start_degrees_ + delta_degrees_ * Progress() };
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
	end_ = relative ? start_ * scale : scale;
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

void BounceScript::OnStart() {
	Target().TryAdd<impl::Offsets>();
	impl::ResetBounceAnimationState(Target());
}

ScriptStatus BounceScript::OnUpdate() {
	auto& offsets{ Target().TryAdd<impl::Offsets>() };
	offsets.bounce.position =
		static_offset + amplitude * BounceWave(Progress(), symmetrical);
	return ScriptStatus::Running;
}

void BounceScript::OnComplete() {
	impl::ResetBounceAnimationState(Target());
}

void BounceScript::OnCancel(SequenceCancelReason) {
	impl::ResetBounceAnimationState(Target());
}

void ShakeScript::OnStart() {
	PTGN_ASSERT(
		intensity >= -1.0f && intensity <= 1.0f,
		"Shake intensity must be in range [-1, 1]"
	);

	auto& state{ Target().TryAdd<impl::ShakeAnimationState>() };
	start_trauma_ = Clamp01(state.trauma);
	target_trauma_ = Clamp01(state.target + intensity);
	state.target = target_trauma_;
	state.seed = RandomNumber<std::int32_t>();
	Target().TryAdd<impl::Offsets>();
}

ScriptStatus ShakeScript::OnUpdate() {
	auto& state{ Target().TryAdd<impl::ShakeAnimationState>() };
	const float progress{ LinearProgress() > 0.0f ? Progress() : 1.0f };
	state.trauma = Clamp01(Lerp(start_trauma_, target_trauma_, progress));

	auto& offsets{ Target().TryAdd<impl::Offsets>() };
	ApplyShake(
		GetScene().ctx().TimeSinceStart(), offsets, state.trauma, config, state.seed
	);
	return ScriptStatus::Running;
}

void ShakeScript::OnRepeat() {
	OnStart();
}

void ShakeScript::OnComplete() {
	if (reset_on_complete) {
		impl::ResetShakeAnimationState(Target());
	} else {
		ClearShakeOffset(Target());
	}
}

void ShakeScript::OnCancel(SequenceCancelReason) {
	ClearShakeOffset(Target());
}

void AddShakeTraumaScript::OnStart() {
	PTGN_ASSERT(
		intensity >= -1.0f && intensity <= 1.0f,
		"Shake intensity must be in range [-1, 1]"
	);

	auto& state{ Target().TryAdd<impl::ShakeAnimationState>() };
	state.target = Clamp01(state.target + intensity);
	state.trauma = state.target;
	state.seed = RandomNumber<std::int32_t>();

	auto& offsets{ Target().TryAdd<impl::Offsets>() };
	ApplyShake(
		GetScene().ctx().TimeSinceStart(), offsets, state.trauma, config, state.seed
	);
}

ScriptStatus RecoverShakeScript::OnUpdate() {
	auto* state{ Target().TryGet<impl::ShakeAnimationState>() };
	if (!state) {
		return ScriptStatus::Complete;
	}

	state->trauma =
		Clamp01(state->trauma - std::max(0.0f, config.recovery_speed) * DeltaSeconds());
	state->target = state->trauma;

	if (state->trauma <= 0.0f) {
		return ScriptStatus::Complete;
	}

	auto& offsets{ Target().TryAdd<impl::Offsets>() };
	ApplyShake(
		GetScene().ctx().TimeSinceStart(), offsets, state->trauma, config, state->seed
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
}

ScriptStatus FollowTargetScript::OnUpdate() {
	const auto& scene{ GetScene() };
	auto target_entity{ scene.GetEntity(target) };

	if (!target_entity || !target_entity.Has<Transform>() || !Target().Has<Transform>()) {
		return ScriptStatus::Complete;
	}

	auto& position{ Target().Get<Transform>().position };
	const V2_float offset{ target_entity.Get<Transform>().position - position };
	const float distance{ std::sqrt(offset.x * offset.x + offset.y * offset.y) };
	if (distance <= std::max(0.0f, stopping_distance)) {
		return ScriptStatus::Complete;
	}

	const float step{ std::max(0.0f, speed) * std::max(0.0f, DeltaSeconds()) };
	if (step >= distance) {
		position += offset;
		return ScriptStatus::Complete;
	}

	position += offset * (step / distance);
	return ScriptStatus::Running;
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

	const V2_float current{ GetWorldPosition(Target()) };
	const V2_float target_position{ GetWorldPosition(target_entity) + config.offset };
	V2_float direction{ target_position - current };

	if (config.move_mode == MoveMode::Velocity) {
		MoveUsingVelocity(config, Target(), direction);
	} else {
		const V2_float next{
			GetFollowPosition(secondsf{ DeltaSeconds() }, config, current, target_position)
		};
		SetPosition(Target(), next);
		direction = target_position - next;
	}

	return ReachedStopDistance(config, direction)
		? ScriptStatus::Complete
		: ScriptStatus::Running;
}

void FollowEntityScript::OnComplete() {
	StopFollowMovement(Target());
}

void FollowEntityScript::OnCancel(SequenceCancelReason) {
	StopFollowMovement(Target());
}

void FollowPathScript::OnStart() {
	auto& state{ Target().TryAdd<impl::PathFollowAnimationState>() };
	const bool path_changed{ !std::ranges::equal(state.waypoints, waypoints) };
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
	if (!config.stop_distance.has_value() ||
		config.stop_distance.value() < kEpsilon<float>) {
		return ScriptStatus::Complete;
	}

	auto& state{ Target().TryAdd<impl::PathFollowAnimationState>() };
	state.current_waypoint =
		std::min(state.current_waypoint, waypoints.size() - 1);

	V2_float current{ GetWorldPosition(Target()) };
	V2_float target_position{
		waypoints[state.current_waypoint] + config.offset
	};
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
		direction = target_position - current;
	}

	if (config.move_mode == MoveMode::Velocity) {
		MoveUsingVelocity(config, Target(), direction);
	} else {
		SetPosition(
			Target(),
			GetFollowPosition(
				secondsf{ DeltaSeconds() }, config, current, target_position
			)
		);
	}

	return ScriptStatus::Running;
}

void FollowPathScript::OnComplete() {
	StopFollowMovement(Target());
}

void FollowPathScript::OnCancel(SequenceCancelReason) {
	StopFollowMovement(Target());
}

void NativeScript::OnStart() {
	if (callbacks && callbacks->on_start) {
		callbacks->on_start(*this);
	}
}

ScriptStatus NativeScript::OnUpdate() {
	if (callbacks && callbacks->on_update) {
		return callbacks->on_update(*this);
	}
	return ScriptStatus::Complete;
}

void NativeScript::OnComplete() {
	if (callbacks && callbacks->on_complete) {
		callbacks->on_complete(*this);
	}
}

void NativeScript::OnCancel(SequenceCancelReason reason) {
	if (callbacks && callbacks->on_cancel) {
		callbacks->on_cancel(*this, reason);
	}
}

void SetVisibleScript::OnStart() {
	SetVisible(Target(), visible);
}

void PlaySoundScript::OnStart() {
	if (sound.value.empty()) {
		PTGN_WARN("Play Sound requires a non empty audio asset key");
		return;
	}

	GetScene().ctx().audio.Play(sound, volume, loops);
}

void AnimationActionScript::OnStart() {
	auto animation{ ResolveAnimation(Target(), animation_key) };
	if (!animation) {
		PTGN_WARN(
			"Animation action requires an Animation owner or an AnimationMap with an active/keyed "
			"animation"
		);
		return;
	}

	switch (action) {
		case AnimationAction::Start:
			animation.Start(force);
			break;
		case AnimationAction::Stop:
			animation.Stop(reset_on_stop);
			break;
		case AnimationAction::Reset:
			animation.Reset();
			break;
		case AnimationAction::Pause:
			animation.Pause();
			break;
		case AnimationAction::Resume:
			animation.Resume();
			break;
		case AnimationAction::TogglePlaying:
			animation.Toggle();
			break;
		case AnimationAction::SetFrame:
			animation.SetCurrentFrame(frame);
			break;
		case AnimationAction::NextFrame:
			animation.IncrementFrame();
			break;
		case AnimationAction::PreviousFrame: {
			const std::size_t frame_count{ animation.GetFrameCount() };
			if (frame_count > 0) {
				animation.SetCurrentFrame(
					(animation.GetCurrentFrame() + frame_count - 1) % frame_count
				);
			}
			break;
		}
	}
}

void TimerActionScript::OnStart() {
	TimerHandle handle{ GetTimer(Target(), timer) };
	if (!handle) {
		PTGN_WARN("Timer action requires an existing timer on the target: ", timer.value);
		return;
	}

	switch (action) {
		case TimerAction::Start:
			handle.Start();
			break;
		case TimerAction::Restart:
			handle.Restart();
			break;
		case TimerAction::Stop:
			handle.Stop();
			break;
		case TimerAction::Reset:
			handle.Reset();
			break;
		case TimerAction::Pause:
			handle.Pause();
			break;
		case TimerAction::Resume:
			handle.Resume();
			break;
		case TimerAction::TogglePaused:
			handle.TogglePaused();
			break;
		case TimerAction::Advance:
			handle.Advance(amount);
			break;
		case TimerAction::Rewind:
			handle.Rewind(amount);
			break;
		case TimerAction::SetDuration:
			handle.SetDuration(amount);
			break;
		case TimerAction::AddDuration:
			handle.AddDuration(amount);
			break;
		case TimerAction::RemoveDuration:
			handle.RemoveDuration(amount);
			break;
	}
}

void SetTextureScript::OnStart() {
	Sprite{ Target() }.SetTexture(texture_key);
}

void SetEnabledScript::OnStart() {
	const auto* registration{ ComponentRegistry::Find(component) };
	if (!registration || !registration->Has(Target())) {
		PTGN_WARN("Cannot set enabled state for missing component: ", component);
		return;
	}

	if (!registration->serializable || !registration->deserializable) {
		PTGN_WARN("Component does not support enabled-state serialization: ", component);
		return;
	}

	json value;
	if (!registration->Serialize(value, Target())) {
		PTGN_WARN("Failed to serialize component enabled state: ", component);
		return;
	}

	if (value.is_boolean()) {
		value = enabled;
	} else if (value.is_object()) {
		auto it{ value.find("enabled") };
		if (it == value.end() || !it->is_boolean()) {
			PTGN_WARN("Component does not expose a bool enabled field: ", component);
			return;
		}

		*it = enabled;
	} else {
		PTGN_WARN("Component does not expose a supported enabled value: ", component);
		return;
	}

	if (!registration->Deserialize(value, Target())) {
		PTGN_WARN("Failed to deserialize component enabled state: ", component);
	}
}

void SceneChangeScript::OnStart() {
	auto& current_scene{ GetScene() };
	auto& scene_manager{
		current_scene.ctx().scene
	};
	auto& app{
		impl::SceneContextAccessor::app(
			current_scene.ctx()
		)
	};
	auto& app_context{
		impl::ApplicationAccessor::ctx(app)
	};

	if (!app_context.project) {
		PTGN_WARN(
			"Scene change requires an active project"
		);
		return;
	}

	const auto& project{
		app_context.project.value()
	};
	const auto* project_scene{
		FindProjectScene(
			project,
			scene_key
		)
	};

	if (!project_scene) {
		PTGN_WARN(
			"Scene key is not part of the active project: ",
			scene_key
		);
		return;
	}

	auto transitions{
		MakeSceneTransitions(*this)
	};
	const SceneTransitionPriority
		transition_priority{ priority };

	if (action ==
		SceneChangeAction::Exit) {
		scene_manager.Exit(
			scene_key,
			std::move(transitions.out),
			transition_priority
		);
		return;
	}

	SerializedScene serialized_scene;
	const auto snapshot{
		std::ranges::find_if(
			app_context.runtime_project_scenes,
			[this](
				const impl::RuntimeProjectSceneSnapshot& candidate
			) {
				return candidate.key ==
					   scene_key;
			}
		)
	};

	if (snapshot !=
		app_context.runtime_project_scenes.end()) {
		serialized_scene = snapshot->scene;
	} else {
		serialized_scene = LoadSceneFile(
			GetProjectScenePath(
				project,
				*project_scene
			)
		);
	}

	auto factory{
		impl::MakeSceneFactory(
			std::move(serialized_scene),
			true
		)
	};

	switch (action) {
		case SceneChangeAction::Enter:
			scene_manager.EnterFactory(
				scene_key,
				std::move(factory),
				std::move(transitions.in),
				transition_priority
			);
			break;

		case SceneChangeAction::Exit:
			break;

		case SceneChangeAction::Switch:
			scene_manager.TransitionFactory(
				current_scene.GetTag(),
				scene_key,
				std::move(factory),
				std::move(transitions.out),
				std::move(transitions.in),
				transition_priority
			);
			break;

		case SceneChangeAction::ReEnter:
			scene_manager.ReEnterFactory(
				scene_key,
				std::move(factory),
				std::move(transitions.out),
				std::move(transitions.in)
			);
			break;
	}
}

void EmitSignalScript::OnStart() {
	script_runtime::DispatchGlobal<Signal>(GetScene(), Signal{ signal });
}

void AddComponentsScript::OnStart() {
	for (const auto& component : components) {
		if (component.apply_live) {
			component.apply_live(Target());
			continue;
		}

		const auto* registration{ ComponentRegistry::Find(component.type) };
		if (!registration) {
			continue;
		}

		if (registration->is_empty && registration->default_constructible) {
			registration->AddDefault(Target());
		} else if (registration->deserializable && !component.value.is_null()) {
			registration->Deserialize(component.value, Target());
		} else if (registration->default_constructible) {
			registration->AddDefault(Target());
		}
	}
}

void RemoveComponentsScript::OnStart() {
	for (const auto& name : components) {
		if (const auto* registration{ ComponentRegistry::Find(name) }) {
			registration->Remove(Target());
		}
	}
}

ScriptSequence& ScriptSequence::EmitSignal(SignalKey signal) {
	return Then(EmitSignalScript{ std::move(signal) });
}

ScriptSequence& EmitSignal(ScriptSequence& sequence, SignalKey signal) {
	return sequence.EmitSignal(std::move(signal));
}

SequenceHandle After(Scene& scene, milliseconds duration, SequenceFunction function) {
	Entity owner{ scene.CreateEntity("After Script Sequence") };
	auto handle_ref{ std::make_shared<SequenceHandle>() };

	NativeScript callback{ NativeScriptCallbacks{
		.on_start = [function = std::move(function), handle_ref](Script&) mutable {
			std::visit(
				[&](auto& fn) {
					if constexpr (std::is_invocable_v<decltype(fn), SequenceHandle>) {
						fn(*handle_ref);
					} else {
						fn();
					}
				},
				function
			);
		},
	} };

	ScriptSequence sequence{ "After" };
	sequence.Wait(static_cast<float>(duration.count()))
		.Then(std::move(callback))
		.Transient()
		.DestroyOwnerOnComplete();

	auto handle{ script_runtime::RunSequence(owner, std::move(sequence)) };
	*handle_ref = handle;
	handle.Start();
	return handle;
}

SequenceHandle During(Scene& scene, milliseconds duration, DuringSequenceFunction function) {
	Entity owner{ scene.CreateEntity("During Script Sequence") };

	NativeScript callback{ NativeScriptCallbacks{
		.on_update = [function = std::move(function)](Script& script) mutable {
			std::visit(
				[&](auto& fn) {
					if constexpr (std::is_invocable_v<decltype(fn), Entity, float>) {
						fn(script.Target(), script.Progress());
					} else {
						fn();
					}
				},
				function
			);
			return ScriptStatus::Running;
		},
	} };

	ScriptSequence sequence{ "During" };
	sequence.During(static_cast<float>(duration.count()), std::move(callback))
		.Transient()
		.DestroyOwnerOnComplete();

	auto handle{ script_runtime::RunSequence(owner, std::move(sequence)) };
	handle.Start();
	return handle;
}

} // namespace ptgn
