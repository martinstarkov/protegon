#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/strong_string.h"
#include "runtime/animation/follow_config.h"
#include "runtime/animation/shake_config.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/scene/scene_transition.h"
#include "runtime/scripting/script.h"
#include "runtime/timer/timer.h"

namespace ptgn {

struct SignalKey : StrongString<SignalKey> {
	using StrongString::StrongString;
	SignalKey() = default;

	PTGN_REFLECT_VALUE(SignalKey, value)
};

struct Signal {
	SignalKey key{};

	PTGN_REFLECT(Signal, key)
};

struct ComponentDefinition {
	TypeHashValue type_hash{ 0 };
	std::string type{};
	json value = json::object();

	/// @brief C++ authored definitions avoid a JSON round trip at runtime.
	std::function<void(Entity)> apply_live{};

	PTGN_REFLECT(ComponentDefinition, type_hash, type, value)
};

[[nodiscard]] ComponentDefinition MakeComponentDefinition(const RegisteredComponent& component);
[[nodiscard]] ComponentDefinition MakeComponentDefinition(std::string_view name);

template <typename T>
[[nodiscard]] ComponentDefinition MakeComponentDefinition() {
	const auto* component{ ComponentRegistry::Find<T>() };
	return component ? MakeComponentDefinition(*component) : ComponentDefinition{};
}

template <typename T>
[[nodiscard]] ComponentDefinition MakeComponentDefinition(T value) {
	const auto* component{ ComponentRegistry::Find<T>() };
	if (!component) {
		return {};
	}

	T typed_value{ std::move(value) };
	json component_json = typed_value;
	auto prototype{ std::make_shared<T>(std::move(typed_value)) };

	return ComponentDefinition{
		.type_hash = Hash<T>(),
		.type = std::string{ component->name },
		.value = std::move(component_json),
		.apply_live = [prototype](Entity entity) {
			if (entity.Has<T>()) {
				entity.Get<T>() = *prototype;
			} else {
				entity.Add<T>(*prototype);
			}
		},
	};
}

namespace impl {

void ResetBounceAnimationState(Entity entity);
void ResetShakeAnimationState(Entity entity);
void ResetFollowAnimationState(Entity entity, bool reset_waypoints);

} // namespace impl

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
	RotateToScript(float rotate_degrees, bool shortest_rotation_path = true, bool relative_rotation = false) :
		degrees{ rotate_degrees }, shortest_path{ shortest_rotation_path }, relative{ relative_rotation } {}

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
	ScaleToScript(V2_float target_scale, bool relative_scaling = false) : scale{ target_scale }, relative{ relative_scaling } {}

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

/// Uses the sequence step's duration, easing, and repeat settings.
struct BounceScript : public Script {
	V2_float amplitude{ 0.0f, -16.0f };
	V2_float static_offset{};
	bool symmetrical{ false };

	BounceScript() = default;
	BounceScript(V2_float bounce_amplitude, V2_float bounce_static_offset = {}, bool symmetrical_bounce = false) :
		amplitude{ bounce_amplitude }, static_offset{ bounce_static_offset }, symmetrical{ symmetrical_bounce } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnComplete() override;
	void OnCancel(SequenceCancelReason) override;

	PTGN_REFLECT(BounceScript, amplitude, static_offset, symmetrical)
};

/// Raises or lowers the entity's persistent shake trauma.
///
/// When used as a timed step, Progress() ramps to the new trauma target. When used without timing,
/// the new target is applied immediately and the script continues until explicitly stopped.
struct ShakeScript : public Script {
	float intensity{ 1.0f };
	ShakeConfig config{};
	bool reset_on_complete{ false };

	ShakeScript() = default;
	ShakeScript(float shake_intensity, ShakeConfig shake_config = {}, bool reset_shake_on_complete = false) :
		intensity{ shake_intensity }, config{ std::move(shake_config) },
		reset_on_complete{ reset_shake_on_complete } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnRepeat() override;
	void OnComplete() override;
	void OnCancel(SequenceCancelReason) override;

	PTGN_REFLECT(ShakeScript, intensity, config, reset_on_complete)

private:
	float start_trauma_{ 0.0f };
	float target_trauma_{ 0.0f };
};

/// Immediately changes the persistent shake trauma.
struct AddShakeTraumaScript : public Script {
	float intensity{ 1.0f };
	ShakeConfig config{};

	AddShakeTraumaScript() = default;
	AddShakeTraumaScript(float shake_intensity, ShakeConfig shake_config = {}) :
		intensity{ shake_intensity }, config{ std::move(shake_config) } {}

