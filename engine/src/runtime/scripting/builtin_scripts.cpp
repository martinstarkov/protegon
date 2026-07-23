#include "runtime/scripting/builtin_scripts.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

#include "core/math/angle.h"
#include "core/math/transform.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"

namespace ptgn {

ComponentDefinition MakeComponentDefinition(const RegisteredComponent& component) {
	if (!component.make_default_json) {
		return {};
	}

	json value{ component.make_default_json() };
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
				registration && registration->add_default) {
				registration->add_default(entity);
			}
		},
	};
}

ComponentDefinition MakeComponentDefinition(std::string_view name) {
	const auto* component{ ComponentRegistry::Find(name) };
	return component ? MakeComponentDefinition(*component) : ComponentDefinition{};
}

void MoveToScript::OnStart() {
	start_ = Owner().Get<Transform>().position;
	end_ = relative ? start_ + destination : destination;
}

ScriptStatus MoveToScript::OnUpdate() {
	Owner().Get<Transform>().position = start_ + (end_ - start_) * Progress();
	return ScriptStatus::Running;
}

void MoveToScript::OnRepeat() {
	if (!IsReversed()) {
		OnStart();
	}
}

void RotateToScript::OnStart() {
	start_degrees_ = Owner().Get<Transform>().rotation.ToDeg().value;
	const float end_degrees{ relative ? start_degrees_ + degrees : degrees };
	delta_degrees_ = end_degrees - start_degrees_;
	if (shortest_path) {
		delta_degrees_ = std::remainder(delta_degrees_, 360.0f);
	}
}

ScriptStatus RotateToScript::OnUpdate() {
	const float value{ start_degrees_ + delta_degrees_ * Progress() };
	Owner().Get<Transform>().rotation = Degrees{ value }.ToRad();
	return ScriptStatus::Running;
}

void RotateToScript::OnRepeat() {
	if (!IsReversed()) {
		OnStart();
	}
}

void ScaleToScript::OnStart() {
	start_ = Owner().Get<Transform>().scale;
	end_ = relative ? start_ * scale : scale;
}

ScriptStatus ScaleToScript::OnUpdate() {
	Owner().Get<Transform>().scale = start_ + (end_ - start_) * Progress();
	return ScriptStatus::Running;
}

void ScaleToScript::OnRepeat() {
	if (!IsReversed()) {
		OnStart();
	}
}

ScriptStatus FollowTargetScript::OnUpdate() {
	if (!target || !target.Has<Transform>() || !Owner().Has<Transform>()) {
		return ScriptStatus::Complete;
	}

	auto& position{ Owner().Get<Transform>().position };
	const V2_float offset{ target.Get<Transform>().position - position };
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
	SetVisible(Owner(), visible);
}

void EmitSignalScript::OnStart() {
	(void)script_runtime::DispatchGlobal<Signal>(GetScene(), Signal{ signal });
}

void AddComponentsScript::OnStart() {
	for (const auto& component : components) {
		if (component.apply_live) {
			component.apply_live(Owner());
			continue;
		}
		const auto* registration{ ComponentRegistry::Find(component.type) };
		if (!registration) {
			continue;
		}
		if (registration->is_empty && registration->add_default) {
			registration->add_default(Owner());
		} else if (registration->deserialize && !component.value.is_null()) {
			registration->deserialize(component.value, Owner());
		} else if (registration->add_default) {
			registration->add_default(Owner());
		}
	}
}

void RemoveComponentsScript::OnStart() {
	for (const auto& name : components) {
		if (const auto* registration{ ComponentRegistry::Find(name) }) {
			registration->remove(Owner());
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
	(void)handle.Start();
	return handle;
}

SequenceHandle During(Scene& scene, milliseconds duration, DuringSequenceFunction function) {
	Entity owner{ scene.CreateEntity("During Script Sequence") };

	NativeScript callback{ NativeScriptCallbacks{
		.on_update = [function = std::move(function)](Script& script) mutable {
			std::visit(
				[&](auto& fn) {
					if constexpr (std::is_invocable_v<decltype(fn), Entity, float>) {
						fn(script.Owner(), script.Progress());
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
	(void)handle.Start();
	return handle;
}

} // namespace ptgn
