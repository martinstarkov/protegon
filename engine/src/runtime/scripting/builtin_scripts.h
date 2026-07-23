#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/strong_string.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/scripting/script.h"

namespace ptgn {

struct SignalKey : StrongString<SignalKey> {
	using StrongString::StrongString;
	SignalKey() = default;

	PTGN_REFLECT_VALUE(SignalKey, value)
};

struct Signal {
	SignalKey key;

	PTGN_REFLECT(Signal, key)
};

struct ComponentDefinition {
	TypeHashValue type_hash{ 0 };
	std::string type;
	json value;

	// C++-authored definitions avoid a JSON round-trip at runtime.
	std::function<void(Entity)> apply_live;

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
	json component_json;
	component_json = typed_value;
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

struct MoveToScript final : Script {
	V2_float destination{ 0.0f, 64.0f };
	bool relative{ true };

	MoveToScript() = default;
	MoveToScript(V2_float destination, bool relative = true) :
		destination{ destination }, relative{ relative } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnRepeat() override;

	PTGN_REFLECT(MoveToScript, destination, relative)

private:
	V2_float start_{};
	V2_float end_{};
};

struct RotateToScript final : Script {
	float degrees{ 90.0f };
	bool shortest_path{ true };
	bool relative{ false };

	RotateToScript() = default;
	RotateToScript(float degrees, bool shortest_path = true, bool relative = false) :
		degrees{ degrees }, shortest_path{ shortest_path }, relative{ relative } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnRepeat() override;

	PTGN_REFLECT(RotateToScript, degrees, shortest_path, relative)

private:
	float start_degrees_{ 0.0f };
	float delta_degrees_{ 0.0f };
};

struct ScaleToScript final : Script {
	V2_float scale{ 1.0f, 1.0f };
	bool relative{ false };

	ScaleToScript() = default;
	ScaleToScript(V2_float scale, bool relative = false) : scale{ scale }, relative{ relative } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnRepeat() override;

	PTGN_REFLECT(ScaleToScript, scale, relative)

private:
	V2_float start_{};
	V2_float end_{};
};

struct FollowTargetScript final : Script {
	Entity target;
	float speed{ 120.0f };
	float stopping_distance{ 2.0f };

	FollowTargetScript() = default;
	FollowTargetScript(Entity target, float speed, float stopping_distance = 2.0f) :
		target{ target }, speed{ speed }, stopping_distance{ stopping_distance } {}

	[[nodiscard]] ScriptStatus OnUpdate() override;

	PTGN_REFLECT(FollowTargetScript, target, speed, stopping_distance)
};

struct NativeScriptCallbacks {
	std::function<void(Script&)> on_start;
	std::function<ScriptStatus(Script&)> on_update;
	std::function<void(Script&)> on_complete;
	std::function<void(Script&, SequenceCancelReason)> on_cancel;
};

struct NativeScript final : Script {
	std::shared_ptr<NativeScriptCallbacks> callbacks;

	NativeScript() = default;
	explicit NativeScript(NativeScriptCallbacks value) :
		callbacks{ std::make_shared<NativeScriptCallbacks>(std::move(value)) } {}

	void OnStart() override;
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnComplete() override;
	void OnCancel(SequenceCancelReason reason) override;

	PTGN_REFLECT_EMPTY(NativeScript)
};

struct SetVisibleScript final : Script {
	bool visible{ true };

	SetVisibleScript() = default;
	explicit SetVisibleScript(bool visible) : visible{ visible } {}

	void OnStart() override;

	PTGN_REFLECT(SetVisibleScript, visible)
};

struct EmitSignalScript final : Script {
	SignalKey signal{ "sequence.completed" };

	EmitSignalScript() = default;
	explicit EmitSignalScript(SignalKey signal) : signal{ std::move(signal) } {}

	void OnStart() override;

	PTGN_REFLECT(EmitSignalScript, signal)
};

struct AddComponentsScript final : Script {
	std::vector<ComponentDefinition> components;

	void OnStart() override;

	PTGN_REFLECT(AddComponentsScript, components)
};

struct RemoveComponentsScript final : Script {
	std::vector<std::string> components;

	void OnStart() override;

	PTGN_REFLECT(RemoveComponentsScript, components)
};

using SequenceFunction = std::variant<std::function<void()>, std::function<void(SequenceHandle)>>;
using DuringSequenceFunction =
	std::variant<std::function<void()>, std::function<void(Entity, float)>>;

ScriptSequence& EmitSignal(ScriptSequence& sequence, SignalKey signal);

SequenceHandle After(Scene& scene, milliseconds duration, SequenceFunction function);
SequenceHandle During(Scene& scene, milliseconds duration, DuringSequenceFunction function);

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
			return ScriptStatus::Running;
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