	void OnStart() override;

	PTGN_REFLECT(AddShakeTraumaScript, intensity, config)
};

/// Reduces shake trauma according to ShakeConfig::recovery_speed and completes at zero.
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

struct FollowTargetScript : public Script {
	UUID target{};
	float speed{ 120.0f };
	float stopping_distance{ 2.0f };

	FollowTargetScript() = default;
	FollowTargetScript(Entity follow_target, float follow_speed, float follow_stopping_distance = 2.0f) :
		target{ follow_target.Get<UUID>() }, speed{ follow_speed }, stopping_distance{ follow_stopping_distance } {}

	[[nodiscard]] ScriptStatus OnUpdate() override;

	PTGN_REFLECT(FollowTargetScript, target, speed, stopping_distance)
};

/// Full configured target-follow behavior matching TargetFollowConfig.
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

/// Full configured waypoint-follow behavior matching PathFollowConfig.
struct FollowPathScript : public Script {
	std::vector<V2_float> waypoints{};
	PathFollowConfig config{};
	bool reset_waypoint_index{ false };

	FollowPathScript() = default;
	FollowPathScript(
		std::vector<V2_float> follow_waypoints, PathFollowConfig follow_config = {},
		bool reset_follow_waypoint_index = false
	) :
		waypoints{ std::move(follow_waypoints) }, config{ std::move(follow_config) },
		reset_waypoint_index{ reset_follow_waypoint_index } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnComplete() override;
	void OnCancel(SequenceCancelReason) override;

	PTGN_REFLECT(FollowPathScript, waypoints, config, reset_waypoint_index)
};

struct NativeScriptCallbacks {
	std::function<void(Script&)> on_start{};
	std::function<ScriptStatus(Script&)> on_update{};
	std::function<void(Script&)> on_complete{};
	std::function<void(Script&, SequenceCancelReason)> on_cancel{};
};

struct NativeScript : public Script {
	std::shared_ptr<NativeScriptCallbacks> callbacks{};

	NativeScript() = default;
	explicit NativeScript(NativeScriptCallbacks value) :
		callbacks{ std::make_shared<NativeScriptCallbacks>(std::move(value)) } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnComplete() override;
	void OnCancel(SequenceCancelReason reason) override;

	PTGN_REFLECT_EMPTY(NativeScript)
};

struct SetVisibleScript : public Script {
	bool visible{ true };

	SetVisibleScript() = default;
	explicit SetVisibleScript(bool set_visible) : visible{ set_visible } {}

	void OnStart() override;

	PTGN_REFLECT(SetVisibleScript, visible)
};

/// @brief Plays a loaded audio asset.
///
/// loops is the number of additional plays after the first play.
/// A value of 0 plays the sound once.
struct PlaySoundScript : public Script {
	AudioKey sound{};
	float volume{ 1.0f };
	int loops{ 0 };

	PlaySoundScript() = default;

	explicit PlaySoundScript(
		AudioKey audio_key,
		float audio_volume = 1.0f,
		int audio_loops = 0
	) :
		sound{ std::move(audio_key) },
		volume{ audio_volume },
		loops{ audio_loops } {}

	void OnStart() override;

	PTGN_REFLECT(PlaySoundScript, sound, volume, loops)
};

enum class AnimationAction : std::uint8_t {
	Start,
	Stop,
	Reset,
	Pause,
	Resume,
	TogglePlaying,
	SetFrame,
	NextFrame,
	PreviousFrame
};

/// @brief Controls an Animation directly, or the active/keyed child of an AnimationMap.
struct AnimationActionScript : public Script {
	AnimationAction action{ AnimationAction::Start };
	std::string animation_key{};
	std::size_t frame{ 0 };
	bool force{ true };
	bool reset_on_stop{ false };

	void OnStart() override;

	PTGN_REFLECT(AnimationActionScript, action, animation_key, frame, force, reset_on_stop)
};

enum class TimerAction : std::uint8_t {
	Start,
	Restart,
	Stop,
	Reset,
	Pause,
	Resume,
	TogglePaused,
	Advance,
	Rewind,
	SetDuration,
	AddDuration,
	RemoveDuration
};
PTGN_REFLECT_ENUM(TimerAction);

/// @brief Controls a named timer on the script target entity.
struct TimerActionScript : public Script {
	TimerKey timer{};
	TimerAction action{ TimerAction::Start };
	millisecondsf amount{ 1000.0f };

	void OnStart() override;

