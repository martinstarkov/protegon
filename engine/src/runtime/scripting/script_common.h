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
#include "core/math/easing.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/strong_string.h"
#include "runtime/animation/follow_config.h"
#include "runtime/animation/shake_config.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_filter.h"
#include "runtime/graphics/custom_shader.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/physics/collider.h"
#include "runtime/scene/scene_transition.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/variables.h"
#include "runtime/timer/timer.h"
#include "runtime/world/tilemap.h"

namespace ptgn {

struct SignalKey : public StrongString<SignalKey> {
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
		.type	   = std::string{ component->name },
		.value	   = std::move(component_json),
		.apply_live =
			[prototype](Entity entity) {
				if (entity.Has<T>()) {
					entity.Get<T>() = *prototype;
				} else {
					entity.Add<T>(*prototype);
				}
			},
	};
}

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

	void OnStart() override {
		if (callbacks && callbacks->on_start) {
			callbacks->on_start(*this);
		}
	}

	ScriptStatus OnUpdate() override {
		if (callbacks && callbacks->on_update) {
			return callbacks->on_update(*this);
		}

		return ScriptStatus::Complete;
	}

	void OnComplete() override {
		if (callbacks && callbacks->on_complete) {
			callbacks->on_complete(*this);
		}
	}

	void OnCancel(SequenceCancelReason reason) override {
		if (callbacks && callbacks->on_cancel) {
			callbacks->on_cancel(*this, reason);
		}
	}

	PTGN_REFLECT_EMPTY(NativeScript)
};

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
		.on_start = [state, getter = std::move(getter)](
						Script& script
					) mutable { state->start = std::invoke(getter, script.Owner()); },
		.on_update =
			[state, setter = std::move(setter)](Script& script) mutable {
				std::invoke(
					setter, script.Owner(),
					state->start + (state->target - state->start) * script.Progress()
				);

				return script.LinearProgress() >= 1.0f ? ScriptStatus::Complete
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

namespace impl {

void ResetBounceAnimationState(Entity entity);
void ResetShakeAnimationState(Entity entity);
void ResetFollowAnimationState(Entity entity, bool reset_waypoints);

} // namespace impl

} // namespace ptgn