	PTGN_REFLECT(TimerActionScript, timer, action, amount)
};

/// @brief Assigns a TextureKey to the owning entity.
struct SetTextureScript : public Script {
	TextureKey texture_key{};

	SetTextureScript() = default;
	explicit SetTextureScript(TextureKey texture) : texture_key{ std::move(texture) } {}

	void OnStart() override;

	PTGN_REFLECT(SetTextureScript, texture_key)
};

/// @brief Sets the reflected enabled value of a registered component on the owning entity.
/// Supports value reflected bool components such as Interactive and object reflected components
/// with a bool field named enabled, such as Draggable and Dropzone.
struct SetEnabledScript : public Script {
	std::string component{ "Interactive" };
	bool enabled{ true };

	SetEnabledScript() = default;
	SetEnabledScript(std::string component_name, bool enable = true) :
		component{ std::move(component_name) }, enabled{ enable } {}

	void OnStart() override;

	PTGN_REFLECT(SetEnabledScript, component, enabled)
};

enum class SceneChangeAction : std::uint8_t {
	Enter,
	Exit,
	Switch,
	ReEnter
};

enum class SceneTransitionStyle : std::uint8_t {
	None,
	Fade,
	CrossFade,
	Slide
};

/// @brief Queues a project scene enter, exit, switch, or re-entry operation.
/// scene_key must identify a scene in the active project's scene list.
struct SceneChangeScript : public Script {
	SceneChangeAction action{ SceneChangeAction::Switch };
	std::string scene_key{ "Main" };

	SceneTransitionStyle transition{ SceneTransitionStyle::None };
	float duration_ms{ 500.0f };
	float delay_ms{ 0.0f };
	Ease ease{ Ease::Linear };
	V2_float direction{ 1.0f, 0.0f };
	std::size_t priority{ 0 };

	void OnStart() override;

	PTGN_REFLECT(
		SceneChangeScript, action, scene_key, transition,
		duration_ms, delay_ms, ease, direction, priority
	)
};

struct EmitSignalScript : public Script {
	SignalKey signal{ "sequence.completed" };

	EmitSignalScript() = default;
	explicit EmitSignalScript(SignalKey signal_key) : signal{ std::move(signal_key) } {}

	void OnStart() override;

	PTGN_REFLECT(EmitSignalScript, signal)
};

struct AddComponentsScript : public Script {
	std::vector<ComponentDefinition> components{};

	void OnStart() override;

	PTGN_REFLECT(AddComponentsScript, components)
};

struct RemoveComponentsScript : public Script {
	std::vector<std::string> components{};

	void OnStart() override;

	PTGN_REFLECT(RemoveComponentsScript, components)
};

using SequenceFunction = std::variant<std::function<void()>, std::function<void(SequenceHandle)>>;
using DuringSequenceFunction =
	std::variant<std::function<void()>, std::function<void(Entity, float)>>;

ScriptSequence& EmitSignal(ScriptSequence& sequence, SignalKey signal);

SequenceHandle After(Scene& scene, milliseconds duration, SequenceFunction function);
SequenceHandle During(Scene& scene, milliseconds duration, DuringSequenceFunction function);

/// Runtime only property animation helper used from custom C++ scripts.
template <typename T, typename TGetter, typename TSetter>
SequenceHandle PropertyTo(
	Entity entity, SequenceChannelKey channel, T target, float duration_ms, TGetter getter,
	TSetter setter, Ease ease = Ease::Linear, bool force = true
) {
	if (!entity) {
		return {};
	}

	struct State {
		T start{};
		T target{};
	};
	auto state{ std::make_shared<State>() };
	state->target = std::move(target);

	NativeScript action{ NativeScriptCallbacks{
		.on_start = [state, getter = std::move(getter)](Script& script) mutable {
			state->start = std::invoke(getter, script.Owner());
		},
		.on_update = [state, setter = std::move(setter)](Script& script) mutable {
			std::invoke(
				setter, script.Owner(),
				state->start + (state->target - state->start) * script.Progress()
			);
			return script.LinearProgress() >= 1.0f
				? ScriptStatus::Complete
				: ScriptStatus::Running;
		},
	} };

	ScriptSequence sequence{ "Property To" };
	sequence.Channel(channel).Transient().During(duration_ms, std::move(action)).Ease(ease);
	return script_runtime::RunInChannel(
		entity, std::move(channel), std::move(sequence),
		force ? ReentryMode::Restart : ReentryMode::Queue
	);
}

} // namespace ptgn
