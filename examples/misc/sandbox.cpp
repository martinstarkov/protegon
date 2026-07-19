// script_sequence_old_ui_registry_demo_v29.cpp
//
// Registry-driven Script and ScriptSequence demo using the engine ptgn::Scene.
//
// Architecture boundaries:
//   runtime - concrete registered Script/ScriptAction objects and sequence playback.
//   editor  - free registry-driven Dear ImGui drawing functions.
//   demo    - a normal ptgn::Scene launched through ptgn::Application.
//
// A ScriptSequence is authored data used by SequenceScript, which is registered and attached through
// the same Script API as custom C++ scripts. Events use the engine Event dispatcher. Sequence event
// conditions store only their registered identity and a JSON value. The event registries convert
// that JSON to the concrete registered condition type for matching and editor drawing.

#include <imgui.h>
#include <imgui_stdlib.h>

#ifndef MAGIC_ENUM_RANGE_MAX
#define MAGIC_ENUM_RANGE_MAX 512
#endif
#include <magic_enum/magic_enum.hpp>

#include "app/application.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"

#include "core/editor.h"
#include "panels/inspector_fields.h"
#include "core/assert.h"
#include "core/event/event.h"
#include "core/event/key_event.h"
#include "core/event/mouse_event.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/math/easing.h"
#include "core/util/hash.h"
#include "core/util/reflection.h"
#include "core/util/strong_string.h"
#include "core/util/type_info.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "panels/component_editor_registry.h"
#include "runtime/ecs/component_registration.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/physics/collision_event.h"
#include "serialization/json/json.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <concepts>
#include <cctype>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <charconv>
#include <cstdio>
#include <deque>
#include <functional>
#include <iterator>
#include <memory>
#include <limits>
#include <optional>
#include <numbers>
#include <ranges>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ptgn {

inline void to_json(ptgn::json& output, const ImVec4& value) {
	output = ptgn::json{ value.x, value.y, value.z, value.w };
}

inline void from_json(const ptgn::json& input, ImVec4& value) {
	value = ImVec4{
		input.at(0).get<float>(),
		input.at(1).get<float>(),
		input.at(2).get<float>(),
		input.at(3).get<float>(),
	};
}

using SequenceId = std::uint64_t;

using TypeHashValue = std::size_t;

template <typename T>
[[nodiscard]] bool TryReadJson(const ptgn::json& input, T& output) {
	if (input.is_null()) {
		return false;
	}
	try {
		input.get_to(output);
		return true;
	} catch (...) {
		return false;
	}
}

template <typename T>
[[nodiscard]] T JsonValueOr(
	const ptgn::json& input,
	std::string_view key,
	T fallback
) {
	if (!input.is_object()) {
		return fallback;
	}
	const auto it{ input.find(std::string{ key }) };
	if (it == input.end() || it->is_null()) {
		return fallback;
	}
	try {
		return it->template get<T>();
	} catch (...) {
		return fallback;
	}
}

enum class ReentryMode {
	IgnoreWhileRunning,
	Restart,
	Queue
};
PTGN_REFLECT_ENUM(ReentryMode);

enum class ActionStatus {
	Running,
	Complete
};

enum class ActionCompletion {
	Instant,
	Duration,
	ActionControlled,
	Infinite
};
PTGN_REFLECT_ENUM(ActionCompletion);

enum class SequenceCancelReason {
	Stopped,
	Reset,
	Cleared,
	Skipped,
	Replaced,
	BindingRemoved,
	OwnerDestroyed
};

enum class SequenceStopMode {
	Current,
	All
};

enum class SequenceLifecycle {
	Start,
	Complete,
	Reset,
	Stop,
	Pause,
	Resume,
	ActionStart,
	ActionComplete,
	ActionCancel,
	Repeat,
	Yoyo
};
PTGN_REFLECT_ENUM(SequenceLifecycle);

struct ActionTiming {
	float duration_ms{ 300.0f };
	ptgn::Ease ease{ ptgn::Ease::Linear };
	int additional_repeats{ 0 };
	bool infinite_repeats{ false };
	bool reversed{ false };
	bool yoyo{ false };

	PTGN_REFLECT(
		ActionTiming, duration_ms, ease, additional_repeats, infinite_repeats, reversed, yoyo
	)
};

struct ComponentDefinition {
	TypeHashValue type_hash{ 0 };
	std::string type;
	ptgn::json value;

	// C++-authored prefab components avoid a needless JSON round-trip at runtime.
	std::function<void(ptgn::Entity)> apply_live;

	PTGN_REFLECT(ComponentDefinition, type_hash, type, value)
};

struct EventCondition {
	bool enabled{ true };
	bool consume{ false };
	TypeHashValue type_hash{ 0 };
	std::string type;
	ptgn::json value{ ptgn::json::object() };

	PTGN_REFLECT(EventCondition, enabled, consume, type_hash, type, value)
};

class ScriptAction;

struct Action {
	bool enabled{ true };
	TypeHashValue type_hash{ 0 };
	std::string type;
	ptgn::json value;
	std::optional<ActionCompletion> completion;
	std::optional<ActionTiming> timing;

	// Authored C++ actions keep a typed construction path so runtime playback does not
	// have to round-trip through JSON. Loaded/editor-authored actions use the registry JSON path.
	std::function<std::unique_ptr<ScriptAction>(ptgn::Entity)> runtime_factory;

	PTGN_REFLECT(Action, enabled, type_hash, type, value, completion, timing)
};

struct LifecycleAction {
	bool enabled{ true };
	SequenceLifecycle lifecycle{ SequenceLifecycle::Complete };
	Action action;

	PTGN_REFLECT(LifecycleAction, enabled, lifecycle, action)
};

class SequenceScript;
struct ScriptsComponent;
struct ScriptEntry;
struct ScriptSequence;
struct SequenceHandle;
struct SequenceChannelKey;
struct SignalKey;

class ScriptAction {
public:
	virtual ~ScriptAction() = default;

	virtual void OnStart() {}
	[[nodiscard]] virtual ActionStatus OnUpdate() { return ActionStatus::Running; }
	virtual void OnRepeat() {}
	virtual void OnComplete() {}
	virtual void OnCancel(SequenceCancelReason) {}

	[[nodiscard]] ptgn::Entity Owner() const { return entity; }
	[[nodiscard]] ptgn::Scene& GetScene() { return entity.GetScene(); }
	[[nodiscard]] const ptgn::Scene& GetScene() const { return entity.GetScene(); }
	[[nodiscard]] float DeltaSeconds() const { return delta_seconds_; }
	[[nodiscard]] float LinearProgress() const { return linear_progress_; }
	[[nodiscard]] float Progress() const { return progress_; }
	[[nodiscard]] int RepeatIndex() const { return repeat_; }
	[[nodiscard]] bool IsReversed() const { return reversed_; }

protected:
	ptgn::Entity entity;

public:
	// Runtime binding hooks used by the registry-driven sequence player.
	void Bind(ptgn::Entity owner) { entity = owner; }

	void SetFrame(
		float delta_seconds,
		float linear_progress,
		float progress,
		int repeat,
		bool reversed
	) {
		delta_seconds_ = delta_seconds;
		linear_progress_ = linear_progress;
		progress_ = progress;
		repeat_ = repeat;
		reversed_ = reversed;
	}

	float delta_seconds_{ 0.0f };
	float linear_progress_{ 0.0f };
	float progress_{ 0.0f };
	int repeat_{ 0 };
	bool reversed_{ false };
};

[[nodiscard]] inline ComponentDefinition MakeComponentDefinition(
	const ptgn::RegisteredComponent& component
) {
	if (!component.make_default_json) {
		return {};
	}

	ptgn::json value{ component.make_default_json() };
	if (value.is_null()) {
		value = ptgn::json::object();
	}

	const std::string component_name{ component.name };
	return ComponentDefinition{
		.type_hash = static_cast<TypeHashValue>(component.type_id),
		.type = component_name,
		.value = std::move(value),
		.apply_live = [component_name](ptgn::Entity entity) {
			if (const auto* registration{ ptgn::ComponentRegistry::Find(component_name) };
				registration && registration->add_default) {
				registration->add_default(entity);
			}
		},
	};
}

[[nodiscard]] inline ComponentDefinition MakeComponentDefinition(std::string_view name) {
	const auto* component{ ptgn::ComponentRegistry::Find(name) };
	return component ? MakeComponentDefinition(*component) : ComponentDefinition{};
}

template <typename T>
[[nodiscard]] ComponentDefinition MakeComponentDefinition() {
	const auto* component{ ptgn::ComponentRegistry::Find<T>() };
	return component ? MakeComponentDefinition(*component) : ComponentDefinition{};
}

template <typename T>
[[nodiscard]] ComponentDefinition MakeComponentDefinition(T value) {
	const auto* component{ ptgn::ComponentRegistry::Find<T>() };
	if (!component) {
		return {};
	}

	T typed_value{ std::move(value) };
	ptgn::json component_json;
	component_json = typed_value;
	auto prototype{ std::make_shared<T>(std::move(typed_value)) };

	return ComponentDefinition{
		.type_hash = ptgn::Hash<T>(),
		.type = std::string{ component->name },
		.value = std::move(component_json),
		.apply_live = [prototype](ptgn::Entity entity) {
			if (entity.Has<T>()) {
				entity.Get<T>() = *prototype;
			} else {
				entity.Add<T>(*prototype);
			}
		},
	};
}

struct PrefabSpawnRequest {
	std::string_view prefab_key;
	ptgn::Entity owner;
	ptgn::V2_float position{};
	bool parent_to_owner{ false };
	bool inherit_owner_rotation{ true };
	bool inherit_owner_scale{ true };
	bool random_rotation{ false };
};

struct SharedScriptSequenceRegistry;

void LogScriptActivity(ptgn::Scene& scene, std::string text);
[[nodiscard]] std::string_view GetEntityName(ptgn::Entity entity);
[[nodiscard]] std::string_view GetEntityTag(ptgn::Entity entity);
[[nodiscard]] int GetEntityMask(ptgn::Entity entity);
[[nodiscard]] ptgn::Entity SpawnScriptPrefab(
	ptgn::Scene& scene,
	const PrefabSpawnRequest& request
);
[[nodiscard]] SharedScriptSequenceRegistry& GetSharedScriptSequences(ptgn::Scene& scene);

struct SequenceEventRegistration {
	TypeHashValue type_hash{ 0 };
	TypeHashValue event_type_hash{ 0 };
	std::uint32_t schema_version{ 1 };
	std::string key;
	std::function<void(EventCondition&)> set_defaults;
	std::function<bool(ptgn::Entity, Event, const EventCondition&, bool consume)> matches;
	std::function<bool(ptgn::Entity)> available;
};

class SequenceEventRegistry {
public:
	template <typename TEvent, typename FMatch, typename FAvailable>
	static bool Register(
		std::string key,
		ptgn::json default_value,
		FMatch&& matches,
		FAvailable&& available
	) {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ ptgn::Hash(std::string_view{ key }) };
		if (Find(key) || Find(type_hash)) {
			return false;
		}
		if (default_value.is_null()) {
			default_value = ptgn::json::object();
		}

		entries.push_back(
			SequenceEventRegistration{
				.type_hash = type_hash,
				.event_type_hash = ptgn::Hash<TEvent>(),
				.key = std::move(key),
				.set_defaults = [value = std::move(default_value)](EventCondition& output) {
					output.value = value;
				},
				.matches =
					[fn = std::forward<FMatch>(matches)](
						ptgn::Entity owner,
						Event event,
						const EventCondition& input,
						bool consume
					) mutable {
						bool matched{ false };
						if constexpr (std::is_empty_v<TEvent>) {
							event.Dispatch<TEvent>([&]() -> bool {
								matched = std::invoke(fn, owner, input.value, TEvent{});
								return matched && consume;
							});
						} else {
							event.Dispatch<TEvent>([&](const TEvent& payload) -> bool {
								matched = std::invoke(fn, owner, input.value, payload);
								return matched && consume;
							});
						}
						return matched;
					},
				.available = std::forward<FAvailable>(available),
			}
		);
		return true;
	}

	template <typename TEvent, typename FMatch>
	static bool Register(
		std::string key,
		ptgn::json default_value,
		FMatch&& matches
	) {
		return Register<TEvent>(
			std::move(key),
			std::move(default_value),
			std::forward<FMatch>(matches),
			[](ptgn::Entity) {
				return true;
			}
		);
	}

	[[nodiscard]] static EventCondition MakeCondition(
		std::string_view key,
		ptgn::json value = nullptr
	) {
		const auto* entry{ Find(key) };
		if (!entry) {
			return {};
		}

		EventCondition condition{
			.enabled = true,
			.consume = false,
			.type_hash = entry->type_hash,
			.type = entry->key,
		};
		if (value.is_null()) {
			entry->set_defaults(condition);
		} else {
			condition.value = std::move(value);
		}
		if (condition.value.is_null()) {
			condition.value = ptgn::json::object();
		}
		return condition;
	}

	[[nodiscard]] static const SequenceEventRegistration* Find(std::string_view key) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [key](const auto& entry) {
			return entry.key == key;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const SequenceEventRegistration* Find(TypeHashValue type_hash) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const std::vector<SequenceEventRegistration>& Entries() {
		return MutableEntries();
	}

private:
	[[nodiscard]] static std::vector<SequenceEventRegistration>& MutableEntries() {
		static std::vector<SequenceEventRegistration> entries;
		return entries;
	}
};

struct ActionRegistration {
	TypeHashValue type_hash{ 0 };
	std::uint32_t schema_version{ 1 };
	std::string key;
	ActionCompletion completion{ ActionCompletion::Instant };
	bool supports_timing{ false };
	bool requires_timing{ false };
	bool serializable{ true };
	std::optional<ActionTiming> default_timing;
	std::function<ptgn::json()> make_default;
	std::function<std::unique_ptr<ScriptAction>(ptgn::Entity)> instantiate_default;
	std::function<std::unique_ptr<ScriptAction>(ptgn::Entity, const ptgn::json&)> instantiate;
};

class ActionRegistry {
public:
	template <typename T>
		requires std::derived_from<T, ScriptAction>
	static bool Register(
		std::string key,
		bool supports_timing = false,
		bool requires_timing = false,
		std::optional<ActionTiming> default_timing = std::nullopt,
		ActionCompletion completion = ActionCompletion::Instant,
		bool serializable = true
	) {
		auto& entries{ MutableEntries() };
		if (Find(key) || Find(ptgn::Hash<T>())) {
			return false;
		}
		if (requires_timing && completion == ActionCompletion::Instant) {
			completion = ActionCompletion::Duration;
		}

		entries.push_back(
			ActionRegistration{
				.type_hash = ptgn::Hash<T>(),
				.key = std::move(key),
				.completion = completion,
				.supports_timing = supports_timing,
				.requires_timing = requires_timing,
				.serializable = serializable,
				.default_timing = default_timing,
				.make_default = [] {
					ptgn::json output;
					output = T{};
					return output;
				},
				.instantiate_default = [](ptgn::Entity owner) {
					auto action{ std::make_unique<T>() };
					action->Bind(owner);
					return action;
				},
				.instantiate = [](ptgn::Entity owner, const ptgn::json& input) {
					auto action{ std::make_unique<T>() };
					(void)TryReadJson(input, *action);
					action->Bind(owner);
					return action;
				},
			}
		);
		return true;
	}

	template <typename T>
	[[nodiscard]] static std::string_view Key() {
		const auto* entry{ Find(ptgn::Hash<T>()) };
		return entry ? std::string_view{ entry->key } : std::string_view{};
	}

	template <typename T>
		requires (!std::convertible_to<std::remove_cvref_t<T>, std::string_view>)
	[[nodiscard]] static Action Make(T value = {}) {
		const auto* entry{ Find(ptgn::Hash<T>()) };
		if (!entry) {
			return {};
		}

		T typed_value{ std::move(value) };
		ptgn::json action_json;
		action_json = typed_value;
		return Action{
			.enabled = true,
			.type_hash = entry->type_hash,
			.type = entry->key,
			.value = std::move(action_json),
			.timing = entry->default_timing,
			.runtime_factory =
				[typed_value = std::move(typed_value)](ptgn::Entity owner) mutable {
					auto action{ std::make_unique<T>(typed_value) };
					action->Bind(owner);
					return action;
				},
		};
	}

	[[nodiscard]] static Action Make(std::string_view key) {
		const auto* entry{ Find(key) };
		return entry
			? Action{
				.enabled = true,
				.type_hash = entry->type_hash,
				.type = entry->key,
				.value = entry->make_default(),
				.timing = entry->default_timing,
				.runtime_factory = entry->instantiate_default,
			}
			: Action{};
	}

	[[nodiscard]] static const ActionRegistration* Find(std::string_view key) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [key](const auto& entry) {
			return entry.key == key;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const ActionRegistration* Find(TypeHashValue type_hash) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const std::vector<ActionRegistration>& Entries() {
		return MutableEntries();
	}

private:
	[[nodiscard]] static std::vector<ActionRegistration>& MutableEntries() {
		static std::vector<ActionRegistration> entries;
		return entries;
	}
};

struct ScriptRegistration {
	TypeHashValue type_hash{ 0 };
	std::uint32_t schema_version{ 1 };
	std::string key;
	std::function<ptgn::json()> make_default;
	std::function<ptgn::Script*(ptgn::Entity, const ptgn::json&)> attach;
	std::function<void(ptgn::Script&, const ptgn::json&)> apply;
};

class ScriptRegistry {
public:
	template <typename T>
		requires std::derived_from<T, ptgn::Script>
	static bool Register(std::string key) {
		auto& entries{ MutableEntries() };
		if (Find(key) || Find(ptgn::Hash<T>())) {
			return false;
		}

		entries.push_back(
			ScriptRegistration{
				.type_hash = ptgn::Hash<T>(),
				.key = std::move(key),
				.make_default = [] {
					ptgn::json output;
					output = T{};
					return output;
				},
				.attach = [](ptgn::Entity owner, const ptgn::json& input) {
					T value{};
					(void)TryReadJson(input, value);
					auto& script{ ptgn::AddScript<T>(owner, std::move(value)) };
					return static_cast<ptgn::Script*>(&script);
				},
				.apply = [](ptgn::Script& script, const ptgn::json& input) {
					(void)TryReadJson(input, static_cast<T&>(script));
				},
			}
		);
		return true;
	}

	template <typename T>
	[[nodiscard]] static std::string_view Key() {
		const auto* entry{ Find(ptgn::Hash<T>()) };
		return entry ? std::string_view{ entry->key } : std::string_view{};
	}

	template <typename T>
	[[nodiscard]] static ScriptEntry Make(T value = {});

	[[nodiscard]] static ScriptEntry Make(std::string_view key);

	[[nodiscard]] static const ScriptRegistration* Find(std::string_view key) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [key](const auto& entry) {
			return entry.key == key;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const ScriptRegistration* Find(TypeHashValue type_hash) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const std::vector<ScriptRegistration>& Entries() {
		return MutableEntries();
	}

private:
	[[nodiscard]] static std::vector<ScriptRegistration>& MutableEntries() {
		static std::vector<ScriptRegistration> entries;
		return entries;
	}
};

struct SequenceChannelKey : ptgn::StrongString<SequenceChannelKey> {
	using StrongString::StrongString;
	SequenceChannelKey() = default;

	PTGN_REFLECT_VALUE(SequenceChannelKey, value)
};

struct ScriptSequenceRuntime {
	bool running{ false };
	bool paused{ false };
	bool completed{ false };
	bool waiting_for_channel{ false };
	int queued_runs{ 0 };
	std::size_t action_index{ 0 };
	float elapsed_ms{ 0.0f };
	int current_repeat{ 0 };
	bool currently_reversed{ false };
	std::unique_ptr<ScriptAction> action_instance;
	int completed_runs{ 0 };

	ScriptSequenceRuntime() = default;
	ScriptSequenceRuntime(ScriptSequenceRuntime&&) noexcept = default;
	ScriptSequenceRuntime& operator=(ScriptSequenceRuntime&&) noexcept = default;
	ScriptSequenceRuntime(const ScriptSequenceRuntime&) = delete;
	ScriptSequenceRuntime& operator=(const ScriptSequenceRuntime&) = delete;

	void ClearActiveAction() {
		action_instance.reset();
		elapsed_ms = 0.0f;
		current_repeat = 0;
		currently_reversed = false;
	}
};

struct ScriptSequence {
private:
	[[nodiscard]] static SequenceId NextSequenceId() {
		static SequenceId next{ 1 };
		return next++;
	}

public:
	SequenceId id{ NextSequenceId() };
	bool enabled{ true };
	bool shared_reference{ false };
	SequenceId shared_sequence_id{ 0 };
	std::string name{ "New Script Sequence" };
	ReentryMode reentry{ ReentryMode::IgnoreWhileRunning };
	std::optional<SequenceChannelKey> channel;
	bool transient{ false };
	bool remove_binding_on_complete{ false };
	bool destroy_owner_on_complete{ false };
	std::vector<EventCondition> start_events;
	std::vector<EventCondition> stop_events;
	std::vector<Action> actions;
	std::vector<LifecycleAction> lifecycle_actions;
	ScriptSequenceRuntime runtime;

	ScriptSequence() = default;
	explicit ScriptSequence(std::string sequence_name) : name{ std::move(sequence_name) } {}
	ScriptSequence(ScriptSequence&&) noexcept = default;
	ScriptSequence& operator=(ScriptSequence&&) noexcept = default;

	ScriptSequence& Reentry(ReentryMode value);
	ScriptSequence& Channel(SequenceChannelKey value);
	ScriptSequence& Transient(bool remove_on_complete = true);
	ScriptSequence& DestroyOwnerOnComplete(bool value = true);

	ScriptSequence& StartOn(std::string_view key, ptgn::json value = nullptr);
	ScriptSequence& StopOn(std::string_view key, ptgn::json value = nullptr);

	template <typename TAction>
	ScriptSequence& Then(TAction action = {});

	template <typename TAction>
	ScriptSequence& UntilComplete(TAction action = {});

	template <typename TAction>
	ScriptSequence& Forever(TAction action = {});

	template <typename TAction>
	ScriptSequence& During(float duration_ms, TAction action = {});

	ScriptSequence& Ease(ptgn::Ease value);
	ScriptSequence& Repeat(int additional_repeats);
	ScriptSequence& Infinite();
	ScriptSequence& Reversed(bool value = true);
	ScriptSequence& Yoyo(bool value = true);
	ScriptSequence& Wait(float duration_ms);
	ScriptSequence& EmitSignal(SignalKey signal);

	ScriptSequence(const ScriptSequence& other) :
		id{ NextSequenceId() },
		enabled{ other.enabled },
		shared_reference{ other.shared_reference },
		shared_sequence_id{ other.shared_sequence_id },
		name{ other.name },
		reentry{ other.reentry },
		channel{ other.channel },
		transient{ other.transient },
		remove_binding_on_complete{ other.remove_binding_on_complete },
		destroy_owner_on_complete{ other.destroy_owner_on_complete },
		start_events{ other.start_events },
		stop_events{ other.stop_events },
		actions{ other.actions },
		lifecycle_actions{ other.lifecycle_actions } {}

	ScriptSequence& operator=(const ScriptSequence& other) {
		if (this == &other) {
			return *this;
		}
		ScriptSequence copy{ other };
		*this = std::move(copy);
		return *this;
	}

	PTGN_REFLECT(
		ScriptSequence, id, enabled, shared_reference, shared_sequence_id, name, reentry, channel,
		transient, remove_binding_on_complete, destroy_owner_on_complete, start_events, stop_events,
		actions, lifecycle_actions
	)

private:
	ActionTiming& LatestDuringTiming();
};

class SequenceScript final : public ptgn::Script {
public:
	ScriptSequence sequence;
	bool initialized{ false };

	void OnCreate() override;
	void OnUpdate() override;
	void OnEvent(Event event) override;

	PTGN_REFLECT(SequenceScript, sequence)
};

struct ScriptEntry {
	bool enabled{ true };
	TypeHashValue type_hash{ 0 };
	std::string type;
	ptgn::json value;

	// Non-owning. The engine's normal ptgn::impl::Scripts container owns the Script.
	ptgn::Script* instance{ nullptr };
	// C++-authored entries retain a typed prototype; serialized entries fall back to JSON.
	std::function<ptgn::Script*(ptgn::Entity)> attach_live;

	ScriptEntry() = default;
	ScriptEntry(ScriptEntry&&) noexcept = default;
	ScriptEntry& operator=(ScriptEntry&&) noexcept = default;

	ScriptEntry(const ScriptEntry& other) :
		enabled{ other.enabled },
		type_hash{ other.type_hash },
		type{ other.type },
		value{ other.value },
		attach_live{ other.attach_live } {}

	ScriptEntry& operator=(const ScriptEntry& other) {
		if (this == &other) {
			return *this;
		}
		ScriptEntry copy{ other };
		*this = std::move(copy);
		return *this;
	}

	PTGN_REFLECT(ScriptEntry, enabled, type_hash, type, value)
};

template <typename T>
ScriptEntry ScriptRegistry::Make(T value) {
	const auto* registration{ Find(ptgn::Hash<T>()) };
	if (!registration) {
		return {};
	}

	T typed_value{ std::move(value) };
	ptgn::json snapshot;
	snapshot = typed_value;
	auto prototype{ std::make_shared<T>(std::move(typed_value)) };

	ScriptEntry entry;
	entry.enabled = true;
	entry.type_hash = registration->type_hash;
	entry.type = registration->key;
	entry.value = std::move(snapshot);
	entry.attach_live = [prototype](ptgn::Entity owner) {
		T script{ *prototype };
		if constexpr (requires { script.sequence.id; prototype->sequence.id; }) {
			script.sequence.id = prototype->sequence.id;
		}
		return static_cast<ptgn::Script*>(
			&ptgn::AddScript<T>(owner, std::move(script))
		);
	};
	return entry;
}

inline ScriptEntry ScriptRegistry::Make(std::string_view key) {
	const auto* registration{ Find(key) };
	if (!registration) {
		return {};
	}

	ScriptEntry entry;
	entry.enabled = true;
	entry.type_hash = registration->type_hash;
	entry.type = registration->key;
	entry.value = registration->make_default();
	return entry;
}

struct SequenceChannelRuntime {
	SequenceChannelKey key;
	std::optional<SequenceId> active;
	std::deque<SequenceId> waiting;
};

struct ScriptsComponent {
	std::vector<ScriptEntry> scripts;
	std::vector<SequenceChannelRuntime> channels;
	std::vector<ScriptEntry> pending_additions;
	std::vector<SequenceId> pending_removals;

	ScriptsComponent() = default;
	ScriptsComponent(ScriptsComponent&&) noexcept = default;
	ScriptsComponent& operator=(ScriptsComponent&&) noexcept = default;

	ScriptsComponent(const ScriptsComponent& other) :
		scripts{ other.scripts } {}

	ScriptsComponent& operator=(const ScriptsComponent& other) {
		if (this != &other) {
			ScriptsComponent copy{ other };
			*this = std::move(copy);
		}
		return *this;
	}

	template <typename TScript>
	void AddDeferred(TScript value = {}) {
		ScriptEntry entry{ ScriptRegistry::Make<TScript>(std::move(value)) };
		if (!entry.type.empty()) {
			pending_additions.push_back(std::move(entry));
		}
	}

	void AddEntryDeferred(ScriptEntry entry) {
		if (!entry.type.empty()) {
			pending_additions.push_back(std::move(entry));
		}
	}

	void RemoveDeferred(SequenceId sequence_id) { pending_removals.push_back(sequence_id); }

	PTGN_REFLECT(ScriptsComponent, scripts)
};

struct SequenceHandle {
	ptgn::Entity owner;
	SequenceId binding_id{ 0 };

	[[nodiscard]] explicit operator bool() const {
		return owner && binding_id != 0;
	}

	bool Start(bool force = false) const;
	bool Stop() const;
	bool Pause() const;
	bool Resume() const;
	bool TogglePaused() const;
	bool Toggle() const;
	bool Reset() const;
	bool Clear() const;
	bool Skip() const;
	bool Seek(float progress) const;

	[[nodiscard]] bool IsRunning() const;
	[[nodiscard]] bool IsPaused() const;
	[[nodiscard]] bool IsCompleted() const;
	[[nodiscard]] float Progress() const;
};

static_assert(std::copy_constructible<Action>);
static_assert(std::is_copy_assignable_v<Action>);
static_assert(std::movable<Action>);
static_assert(std::copy_constructible<ScriptEntry>);
static_assert(std::is_copy_assignable_v<ScriptEntry>);
static_assert(std::copy_constructible<ScriptSequence>);
static_assert(std::is_copy_assignable_v<ScriptSequence>);
static_assert(std::copy_constructible<ScriptsComponent>);
static_assert(std::is_copy_assignable_v<ScriptsComponent>);

struct SharedScriptSequenceRegistry {
	std::vector<ScriptSequence> sequences;

	[[nodiscard]] ScriptSequence* Find(SequenceId id) {
		const auto it{ std::ranges::find_if(sequences, [id](const auto& sequence) {
			return sequence.id == id;
		}) };
		return it == sequences.end() ? nullptr : &*it;
	}

	[[nodiscard]] const ScriptSequence* Find(SequenceId id) const {
		return const_cast<SharedScriptSequenceRegistry*>(this)->Find(id);
	}
};

namespace script_runtime {

void AttachEntry(ptgn::Entity entity, ScriptEntry& entry);
void AttachAll(ptgn::Entity entity);
[[nodiscard]] bool IsScriptEnabled(ptgn::Entity entity, const ptgn::Script* script);
void Update(ptgn::Scene& scene, float delta_seconds);
[[nodiscard]] float DeltaSeconds();
void ApplyPending(ptgn::Scene& scene);

[[nodiscard]] ScriptSequence* Resolve(
	ptgn::Entity owner,
	ScriptSequence& binding
);
[[nodiscard]] const ScriptSequence* Resolve(
	ptgn::Entity owner,
	const ScriptSequence& binding
);
[[nodiscard]] SequenceHandle RunSequence(
	ptgn::Entity owner,
	ScriptSequence sequence
);
[[nodiscard]] SequenceHandle RunInChannel(
	ptgn::Entity owner,
	SequenceChannelKey channel,
	ScriptSequence sequence,
	ReentryMode reentry
);
[[nodiscard]] bool Start(ptgn::Entity owner, SequenceId id, bool force = false);
void StopChannel(
	ptgn::Entity owner,
	SequenceChannelKey channel,
	SequenceStopMode mode
);
[[nodiscard]] bool Stop(
	ptgn::Entity owner,
	SequenceId id,
	SequenceCancelReason reason = SequenceCancelReason::Stopped
);
[[nodiscard]] bool Reset(ptgn::Entity owner, SequenceId id);
[[nodiscard]] bool Clear(ptgn::Entity owner, SequenceId id);
[[nodiscard]] bool Skip(ptgn::Entity owner, SequenceId id);
[[nodiscard]] bool Seek(ptgn::Entity owner, SequenceId id, float progress);
[[nodiscard]] bool SetPaused(ptgn::Entity owner, SequenceId id, bool paused);
[[nodiscard]] float Progress(ptgn::Entity owner, SequenceId id);
[[nodiscard]] bool IsRunning(ptgn::Entity owner, SequenceId id);
[[nodiscard]] bool IsPaused(ptgn::Entity owner, SequenceId id);
[[nodiscard]] bool IsCompleted(ptgn::Entity owner, SequenceId id);

} // namespace script_runtime

struct SignalKey : ptgn::StrongString<SignalKey> {
	using StrongString::StrongString;
	SignalKey() = default;

	PTGN_REFLECT_VALUE(SignalKey, value)
};

struct Signal {
	SignalKey key;

	PTGN_REFLECT(Signal, key)
};

struct MouseMoveOver {
	ptgn::Entity pointer;

	PTGN_REFLECT(MouseMoveOver, pointer)
};

struct MouseMoveOut {
	ptgn::Entity pointer;

	PTGN_REFLECT(MouseMoveOut, pointer)
};

struct MousePressedOver {
	ptgn::Mouse button{ ptgn::Mouse::Left };

	PTGN_REFLECT(MousePressedOver, button)
};

struct MouseReleasedOver {
	ptgn::Mouse button{ ptgn::Mouse::Left };
	bool released_over{ false };

	PTGN_REFLECT(MouseReleasedOver, button, released_over)
};

struct ButtonPress {
	ptgn::Mouse button{ ptgn::Mouse::Left };

	PTGN_REFLECT(ButtonPress, button)
};

struct DragStart {
	ptgn::Entity pointer;

	PTGN_REFLECT(DragStart, pointer)
};

struct Drag {
	ptgn::Entity pointer;
	ptgn::V2_float position{};

	PTGN_REFLECT(Drag, pointer, position)
};

struct DragStop {
	ptgn::Entity pointer;

	PTGN_REFLECT(DragStop, pointer)
};

inline constexpr std::array kMouseButtons{
	ptgn::Mouse::Left,
	ptgn::Mouse::Right,
	ptgn::Mouse::Middle,
};

[[nodiscard]] inline const char* MouseButtonLabel(ptgn::Mouse button) {
	switch (button) {
		case ptgn::Mouse::Left: return "Left";
		case ptgn::Mouse::Right: return "Right";
		case ptgn::Mouse::Middle: return "Middle";
		default: return "Mouse";
	}
}

template <typename TEvent>
[[nodiscard]] ptgn::Mouse EventMouse(const TEvent& event) {
	if constexpr (requires { event.mouse; }) {
		return event.mouse;
	} else {
		return event.button;
	}
}

// Key parsing is driven directly by the reflected ptgn::Key enum.
inline constexpr auto kEventKeys{ magic_enum::enum_values<ptgn::Key>() };
static_assert(!kEventKeys.empty(), "ptgn::Key must define at least one reflected value");

enum class SpawnOrigin {
	OwnerEntity,
	Position
};
PTGN_REFLECT_ENUM(SpawnOrigin);

enum class SpawnArea {
	Point,
	Rectangle,
	Circle
};
PTGN_REFLECT_ENUM(SpawnArea);

// Built-in Actions. Each action directly owns the entity it affects.
struct WaitAction final : ScriptAction {
	PTGN_REFLECT_EMPTY(WaitAction)
};

struct MoveToAction final : ScriptAction {
	ptgn::V2_float destination{ 0.0f, 64.0f };
	bool relative{ true };

	MoveToAction() = default;
	MoveToAction(ptgn::V2_float destination, bool relative) :
		destination{ destination }, relative{ relative } {}

	void OnStart() override;
	ActionStatus OnUpdate() override;
	void OnRepeat() override;

	PTGN_REFLECT(MoveToAction, destination, relative)

private:
	ptgn::V2_float start_{};
	ptgn::V2_float end_{};
};

struct RotateToAction final : ScriptAction {
	float degrees{ 90.0f };
	bool shortest_path{ true };
	bool relative{ false };

	RotateToAction() = default;
	RotateToAction(float degrees, bool shortest_path = true, bool relative = false) :
		degrees{ degrees }, shortest_path{ shortest_path }, relative{ relative } {}

	void OnStart() override;
	ActionStatus OnUpdate() override;
	void OnRepeat() override;

	PTGN_REFLECT(RotateToAction, degrees, shortest_path, relative)

private:
	float start_degrees_{ 0.0f };
	float delta_degrees_{ 0.0f };
};

struct ScaleToAction final : ScriptAction {
	ptgn::V2_float scale{ 1.0f, 1.0f };
	bool relative{ false };

	ScaleToAction() = default;
	ScaleToAction(ptgn::V2_float scale, bool relative = false) :
		scale{ scale }, relative{ relative } {}

	void OnStart() override;
	ActionStatus OnUpdate() override;
	void OnRepeat() override;

	PTGN_REFLECT(ScaleToAction, scale, relative)

private:
	ptgn::V2_float start_{};
	ptgn::V2_float end_{};
};

struct FollowTargetAction final : ScriptAction {
	ptgn::Entity target;
	float speed{ 120.0f };
	float stopping_distance{ 2.0f };

	FollowTargetAction() = default;
	FollowTargetAction(ptgn::Entity target, float speed, float stopping_distance = 2.0f) :
		target{ target }, speed{ speed }, stopping_distance{ stopping_distance } {}

	[[nodiscard]] ActionStatus OnUpdate() override;

	// Entity serialization is intentionally omitted from the demo inspector.
	PTGN_REFLECT(FollowTargetAction, target, speed, stopping_distance)
};

struct NativeActionCallbacks {
	std::function<void(ScriptAction&)> on_start;
	std::function<ActionStatus(ScriptAction&)> on_update;
	std::function<void(ScriptAction&)> on_complete;
	std::function<void(ScriptAction&, SequenceCancelReason)> on_cancel;
};

struct NativeAction final : ScriptAction {
	std::shared_ptr<NativeActionCallbacks> callbacks;

	NativeAction() = default;
	explicit NativeAction(NativeActionCallbacks value) :
		callbacks{ std::make_shared<NativeActionCallbacks>(std::move(value)) } {}

	void OnStart() override {
		if (callbacks && callbacks->on_start) {
			callbacks->on_start(*this);
		}
	}

	[[nodiscard]] ActionStatus OnUpdate() override {
		if (callbacks && callbacks->on_update) {
			return callbacks->on_update(*this);
		}
		return ActionStatus::Complete;
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

	PTGN_REFLECT_EMPTY(NativeAction)
};

struct SetVisibleAction final : ScriptAction {
	bool visible{ true };
	void OnStart() override;

	PTGN_REFLECT(SetVisibleAction, visible)
};

struct PlayAudioAction final : ScriptAction {
	std::string asset{ "door_open" };
	float volume{ 1.0f };
	int loops{ 0 };
	void OnStart() override;

	PTGN_REFLECT(PlayAudioAction, asset, volume, loops)
};

struct EmitSignalAction final : ScriptAction {
	SignalKey signal{ "sequence.completed" };

	EmitSignalAction() = default;
	explicit EmitSignalAction(SignalKey signal) : signal{ std::move(signal) } {}

	void OnStart() override;

	PTGN_REFLECT(EmitSignalAction, signal)
};

struct AddComponentsAction final : ScriptAction {
	std::vector<ComponentDefinition> components;
	void OnStart() override;

	PTGN_REFLECT(AddComponentsAction, components)
};

struct RemoveComponentsAction final : ScriptAction {
	std::vector<std::string> components;
	void OnStart() override;

	PTGN_REFLECT(RemoveComponentsAction, components)
};

struct SpawnEntityAction final : ScriptAction {
	std::string prefab_key{ "prefabs/zombie" };
	int count{ 1 };
	SpawnOrigin origin{ SpawnOrigin::OwnerEntity };
	SpawnArea area{ SpawnArea::Point };
	ptgn::V2_float center{};
	ptgn::V2_float rectangle_size{ 128.0f, 128.0f };
	float radius{ 64.0f };
	bool parent_to_owner{ false };
	bool inherit_owner_rotation{ true };
	bool inherit_owner_scale{ true };
	bool random_rotation{ false };
	void OnStart() override;

	PTGN_REFLECT(
		SpawnEntityAction, prefab_key, count, origin, area, center, rectangle_size, radius,
		parent_to_owner, inherit_owner_rotation, inherit_owner_scale, random_rotation
	)
};

inline ScriptSequence& ScriptSequence::Reentry(ReentryMode value) {
	reentry = value;
	return *this;
}

inline ScriptSequence& ScriptSequence::Channel(SequenceChannelKey value) {
	channel = std::move(value);
	return *this;
}

inline ScriptSequence& ScriptSequence::Transient(bool remove_on_complete) {
	transient = true;
	remove_binding_on_complete = remove_on_complete;
	return *this;
}

inline ScriptSequence& ScriptSequence::DestroyOwnerOnComplete(bool value) {
	destroy_owner_on_complete = value;
	return *this;
}

inline ScriptSequence& ScriptSequence::StartOn(
	std::string_view key,
	ptgn::json value
) {
	start_events.push_back(SequenceEventRegistry::MakeCondition(key, std::move(value)));
	return *this;
}

inline ScriptSequence& ScriptSequence::StopOn(
	std::string_view key,
	ptgn::json value
) {
	stop_events.push_back(SequenceEventRegistry::MakeCondition(key, std::move(value)));
	return *this;
}

template <typename TAction>
ScriptSequence& ScriptSequence::Then(TAction action) {
	actions.push_back(ActionRegistry::Make<TAction>(std::move(action)));
	return *this;
}

template <typename TAction>
ScriptSequence& ScriptSequence::UntilComplete(TAction action) {
	Action definition{ ActionRegistry::Make<TAction>(std::move(action)) };
	definition.completion = ActionCompletion::ActionControlled;
	actions.push_back(std::move(definition));
	return *this;
}

template <typename TAction>
ScriptSequence& ScriptSequence::Forever(TAction action) {
	Action definition{ ActionRegistry::Make<TAction>(std::move(action)) };
	definition.completion = ActionCompletion::Infinite;
	actions.push_back(std::move(definition));
	return *this;
}

template <typename TAction>
ScriptSequence& ScriptSequence::During(float duration_ms, TAction action) {
	Action definition{ ActionRegistry::Make<TAction>(std::move(action)) };
	definition.completion = ActionCompletion::Duration;
	definition.timing = definition.timing.value_or(ActionTiming{});
	definition.timing->duration_ms = std::max(0.0f, duration_ms);
	actions.push_back(std::move(definition));
	return *this;
}

inline ActionTiming& ScriptSequence::LatestDuringTiming() {
	auto it{ std::ranges::find_if(
		actions.rbegin(),
		actions.rend(),
		[](const Action& action) {
			return action.timing.has_value() &&
				action.completion == ActionCompletion::Duration &&
				action.type_hash != ptgn::Hash<WaitAction>();
		}
	) };
	PTGN_ASSERT(
		it != actions.rend(),
		"Ease, Repeat, Infinite, Reversed, and Yoyo require a preceding During call"
	);
	return *it->timing;
}

inline ScriptSequence& ScriptSequence::Ease(ptgn::Ease value) {
	LatestDuringTiming().ease = value;
	return *this;
}

inline ScriptSequence& ScriptSequence::Repeat(int additional_repeats) {
	auto& timing{ LatestDuringTiming() };
	timing.additional_repeats = std::max(0, additional_repeats);
	timing.infinite_repeats = false;
	return *this;
}

inline ScriptSequence& ScriptSequence::Infinite() {
	LatestDuringTiming().infinite_repeats = true;
	return *this;
}

inline ScriptSequence& ScriptSequence::Reversed(bool value) {
	LatestDuringTiming().reversed = value;
	return *this;
}

inline ScriptSequence& ScriptSequence::Yoyo(bool value) {
	LatestDuringTiming().yoyo = value;
	return *this;
}

inline ScriptSequence& ScriptSequence::Wait(float duration_ms) {
	Action action{ ActionRegistry::Make<WaitAction>() };
	action.completion = ActionCompletion::Duration;
	action.timing = action.timing.value_or(ActionTiming{});
	action.timing->duration_ms = std::max(0.0f, duration_ms);
	actions.push_back(std::move(action));
	return *this;
}

inline ScriptSequence& ScriptSequence::EmitSignal(SignalKey signal) {
	return Then(EmitSignalAction{ std::move(signal) });
}

void MoveToAction::OnStart() {
	start_ = Owner().Get<ptgn::Transform>().position;
	end_ = relative ? start_ + destination : destination;
}

ActionStatus MoveToAction::OnUpdate() {
	Owner().Get<ptgn::Transform>().position =
		start_ + (end_ - start_) * Progress();
	return ActionStatus::Running;
}

void MoveToAction::OnRepeat() {
	if (!IsReversed()) {
		OnStart();
	}
}

void RotateToAction::OnStart() {
	start_degrees_ = Owner().Get<ptgn::Transform>().rotation.ToDeg().value;
	const float end_degrees{ relative ? start_degrees_ + degrees : degrees };
	delta_degrees_ = end_degrees - start_degrees_;
	if (shortest_path) {
		delta_degrees_ = std::remainder(delta_degrees_, 360.0f);
	}
}

ActionStatus RotateToAction::OnUpdate() {
	const float value{ start_degrees_ + delta_degrees_ * Progress() };
	Owner().Get<ptgn::Transform>().rotation = ptgn::Degrees{ value }.ToRad();
	return ActionStatus::Running;
}

void RotateToAction::OnRepeat() {
	if (!IsReversed()) {
		OnStart();
	}
}

void ScaleToAction::OnStart() {
	start_ = Owner().Get<ptgn::Transform>().scale;
	end_ = relative ? start_ * scale : scale;
}

ActionStatus ScaleToAction::OnUpdate() {
	Owner().Get<ptgn::Transform>().scale =
		start_ + (end_ - start_) * Progress();
	return ActionStatus::Running;
}

void ScaleToAction::OnRepeat() {
	if (!IsReversed()) {
		OnStart();
	}
}

ActionStatus FollowTargetAction::OnUpdate() {
	if (!target || !target.Has<ptgn::Transform>() || !Owner().Has<ptgn::Transform>()) {
		return ActionStatus::Complete;
	}

	auto& position{ Owner().Get<ptgn::Transform>().position };
	const ptgn::V2_float offset{ target.Get<ptgn::Transform>().position - position };
	const float distance{ std::sqrt(offset.x * offset.x + offset.y * offset.y) };
	if (distance <= std::max(0.0f, stopping_distance)) {
		return ActionStatus::Complete;
	}

	const float step{ std::max(0.0f, speed) * std::max(0.0f, DeltaSeconds()) };
	if (step >= distance) {
		position += offset;
		return ActionStatus::Complete;
	}

	position += offset * (step / distance);
	return ActionStatus::Running;
}

void SetVisibleAction::OnStart() {
	ptgn::SetVisible(Owner(), visible);
}

void PlayAudioAction::OnStart() {
	LogScriptActivity(
		GetScene(),
		std::string{ GetEntityName(Owner()) } + " played " + asset +
			" (volume " + std::to_string(volume) + ", loops " + std::to_string(loops) + ")"
	);
}

void EmitSignalAction::OnStart() {
	GetScene().ctx().event.PushGlobal<Signal>(Signal{ signal });
}

void AddComponentsAction::OnStart() {
	for (const auto& component : components) {
		if (component.apply_live) {
			component.apply_live(Owner());
			continue;
		}
		const auto* registration{ ptgn::ComponentRegistry::Find(component.type) };
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

void RemoveComponentsAction::OnStart() {
	for (const auto& name : components) {
		if (const auto* registration{ ptgn::ComponentRegistry::Find(name) }) {
			registration->remove(Owner());
		}
	}
}

void SpawnEntityAction::OnStart() {
	static std::mt19937 generator{ std::random_device{}() };
	std::uniform_real_distribution<float> unit{ 0.0f, 1.0f };
	const ptgn::V2_float base{
		origin == SpawnOrigin::OwnerEntity
			? Owner().Get<ptgn::Transform>().position + center
			: center
	};

	for (int i{ 0 }; i < std::clamp(count, 1, 100); ++i) {
		ptgn::V2_float position{ base };
		if (area == SpawnArea::Rectangle) {
			position.x += (unit(generator) - 0.5f) * std::max(0.0f, rectangle_size.x);
			position.y += (unit(generator) - 0.5f) * std::max(0.0f, rectangle_size.y);
		} else if (area == SpawnArea::Circle) {
			const float distance{ std::sqrt(unit(generator)) * std::max(0.0f, radius) };
			const float angle{ unit(generator) * 2.0f * std::numbers::pi_v<float> };
			position += ptgn::V2_float{ std::cos(angle), std::sin(angle) } * distance;
		}

		(void)SpawnScriptPrefab(
			GetScene(),
			PrefabSpawnRequest{
				.prefab_key = prefab_key,
				.owner = Owner(),
				.position = position,
				.parent_to_owner = parent_to_owner,
				.inherit_owner_rotation = inherit_owner_rotation,
				.inherit_owner_scale = inherit_owner_scale,
				.random_rotation = random_rotation,
			}
		);
	}
}

/// @brief Runtime-only property tween helper used from custom C++ scripts.
template <typename T, typename TGetter, typename TSetter>
SequenceHandle PropertyTo(
	ptgn::Entity entity,
	SequenceChannelKey channel,
	T target,
	float duration_ms,
	TGetter getter,
	TSetter setter,
	ptgn::Ease ease = ptgn::Ease::Linear,
	bool force = true
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

	NativeAction action{ NativeActionCallbacks{
		.on_start = [state, getter = std::move(getter)](ScriptAction& action) mutable {
			state->start = std::invoke(getter, action.Owner());
		},
		.on_update = [state, setter = std::move(setter)](ScriptAction& action) mutable {
			std::invoke(
				setter,
				action.Owner(),
				state->start + (state->target - state->start) * action.Progress()
			);
			return action.LinearProgress() >= 1.0f
				? ActionStatus::Complete
				: ActionStatus::Running;
		},
	} };

	ScriptSequence sequence{ "Property To" };
	sequence
		.Channel(channel)
		.Transient()
		.During(duration_ms, std::move(action))
		.Ease(ease);
	return script_runtime::RunInChannel(
		entity,
		std::move(channel),
		std::move(sequence),
		force ? ReentryMode::Restart : ReentryMode::Queue
	);
}

inline SequenceHandle TranslateTo(
	ptgn::Entity entity,
	ptgn::V2_float destination,
	float duration_ms,
	ptgn::Ease ease = ptgn::Ease::Linear,
	bool force = true,
	bool relative = false
) {
	if (!entity) {
		return {};
	}
	ScriptSequence sequence{ "Translate To" };
	sequence
		.Channel(SequenceChannelKey{ "transform.position" })
		.Transient()
		.During(duration_ms, MoveToAction{ destination, relative })
		.Ease(ease);
	return script_runtime::RunInChannel(
		entity,
		*sequence.channel,
		std::move(sequence),
		force ? ReentryMode::Restart : ReentryMode::Queue
	);
}

inline SequenceHandle RotateTo(
	ptgn::Entity entity,
	float degrees,
	float duration_ms,
	ptgn::Ease ease = ptgn::Ease::Linear,
	bool force = true,
	bool shortest_path = true,
	bool relative = false
) {
	if (!entity) {
		return {};
	}
	ScriptSequence sequence{ "Rotate To" };
	sequence
		.Channel(SequenceChannelKey{ "transform.rotation" })
		.Transient()
		.During(duration_ms, RotateToAction{ degrees, shortest_path, relative })
		.Ease(ease);
	return script_runtime::RunInChannel(
		entity,
		*sequence.channel,
		std::move(sequence),
		force ? ReentryMode::Restart : ReentryMode::Queue
	);
}

inline SequenceHandle ScaleTo(
	ptgn::Entity entity,
	ptgn::V2_float scale,
	float duration_ms,
	ptgn::Ease ease = ptgn::Ease::Linear,
	bool force = true,
	bool relative = false
) {
	if (!entity) {
		return {};
	}
	ScriptSequence sequence{ "Scale To" };
	sequence
		.Channel(SequenceChannelKey{ "transform.scale" })
		.Transient()
		.During(duration_ms, ScaleToAction{ scale, relative })
		.Ease(ease);
	return script_runtime::RunInChannel(
		entity,
		*sequence.channel,
		std::move(sequence),
		force ? ReentryMode::Restart : ReentryMode::Queue
	);
}

inline SequenceHandle Follow(
	ptgn::Entity entity,
	ptgn::Entity target,
	float speed,
	float stopping_distance = 2.0f,
	bool force = true
) {
	if (!entity) {
		return {};
	}
	ScriptSequence sequence{ "Follow Target" };
	sequence
		.Channel(SequenceChannelKey{ "movement.follow" })
		.Transient()
		.UntilComplete(FollowTargetAction{ target, speed, stopping_distance });
	return script_runtime::RunInChannel(
		entity,
		*sequence.channel,
		std::move(sequence),
		force ? ReentryMode::Restart : ReentryMode::Queue
	);
}

template <std::ranges::input_range TRange>
std::vector<SequenceHandle> TranslateTo(
	TRange&& entities,
	ptgn::V2_float destination,
	float duration_ms,
	ptgn::Ease ease = ptgn::Ease::Linear,
	bool force = true,
	bool relative = false
) {
	std::vector<SequenceHandle> handles;
	for (ptgn::Entity entity : entities) {
		handles.push_back(TranslateTo(
			entity, destination, duration_ms, ease, force, relative
		));
	}
	return handles;
}

template <std::ranges::input_range TRange>
std::vector<SequenceHandle> RotateTo(
	TRange&& entities,
	float degrees,
	float duration_ms,
	ptgn::Ease ease = ptgn::Ease::Linear,
	bool force = true,
	bool shortest_path = true,
	bool relative = false
) {
	std::vector<SequenceHandle> handles;
	for (ptgn::Entity entity : entities) {
		handles.push_back(RotateTo(
			entity, degrees, duration_ms, ease, force, shortest_path, relative
		));
	}
	return handles;
}

template <std::ranges::input_range TRange>
std::vector<SequenceHandle> ScaleTo(
	TRange&& entities,
	ptgn::V2_float scale,
	float duration_ms,
	ptgn::Ease ease = ptgn::Ease::Linear,
	bool force = true,
	bool relative = false
) {
	std::vector<SequenceHandle> handles;
	for (ptgn::Entity entity : entities) {
		handles.push_back(ScaleTo(
			entity, scale, duration_ms, ease, force, relative
		));
	}
	return handles;
}

inline SequenceHandle After(
	ptgn::Entity owner,
	float delay_ms,
	std::function<void(ScriptAction&)> callback
) {
	if (!owner) {
		return {};
	}

	ScriptSequence sequence{ "After" };
	sequence
		.Transient()
		.Wait(delay_ms)
		.Then(NativeAction{ NativeActionCallbacks{ .on_start = std::move(callback) } });
	return script_runtime::RunSequence(owner, std::move(sequence));
}

inline void StopChannel(
	ptgn::Entity entity,
	SequenceChannelKey channel,
	SequenceStopMode mode = SequenceStopMode::All
) {
	script_runtime::StopChannel(entity, std::move(channel), mode);
}

struct PrefabDefinition {
	std::string key{ "prefabs/new_entity" };
	std::string name{ "New Prefab" };
	std::string tag;
	std::vector<ComponentDefinition> components;
	std::optional<ScriptsComponent> scripts;

	PTGN_REFLECT(PrefabDefinition, key, name, tag, components, scripts)
};

struct PrefabRegistry {
	std::vector<PrefabDefinition> definitions;

	[[nodiscard]] PrefabDefinition* Find(std::string_view key) {
		const auto it{ std::ranges::find_if(definitions, [key](const auto& prefab) {
			return prefab.key == key;
		}) };
		return it == definitions.end() ? nullptr : &*it;
	}

	[[nodiscard]] const PrefabDefinition* Find(std::string_view key) const {
		return const_cast<PrefabRegistry*>(this)->Find(key);
	}
};

namespace editor {

struct EditorContextTemp;

struct EditorVisual {
	ImVec4 color{ 0.35f, 0.43f, 0.57f, 1.0f };
	bool sensor{ false };
	float health_fraction{ -1.0f };
};



[[nodiscard]] const ptgn::editor::RegisteredComponentEditor* FindComponentEditor(
	const ptgn::RegisteredComponent& component
) {
	return ptgn::editor::ComponentEditorRegistry::Find(component.type_id);
}

[[nodiscard]] ptgn::editor::ResolvedComponentEditorOptions ResolveComponentEditor(
	const ptgn::RegisteredComponent& component
) {
	const auto* editor{ FindComponentEditor(component) };
	if (!editor) {
		return ptgn::editor::ResolvedComponentEditorOptions{
			.label = std::string{ component.name },
			.group = component.is_empty
				? std::string{ ptgn::editor::kTagComponentGroup }
				: std::string{},
		};
	}

	return ptgn::editor::ComponentEditorRegistry::Resolve(component, *editor);
}

[[nodiscard]] bool HasComponentJsonEditor(const ptgn::RegisteredComponent& component) {
	const auto* editor{ FindComponentEditor(component) };
	return editor && editor->draw_json;
}

struct EventEditorOptions {
	std::string label;
	std::string group;
	std::string description;
};

struct EventEditorRegistration {
	TypeHashValue type_hash{ 0 };
	std::string key;
	EventEditorOptions options;
	int inline_fields{ 0 };
	std::function<bool(ptgn::json&)> draw;
};

class EventEditorRegistry {
public:
	template <typename F>
	static bool Register(
		std::string key,
		EventEditorOptions options,
		int inline_fields,
		F&& draw
	) {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ ptgn::Hash(std::string_view{ key }) };
		const bool inserted{ !Find(key) };

		std::erase_if(entries, [&](const EventEditorRegistration& entry) {
			return entry.key == key || entry.type_hash == type_hash;
		});

		entries.push_back(
			EventEditorRegistration{
				.type_hash = type_hash,
				.key = std::move(key),
				.options = std::move(options),
				.inline_fields = std::max(0, inline_fields),
				.draw = [fn = std::forward<F>(draw)](ptgn::json& value) mutable {
					if (value.is_null()) {
						value = ptgn::json::object();
					}
					const ptgn::json previous{ value };
					const bool changed{ std::invoke(fn, value) };
					return changed || value != previous;
				},
			}
		);
		return inserted;
	}

	[[nodiscard]] static const EventEditorRegistration* Find(std::string_view key) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [key](const auto& entry) {
			return entry.key == key;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const EventEditorRegistration* Find(TypeHashValue type_hash) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const std::vector<EventEditorRegistration>& Entries() {
		return MutableEntries();
	}

private:
	[[nodiscard]] static std::vector<EventEditorRegistration>& MutableEntries() {
		static std::vector<EventEditorRegistration> entries;
		return entries;
	}
};

struct ActionEditorOptions {
	std::string label;
	std::string group;
	std::string description;
	int menu_order{ 100 };
	bool separator_after{ false };
};

struct ActionEditorRegistration {
	TypeHashValue type_hash{ 0 };
	std::string key;
	ActionEditorOptions options;
	std::function<bool(ptgn::json&, EditorContextTemp&)> draw_inline;
	std::function<bool(ptgn::json&, EditorContextTemp&)> draw;
};

template <typename T>
struct TypedJsonEditorState {
	T value{};
	ptgn::json synchronized_value;
	bool initialized{ false };
};

template <typename T, typename F>
bool DrawTypedJsonEditor(
	ptgn::json& input,
	EditorContextTemp& context,
	F& fn
) {
	static std::unordered_map<const ptgn::json*, TypedJsonEditorState<T>> states;
	auto& state{ states[&input] };
	if (!state.initialized || state.synchronized_value != input) {
		state.value = T{};
		(void)TryReadJson(input, state.value);
		state.synchronized_value = input;
		state.initialized = true;
	}

	const bool changed{ std::invoke(fn, state.value, context) };
	ptgn::json updated;
	try {
		updated = state.value;
	} catch (...) {
		updated = input;
	}
	const bool serialized_changed{ updated != input };
	input = std::move(updated);
	state.synchronized_value = input;
	return changed || serialized_changed;
}

class ActionEditorRegistry {
public:
	template <typename T, typename F>
	static bool Register(std::string key, ActionEditorOptions options, F&& draw) {
		auto& entries{ MutableEntries() };
		const bool inserted{ !Find(key) };
		std::erase_if(entries, [&](const ActionEditorRegistration& entry) {
			return entry.key == key || entry.type_hash == ptgn::Hash<T>();
		});
		entries.push_back(ActionEditorRegistration{
			.type_hash = ptgn::Hash<T>(),
			.key = std::move(key),
			.options = std::move(options),
			.draw_inline = {},
			.draw = [fn = std::forward<F>(draw)](
				ptgn::json& input,
				EditorContextTemp& context
			) mutable {
				return DrawTypedJsonEditor<T>(input, context, fn);
			},
		});
		return inserted;
	}

	template <typename T, typename FInline, typename FDetails>
	static bool RegisterInline(
		std::string key,
		ActionEditorOptions options,
		FInline&& draw_inline,
		FDetails&& draw_details
	) {
		auto& entries{ MutableEntries() };
		const bool inserted{ !Find(key) };
		std::erase_if(entries, [&](const ActionEditorRegistration& entry) {
			return entry.key == key || entry.type_hash == ptgn::Hash<T>();
		});
		entries.push_back(ActionEditorRegistration{
			.type_hash = ptgn::Hash<T>(),
			.key = std::move(key),
			.options = std::move(options),
			.draw_inline = [fn = std::forward<FInline>(draw_inline)](
				ptgn::json& input,
				EditorContextTemp& context
			) mutable {
				return DrawTypedJsonEditor<T>(input, context, fn);
			},
			.draw = [fn = std::forward<FDetails>(draw_details)](
				ptgn::json& input,
				EditorContextTemp& context
			) mutable {
				return DrawTypedJsonEditor<T>(input, context, fn);
			},
		});
		return inserted;
	}

	[[nodiscard]] static const ActionEditorRegistration* Find(std::string_view key) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [key](const auto& entry) {
			return entry.key == key;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const std::vector<ActionEditorRegistration>& Entries() {
		return MutableEntries();
	}

private:
	[[nodiscard]] static std::vector<ActionEditorRegistration>& MutableEntries() {
		static std::vector<ActionEditorRegistration> entries;
		return entries;
	}
};

struct ScriptEditorOptions {
	std::string label;
	std::string group;
	std::string description;
};

struct ScriptEditorRegistration {
	TypeHashValue type_hash{ 0 };
	std::string key;
	ScriptEditorOptions options;
	bool has_contents{ false };
	std::function<bool(ptgn::json&)> draw;
};

class ScriptEditorRegistry {
public:
	template <typename T, typename F>
	static bool Register(std::string key, ScriptEditorOptions options, F&& draw) {
		auto& entries{ MutableEntries() };
		const bool inserted{ !Find(key) };
		std::erase_if(entries, [&](const ScriptEditorRegistration& entry) {
			return entry.key == key || entry.type_hash == ptgn::Hash<T>();
		});
		entries.push_back(ScriptEditorRegistration{
			.type_hash = ptgn::Hash<T>(),
			.key = std::move(key),
			.options = std::move(options),
			.has_contents = !std::is_empty_v<T>,
			.draw = [fn = std::forward<F>(draw)](ptgn::json& input) mutable {
				T value{};
				if (!TryReadJson(input, value)) {
					input = value;
				}
				if (!std::invoke(fn, value)) {
					return false;
				}
				input = std::move(value);
				return true;
			},
		});
		return inserted;
	}

	[[nodiscard]] static const ScriptEditorRegistration* Find(std::string_view key) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [key](const auto& entry) {
			return entry.key == key;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const std::vector<ScriptEditorRegistration>& Entries() {
		return MutableEntries();
	}

private:
	[[nodiscard]] static std::vector<ScriptEditorRegistration>& MutableEntries() {
		static std::vector<ScriptEditorRegistration> entries;
		return entries;
	}
};

struct PointerFrame {
	ptgn::V2_float world_position{};
	bool inside_scene{ false };
	std::array<bool, 3> pressed{};
	std::array<bool, 3> released{};
};

class EditorHost {
public:
	virtual ~EditorHost() = default;

	[[nodiscard]] virtual PrefabRegistry& GetPrefabs() = 0;
	[[nodiscard]] virtual SharedScriptSequenceRegistry& GetSharedSequences() = 0;
	[[nodiscard]] virtual std::vector<ptgn::Entity> Entities() const = 0;
	[[nodiscard]] virtual std::vector<std::string> ActivityText() const = 0;
	[[nodiscard]] virtual std::string_view Name(ptgn::Entity entity) const = 0;
	[[nodiscard]] virtual EditorVisual Visual(ptgn::Entity entity) const = 0;
	[[nodiscard]] virtual std::string& EditableName(ptgn::Entity entity) = 0;
	[[nodiscard]] virtual std::string& EditableTag(ptgn::Entity entity) = 0;
	virtual ptgn::Entity CreateEntity(std::string name, std::string tag = {}) = 0;
	[[nodiscard]] virtual ScriptSequence* Resolve(
		ptgn::Entity owner, ScriptSequence& binding
	) = 0;
	[[nodiscard]] virtual const ScriptSequence* Resolve(
		ptgn::Entity owner, const ScriptSequence& binding
	) const = 0;
	virtual void Start(ptgn::Entity owner, ScriptSequence& binding, bool force = false) = 0;
	virtual void Stop(ptgn::Entity owner, ScriptSequence& binding, bool log = true) = 0;
	virtual void SetPaused(ptgn::Entity owner, ScriptSequence& binding, bool paused) = 0;
	[[nodiscard]] virtual float Progress(
		ptgn::Entity owner, const ScriptSequence& binding
	) const = 0;
	virtual void SubmitPointerFrame(PointerFrame frame) = 0;
};

struct EditorContextTemp {
	EditorHost& host;
	PrefabRegistry& prefabs;
};

// Wraps any registry call while preserving its return value. This keeps all
// registration sites visually consistent, including direct editor-only registrations.
#define PTGN_REGISTER(...) (__VA_ARGS__)

template <typename T>
struct ActionRegistrationOptions {
	std::string key;
	bool supports_timing{ false };
	bool requires_timing{ false };
	std::optional<ActionTiming> default_timing;
	ActionCompletion completion{ ActionCompletion::Instant };
	bool serializable{ true };
	ActionEditorOptions editor;
	std::function<bool(T&, EditorContextTemp&)> draw_inline;
	std::function<bool(T&, EditorContextTemp&)> draw;
};

template <typename T>
bool RegisterAction(ActionRegistrationOptions<T> options) {
	const std::string key{ options.key };
	const bool runtime_registered{ PTGN_REGISTER(
		ActionRegistry::Register<T>(
			key, options.supports_timing, options.requires_timing, options.default_timing,
			options.completion, options.serializable
		)
	) };

	std::function<bool(T&, EditorContextTemp&)> draw{ std::move(options.draw) };

	if (!draw) {
		draw = [](T&, EditorContextTemp&) {
			return false;
		};
	}

	if (options.draw_inline) {
		PTGN_REGISTER(
			ActionEditorRegistry::RegisterInline<T>(
				key, std::move(options.editor), std::move(options.draw_inline), std::move(draw)
			)
		);
	} else {
		PTGN_REGISTER(
			ActionEditorRegistry::Register<T>(
				key, std::move(options.editor), std::move(draw)
			)
		);
	}
	return runtime_registered;
}

template <typename... TComponent>
[[nodiscard]] std::function<bool(ptgn::Entity)> RequireComponents() {
	return [](ptgn::Entity entity) {
		return (entity.Has<TComponent>() && ...);
	};
}

template <typename TEvent>
struct EventRegistrationOptions {
	std::string key;
	EventEditorOptions editor;
	ptgn::json default_value{ ptgn::json::object() };
	int inline_fields{ 0 };
	std::function<bool(ptgn::Entity, const ptgn::json&, const TEvent&)> matches;
	std::function<bool(ptgn::json&)> draw{
		[](ptgn::json&) {
			return false;
		}
	};
	std::function<bool(ptgn::Entity)> available{
		[](ptgn::Entity) {
			return true;
		}
	};
};

template <typename TEvent>
bool RegisterEvent(EventRegistrationOptions<TEvent> options) {
	const std::string key{ options.key };
	const bool runtime_registered{ PTGN_REGISTER(
		SequenceEventRegistry::Register<TEvent>(
			key,
			std::move(options.default_value),
			std::move(options.matches),
			std::move(options.available)
		)
	) };

	PTGN_REGISTER(
		EventEditorRegistry::Register(
			key,
			std::move(options.editor),
			options.inline_fields,
			std::move(options.draw)
		)
	);
	return runtime_registered;
}

template <typename T>
struct ScriptRegistrationOptions {
	std::string key;
	ScriptEditorOptions editor;
	// Custom Script drawers are opt-in. This avoids instantiating the generic
	// reflected drawer for Script types that contain registry-backed JSON values.
	std::function<bool(T&)> draw{ [](T&) { return false; } };
};

template <typename T>
bool RegisterScript(ScriptRegistrationOptions<T> options) {
	const std::string key{ options.key };
	const bool runtime_registered{
		PTGN_REGISTER(ScriptRegistry::Register<T>(key))
	};
	PTGN_REGISTER(
		ScriptEditorRegistry::Register<T>(
			key, std::move(options.editor), std::move(options.draw)
		)
	);
	return runtime_registered;
}

#define PTGN_REGISTER_ACTION(Type, ...)                                      \
	(void)PTGN_REGISTER(::ptgn::editor::RegisterAction<Type>(                  \
		::ptgn::editor::ActionRegistrationOptions<Type> __VA_ARGS__               \
	))

#define PTGN_REGISTER_EVENT(EventType, ...)                                  \
	(void)PTGN_REGISTER(::ptgn::editor::RegisterEvent<EventType>(                \
		::ptgn::editor::EventRegistrationOptions<EventType> __VA_ARGS__              \
	))

#define PTGN_REGISTER_SCRIPT(Type, ...)                                      \
	(void)PTGN_REGISTER(::ptgn::editor::RegisterScript<Type>(                  \
		::ptgn::editor::ScriptRegistrationOptions<Type> __VA_ARGS__               \
	))

enum class ActionForm {
	Action,
	Tween,
	Delay
};

struct ActionDragPayload {
	int index;
};

struct DurationEditState {
	std::array<char, 32> buffer{};
	bool initialized{ false };
	bool was_active{ false };
};

inline constexpr std::array kReentryLabels{ "Ignore", "Restart", "Queue" };
inline constexpr std::array kLifecycleLabels{
	"On Start", "On Complete", "On Reset", "On Stop", "On Pause",
	"On Resume", "On Action Start", "On Action Complete", "On Action Cancel",
	"On Repeat", "On Yoyo"
};
inline constexpr std::array kActionFormLabels{ "Action", "Tween", "Delay" };
inline constexpr std::array kEaseEntries{
	std::pair{ ptgn::Ease::Linear, "Linear" },
	std::pair{ ptgn::Ease::InQuad, "In Quad" },
	std::pair{ ptgn::Ease::OutQuad, "Out Quad" },
	std::pair{ ptgn::Ease::InOutQuad, "In-Out Quad" },
	std::pair{ ptgn::Ease::OutCubic, "Out Cubic" },
	std::pair{ ptgn::Ease::OutBack, "Out Back" },
};

struct DemoEditorState {
	int selected_entity{ 0 };
	int selected_prefab{ -1 };
	bool inspect_prefab{ false };
	bool show_runtime_controls{ false };
	std::optional<SequenceId> editing_sequence_name;
	std::string editing_sequence_original_name;
	std::unordered_map<SequenceId, bool> sequence_open_states;
};

struct DemoEditorDrawContext {
	EditorContextTemp& context;
	DemoEditorState& state;
};

void RegisterEditorTypes();
void DrawItemTooltip(const char* text);
bool DrawDurationInput(
	const char* label, float& milliseconds, float width, const char* tooltip
);
float CompactControlSpacing();
void SameLineControl();
float EnabledDeleteControlsWidth();
bool DrawEnabledDeleteControls(
	bool& enabled, const char* enabled_tooltip, const char* delete_tooltip
);
bool DrawCenteredTextButton(const char* id, const char* text, ImVec2 size);
bool DrawToggleButton(
	const char* label, bool& value, ImVec2 size, const char* tooltip
);
float GetHalfRowWidth();
float GetCountControlWidth(const char* label);
void DrawCountControl(
	const char* label, int& value, int minimum, int maximum = 100,
	bool disabled = false, const char* tooltip = nullptr
);
bool DrawAddableSectionHeader(
	const char* id, const char* label, bool default_open, bool empty,
	const char* section_tooltip, const char* empty_tooltip,
	const char* add_tooltip, bool& add_requested
);
bool DrawUnframedSectionHeader(
	const char* id, const char* label, bool default_open, bool empty,
	const char* tooltip, bool show_add_button,
	const char* add_tooltip, bool& add_requested
);
void DrawSelectedItemsTooltip(std::span<const std::string> items);
ActionForm GetActionForm(const Action& action);
void SetActionForm(Action& action, ActionForm form);
std::string ActionSummary(const Action& action);
void MoveAction(std::vector<Action>& actions, int from, int to);
bool DrawAddComponentButton(AddComponentsAction& action, const char* popup_id);

[[nodiscard]] ptgn::Entity SelectedEntity(const DemoEditorDrawContext& ui);
void DrawSidebar(DemoEditorDrawContext& ui);
void DrawScene(DemoEditorDrawContext& ui);
void DrawInspector(DemoEditorDrawContext& ui);
void DrawEntityComponents(DemoEditorDrawContext& ui, ptgn::Entity entity);
void DrawScripts(
	DemoEditorDrawContext& ui, ptgn::Entity entity, ScriptsComponent& scripts
);
void DrawResidentScripts(
	DemoEditorDrawContext& ui, ptgn::Entity entity, ScriptsComponent& scripts
);
bool DrawSequence(
	DemoEditorDrawContext& ui, ptgn::Entity owner, ScriptSequence& binding
);
void DrawRuntimeButtons(
	DemoEditorDrawContext& ui, ptgn::Entity owner, ScriptSequence& binding
);
void DrawEvents(
	DemoEditorDrawContext& ui, ptgn::Entity owner, ScriptSequence& sequence
);
bool DrawEvent(
	DemoEditorDrawContext& ui, ptgn::Entity owner, EventCondition& event,
	bool stop_event, bool& switch_kind
);
void DrawActionPicker(
	DemoEditorDrawContext& ui, Action& action, bool timed_only,
	float width = -FLT_MIN
);
void DrawActionPickerWithInline(
	DemoEditorDrawContext& ui, Action& action, bool timed_only
);
void DrawActionParameters(
	DemoEditorDrawContext& ui, Action& action, float left_screen_x
);
void DrawActions(
	DemoEditorDrawContext& ui, ScriptSequence& sequence, ScriptSequence& binding
);
void DrawLifecycleRows(DemoEditorDrawContext& ui, ScriptSequence& sequence);
void DrawTimingOptions(
	DemoEditorDrawContext& ui, Action& action, ActionTiming& timing,
	float left_screen_x
);
void DrawEmitSignalCompact(DemoEditorDrawContext& ui, EmitSignalAction& emit);
void DrawComponentDefinition(
	DemoEditorDrawContext& ui, ComponentDefinition& component, bool removable,
	int* remove_index = nullptr, int index = -1
);
void DrawPrefabs(DemoEditorDrawContext& ui);
void DrawPrefabInspector(DemoEditorDrawContext& ui, PrefabDefinition& prefab);
void DrawActivity(DemoEditorDrawContext& ui);
void PromoteToShared(DemoEditorDrawContext& ui, ScriptSequence& binding);
void DetachToLocal(DemoEditorDrawContext& ui, ScriptSequence& binding);
void DrawAddResidentScriptPopup(
	DemoEditorDrawContext& ui, ScriptsComponent& scripts
);

class DemoEditor {
public:
	explicit DemoEditor(EditorHost& world) :
		context_{ .host = world, .prefabs = world.GetPrefabs() } {}

	void Draw();

private:
	EditorContextTemp context_;
	DemoEditorState state_;
};

void DemoEditor::Draw() {
	DemoEditorDrawContext ui{ .context = context_, .state = state_ };

	ImGuiViewport* viewport{ ImGui::GetMainViewport() };
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	constexpr ImGuiWindowFlags flags{
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus
	};
	ImGui::Begin("Old UI + Static Registry Script Sequence Demo", nullptr, flags);
	ImGui::Checkbox("Runtime", &state_.show_runtime_controls);
	DrawItemTooltip("Show sequence playback controls in the inspector.");
	ImGui::Separator();
	if (ImGui::BeginTable(
			"ApplicationLayout", 3,
			ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
				ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 235.0f);
		ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 680.0f);

		ImGui::TableNextColumn();
		ImGui::BeginChild("Sidebar");
		DrawSidebar(ui);
		ImGui::EndChild();

		ImGui::TableNextColumn();
		ImGui::BeginChild("Scene");
		DrawScene(ui);
		ImGui::EndChild();

		ImGui::TableNextColumn();
		ImGui::BeginChild("Inspector");
		DrawInspector(ui);
		ImGui::EndChild();
		ImGui::EndTable();
	}
	ImGui::End();
}

void DrawItemTooltip(const char* text) {
	if (text && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", text);
	}
}

bool DrawDurationInput(
	const char* label, float& milliseconds, float width, const char* tooltip
) {
	static std::unordered_map<ImGuiID, DurationEditState> states;
	const ImGuiID id{ ImGui::GetID(label) };
	auto& state{ states[id] };

	auto format = [](float value, char* buffer, std::size_t size) {
		const double clamped{ std::max(0.0, static_cast<double>(value)) };
		if (clamped == 0.0) {
			std::snprintf(buffer, size, "0s");
		} else if (clamped >= 1000.0 && std::fmod(clamped, 1000.0) == 0.0) {
			std::snprintf(buffer, size, "%.4gs", clamped / 1000.0);
		} else {
			std::snprintf(buffer, size, "%.4gms", clamped);
		}
	};

	if (!state.initialized || !state.was_active) {
		format(milliseconds, state.buffer.data(), state.buffer.size());
		state.initialized = true;
	}

	ImGui::SetNextItemWidth(width);
	const bool submitted{ ImGui::InputText(
		label, state.buffer.data(), state.buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue
	) };
	const bool active{ ImGui::IsItemActive() };
	const bool commit{ submitted || ImGui::IsItemDeactivatedAfterEdit() };

	if (commit) {
		std::string text{ state.buffer.data() };
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
			text.pop_back();
		}
		std::size_t first{ 0 };
		while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) {
			++first;
		}
		text.erase(0, first);

		char* end{ nullptr };
		const double value{ std::strtod(text.c_str(), &end) };
		std::string unit{ end ? end : "" };
		while (!unit.empty() && std::isspace(static_cast<unsigned char>(unit.front()))) {
			unit.erase(unit.begin());
		}
		std::ranges::transform(unit, unit.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});

		double multiplier{ 1.0 };
		bool valid{ end != text.c_str() && std::isfinite(value) && value >= 0.0 };
		if (unit.empty() || unit == "ms") {
			multiplier = 1.0;
		} else if (unit == "s" || unit == "sec") {
			multiplier = 1000.0;
		} else if (unit == "m" || unit == "min") {
			multiplier = 60000.0;
		} else {
			valid = false;
		}

		if (valid) {
			milliseconds = static_cast<float>(value * multiplier);
		}
		format(milliseconds, state.buffer.data(), state.buffer.size());
	}

	state.was_active = active;
	DrawItemTooltip(tooltip);
	return commit;
}

float CompactControlSpacing() {
	return ImGui::GetStyle().ItemSpacing.x;
}

void SameLineControl() {
	ImGui::SameLine(0.0f, CompactControlSpacing());
}

float EnabledDeleteControlsWidth() {
	return ImGui::GetFrameHeight() * 2.0f + CompactControlSpacing();
}

bool DrawEnabledDeleteControls(
	bool& enabled, const char* enabled_tooltip, const char* delete_tooltip
) {
	const float size{ ImGui::GetFrameHeight() };
	ImGui::Checkbox("##Enabled", &enabled);
	DrawItemTooltip(enabled_tooltip);
	SameLineControl();
	const bool remove{ ImGui::Button("x", ImVec2{ size, size }) };
	DrawItemTooltip(delete_tooltip);
	return remove;
}

bool DrawCenteredTextButton(
	const char* id, const char* text, ImVec2 size
) {
	const bool pressed{ ImGui::Button(id, size) };
	const ImVec2 minimum{ ImGui::GetItemRectMin() };
	const ImVec2 maximum{ ImGui::GetItemRectMax() };
	const ImVec2 text_size{ ImGui::CalcTextSize(text) };
	const ImVec2 text_position{
		minimum.x + (maximum.x - minimum.x - text_size.x) * 0.5f,
		minimum.y + (maximum.y - minimum.y - text_size.y) * 0.5f
	};
	ImGui::GetWindowDrawList()->AddText(
		text_position, ImGui::GetColorU32(ImGuiCol_Text), text
	);
	return pressed;
}

bool DrawToggleButton(
	const char* label, bool& value, ImVec2 size, const char* tooltip
) {
	const bool dimmed{ !value };
	if (dimmed) {
		ImGui::PushStyleVar(
			ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f
		);
	}
	const bool pressed{ ImGui::Button(label, size) };
	if (dimmed) {
		ImGui::PopStyleVar();
	}
	if (pressed) {
		value = !value;
	}
	DrawItemTooltip(tooltip);
	return pressed;
}

float GetHalfRowWidth() {
	return std::max(
		1.0f,
		(ImGui::GetContentRegionAvail().x - CompactControlSpacing()) * 0.5f
	);
}

float GetCountControlWidth(const char* label) {
	const float button_width{ ImGui::GetFrameHeight() };
	const float spacing{ CompactControlSpacing() };
	const std::string widest{ std::string{ label } + ": 100" };
	return ImGui::CalcTextSize(widest.c_str()).x +
		button_width * 2.0f + spacing * 2.0f;
}

void DrawCountControl(
	const char* label, int& value, int minimum, int maximum, bool disabled, const char* tooltip
) {
	value = std::clamp(value, minimum, maximum);
	const float button_width{ ImGui::GetFrameHeight() };
	const float spacing{ CompactControlSpacing() };
	const std::string widest{ std::string{ label } + ": 100" };
	const float text_width{ ImGui::CalcTextSize(widest.c_str()).x };
	const float start_x{ ImGui::GetCursorScreenPos().x };

	ImGui::PushID(label);
	ImGui::BeginDisabled(disabled);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s: %d", label, value);
	DrawItemTooltip(tooltip);
	ImGui::SameLine(0.0f, spacing);
	ImGui::SetCursorScreenPos(
		ImVec2{ start_x + text_width + spacing, ImGui::GetCursorScreenPos().y }
	);
	ImGui::BeginDisabled(value >= maximum);
	if (ImGui::Button("+", ImVec2{ button_width, button_width })) {
		++value;
	}
	ImGui::EndDisabled();
	SameLineControl();
	ImGui::BeginDisabled(value <= minimum);
	if (ImGui::Button("-", ImVec2{ button_width, button_width })) {
		--value;
	}
	ImGui::EndDisabled();
	ImGui::EndDisabled();
	ImGui::PopID();
}

bool DrawAddableSectionHeader(
	const char* id, const char* label, bool default_open, bool empty,
	const char* section_tooltip, const char* empty_tooltip,
	const char* add_tooltip, bool& add_requested
) {
	static std::unordered_map<ImGuiID, bool> force_open_next_frame;
	ImGui::PushID(id);
	const ImGuiID tree_id{ ImGui::GetID("Tree") };
	if (force_open_next_frame[tree_id]) {
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		force_open_next_frame[tree_id] = false;
	}

	bool open{ false };
	const float add_width{ ImGui::GetFrameHeight() };
	if (ImGui::BeginTable("SectionHeaderRow", 2, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Section", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, add_width);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
		ImGui::TableSetColumnIndex(0);
		ImGuiTreeNodeFlags flags{
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen |
			ImGuiTreeNodeFlags_Framed
		};
		if (default_open) {
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{ 0.31f, 0.24f, 0.34f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4{ 0.40f, 0.31f, 0.44f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4{ 0.47f, 0.36f, 0.51f, 1.0f });
		open = ImGui::TreeNodeEx("Tree", flags, "%s", label);
		ImGui::PopStyleColor(3);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", empty ? empty_tooltip : section_tooltip);
		}
		ImGui::TableSetColumnIndex(1);
		if (ImGui::Button("+", ImVec2{ add_width, ImGui::GetFrameHeight() })) {
			add_requested = true;
			open = true;
			force_open_next_frame[tree_id] = true;
		}
		DrawItemTooltip(add_tooltip);
		ImGui::EndTable();
	}
	ImGui::PopID();
	return open;
}

bool DrawUnframedSectionHeader(
	const char* id, const char* label, bool default_open, bool empty,
	const char* tooltip, bool show_add_button,
	const char* add_tooltip, bool& add_requested
) {
	static std::unordered_map<ImGuiID, bool> force_open_next_frame;
	ImGui::PushID(id);
	const ImGuiID tree_id{ ImGui::GetID("Tree") };
	if (force_open_next_frame[tree_id]) {
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		force_open_next_frame[tree_id] = false;
	}

	bool open{ false };
	const float button_size{ ImGui::GetFrameHeight() };
	const int columns{ show_add_button ? 2 : 1 };
	if (ImGui::BeginTable("SectionHeaderRow", columns, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Section", ImGuiTableColumnFlags_WidthStretch);
		if (show_add_button) {
			ImGui::TableSetupColumn(
				"Add", ImGuiTableColumnFlags_WidthFixed, button_size
			);
		}
		ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);
		ImGui::TableSetColumnIndex(0);
		ImGuiTreeNodeFlags flags{
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_NoTreePushOnOpen |
			ImGuiTreeNodeFlags_FramePadding
		};
		if (empty) {
			flags |= ImGuiTreeNodeFlags_Leaf;
		} else if (default_open) {
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}
		const bool tree_open{ ImGui::TreeNodeEx("Tree", flags, "%s", label) };
		open = !empty && tree_open;
		DrawItemTooltip(empty ? "Add an item to use this section." : tooltip);

		if (show_add_button) {
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Button("+", ImVec2{ button_size, button_size })) {
				add_requested = true;
				open = true;
				force_open_next_frame[tree_id] = true;
			}
			DrawItemTooltip(add_tooltip);
		}
		ImGui::EndTable();
	}
	ImGui::PopID();
	return open || add_requested;
}

void DrawSelectedItemsTooltip(std::span<const std::string> items) {
	if (!ImGui::IsItemHovered()) {
		return;
	}
	std::string tooltip{ "Selected:" };
	if (items.empty()) {
		tooltip += "\nNone";
	} else {
		for (const auto& item : items) {
			tooltip += "\n- ";
			tooltip += item;
		}
	}
	ImGui::SetTooltip("%s", tooltip.c_str());
}

ActionForm GetActionForm(const Action& action) {
	if (action.type == ActionRegistry::Key<WaitAction>()) {
		return ActionForm::Delay;
	}
	return action.timing ? ActionForm::Tween : ActionForm::Action;
}

void SetActionForm(Action& action, ActionForm form) {
	const bool enabled{ action.enabled };
	const auto* registration{ ActionRegistry::Find(action.type) };

	switch (form) {
		case ActionForm::Action:
			if (!registration || registration->requires_timing ||
				action.type == ActionRegistry::Key<WaitAction>()) {
				action = ActionRegistry::Make<SetVisibleAction>();
			}
			action.completion.reset();
			action.timing.reset();
			break;
		case ActionForm::Tween:
			if (!registration || !registration->supports_timing ||
				action.type == ActionRegistry::Key<WaitAction>()) {
				action = ActionRegistry::Make<MoveToAction>();
			}
			registration = ActionRegistry::Find(action.type);
			action.completion = ActionCompletion::Duration;
			action.timing = registration && registration->default_timing
				? registration->default_timing
				: std::optional<ActionTiming>{ ActionTiming{} };
			break;
		case ActionForm::Delay:
			action = ActionRegistry::Make<WaitAction>();
			action.completion = ActionCompletion::Duration;
			break;
	}

	action.enabled = enabled;
}

void EnsureActionValue(Action& action) {
	if (!action.value.is_null()) {
		return;
	}
	if (const auto* registration{ ActionRegistry::Find(action.type_hash) };
		registration && registration->make_default) {
		action.value = registration->make_default();
	}
	if (action.value.is_null()) {
		action.value = ptgn::json::object();
	}
}

std::string ActionSummary(const Action& action) {
	const auto* editor{ editor::ActionEditorRegistry::Find(action.type) };
	std::string result{ editor ? editor->options.label : action.type };
	if (action.timing) {
		result += " (" + std::to_string(static_cast<int>(action.timing->duration_ms)) + "ms)";
	}
	return result;
}

void MoveAction(std::vector<Action>& actions, int from, int to) {
	if (from < 0 || to < 0 || from >= static_cast<int>(actions.size()) ||
		to >= static_cast<int>(actions.size()) || from == to) {
		return;
	}
	Action moved{ std::move(actions[static_cast<std::size_t>(from)]) };
	actions.erase(actions.begin() + from);
	actions.insert(actions.begin() + to, std::move(moved));
}

bool DrawAddComponentButton(
	AddComponentsAction& action,
	const char* popup_id
) {
	bool changed{ false };
	const float button_size{ ImGui::GetFrameHeight() };
	if (ImGui::Button("+##AddComponent", ImVec2{ button_size, button_size })) {
		ImGui::OpenPopup(popup_id);
	}
	DrawItemTooltip("Add a registered component.");

	if (!ImGui::BeginPopup(popup_id)) {
		return false;
	}

	std::vector<std::string> groups;
	for (const auto& component : ptgn::ComponentRegistry::Components()) {
		if (!component.make_default_json || !HasComponentJsonEditor(component)) {
			continue;
		}

		auto options{ ResolveComponentEditor(component) };
		const std::string group{
			options.group.empty() ? "Other" : options.group
		};
		if (!std::ranges::contains(groups, group)) {
			groups.push_back(group);
		}
	}

	for (const auto& group : groups) {
		if (!ImGui::BeginMenu(group.c_str())) {
			continue;
		}

		for (const auto& component : ptgn::ComponentRegistry::Components()) {
			if (!component.make_default_json || !HasComponentJsonEditor(component)) {
				continue;
			}

			auto options{ ResolveComponentEditor(component) };
			const std::string_view candidate_group{
				options.group.empty() ? std::string_view{ "Other" }
									  : std::string_view{ options.group }
			};
			if (candidate_group != group) {
				continue;
			}

			const bool already_added{
				std::ranges::any_of(action.components, [&](const auto& definition) {
					return definition.type == component.name;
				})
			};

			ImGui::BeginDisabled(already_added);
			if (ImGui::MenuItem(options.label.c_str())) {
				action.components.push_back(MakeComponentDefinition(component));
				changed = true;
			}
			ImGui::EndDisabled();

			if (ImGui::IsItemHovered(
					already_added ? ImGuiHoveredFlags_AllowWhenDisabled
								  : ImGuiHoveredFlags_None
				)) {
				if (component.is_empty) {
					ImGui::SetTooltip("Tag component");
				} else if (already_added) {
					ImGui::SetTooltip("Already added.");
				} else {
					ImGui::SetTooltip("%.*s", static_cast<int>(component.name.size()), component.name.data());
				}
			}
		}

		ImGui::EndMenu();
	}

	ImGui::EndPopup();
	return changed;
}

void RegisterEditorTypes() {
	if (!ptgn::editor::ComponentEditorRegistry::Find(
			ptgn::Hash<ptgn::Transform>()
		)) {
		PTGN_REGISTER(ptgn::editor::ComponentEditorRegistry::Register<ptgn::Transform>(
			ptgn::editor::MakeComponentEditorRegistration(
				ptgn::editor::ComponentEditorOptions{
					.label = "Transform",
					.group = "Core",
				}
			)
		));
	}
	PTGN_REGISTER(editor::ActionEditorRegistry::Register<WaitAction>(
		"engine.wait",
		{ .label = "Delay", .group = "Timing", .description = "Delay before continuing the sequence." },
		[](WaitAction&, EditorContextTemp&) { return false; }
	));
	PTGN_REGISTER(editor::ActionEditorRegistry::RegisterInline<MoveToAction>(
		"engine.move_to",
		{ .label = "Move To", .group = "Transform", .description = "Move the owning entity." },
		[](MoveToAction& action, EditorContextTemp&) {
			bool changed{ false };
			const float available{ ImGui::GetContentRegionAvail().x };
			const float spacing{ CompactControlSpacing() };
			const float mode_width{
				std::max(
					ImGui::CalcTextSize("Relative").x,
					ImGui::CalcTextSize("Absolute").x
				) + ImGui::GetStyle().FramePadding.x * 2.0f
			};
			const float field_width{ std::max(
				36.0f, (available - mode_width - spacing * 2.0f) * 0.5f
			) };
			ImGui::SetNextItemWidth(field_width);
			changed |= ImGui::DragFloat(
				"##X", &action.destination.x, 1.0f,
				-100000.0f, 100000.0f, "X: %.0f"
			);
			SameLineControl();
			ImGui::SetNextItemWidth(field_width);
			changed |= ImGui::DragFloat(
				"##Y", &action.destination.y, 1.0f,
				-100000.0f, 100000.0f, "Y: %.0f"
			);
			SameLineControl();
			if (ImGui::Button(
					action.relative ? "Relative" : "Absolute",
					ImVec2{ mode_width, ImGui::GetFrameHeight() }
				)) {
				action.relative = !action.relative;
				changed = true;
			}
			editor::DrawItemTooltip(
				action.relative
					? "Offset from the entity's current position."
					: "Use an absolute world position."
			);
			return changed;
		},
		[](MoveToAction&, EditorContextTemp&) { return false; }
	));
	PTGN_REGISTER(editor::ActionEditorRegistry::RegisterInline<RotateToAction>(
		"engine.rotate_to",
		{ .label = "Rotate To", .group = "Transform", .description = "Rotate the owning entity to an angle." },
		[](RotateToAction& action, EditorContextTemp&) {
			const float available{ ImGui::GetContentRegionAvail().x };
			const float spacing{ CompactControlSpacing() };
			const float shortest_width{
				ImGui::CalcTextSize("Shortest").x +
				ImGui::GetStyle().FramePadding.x * 2.0f
			};
			const float relative_width{
				ImGui::CalcTextSize("Relative").x +
				ImGui::GetStyle().FramePadding.x * 2.0f
			};
			const float degrees_width{ std::max(
				48.0f, available - shortest_width - relative_width - spacing * 2.0f
			) };
			bool changed{ false };
			ImGui::SetNextItemWidth(degrees_width);
			changed |= ImGui::DragFloat(
				"##Degrees", &action.degrees, 1.0f,
				-3600.0f, 3600.0f, "%.1f deg"
			);
			SameLineControl();
			changed |= DrawToggleButton(
				"Shortest", action.shortest_path,
				ImVec2{ shortest_width, ImGui::GetFrameHeight() },
				"Toggle the shortest rotational path."
			);
			SameLineControl();
			changed |= DrawToggleButton(
				"Relative", action.relative,
				ImVec2{ relative_width, ImGui::GetFrameHeight() },
				"Treat the angle as an offset from the current rotation."
			);
			return changed;
		},
		[](RotateToAction&, EditorContextTemp&) { return false; }
	));
	PTGN_REGISTER(editor::ActionEditorRegistry::RegisterInline<SetVisibleAction>(
		"engine.set_visible",
		{ .label = "Set Visible", .group = "Entity", .description = "Set the owning entity visibility.", .menu_order = 3 },
		[](SetVisibleAction& action, EditorContextTemp&) {
			const char* preview{ action.visible ? "True" : "False" };
			bool changed{ false };
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##VisibleValue", preview)) {
				if (ImGui::Selectable("True", action.visible)) {
					action.visible = true;
					changed = true;
				}
				if (ImGui::Selectable("False", !action.visible)) {
					action.visible = false;
					changed = true;
				}
				ImGui::EndCombo();
			}
			DrawItemTooltip("Visibility value assigned by this action.");
			return changed;
		},
		[](SetVisibleAction&, EditorContextTemp&) { return false; }
	));
	PTGN_REGISTER(editor::ActionEditorRegistry::Register<PlayAudioAction>(
		"engine.play_audio",
		{ .label = "Play Audio", .group = "Audio", .description = "Play an audio asset." },
		[](PlayAudioAction& action, EditorContextTemp&) {
			action.loops = std::clamp(action.loops, 0, 100);
			bool changed{ false };
			if (ImGui::BeginTable("AudioParams", 3, ImGuiTableFlags_SizingStretchProp)) {
				ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthFixed, 96.0f);
				ImGui::TableSetupColumn(
					"Loops", ImGuiTableColumnFlags_WidthFixed, GetCountControlWidth("Loops")
				);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::InputText("##Audio", &action.asset);
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::SliderFloat(
					"##Volume", &action.volume, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp
				);
				DrawItemTooltip("Audio volume. Double-click to enter an exact value.");
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					ImGui::OpenPopup("ExactVolume");
				}
				if (ImGui::BeginPopup("ExactVolume")) {
					ImGui::SetNextItemWidth(110.0f);
					changed |= ImGui::InputFloat("Volume", &action.volume, 0.01f, 0.1f, "%.3f");
					action.volume = std::clamp(action.volume, 0.0f, 1.0f);
					ImGui::EndPopup();
				}
				ImGui::TableSetColumnIndex(2);
				const int previous_loops{ action.loops };
				DrawCountControl("Loops", action.loops, 0, 100, false, "Additional audio loops.");
				changed |= previous_loops != action.loops;
				ImGui::EndTable();
			}
			return changed;
		}
	));
	PTGN_REGISTER(editor::ActionEditorRegistry::RegisterInline<EmitSignalAction>(
		"engine.emit_signal",
		{ .label = "Emit Signal", .group = "", .description = "Broadcast a Signal identified by a strong string key." },
		[](EmitSignalAction& action, EditorContextTemp&) {
			ImGui::SetNextItemWidth(-FLT_MIN);
			const bool changed{ ImGui::InputTextWithHint(
				"##SignalName", "Signal name", &action.signal.value
			) };
			DrawItemTooltip("Signal name to broadcast.");
			return changed;
		},
		[](EmitSignalAction&, EditorContextTemp&) { return false; }
	));
	PTGN_REGISTER(editor::ActionEditorRegistry::RegisterInline<AddComponentsAction>(
		"engine.add_components",
		{ .label = "Add Components", .group = "Entity", .description = "Add registered components to the owner.", .menu_order = 1 },
		[](AddComponentsAction& action, EditorContextTemp&) {
			std::vector<std::string> selected_labels;
			std::string preview;
			for (const auto& definition : action.components) {
				const auto* component{ ptgn::ComponentRegistry::Find(definition.type) };
				const std::string label{
					component ? ResolveComponentEditor(*component).label : definition.type
				};
				selected_labels.push_back(label);
				if (!preview.empty()) {
					preview += ", ";
				}
				preview += label;
			}
			if (preview.empty()) {
				preview = "None";
			}

			bool changed{ false };
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##AddComponents", preview.c_str())) {
				std::vector<std::string> groups;
				for (const auto& component : ptgn::ComponentRegistry::Components()) {
					if (!component.make_default_json || !HasComponentJsonEditor(component)) {
						continue;
					}

					auto options{ ResolveComponentEditor(component) };
					if (!options.group.empty() &&
						!std::ranges::contains(groups, options.group)) {
						groups.push_back(options.group);
					}
				}

				auto draw_component = [&](const ptgn::RegisteredComponent& component) {
					if (!component.make_default_json || !HasComponentJsonEditor(component)) {
						return;
					}

					auto options{ ResolveComponentEditor(component) };
					bool selected{ std::ranges::any_of(
						action.components,
						[&](const ComponentDefinition& definition) {
							return definition.type == component.name;
						}
					) };

					if (ImGui::Checkbox(options.label.c_str(), &selected)) {
						if (selected) {
							action.components.push_back(MakeComponentDefinition(component));
						} else {
							std::erase_if(
								action.components,
								[&](const ComponentDefinition& definition) {
									return definition.type == component.name;
								}
							);
						}
						changed = true;
					}

					DrawItemTooltip(component.is_empty ? "Tag component" : component.name.data());
				};

				for (const auto& component : ptgn::ComponentRegistry::Components()) {
					if (!component.make_default_json || !HasComponentJsonEditor(component)) {
						continue;
					}
					if (ResolveComponentEditor(component).group.empty()) {
						draw_component(component);
					}
				}

				for (const auto& group : groups) {
					if (!ImGui::BeginMenu(group.c_str())) {
						continue;
					}
					for (const auto& component : ptgn::ComponentRegistry::Components()) {
						if (!component.make_default_json || !HasComponentJsonEditor(component)) {
							continue;
						}
						if (ResolveComponentEditor(component).group == group) {
							draw_component(component);
						}
					}
					ImGui::EndMenu();
				}
				ImGui::EndCombo();
			}
			DrawSelectedItemsTooltip(selected_labels);
			return changed;
		},
		[](AddComponentsAction& action, EditorContextTemp&) {
			bool changed{ false };
			int remove{ -1 };
			for (int i{ 0 }; i < static_cast<int>(action.components.size()); ++i) {
				auto& definition{ action.components[static_cast<std::size_t>(i)] };
				const auto* component{ ptgn::ComponentRegistry::Find(definition.type) };
				ImGui::PushID(definition.type.c_str());

				const std::string label{
					component ? ResolveComponentEditor(*component).label
							  : definition.type
				};
				if (ImGui::BeginTable(
						"ComponentTitle", 2, ImGuiTableFlags_SizingStretchProp
					)) {
					ImGui::TableSetupColumn(
						"Title", ImGuiTableColumnFlags_WidthStretch
					);
					ImGui::TableSetupColumn(
						"Remove", ImGuiTableColumnFlags_WidthFixed,
						ImGui::GetFrameHeight()
					);
					ImGui::TableNextRow(
						ImGuiTableRowFlags_None, ImGui::GetFrameHeight()
					);
					ImGui::TableSetColumnIndex(0);
					ImGui::SeparatorText(label.c_str());
					if (component && component->is_empty) {
						DrawItemTooltip("Tag component");
					}

					ImGui::TableSetColumnIndex(1);
					if (ImGui::Button(
							"x",
							ImVec2{
								ImGui::GetFrameHeight(),
								ImGui::GetFrameHeight()
							}
						)) {
						remove = i;
					}
					ImGui::EndTable();
				}

				if (component && !component->is_empty) {
					if (definition.value.is_null() && component->make_default_json) {
						definition.value = component->make_default_json();
					}
					if (definition.value.is_null()) {
						definition.value = ptgn::json::object();
					}
					const bool component_changed{
						ptgn::editor::ComponentEditorRegistry::DrawJson(
							*component, definition.value
						)
					};
					changed |= component_changed;
					if (component_changed) {
						definition.apply_live = {};
					}
				}
				ImGui::PopID();
			}

			if (remove >= 0) {
				action.components.erase(action.components.begin() + remove);
				changed = true;
			}
			return changed;
		}
	));
	PTGN_REGISTER(editor::ActionEditorRegistry::RegisterInline<RemoveComponentsAction>(
		"engine.remove_components",
		{ .label = "Remove Components", .group = "Entity", .description = "Remove selected registered components from the owner.", .menu_order = 2, .separator_after = true },
		[](RemoveComponentsAction& action, EditorContextTemp&) {
			std::vector<std::string> selected_labels;
			std::string preview;
			for (const auto& name : action.components) {
				const auto* component{ ptgn::ComponentRegistry::Find(name) };
				const std::string label{
					component ? ResolveComponentEditor(*component).label : name
				};
				selected_labels.push_back(label);
				if (!preview.empty()) {
					preview += ", ";
				}
				preview += label;
			}
			if (preview.empty()) {
				preview = "None";
			}

			bool changed{ false };
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##RemoveComponents", preview.c_str())) {
				std::vector<std::string> groups;
				for (const auto& component : ptgn::ComponentRegistry::Components()) {
					if (!FindComponentEditor(component)) {
						continue;
					}
					auto options{ ResolveComponentEditor(component) };
					if (!options.group.empty() &&
						!std::ranges::contains(groups, options.group)) {
						groups.push_back(options.group);
					}
				}

				auto draw_component = [&](const ptgn::RegisteredComponent& component) {
					if (!FindComponentEditor(component)) {
						return;
					}

					auto options{ ResolveComponentEditor(component) };
					bool selected{
						std::ranges::contains(action.components, component.name)
					};
					if (ImGui::Checkbox(options.label.c_str(), &selected)) {
						if (selected) {
							action.components.emplace_back(component.name);
						} else {
							std::erase(action.components, component.name);
						}
						changed = true;
					}
					DrawItemTooltip(component.name.data());
				};

				for (const auto& component : ptgn::ComponentRegistry::Components()) {
					if (FindComponentEditor(component) &&
						ResolveComponentEditor(component).group.empty()) {
						draw_component(component);
					}
				}

				for (const auto& group : groups) {
					if (!ImGui::BeginMenu(group.c_str())) {
						continue;
					}
					for (const auto& component : ptgn::ComponentRegistry::Components()) {
						if (FindComponentEditor(component) &&
							ResolveComponentEditor(component).group == group) {
							draw_component(component);
						}
					}
					ImGui::EndMenu();
				}
				ImGui::EndCombo();
			}

			DrawSelectedItemsTooltip(selected_labels);
			return changed;
		},
		[](RemoveComponentsAction&, EditorContextTemp&) { return false; }
	));
	PTGN_REGISTER(editor::ActionEditorRegistry::Register<SpawnEntityAction>(
		"engine.spawn_entity",
		{ .label = "Spawn Entity", .group = "Entity", .description = "Spawn one or more prefab instances.", .menu_order = 0 },
		[](SpawnEntityAction& action, EditorContextTemp& context) {
			action.count = std::clamp(action.count, 1, 100);
			action.rectangle_size.x = std::max(0.0f, action.rectangle_size.x);
			action.rectangle_size.y = std::max(0.0f, action.rectangle_size.y);
			action.radius = std::max(0.0f, action.radius);
			bool changed{ false };

			if (ImGui::BeginTable(
					"SpawnPrimaryRow", 3, ImGuiTableFlags_SizingStretchProp
				)) {
				ImGui::TableSetupColumn(
					"Prefab", ImGuiTableColumnFlags_WidthStretch, 1.0f
				);
				ImGui::TableSetupColumn(
					"Options", ImGuiTableColumnFlags_WidthStretch, 1.0f
				);
				ImGui::TableSetupColumn(
					"Count", ImGuiTableColumnFlags_WidthFixed,
					GetCountControlWidth("Count")
				);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

				ImGui::TableSetColumnIndex(0);
				const auto* current{ context.prefabs.Find(action.prefab_key) };
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::BeginCombo(
						"##Prefab", current ? current->name.c_str() : "Missing Prefab"
					)) {
					for (const auto& prefab : context.prefabs.definitions) {
						if (ImGui::Selectable(
								prefab.name.c_str(), prefab.key == action.prefab_key
							)) {
							action.prefab_key = prefab.key;
							changed = true;
						}
					}
					ImGui::EndCombo();
				}
				DrawItemTooltip("Prefab instantiated by this action.");

				ImGui::TableSetColumnIndex(1);
				std::vector<std::string> selected_options;
				if (action.parent_to_owner) selected_options.emplace_back("Parent to Owner");
				if (action.inherit_owner_rotation) selected_options.emplace_back("Inherit Rotation");
				if (action.inherit_owner_scale) selected_options.emplace_back("Inherit Scale");
				if (action.random_rotation) selected_options.emplace_back("Random Rotation");
				std::string options;
				for (const auto& option : selected_options) {
					if (!options.empty()) {
						options += ", ";
					}
					options += option;
				}
				if (options.empty()) {
					options = "Options";
				}
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::BeginCombo("##SpawnOptions", options.c_str())) {
					changed |= ImGui::Checkbox(
						"Parent to Owner", &action.parent_to_owner
					);
					if (ImGui::Checkbox(
							"Inherit Rotation", &action.inherit_owner_rotation
						) && action.inherit_owner_rotation) {
						action.random_rotation = false;
						changed = true;
					}
					changed |= ImGui::Checkbox(
						"Inherit Scale", &action.inherit_owner_scale
					);
					if (ImGui::Checkbox(
							"Random Rotation", &action.random_rotation
						) && action.random_rotation) {
						action.inherit_owner_rotation = false;
						changed = true;
					}
					ImGui::EndCombo();
				}
				DrawSelectedItemsTooltip(selected_options);

				ImGui::TableSetColumnIndex(2);
				const int previous_count{ action.count };
				DrawCountControl(
					"Count", action.count, 1, 100, false,
					"Number of prefab instances to create."
				);
				changed |= previous_count != action.count;
				ImGui::EndTable();
			}

			auto draw_area_combo = [](const char* id, SpawnArea& value) {
				const std::string preview{ magic_enum::enum_name(value) };
				bool local_changed{ false };
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::BeginCombo(id, preview.c_str())) {
					for (const auto candidate : magic_enum::enum_values<SpawnArea>()) {
						const std::string label{ magic_enum::enum_name(candidate) };
						if (ImGui::Selectable(label.c_str(), candidate == value)) {
							value = candidate;
							local_changed = true;
						}
					}
					ImGui::EndCombo();
				}
				return local_changed;
			};

			const SpawnArea displayed_area{ action.area };
			int columns{ 4 };
			if (displayed_area == SpawnArea::Rectangle) {
				columns += 2;
			} else if (displayed_area == SpawnArea::Circle) {
				++columns;
			}
			if (ImGui::BeginTable(
					"SpawnEntityPlacementRow", columns,
					ImGuiTableFlags_SizingStretchProp
				)) {
				ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				ImGui::TableSetupColumn(
					"Mode", ImGuiTableColumnFlags_WidthFixed, 82.0f
				);
				ImGui::TableSetupColumn("Shape", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				if (displayed_area == SpawnArea::Rectangle) {
					ImGui::TableSetupColumn("Width", ImGuiTableColumnFlags_WidthStretch, 1.0f);
					ImGui::TableSetupColumn("Height", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				} else if (displayed_area == SpawnArea::Circle) {
					ImGui::TableSetupColumn("Radius", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				}
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::DragFloat(
					"##SpawnX", &action.center.x, 1.0f,
					-100000.0f, 100000.0f, "X: %.0f"
				);
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::DragFloat(
					"##SpawnY", &action.center.y, 1.0f,
					-100000.0f, 100000.0f, "Y: %.0f"
				);
				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button(
						action.origin == SpawnOrigin::OwnerEntity
							? "Relative"
							: "Absolute",
						ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
					)) {
					action.origin = action.origin == SpawnOrigin::OwnerEntity
						? SpawnOrigin::Position
						: SpawnOrigin::OwnerEntity;
					changed = true;
				}
				DrawItemTooltip(
					action.origin == SpawnOrigin::OwnerEntity
						? "Offset from the owning entity."
						: "Use an absolute world position."
				);
				ImGui::TableSetColumnIndex(3);
				changed |= draw_area_combo("##SpawnArea", action.area);
				if (displayed_area == SpawnArea::Rectangle) {
					ImGui::TableSetColumnIndex(4);
					ImGui::SetNextItemWidth(-FLT_MIN);
					changed |= ImGui::DragFloat(
						"##SpawnWidth", &action.rectangle_size.x, 1.0f,
						0.0f, 100000.0f, "W: %.0f"
					);
					ImGui::TableSetColumnIndex(5);
					ImGui::SetNextItemWidth(-FLT_MIN);
					changed |= ImGui::DragFloat(
						"##SpawnHeight", &action.rectangle_size.y, 1.0f,
						0.0f, 100000.0f, "H: %.0f"
					);
				} else if (displayed_area == SpawnArea::Circle) {
					ImGui::TableSetColumnIndex(4);
					ImGui::SetNextItemWidth(-FLT_MIN);
					changed |= ImGui::DragFloat(
						"##SpawnRadius", &action.radius, 1.0f,
						0.0f, 100000.0f, "R: %.0f"
					);
				}
				ImGui::EndTable();
			}
			return changed;
		}
	));



}

ptgn::Entity SelectedEntity(const DemoEditorDrawContext& ui) {
	const auto& entities{ ui.context.host.Entities() };
	return ui.state.selected_entity >= 0 &&
		ui.state.selected_entity < static_cast<int>(entities.size())
		? entities[static_cast<std::size_t>(ui.state.selected_entity)]
		: ptgn::Entity{};
}

void DrawSidebar(DemoEditorDrawContext& ui) {
	if (!ImGui::BeginTabBar("SidebarTabs")) {
		return;
	}
	if (ImGui::BeginTabItem("Scene")) {
		ImGui::TextDisabled("Scene Hierarchy");
		ImGui::Separator();
		for (int i{ 0 }; i < static_cast<int>(ui.context.host.Entities().size()); ++i) {
			const auto entity{ ui.context.host.Entities()[static_cast<std::size_t>(i)] };
			if (ImGui::Selectable(
					std::string{ ui.context.host.Name(entity) }.c_str(),
					!ui.state.inspect_prefab && ui.state.selected_entity == i,
					0, ImVec2{ 0.0f, 25.0f }
				)) {
				ui.state.selected_entity = i;
				ui.state.inspect_prefab = false;
			}
		}
		if (ImGui::Button("+ Entity", ImVec2{ -FLT_MIN, 0.0f })) {
			ui.context.host.CreateEntity("New Entity");
			ui.state.selected_entity = static_cast<int>(ui.context.host.Entities().size()) - 1;
			ui.state.inspect_prefab = false;
		}
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Prefabs")) {
		DrawPrefabs(ui);
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
}

void DrawScene(DemoEditorDrawContext& ui) {
	const ImVec2 start{ ImGui::GetCursorScreenPos() };
	const ImVec2 size{ ImGui::GetContentRegionAvail() };
	ImDrawList* draw{ ImGui::GetWindowDrawList() };
	draw->AddRectFilled(
		start, ImVec2{ start.x + size.x, start.y + size.y }, ImGui::GetColorU32(ImGuiCol_FrameBg)
	);
	const ImVec2 center{ start.x + size.x * 0.5f, start.y + size.y * 0.5f };
	draw->AddLine(
		ImVec2{ start.x + 20.0f, center.y }, ImVec2{ start.x + size.x - 20.0f, center.y },
		ImGui::GetColorU32(ImGuiCol_Border)
	);

	for (int i{ 0 }; i < static_cast<int>(ui.context.host.Entities().size()); ++i) {
		const auto entity{ ui.context.host.Entities()[static_cast<std::size_t>(i)] };
		if (!entity.Has<ptgn::Transform>() || !ptgn::IsVisible(entity)) {
			continue;
		}
		const auto& transform{ entity.Get<ptgn::Transform>() };
		const auto visual{ ui.context.host.Visual(entity) };
		const ImVec2 position{ center.x + transform.position.x, center.y - transform.position.y };
		const ImU32 color{ ImGui::ColorConvertFloat4ToU32(visual.color) };
		ImVec2 minimum{};
		ImVec2 maximum{};
		if (entity.Has<ptgn::Rect>()) {
			const auto rect_size{ entity.Get<ptgn::Rect>().GetSize(transform) };
			const ImVec2 half{ rect_size.x * 0.5f, rect_size.y * 0.5f };
			minimum = { position.x - half.x, position.y - half.y };
			maximum = { position.x + half.x, position.y + half.y };
			draw->AddRectFilled(minimum, maximum, color, 4.0f);
			if (visual.sensor) {
				draw->AddRect(minimum, maximum, IM_COL32(70, 230, 135, 220), 4.0f, 0, 2.0f);
			}
		} else if (entity.Has<ptgn::Circle>()) {
			const float radius{ entity.Get<ptgn::Circle>().GetRadius(transform) };
			minimum = { position.x - radius, position.y - radius };
			maximum = { position.x + radius, position.y + radius };
			draw->AddCircleFilled(position, radius, color);
		} else {
			continue;
		}
		if (visual.health_fraction >= 0.0f) {
			const float bar_height{ 6.0f };
			const float bar_y{ minimum.y - 11.0f };
			const ImVec2 bar_min{ minimum.x, bar_y };
			const ImVec2 bar_max{ maximum.x, bar_y + bar_height };
			draw->AddRectFilled(bar_min, bar_max, IM_COL32(48, 38, 42, 235), 2.0f);
			draw->AddRectFilled(
				bar_min,
				ImVec2{
					bar_min.x + (bar_max.x - bar_min.x) * std::clamp(visual.health_fraction, 0.0f, 1.0f),
					bar_max.y
				},
				IM_COL32(70, 210, 95, 255), 2.0f
			);
			draw->AddRect(bar_min, bar_max, IM_COL32(225, 225, 225, 170), 2.0f);
		}
		if (!ui.state.inspect_prefab && i == ui.state.selected_entity) {
			draw->AddRect(
				ImVec2{ minimum.x - 3.0f, minimum.y - 3.0f },
				ImVec2{ maximum.x + 3.0f, maximum.y + 3.0f },
				ImGui::GetColorU32(ImGuiCol_ButtonHovered), 5.0f, 0, 2.0f
			);
		}
		const std::string name{ ui.context.host.Name(entity) };
		const ImVec2 text_size{ ImGui::CalcTextSize(name.c_str()) };
		draw->AddText(
			ImVec2{ position.x - text_size.x * 0.5f, maximum.y + 5.0f },
			ImGui::GetColorU32(ImGuiCol_Text), name.c_str()
		);
	}

	draw->AddText(
		ImVec2{ start.x + 12.0f, start.y + 10.0f }, ImGui::GetColorU32(ImGuiCol_Text),
		"WASD: move the Player"
	);
	draw->AddText(
		ImVec2{ start.x + 12.0f, start.y + 30.0f }, ImGui::GetColorU32(ImGuiCol_TextDisabled),
		"Door Sensor emits door.opened / door.closed Signals."
	);
	draw->AddText(
		ImVec2{ start.x + 12.0f, start.y + 50.0f }, ImGui::GetColorU32(ImGuiCol_TextDisabled),
		"Sliding Panel owns Actions that affect itself."
	);
	draw->AddText(
		ImVec2{ start.x + 12.0f, start.y + 70.0f }, ImGui::GetColorU32(ImGuiCol_TextDisabled),
		"Circle Spawner creates random circles; leaving recalls them by Signal."
	);
	draw->AddText(
		ImVec2{ start.x + 12.0f, start.y + 90.0f }, ImGui::GetColorU32(ImGuiCol_TextDisabled),
		"Damage Target takes 25 damage every 3 seconds while overlapped."
	);
	draw->AddText(
		ImVec2{ start.x + 12.0f, start.y + 110.0f }, ImGui::GetColorU32(ImGuiCol_TextDisabled),
		"Buttons expose component-driven pointer Events; one emits a global Signal."
	);
	ImGui::InvisibleButton("SceneCanvas", size);
	const bool inside_scene{ ImGui::IsItemHovered() };
	const ImVec2 mouse{ ImGui::GetMousePos() };
	PointerFrame pointer_frame{
		.world_position = { mouse.x - center.x, center.y - mouse.y },
		.inside_scene = inside_scene,
	};
	for (std::size_t i{ 0 }; i < kMouseButtons.size(); ++i) {
		const auto button{ static_cast<ImGuiMouseButton>(i) };
		pointer_frame.pressed[i] =
			inside_scene && ImGui::IsMouseClicked(button);
		pointer_frame.released[i] = ImGui::IsMouseReleased(button);
	}
	ui.context.host.SubmitPointerFrame(pointer_frame);
}

void DrawInspector(DemoEditorDrawContext& ui) {
	if (ui.state.inspect_prefab) {
		if (ui.state.selected_prefab >= 0 &&
			ui.state.selected_prefab < static_cast<int>(ui.context.prefabs.definitions.size())) {
			DrawPrefabInspector(ui, ui.context.prefabs.definitions[static_cast<std::size_t>(ui.state.selected_prefab)]);
		}
		return;
	}

	ptgn::Entity entity{ SelectedEntity(ui) };
	if (!entity) {
		ImGui::TextDisabled("Select an entity.");
		return;
	}

	ImGui::TextDisabled("Entity");
	if (ImGui::BeginTable("EntityIdentity", 2, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthFixed, 170.0f);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
		ImGui::TableSetColumnIndex(0);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##Name", "Name", &ui.context.host.EditableName(entity));
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##Tag", "Tag", &ui.context.host.EditableTag(entity));
		ImGui::EndTable();
	}

	DrawEntityComponents(ui, entity);
	if (auto* scripts{ entity.TryGet<ScriptsComponent>() }) {
		DrawScripts(ui, entity, *scripts);
	} else if (ImGui::Button("+ Scripts Component", ImVec2{ -FLT_MIN, 0.0f })) {
		entity.Add<ScriptsComponent>();
	}
}

void DrawEntityComponents(DemoEditorDrawContext& ui, ptgn::Entity entity) {
	ImGui::PushID("EntityComponents");
	const bool open{ ImGui::TreeNodeEx(
		"##Components", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth,
		"Components"
	) };
	if (open) {
		for (const auto& component : ptgn::ComponentRegistry::Components()) {
			if (!component.has(entity)) {
				continue;
			}

			const auto* editor{ FindComponentEditor(component) };
			if (!editor) {
				continue;
			}

			auto options{ ResolveComponentEditor(component) };
			ImGui::PushID(component.type_id);
			bool component_open{ false };
			bool remove{ false };

			if (ImGui::BeginTable(
					"ComponentHeader", options.removable ? 2 : 1,
					ImGuiTableFlags_SizingStretchProp
				)) {
				ImGui::TableSetupColumn(
					"Component", ImGuiTableColumnFlags_WidthStretch
				);
				if (options.removable) {
					ImGui::TableSetupColumn(
						"Remove", ImGuiTableColumnFlags_WidthFixed,
						ImGui::GetFrameHeight()
					);
				}
				ImGui::TableNextRow(
					ImGuiTableRowFlags_None, ImGui::GetFrameHeight()
				);
				ImGui::TableSetColumnIndex(0);
				const ImGuiTreeNodeFlags flags{
					ImGuiTreeNodeFlags_DefaultOpen |
					ImGuiTreeNodeFlags_Framed |
					ImGuiTreeNodeFlags_SpanAvailWidth |
					ImGuiTreeNodeFlags_NoTreePushOnOpen
				};
				component_open = ImGui::TreeNodeEx(
					"##Component", flags, "%s", options.label.c_str()
				);
				DrawItemTooltip(component.name.data());

				if (options.removable) {
					ImGui::TableSetColumnIndex(1);
					if (ImGui::Button(
							"x",
							ImVec2{
								ImGui::GetFrameHeight(),
								ImGui::GetFrameHeight()
							}
						)) {
						remove = true;
					}
				}
				ImGui::EndTable();
			}

			if (component_open && component.serialize && component.deserialize) {
				ptgn::json value;
				component.serialize(value, entity);
				if (ptgn::editor::ComponentEditorRegistry::DrawJson(
						component, value
					)) {
					component.deserialize(value, entity);
				}
			}

			if (remove) {
				component.remove(entity);
			}
			ImGui::PopID();
		}

		if (ImGui::Button("+ Add Component", ImVec2{ -FLT_MIN, 0.0f })) {
			ImGui::OpenPopup("AddEntityComponent");
		}
		if (ImGui::BeginPopup("AddEntityComponent")) {
			std::string current_group;
			for (const auto& component : ptgn::ComponentRegistry::Components()) {
				const auto* editor{ FindComponentEditor(component) };
				if (!editor || !component.add_default || component.has(entity)) {
					continue;
				}

				auto options{ ResolveComponentEditor(component) };
				if (!options.addable) {
					continue;
				}

				if (options.group != current_group) {
					if (!current_group.empty()) {
						ImGui::Separator();
					}
					if (!options.group.empty()) {
						ImGui::TextDisabled("%s", options.group.c_str());
					}
					current_group = options.group;
				}

				if (ImGui::MenuItem(options.label.c_str())) {
					component.add_default(entity);
				}
			}
			ImGui::EndPopup();
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void DrawScripts(
	DemoEditorDrawContext& ui,
	ptgn::Entity entity,
	ScriptsComponent& scripts
) {
	ImGui::PushID("ScriptsComponent");
	const bool open{ ImGui::TreeNodeEx(
		"##Scripts",
		ImGuiTreeNodeFlags_DefaultOpen |
			ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth,
		"Scripts"
	) };

	bool remove_component{ false };
	if (ImGui::BeginPopupContextItem("ScriptsContext")) {
		if (ImGui::MenuItem("Delete Component")) {
			remove_component = true;
		}
		ImGui::EndPopup();
	}

	if (remove_component) {
		if (open) {
			ImGui::TreePop();
		}
		entity.Remove<ScriptsComponent>();
		ImGui::PopID();
		return;
	}

	if (!open) {
		ImGui::PopID();
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.20f, 0.34f, 0.33f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.26f, 0.43f, 0.41f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.31f, 0.49f, 0.47f, 1.0f });
	if (ImGui::Button("+ Script", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddScript");
	}
	ImGui::PopStyleColor(3);
	DrawItemTooltip("Add a custom script or an editor-authored sequence script.");

	DrawAddResidentScriptPopup(ui, scripts);
	DrawResidentScripts(ui, entity, scripts);

	const ImVec2 activity_position{ ImGui::GetCursorScreenPos() };
	ImGui::SetCursorScreenPos(ImVec2{
		activity_position.x,
		activity_position.y + 4.0f
	});
	DrawActivity(ui);

	ImGui::TreePop();
	ImGui::PopID();
}

void DrawResidentScripts(
	DemoEditorDrawContext& ui,
	ptgn::Entity entity,
	ScriptsComponent& scripts
) {
	int remove{ -1 };

	std::vector<int> display_order;
	display_order.reserve(scripts.scripts.size());
	for (int i{ 0 }; i < static_cast<int>(scripts.scripts.size()); ++i) {
		if (scripts.scripts[static_cast<std::size_t>(i)].type_hash !=
			ptgn::Hash<SequenceScript>()) {
			display_order.push_back(i);
		}
	}
	for (int i{ 0 }; i < static_cast<int>(scripts.scripts.size()); ++i) {
		if (scripts.scripts[static_cast<std::size_t>(i)].type_hash ==
			ptgn::Hash<SequenceScript>()) {
			display_order.push_back(i);
		}
	}

	for (const int i : display_order) {
		auto& script{ scripts.scripts[static_cast<std::size_t>(i)] };
		const auto* registration{ ScriptRegistry::Find(script.type_hash) };
		const auto* editor{ ScriptEditorRegistry::Find(script.type) };

		if (script.type_hash == ptgn::Hash<SequenceScript>()) {
			if (!script.instance && registration) {
				script_runtime::AttachEntry(entity, script);
			}

			auto* sequence_script{
				dynamic_cast<SequenceScript*>(script.instance)
			};
			if (!sequence_script) {
				continue;
			}

			sequence_script->sequence.enabled = script.enabled;
			if (DrawSequence(ui, entity, sequence_script->sequence)) {
				remove = i;
			}
			script.enabled = sequence_script->sequence.enabled;
			script.value = *sequence_script;
			continue;
		}

		ImGui::PushID(&script);
		bool open{ false };
		const float button_size{ ImGui::GetFrameHeight() };

		if (ImGui::BeginTable(
				"ScriptRow", 2, ImGuiTableFlags_SizingStretchProp
			)) {
			ImGui::TableSetupColumn("Script", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(
				"Controls",
				ImGuiTableColumnFlags_WidthFixed,
				EnabledDeleteControlsWidth()
			);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);
			ImGui::TableSetColumnIndex(0);

			const ImVec4 header{
				script.enabled
					? ImVec4{ 0.20f, 0.34f, 0.33f, 1.0f }
					: ImVec4{ 0.25f, 0.25f, 0.25f, 1.0f }
			};
			const ImVec4 header_hovered{
				script.enabled
					? ImVec4{ 0.26f, 0.43f, 0.41f, 1.0f }
					: ImVec4{ 0.30f, 0.30f, 0.30f, 1.0f }
			};
			const ImVec4 header_active{
				script.enabled
					? ImVec4{ 0.31f, 0.49f, 0.47f, 1.0f }
					: ImVec4{ 0.34f, 0.34f, 0.34f, 1.0f }
			};

			ImGui::PushStyleColor(ImGuiCol_Header, header);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, header_hovered);
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, header_active);

			const bool has_contents{ editor && editor->has_contents };
			ImGuiTreeNodeFlags flags{
				ImGuiTreeNodeFlags_Framed |
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_NoTreePushOnOpen
			};
			if (has_contents) {
				flags |= ImGuiTreeNodeFlags_DefaultOpen;
			} else {
				flags |= ImGuiTreeNodeFlags_Leaf;
			}

			open = ImGui::TreeNodeEx(
				"##Script",
				flags,
				"%s",
				editor ? editor->options.label.c_str() : script.type.c_str()
			);
			ImGui::PopStyleColor(3);

			if (editor) {
				DrawItemTooltip(editor->options.description.c_str());
			}

			ImGui::TableSetColumnIndex(1);
			if (DrawEnabledDeleteControls(
					script.enabled,
					"Enable or disable this script.",
					"Remove this script."
				)) {
				remove = i;
			}
			ImGui::EndTable();
		}

		if (open && editor && editor->has_contents && editor->draw(script.value)) {
			if (!script.instance) {
				script_runtime::AttachEntry(entity, script);
			} else if (registration && registration->apply) {
				registration->apply(*script.instance, script.value);
			}
		}

		ImGui::PopID();
	}

	if (remove >= 0) {
		auto& entry{ scripts.scripts[static_cast<std::size_t>(remove)] };
		entry.enabled = false;
		if (auto* sequence{ dynamic_cast<SequenceScript*>(entry.instance) }) {
			sequence->sequence.enabled = false;
		}
		scripts.scripts.erase(scripts.scripts.begin() + remove);
	}
}

void PromoteToShared(DemoEditorDrawContext& ui, ScriptSequence& binding) {
	if (binding.shared_reference) {
		return;
	}
	ScriptSequence shared{ binding };
	shared.shared_reference = false;
	shared.shared_sequence_id = 0;
	shared.runtime = ScriptSequenceRuntime{};
	const SequenceId shared_id{ shared.id };
	ui.context.host.GetSharedSequences().sequences.push_back(std::move(shared));
	binding.shared_reference = true;
	binding.shared_sequence_id = shared_id;
	binding.runtime = ScriptSequenceRuntime{};
}

void DetachToLocal(DemoEditorDrawContext& ui, ScriptSequence& binding) {
	if (!binding.shared_reference) {
		return;
	}
	const auto* shared{ ui.context.host.GetSharedSequences().Find(binding.shared_sequence_id) };
	if (!shared) {
		binding.shared_reference = false;
		binding.shared_sequence_id = 0;
		return;
	}
	ScriptSequence local{ *shared };
	const SequenceId binding_id{ binding.id };
	const bool enabled{ binding.enabled };
	binding = std::move(local);
	binding.id = binding_id;
	binding.enabled = enabled;
	binding.shared_reference = false;
	binding.shared_sequence_id = 0;
	binding.runtime = ScriptSequenceRuntime{};
}

void DrawRuntimeButtons(DemoEditorDrawContext& ui, ptgn::Entity owner, ScriptSequence& binding) {
	if (!ImGui::BeginTable("RuntimeButtons", 3, ImGuiTableFlags_SizingStretchSame)) {
		return;
	}
	ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
	ImGui::TableSetColumnIndex(0);
	if (ImGui::Button(binding.runtime.running ? "Restart" : "Start", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
		ui.context.host.Start(owner, binding, true);
	}
	ImGui::TableSetColumnIndex(1);
	ImGui::BeginDisabled(!binding.runtime.running);
	if (ImGui::Button(binding.runtime.paused ? "Resume" : "Pause", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
		ui.context.host.SetPaused(owner, binding, !binding.runtime.paused);
	}
	ImGui::EndDisabled();
	ImGui::TableSetColumnIndex(2);
	if (ImGui::Button("Stop", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
		ui.context.host.Stop(owner, binding);
	}
	ImGui::EndTable();
}

bool DrawSequence(DemoEditorDrawContext& ui, ptgn::Entity owner, ScriptSequence& binding) {
	ScriptSequence* sequence{ ui.context.host.Resolve(owner, binding) };
	if (!sequence) {
		ImGui::TextDisabled("Missing shared Script Sequence");
		return false;
	}

	bool remove{ false };
	ImGui::PushID(static_cast<int>(binding.id));
	const float button_size{ ImGui::GetFrameHeight() };
	const int runtime_columns{ ui.state.show_runtime_controls ? 3 : 0 };
	const int column_count{ 3 + runtime_columns };
	const float available_width{ ImGui::GetContentRegionAvail().x };
	const float sequence_width{ std::max(
		1.0f, (available_width - CompactControlSpacing()) * 0.5f
	) };
	bool open{ ui.state.sequence_open_states.try_emplace(binding.id, true).first->second };
	ImVec2 name_input_min{};
	ImVec2 name_input_max{};
	bool name_input_drawn{ false };
	bool name_input_hovered{ false };
	bool began_name_edit_this_frame{ false };

	if (ImGui::BeginTable(
			"ScriptSequenceHeader", column_count,
			ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn(
			"Sequence", ImGuiTableColumnFlags_WidthFixed, sequence_width
		);
		ImGui::TableSetupColumn(
			"Options", ImGuiTableColumnFlags_WidthStretch
		);
		if (ui.state.show_runtime_controls) {
			ImGui::TableSetupColumn(
				"Play", ImGuiTableColumnFlags_WidthFixed, button_size
			);
			ImGui::TableSetupColumn(
				"Pause", ImGuiTableColumnFlags_WidthFixed, button_size
			);
			ImGui::TableSetupColumn(
				"Stop", ImGuiTableColumnFlags_WidthFixed, button_size
			);
		}
		ImGui::TableSetupColumn(
			"Controls", ImGuiTableColumnFlags_WidthFixed,
			EnabledDeleteControlsWidth()
		);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);

		ImGui::TableSetColumnIndex(0);
		auto& stored_open{ ui.state.sequence_open_states[binding.id] };
		ImGui::SetNextItemOpen(stored_open, ImGuiCond_Always);
		const bool editing_before_draw{
			ui.state.editing_sequence_name == binding.id
		};
		const ImVec4 header{
			binding.enabled
				? ImVec4{ 0.35f, 0.24f, 0.39f, 1.0f }
				: ImVec4{ 0.25f, 0.25f, 0.25f, 1.0f }
		};
		const ImVec4 header_hovered{
			binding.enabled
				? ImVec4{ 0.44f, 0.31f, 0.48f, 1.0f }
				: ImVec4{ 0.30f, 0.30f, 0.30f, 1.0f }
		};
		const ImVec4 header_active{
			binding.enabled
				? ImVec4{ 0.50f, 0.36f, 0.55f, 1.0f }
				: ImVec4{ 0.34f, 0.34f, 0.34f, 1.0f }
		};
		ImGui::PushStyleColor(ImGuiCol_Header, header);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, header_hovered);
		ImGui::PushStyleColor(
			ImGuiCol_HeaderActive,
			editing_before_draw ? header : header_active
		);
		open = ImGui::TreeNodeEx(
			"##ScriptSequence",
			ImGuiTreeNodeFlags_OpenOnArrow |
				ImGuiTreeNodeFlags_Framed |
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_AllowOverlap,
			"%s", editing_before_draw ? "" : sequence->name.c_str()
		);
		ImGui::PopStyleColor(3);
		stored_open = open;
		const ImVec2 tree_min{ ImGui::GetItemRectMin() };
		const ImVec2 tree_max{ ImGui::GetItemRectMax() };
		const bool tree_hovered{ ImGui::IsItemHovered() };
		const ImVec2 mouse{ ImGui::GetMousePos() };
		const float text_start_x{
			tree_min.x + ImGui::GetFrameHeight()
		};
		const float minimum_name_width{ 48.0f };
		const float visible_name_width{ std::max(
			minimum_name_width,
			ImGui::CalcTextSize(sequence->name.c_str()).x
		) };
		const float name_hit_end_x{ std::min(
			tree_max.x, text_start_x + visible_name_width
		) };
		const bool name_hit_hovered{
			tree_hovered && mouse.x >= text_start_x &&
			mouse.x <= name_hit_end_x
		};
		const bool tree_clicked_left{
			ImGui::IsItemClicked(ImGuiMouseButton_Left)
		};

		bool begin_edit{
			!editing_before_draw && name_hit_hovered &&
			ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
		};
		if (ImGui::BeginPopupContextItem("SequenceNameContext")) {
			if (ImGui::MenuItem("Rename")) {
				begin_edit = true;
			}
			ImGui::EndPopup();
		}
		if (begin_edit) {
			ui.state.editing_sequence_name = binding.id;
			ui.state.editing_sequence_original_name = sequence->name;
			began_name_edit_this_frame = true;
		}

		if (!editing_before_draw && !begin_edit &&
			tree_clicked_left && mouse.x > name_hit_end_x) {
			open = !open;
			stored_open = open;
		}

		if (ui.state.editing_sequence_name == binding.id) {
			ImGui::SetCursorScreenPos(
				ImVec2{ text_start_x, tree_min.y }
			);
			ImGui::SetNextItemWidth(std::max(
				minimum_name_width,
				tree_max.x - text_start_x -
					ImGui::GetStyle().FramePadding.x
			));
			if (begin_edit) {
				ImGui::SetKeyboardFocusHere();
			}
			const bool submitted{ ImGui::InputText(
				"##SequenceName", &sequence->name,
				ImGuiInputTextFlags_EnterReturnsTrue
			) };
			name_input_min = ImGui::GetItemRectMin();
			name_input_max = ImGui::GetItemRectMax();
			name_input_drawn = true;
			name_input_hovered =
				ImGui::IsItemHovered() || ImGui::IsItemActive();
			if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				sequence->name = ui.state.editing_sequence_original_name;
				ui.state.editing_sequence_name.reset();
			} else if (submitted) {
				ui.state.editing_sequence_name.reset();
			}
		}
		if (tree_hovered && ui.state.editing_sequence_name != binding.id) {
			ImGui::SetTooltip("Double-click name to rename.");
		}

		ImGui::TableSetColumnIndex(1);
		std::vector<std::string> selected_sequence_options;
		if (binding.shared_reference) {
			selected_sequence_options.emplace_back("Global sequence");
		}
		if (sequence->remove_binding_on_complete) {
			selected_sequence_options.emplace_back("Remove binding on complete");
		}
		if (sequence->destroy_owner_on_complete) {
			selected_sequence_options.emplace_back("Destroy owner on complete");
		}
		if (sequence->channel) {
			selected_sequence_options.emplace_back("Channel: " + sequence->channel->value);
		}
		switch (sequence->reentry) {
			case ReentryMode::IgnoreWhileRunning:
				selected_sequence_options.emplace_back("Ignore on retrigger");
				break;
			case ReentryMode::Restart:
				selected_sequence_options.emplace_back("Restart on retrigger");
				break;
			case ReentryMode::Queue:
				selected_sequence_options.emplace_back("Queue on retrigger");
				break;
		}
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo("##SequenceOptions", "Options")) {
			const bool global{ binding.shared_reference };
			if (ImGui::MenuItem(
					"Global sequence", nullptr, global
				)) {
				if (global) {
					DetachToLocal(ui, binding);
				} else {
					PromoteToShared(ui, binding);
				}
				sequence = ui.context.host.Resolve(owner, binding);
			}
			if (sequence && ImGui::MenuItem(
					"Remove binding on complete", nullptr,
					sequence->remove_binding_on_complete
				)) {
				sequence->remove_binding_on_complete =
					!sequence->remove_binding_on_complete;
			}
			if (sequence && ImGui::MenuItem(
					"Destroy owner on complete", nullptr,
					sequence->destroy_owner_on_complete
				)) {
				sequence->destroy_owner_on_complete =
					!sequence->destroy_owner_on_complete;
			}
			if (sequence) {
				const bool has_channel{ sequence->channel.has_value() };
				if (ImGui::MenuItem("Use sequence channel", nullptr, has_channel)) {
					if (has_channel) {
						sequence->channel.reset();
					} else {
						sequence->channel = SequenceChannelKey{ "default" };
					}
				}
			}
			ImGui::Separator();
			if (sequence && ImGui::MenuItem(
					"Ignore on retrigger", nullptr,
					sequence->reentry ==
						ReentryMode::IgnoreWhileRunning
				)) {
				sequence->reentry =
					ReentryMode::IgnoreWhileRunning;
			}
			if (sequence && ImGui::MenuItem(
					"Restart on retrigger", nullptr,
					sequence->reentry == ReentryMode::Restart
				)) {
				sequence->reentry = ReentryMode::Restart;
			}
			if (sequence && ImGui::MenuItem(
					"Queue on retrigger", nullptr,
					sequence->reentry == ReentryMode::Queue
				)) {
				sequence->reentry = ReentryMode::Queue;
			}
			ImGui::EndCombo();
		}
		DrawSelectedItemsTooltip(selected_sequence_options);

		int column{ 2 };
		if (ui.state.show_runtime_controls) {
			ImGui::TableSetColumnIndex(column++);
			if (DrawCenteredTextButton(
					"##Play", ">", ImVec2{ button_size, button_size }
				)) {
				ui.context.host.Start(owner, binding, true);
			}
			DrawItemTooltip(
				binding.runtime.running
					? "Restart this sequence."
					: "Start this sequence."
			);

			ImGui::TableSetColumnIndex(column++);
			ImGui::BeginDisabled(!binding.runtime.running);
			if (DrawCenteredTextButton(
					"##Pause", "||", ImVec2{ button_size, button_size }
				)) {
				ui.context.host.SetPaused(
					owner, binding, !binding.runtime.paused
				);
			}
			ImGui::EndDisabled();
			DrawItemTooltip(
				binding.runtime.paused
					? "Resume this sequence."
					: "Pause this sequence."
			);

			ImGui::TableSetColumnIndex(column++);
			ImGui::BeginDisabled(!binding.runtime.running);
			if (DrawCenteredTextButton(
					"##Stop", "[]", ImVec2{ button_size, button_size }
				)) {
				ui.context.host.Stop(owner, binding);
			}
			ImGui::EndDisabled();
			DrawItemTooltip("Stop this sequence.");
		}

		ImGui::TableSetColumnIndex(column);
		remove = DrawEnabledDeleteControls(
			binding.enabled,
			"Enable or disable this script sequence.",
			"Delete this script sequence."
		);
		ImGui::EndTable();
	}

	if (ui.state.editing_sequence_name == binding.id && name_input_drawn &&
		!began_name_edit_this_frame) {
		const bool clicked{
			ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
			ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
			ImGui::IsMouseClicked(ImGuiMouseButton_Right)
		};
		const ImVec2 mouse{ ImGui::GetMousePos() };
		const bool inside_input{
			mouse.x >= name_input_min.x &&
			mouse.x <= name_input_max.x &&
			mouse.y >= name_input_min.y &&
			mouse.y <= name_input_max.y
		};
		if (clicked && !inside_input && !name_input_hovered) {
			ui.state.editing_sequence_name.reset();
		}
	}

	if (open && !remove) {
		sequence = ui.context.host.Resolve(owner, binding);
		if (sequence) {
			if (sequence->channel &&
				ImGui::BeginTable("SequenceChannelRow", 2, ImGuiTableFlags_SizingStretchProp)) {
				const float label_width{
					ImGui::CalcTextSize("Channel:").x +
					ImGui::GetStyle().ItemInnerSpacing.x
				};
				ImGui::TableSetupColumn(
					"Label", ImGuiTableColumnFlags_WidthFixed, label_width
				);
				ImGui::TableSetupColumn(
					"Value", ImGuiTableColumnFlags_WidthStretch
				);
				ImGui::TableNextRow(
					ImGuiTableRowFlags_None, ImGui::GetFrameHeight()
				);
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("Channel:");
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##SequenceChannel", &sequence->channel->value);
				DrawItemTooltip(
					"Only one binding owns a channel at a time. Restart replaces it; Queue waits."
				);
				ImGui::EndTable();
			}
			DrawEvents(ui, owner, *sequence);

			bool add_action_requested{ false };
			const bool sequence_open{
				DrawUnframedSectionHeader(
					"SequenceSection", "Sequence", true,
					sequence->actions.empty(),
					"Ordered actions executed by this script sequence.",
					true, "Add an action to this sequence.",
					add_action_requested
				)
			};
			ImGui::PushID("SequenceSection");
			if (add_action_requested) {
				ImGui::OpenPopup("AddSequenceAction");
			}
			if (ImGui::BeginPopup("AddSequenceAction")) {
				if (ImGui::MenuItem("Action")) {
					sequence->actions.push_back(
						ActionRegistry::Make<SetVisibleAction>()
					);
				}
				if (ImGui::MenuItem("Tween")) {
					sequence->actions.push_back(
						ActionRegistry::Make<MoveToAction>()
					);
				}
				if (ImGui::MenuItem("Delay")) {
					sequence->actions.push_back(
						ActionRegistry::Make<WaitAction>()
					);
				}
				if (ImGui::MenuItem("Emit Signal")) {
					sequence->actions.push_back(
						ActionRegistry::Make<EmitSignalAction>()
					);
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();
			if (sequence_open) {
				DrawActions(ui, *sequence, binding);
			}
		}
	}

	ImGui::PopID();
	return remove;
}

void DrawEvents(DemoEditorDrawContext& ui, ptgn::Entity owner, ScriptSequence& sequence) {
	bool add_requested{ false };
	const bool empty{
		sequence.start_events.empty() && sequence.stop_events.empty() &&
		sequence.lifecycle_actions.empty()
	};
	const bool open{ DrawUnframedSectionHeader(
		"EventsSection", "Events", true, empty,
		"Start and stop triggers plus lifecycle callbacks.",
		true, "Add a trigger or lifecycle callback.", add_requested
	) };

	ImGui::PushID("EventsSection");
	if (add_requested) {
		ImGui::OpenPopup("AddEventEntry");
	}
	if (ImGui::BeginPopup("AddEventEntry")) {
		if (ImGui::BeginMenu("Trigger")) {
			auto candidate_available = [&](const EventEditorRegistration& candidate) {
				const auto* registration{
					SequenceEventRegistry::Find(candidate.type_hash)
				};
				return registration &&
					(!registration->available || registration->available(owner));
			};
			auto add_candidate = [&](const EventEditorRegistration& candidate) {
				const auto* registration{
					SequenceEventRegistry::Find(candidate.type_hash)
				};
				if (!candidate_available(candidate)) {
					return;
				}

				if (ImGui::MenuItem(candidate.options.label.c_str())) {
					EventCondition event{
						.enabled = true,
						.type_hash = registration->type_hash,
						.type = registration->key,
					};
					registration->set_defaults(event);
					sequence.start_events.push_back(std::move(event));
				}
				DrawItemTooltip(candidate.options.description.c_str());
			};

			for (const auto& candidate : editor::EventEditorRegistry::Entries()) {
				if (candidate.options.group.empty()) {
					add_candidate(candidate);
				}
			}
			std::vector<std::string> groups;
			for (const auto& candidate : editor::EventEditorRegistry::Entries()) {
				if (!candidate.options.group.empty() &&
					candidate_available(candidate) &&
					!std::ranges::contains(groups, candidate.options.group)) {
					groups.push_back(candidate.options.group);
				}
			}
			for (const auto& group : groups) {
				if (!ImGui::BeginMenu(group.c_str())) {
					continue;
				}
				for (const auto& candidate : editor::EventEditorRegistry::Entries()) {
					if (candidate.options.group == group) {
						add_candidate(candidate);
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Lifecycle callback")) {
			for (int i{ 0 }; i < static_cast<int>(kLifecycleLabels.size()); ++i) {
				if (ImGui::MenuItem(kLifecycleLabels[static_cast<std::size_t>(i)])) {
					sequence.lifecycle_actions.push_back(LifecycleAction{
						.enabled = true,
						.lifecycle = static_cast<SequenceLifecycle>(i),
						.action = ActionRegistry::Make<EmitSignalAction>(),
					});
				}
			}
			ImGui::EndMenu();
		}
		DrawItemTooltip("Add a lifecycle callback.");
		ImGui::EndPopup();
	}
	ImGui::PopID();

	if (!open) {
		return;
	}

	int remove_start{ -1 };
	int move_start_to_stop{ -1 };
	for (int i{ 0 }; i < static_cast<int>(sequence.start_events.size()); ++i) {
		bool switch_kind{ false };
		if (DrawEvent(
				ui, owner, sequence.start_events[static_cast<std::size_t>(i)], false, switch_kind
			)) {
			remove_start = i;
		}
		if (switch_kind) {
			move_start_to_stop = i;
		}
	}

	int remove_stop{ -1 };
	int move_stop_to_start{ -1 };
	for (int i{ 0 }; i < static_cast<int>(sequence.stop_events.size()); ++i) {
		bool switch_kind{ false };
		if (DrawEvent(
				ui, owner, sequence.stop_events[static_cast<std::size_t>(i)], true, switch_kind
			)) {
			remove_stop = i;
		}
		if (switch_kind) {
			move_stop_to_start = i;
		}
	}

	std::optional<EventCondition> moved_to_stop;
	std::optional<EventCondition> moved_to_start;
	if (remove_start < 0 && move_start_to_stop >= 0) {
		moved_to_stop.emplace(std::move(
			sequence.start_events[static_cast<std::size_t>(move_start_to_stop)]
		));
	}
	if (remove_stop < 0 && move_stop_to_start >= 0) {
		moved_to_start.emplace(std::move(
			sequence.stop_events[static_cast<std::size_t>(move_stop_to_start)]
		));
	}

	if (remove_start >= 0) {
		sequence.start_events.erase(sequence.start_events.begin() + remove_start);
	} else if (move_start_to_stop >= 0) {
		sequence.start_events.erase(
			sequence.start_events.begin() + move_start_to_stop
		);
	}
	if (remove_stop >= 0) {
		sequence.stop_events.erase(sequence.stop_events.begin() + remove_stop);
	} else if (move_stop_to_start >= 0) {
		sequence.stop_events.erase(sequence.stop_events.begin() + move_stop_to_start);
	}
	if (moved_to_stop) {
		sequence.stop_events.push_back(std::move(*moved_to_stop));
	}
	if (moved_to_start) {
		sequence.start_events.push_back(std::move(*moved_to_start));
	}

	DrawLifecycleRows(ui, sequence);
}

bool DrawEvent(
	DemoEditorDrawContext& ui,
	ptgn::Entity owner,
	EventCondition& event,
	bool stop_event,
	bool& switch_kind
) {
	(void)ui;
	bool remove{ false };
	ImGui::PushID(&event);

	const float kind_width{
		std::max(
			ImGui::CalcTextSize("Start").x,
			ImGui::CalcTextSize("Stop").x
		) + ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const float consume_width{
		ImGui::CalcTextSize("Consume").x + ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const EventEditorRegistration* selected{
		editor::EventEditorRegistry::Find(event.type_hash)
	};
	bool event_type_changed{ false };

	auto inline_field_count = [](const EventEditorRegistration* registration) {
		return registration ? registration->inline_fields : 0;
	};

	if (ImGui::BeginTable(
			"EventRow", 4,
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
		)) {
		ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, kind_width);
		ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Consume", ImGuiTableColumnFlags_WidthFixed, consume_width);
		ImGui::TableSetupColumn(
			"Controls", ImGuiTableColumnFlags_WidthFixed,
			EnabledDeleteControlsWidth()
		);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

		int column{};
		ImGui::TableSetColumnIndex(column++);
		ImGui::BeginDisabled(!event.enabled);
		if (ImGui::Button(
				stop_event ? "Stop" : "Start",
				ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			switch_kind = true;
		}
		DrawItemTooltip(
			stop_event ? "Change this to a start event." : "Change this to a stop event."
		);
		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(column++);
		ImGui::BeginDisabled(!event.enabled);

		const int initial_inline_fields{ inline_field_count(selected) };
		const float available_width{ ImGui::GetContentRegionAvail().x };
		const float spacing{ ImGui::GetStyle().ItemSpacing.x };
		float event_width{ available_width };
		if (initial_inline_fields > 0) {
			event_width = std::clamp(available_width * 0.4f, 110.0f, 190.0f);
			const float minimum_fields_width{ 80.0f * initial_inline_fields };
			if (available_width - event_width - spacing * initial_inline_fields <
				minimum_fields_width) {
				event_width = std::max(
					90.0f,
					available_width - spacing * initial_inline_fields - minimum_fields_width
				);
			}
		}

		ImGui::SetNextItemWidth(std::max(1.0f, event_width));
		if (ImGui::BeginCombo(
				"##Event",
				selected ? selected->options.label.c_str() : "Missing Event"
			)) {
			auto candidate_available = [&](const EventEditorRegistration& candidate) {
				const auto* registration{
					SequenceEventRegistry::Find(candidate.type_hash)
				};
				return registration &&
					(!registration->available || registration->available(owner));
			};
			auto select_candidate = [&](const EventEditorRegistration& candidate) {
				const auto* registration{
					SequenceEventRegistry::Find(candidate.type_hash)
				};
				if (!candidate_available(candidate)) {
					return;
				}

				if (ImGui::MenuItem(
						candidate.options.label.c_str(),
						nullptr,
						candidate.type_hash == event.type_hash
					)) {
					event.type_hash = candidate.type_hash;
					event.type = candidate.key;
					registration->set_defaults(event);
					selected = editor::EventEditorRegistry::Find(event.type_hash);
					event_type_changed = true;
				}
				DrawItemTooltip(candidate.options.description.c_str());
			};

			for (const auto& candidate : editor::EventEditorRegistry::Entries()) {
				if (candidate.options.group.empty()) {
					select_candidate(candidate);
				}
			}

			std::vector<std::string> groups;
			for (const auto& candidate : editor::EventEditorRegistry::Entries()) {
				if (!candidate.options.group.empty() &&
					candidate_available(candidate) &&
					!std::ranges::contains(groups, candidate.options.group)) {
					groups.push_back(candidate.options.group);
				}
			}
			for (const auto& group : groups) {
				if (!ImGui::BeginMenu(group.c_str())) {
					continue;
				}
				for (const auto& candidate : editor::EventEditorRegistry::Entries()) {
					if (candidate.options.group == group) {
						select_candidate(candidate);
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndCombo();
		}

		if (selected && !event_type_changed &&
			selected->inline_fields > 0 && selected->draw) {
			ImGui::SameLine();
			(void)selected->draw(event.value);
		}
		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(column++);
		ImGui::BeginDisabled(!event.enabled);
		DrawToggleButton(
			"Consume",
			event.consume,
			ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() },
			"Stop propagation after this event matches."
		);
		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(column);
		remove = DrawEnabledDeleteControls(
			event.enabled,
			"Enable or disable this event.",
			"Remove this event."
		);
		ImGui::EndTable();
	}

	ImGui::PopID();
	return remove;
}

void DrawActionPicker(DemoEditorDrawContext& ui, Action& action, bool timed_only, float width) {
	const auto* current{ editor::ActionEditorRegistry::Find(action.type) };
	ImGui::SetNextItemWidth(width);
	const bool open{ ImGui::BeginCombo(
		"##RegisteredAction", current ? current->options.label.c_str() : "Missing Action"
	) };
	DrawItemTooltip(
		current ? current->options.description.c_str()
			: "Choose a registered Action for this sequence entry."
	);
	if (!open) {
		return;
	}

	auto is_available = [&](const ActionEditorRegistration& candidate) {
		const auto* registration{ ActionRegistry::Find(candidate.key) };
		return registration && registration->serializable &&
			candidate.key != ActionRegistry::Key<WaitAction>() &&
			(!timed_only || registration->supports_timing) &&
			(timed_only || !registration->requires_timing);
	};
	auto select_candidate = [&](const ActionEditorRegistration& candidate) {
		const auto* registration{ ActionRegistry::Find(candidate.key) };
		if (!registration) {
			return;
		}
		if (ImGui::MenuItem(candidate.options.label.c_str(), nullptr, candidate.key == action.type)) {
			const bool enabled{ action.enabled };
			action = ActionRegistry::Make(std::string_view{ candidate.key });
			action.enabled = enabled;
			if (timed_only) {
				action.completion = ActionCompletion::Duration;
				action.timing = registration->default_timing.value_or(ActionTiming{});
			} else {
				action.completion.reset();
				action.timing.reset();
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s\n%s", candidate.options.description.c_str(), candidate.key.c_str());
		}
	};

	for (const auto& candidate : editor::ActionEditorRegistry::Entries()) {
		if (is_available(candidate) && candidate.options.group.empty()) {
			select_candidate(candidate);
		}
	}
	std::vector<std::string> groups;
	for (const auto& candidate : editor::ActionEditorRegistry::Entries()) {
		if (is_available(candidate) && !candidate.options.group.empty() &&
			!std::ranges::contains(groups, candidate.options.group)) {
			groups.push_back(candidate.options.group);
		}
	}
	for (const auto& group : groups) {
		if (!ImGui::BeginMenu(group.c_str())) {
			continue;
		}
		std::vector<const ActionEditorRegistration*> candidates;
		for (const auto& candidate : editor::ActionEditorRegistry::Entries()) {
			if (is_available(candidate) && candidate.options.group == group) {
				candidates.push_back(&candidate);
			}
		}
		std::ranges::sort(candidates, {}, [](const auto* candidate) {
			return candidate->options.menu_order;
		});
		for (std::size_t i{ 0 }; i < candidates.size(); ++i) {
			const auto* candidate{ candidates[i] };
			select_candidate(*candidate);
			if (candidate->options.separator_after &&
				i + 1 < candidates.size()) {
				ImGui::Separator();
			}
		}
		ImGui::EndMenu();
	}
	ImGui::EndCombo();
}

void DrawActionPickerWithInline(DemoEditorDrawContext& ui, Action& action, bool timed_only) {
	EnsureActionValue(action);
	const auto* editor{ editor::ActionEditorRegistry::Find(action.type) };
	const bool has_inline_editor{
		!timed_only && editor && static_cast<bool>(editor->draw_inline)
	};
	if (!has_inline_editor) {
		DrawActionPicker(ui, action, timed_only);
		return;
	}

	const float available{ ImGui::GetContentRegionAvail().x };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float picker_width{
		std::min(150.0f, std::max(110.0f, available * 0.32f))
	};
	DrawActionPicker(ui, action, timed_only, picker_width);

	editor = editor::ActionEditorRegistry::Find(action.type);
	if (editor && editor->draw_inline) {
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (editor->draw_inline(action.value, ui.context)) {
			action.runtime_factory = {};
		}
	}
}

void DrawEmitSignalCompact(DemoEditorDrawContext& ui, EmitSignalAction& emit) {
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##EmitSignal", "Signal name", &emit.signal.value);
	DrawItemTooltip("Broadcast Signal name.");
}

void DrawTimingOptions(
	DemoEditorDrawContext& ui, Action& action, ActionTiming& timing, float left_screen_x
) {
	(void)ui;
	EnsureActionValue(action);

	const float right_screen_x{
		ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x
	};
	const float width{ std::max(1.0f, right_screen_x - left_screen_x) };
	ImGui::SetCursorScreenPos(
		ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y }
	);

	std::optional<MoveToAction> move;
	std::optional<RotateToAction> rotate;
	std::optional<ScaleToAction> scale;

	if (action.type_hash == ptgn::Hash<MoveToAction>()) {
		MoveToAction value{};
		if (TryReadJson(action.value, value)) {
			move = std::move(value);
		}
	} else if (action.type_hash == ptgn::Hash<RotateToAction>()) {
		RotateToAction value{};
		if (TryReadJson(action.value, value)) {
			rotate = std::move(value);
		}
	} else if (action.type_hash == ptgn::Hash<ScaleToAction>()) {
		ScaleToAction value{};
		if (TryReadJson(action.value, value)) {
			scale = std::move(value);
		}
	}

	const int columns{ move || scale ? 5 : (rotate ? 4 : 2) };
	if (!ImGui::BeginTable(
			"TweenOptions", columns, ImGuiTableFlags_SizingStretchProp,
			ImVec2{ width, 0.0f }
		)) {
		return;
	}

	if (move || scale) {
		ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn(
			"Mode", ImGuiTableColumnFlags_WidthFixed, 82.0f
		);
	} else if (rotate) {
		ImGui::TableSetupColumn(
			"Degrees", ImGuiTableColumnFlags_WidthStretch, 1.0f
		);
		ImGui::TableSetupColumn(
			"Shortest", ImGuiTableColumnFlags_WidthFixed, 82.0f
		);
	}
	ImGui::TableSetupColumn("Ease", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableSetupColumn("Options", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

	int column{};
	if (move) {
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat(
			"##TweenX", &move->destination.x, 1.0f,
			-100000.0f, 100000.0f, "X: %.0f"
		);
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat(
			"##TweenY", &move->destination.y, 1.0f,
			-100000.0f, 100000.0f, "Y: %.0f"
		);
		ImGui::TableSetColumnIndex(column++);
		if (ImGui::Button(
				move->relative ? "Relative" : "Absolute",
				ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			move->relative = !move->relative;
		}
		DrawItemTooltip(
			move->relative
				? "Offset from the entity's current position."
				: "Use an absolute world position."
		);
	} else if (scale) {
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat(
			"##TweenScaleX", &scale->scale.x, 0.01f,
			-100.0f, 100.0f, "X: %.2f"
		);
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat(
			"##TweenScaleY", &scale->scale.y, 0.01f,
			-100.0f, 100.0f, "Y: %.2f"
		);
		ImGui::TableSetColumnIndex(column++);
		if (ImGui::Button(
				scale->relative ? "Relative" : "Absolute",
				ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			scale->relative = !scale->relative;
		}
		DrawItemTooltip(
			scale->relative
				? "Multiply the entity's current scale."
				: "Use an absolute scale."
		);
	} else if (rotate) {
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat(
			"##TweenDegrees", &rotate->degrees, 1.0f,
			-3600.0f, 3600.0f, "%.1f deg"
		);
		ImGui::TableSetColumnIndex(column++);
		DrawToggleButton(
			"Shortest", rotate->shortest_path,
			ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() },
			"Toggle the shortest rotational path."
		);
	}

	ImGui::TableSetColumnIndex(column++);
	const char* ease_preview{ "Linear" };
	for (const auto& [ease, label] : kEaseEntries) {
		if (timing.ease == ease) {
			ease_preview = label;
		}
	}
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::BeginCombo("##Ease", ease_preview)) {
		for (const auto& [ease, label] : kEaseEntries) {
			if (ImGui::Selectable(label, timing.ease == ease)) {
				timing.ease = ease;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::TableSetColumnIndex(column);
	std::vector<std::string> selected_options;
	if (timing.infinite_repeats) selected_options.emplace_back("Infinite");
	if (timing.reversed) selected_options.emplace_back("Reversed");
	if (timing.yoyo) selected_options.emplace_back("Yoyo");
	std::string options;
	for (const auto& option : selected_options) {
		if (!options.empty()) {
			options += ", ";
		}
		options += option;
	}
	if (options.empty()) {
		options = "Options";
	}
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::BeginCombo("##Options", options.c_str())) {
		ImGui::Checkbox("Infinite", &timing.infinite_repeats);
		DrawItemTooltip(
			"Repeat this Tween indefinitely. Duration still controls every cycle."
		);
		ImGui::Checkbox("Reversed", &timing.reversed);
		ImGui::Checkbox("Yoyo", &timing.yoyo);
		ImGui::EndCombo();
	}
	DrawSelectedItemsTooltip(selected_options);
	ImGui::EndTable();

	if (move) {
		action.value = *move;
		action.runtime_factory = {};
	} else if (rotate) {
		action.value = *rotate;
		action.runtime_factory = {};
	} else if (scale) {
		action.value = *scale;
		action.runtime_factory = {};
	}
}

void DrawActionParameters(DemoEditorDrawContext& ui, Action& action, float left_screen_x) {
	EnsureActionValue(action);
	const auto* action_editor{ editor::ActionEditorRegistry::Find(action.type) };
	if (!action_editor || action.type == ActionRegistry::Key<WaitAction>() ||
		action.type == ActionRegistry::Key<EmitSignalAction>() ||
		action.type == ActionRegistry::Key<SetVisibleAction>() ||
		action.type == ActionRegistry::Key<MoveToAction>() ||
		action.type == ActionRegistry::Key<RotateToAction>() ||
		action.type == ActionRegistry::Key<ScaleToAction>() ||
		action.type == ActionRegistry::Key<RemoveComponentsAction>()) {
		return;
	}

	if (action.type_hash == ptgn::Hash<AddComponentsAction>()) {
		AddComponentsAction add_components;
		if (!TryReadJson(action.value, add_components) ||
			add_components.components.empty()) {
			return;
		}
	}

	const float right_screen_x{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
	ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });
	if (ImGui::BeginChild(
			"ActionParameters", ImVec2{ std::max(1.0f, right_screen_x - left_screen_x), 0.0f },
			ImGuiChildFlags_AutoResizeY
		)) {
		if (action_editor->draw(action.value, ui.context)) {
			action.runtime_factory = {};
		}
	}
	ImGui::EndChild();
}

bool DrawAction(
	DemoEditorDrawContext& ui, Action& action, int index, ScriptSequence* binding,
	bool& duplicate, int& move_from, int& move_to, bool lifecycle
) {
	bool remove{ false };
	ImGui::PushID(&action);
	const float drag_width{ lifecycle ? 0.0f : 28.0f };
	const float label_width{
		std::max({
			ImGui::CalcTextSize("Action").x,
			ImGui::CalcTextSize("Tween").x,
			ImGui::CalcTextSize("Delay").x
		})
	};
	const float type_width{
		label_width + ImGui::GetFrameHeight() +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const float duration_width{
		ImGui::CalcTextSize("5000ms").x +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const float repeats_width{ GetCountControlWidth("Repeats") };
	const float button_width{ ImGui::GetFrameHeight() };
	const float controls_width{
		EnabledDeleteControlsWidth() +
		(GetActionForm(action) == ActionForm::Tween
			? CompactControlSpacing() + repeats_width
			: 0.0f)
	};
	ActionForm displayed_form{ GetActionForm(action) };
	ActionForm requested_form{ displayed_form };
	bool form_changed{ false };
	float parameter_left_screen_x{ ImGui::GetCursorScreenPos().x };

	const int column_count{
		displayed_form == ActionForm::Tween
			? (lifecycle ? 4 : 5)
			: (lifecycle ? 3 : 4)
	};

	if (ImGui::BeginTable(
			"ActionRow", column_count, ImGuiTableFlags_SizingStretchProp
		)) {
		if (!lifecycle) {
			ImGui::TableSetupColumn(
				"Drag", ImGuiTableColumnFlags_WidthFixed, drag_width
			);
		}
		ImGui::TableSetupColumn(
			"Type", ImGuiTableColumnFlags_WidthFixed, type_width
		);
		if (displayed_form == ActionForm::Tween) {
			ImGui::TableSetupColumn(
				"Duration", ImGuiTableColumnFlags_WidthFixed, duration_width
			);
			ImGui::TableSetupColumn(
				"Action", ImGuiTableColumnFlags_WidthStretch
			);
		} else {
			ImGui::TableSetupColumn(
				"Value", ImGuiTableColumnFlags_WidthStretch
			);
		}
		ImGui::TableSetupColumn(
			"Controls", ImGuiTableColumnFlags_WidthFixed, controls_width
		);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, button_width);

		int column{};
		if (!lifecycle) {
			ImGui::TableSetColumnIndex(column++);
			const bool dimmed{ !action.enabled };
			if (dimmed) {
				ImGui::PushStyleVar(
					ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f
				);
			}
			ImGui::Button("::", ImVec2{ drag_width, button_width });
			if (dimmed) {
				ImGui::PopStyleVar();
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
				duplicate = true;
			}
			DrawItemTooltip(
				action.enabled
					? "Drag to reorder. Right-click to duplicate."
					: "Disabled action. Right-click to duplicate."
			);
			if (ImGui::BeginDragDropSource(
					ImGuiDragDropFlags_SourceAllowNullID
				)) {
				const ActionDragPayload payload{ index };
				ImGui::SetDragDropPayload(
					"PTGN_SCRIPT_ACTION", &payload, sizeof(payload)
				);
				ImGui::Text(
					"%d. %s", index + 1, ActionSummary(action).c_str()
				);
				ImGui::EndDragDropSource();
			}
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload{
						ImGui::AcceptDragDropPayload("PTGN_SCRIPT_ACTION")
					}) {
					const auto* drag{
						static_cast<const ActionDragPayload*>(payload->Data)
					};
					if (drag) {
						move_from = drag->index;
						move_to = index;
					}
				}
				ImGui::EndDragDropTarget();
			}
		}

		ImGui::TableSetColumnIndex(column++);
		parameter_left_screen_x = ImGui::GetCursorScreenPos().x;
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo(
				"##Form",
				kActionFormLabels[static_cast<std::size_t>(displayed_form)]
			)) {
			for (int i{ 0 }; i < static_cast<int>(kActionFormLabels.size()); ++i) {
				const auto candidate{ static_cast<ActionForm>(i) };
				if (lifecycle &&
					(candidate == ActionForm::Tween ||
						candidate == ActionForm::Delay)) {
					continue;
				}
				if (ImGui::Selectable(
						kActionFormLabels[static_cast<std::size_t>(i)],
						candidate == displayed_form
					)) {
					requested_form = candidate;
					form_changed = true;
				}
			}
			ImGui::EndCombo();
		}
		DrawItemTooltip("Choose an Action, Tween, or Delay.");

		if (displayed_form == ActionForm::Tween) {
			ImGui::TableSetColumnIndex(column++);
			DrawDurationInput(
				"##Duration", action.timing->duration_ms, -FLT_MIN,
				"Duration of each Tween cycle."
			);
			ImGui::TableSetColumnIndex(column++);
			DrawActionPicker(ui, action, true);
		} else {
			ImGui::TableSetColumnIndex(column++);
			switch (displayed_form) {
				case ActionForm::Action:
					DrawActionPickerWithInline(ui, action, false);
					break;
				case ActionForm::Delay:
					DrawDurationInput(
						"##Duration", action.timing->duration_ms, -FLT_MIN,
						"Delay before continuing."
					);
					break;
				case ActionForm::Tween:
					break;
			}
		}

		ImGui::TableSetColumnIndex(column);
		if (displayed_form == ActionForm::Tween) {
			DrawCountControl(
				"Repeats", action.timing->additional_repeats, 0, 100,
				action.timing->infinite_repeats,
				"Additional full-duration cycles."
			);
			SameLineControl();
		}
		remove = DrawEnabledDeleteControls(
			action.enabled,
			"Enable or disable this action.",
			"Delete this action."
		);
		ImGui::EndTable();
	}

	if (form_changed) {
		SetActionForm(action, requested_form);
		displayed_form = requested_form;
	}
	if (displayed_form == ActionForm::Tween && action.timing) {
		DrawTimingOptions(ui, action, *action.timing, parameter_left_screen_x);
	}
	if (displayed_form == ActionForm::Action ||
		displayed_form == ActionForm::Tween) {
		DrawActionParameters(ui, action, parameter_left_screen_x);
	}
	if (binding && binding->runtime.running &&
		binding->runtime.action_index == static_cast<std::size_t>(index)) {
		const auto& runtime_action{ binding->actions[binding->runtime.action_index] };
		const float progress{
			runtime_action.timing && runtime_action.timing->duration_ms > 0.0f
				? std::clamp(
					binding->runtime.elapsed_ms / runtime_action.timing->duration_ms,
					0.0f, 1.0f
				)
				: 0.0f
		};
		ImGui::ProgressBar(progress, ImVec2{ -FLT_MIN, 2.0f }, "");
	}
	ImGui::PopID();
	return remove;
}

void DrawActions(DemoEditorDrawContext& ui, ScriptSequence& sequence, ScriptSequence& binding) {
	int remove_index{ -1 };
	int duplicate_index{ -1 };
	int move_from{ -1 };
	int move_to{ -1 };
	for (int i{ 0 }; i < static_cast<int>(sequence.actions.size()); ++i) {
		bool duplicate{ false };
		if (DrawAction(
				ui, sequence.actions[static_cast<std::size_t>(i)], i, &binding,
				duplicate, move_from, move_to, false
			)) {
			remove_index = i;
		}
		if (duplicate) {
			duplicate_index = i;
		}
	}
	if (move_from >= 0 && move_to >= 0) {
		MoveAction(sequence.actions, move_from, move_to);
	}
	if (duplicate_index >= 0) {
		Action copy{ sequence.actions[static_cast<std::size_t>(duplicate_index)] };
		sequence.actions.insert(sequence.actions.begin() + duplicate_index + 1, std::move(copy));
	}
	if (remove_index >= 0) {
		sequence.actions.erase(sequence.actions.begin() + remove_index);
		binding.runtime = ScriptSequenceRuntime{};
	}
}

void DrawLifecycleRows(DemoEditorDrawContext& ui, ScriptSequence& sequence) {
	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(sequence.lifecycle_actions.size()); ++i) {
		auto& callback{ sequence.lifecycle_actions[static_cast<std::size_t>(i)] };
		ImGui::PushID(&callback);

		const float lifecycle_width{ 145.0f };
		const float button_width{ ImGui::GetFrameHeight() };
		if (ImGui::BeginTable(
				"LifecycleRow", 3, ImGuiTableFlags_SizingStretchProp
			)) {
			ImGui::TableSetupColumn(
				"Lifecycle", ImGuiTableColumnFlags_WidthFixed, lifecycle_width
			);
			ImGui::TableSetupColumn(
				"Action", ImGuiTableColumnFlags_WidthStretch
			);
			ImGui::TableSetupColumn(
				"Controls", ImGuiTableColumnFlags_WidthFixed,
				EnabledDeleteControlsWidth()
			);
			ImGui::TableNextRow(
				ImGuiTableRowFlags_None, ImGui::GetFrameHeight()
			);

			ImGui::TableSetColumnIndex(0);
			ImGui::BeginDisabled(!callback.enabled);
			int lifecycle{ static_cast<int>(callback.lifecycle) };
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::Combo(
					"##Lifecycle", &lifecycle, kLifecycleLabels.data(),
					static_cast<int>(kLifecycleLabels.size())
				)) {
				callback.lifecycle = static_cast<SequenceLifecycle>(lifecycle);
			}
			DrawItemTooltip("Choose when this callback runs.");
			ImGui::EndDisabled();

			ImGui::TableSetColumnIndex(1);
			ImGui::BeginDisabled(!callback.enabled);
			DrawActionPickerWithInline(ui, callback.action, false);
			ImGui::EndDisabled();

			ImGui::TableSetColumnIndex(2);
			if (DrawEnabledDeleteControls(
					callback.enabled,
					"Enable or disable this lifecycle callback.",
					"Remove this lifecycle callback."
				)) {
				remove = i;
			}
			ImGui::EndTable();
		}

		ImGui::BeginDisabled(!callback.enabled);
		DrawActionParameters(ui, 
			callback.action, ImGui::GetCursorScreenPos().x
		);
		ImGui::EndDisabled();
		ImGui::PopID();
	}
	if (remove >= 0) {
		sequence.lifecycle_actions.erase(
			sequence.lifecycle_actions.begin() + remove
		);
	}
}

void DrawAddResidentScriptPopup(DemoEditorDrawContext& ui, ScriptsComponent& scripts) {
	if (!ImGui::BeginPopup("AddScript")) {
		return;
	}
	std::vector<std::string> groups;
	for (const auto& registration : ScriptRegistry::Entries()) {
		const auto* editor{ ScriptEditorRegistry::Find(registration.key) };
		if (!editor) {
			continue;
		}
		const std::string group{
			editor->options.group.empty() ? "Other" : editor->options.group
		};
		if (!std::ranges::contains(groups, group)) {
			groups.push_back(group);
		}
	}
	for (const auto& group : groups) {
		if (!ImGui::BeginMenu(group.c_str())) {
			continue;
		}
		for (const auto& registration : ScriptRegistry::Entries()) {
			const auto* editor{ ScriptEditorRegistry::Find(registration.key) };
			if (!editor) {
				continue;
			}
			const std::string_view candidate_group{
				editor->options.group.empty()
					? std::string_view{ "Other" }
					: std::string_view{ editor->options.group }
			};
			if (candidate_group != group) {
				continue;
			}
			if (ImGui::MenuItem(editor->options.label.c_str())) {
				ScriptEntry script;
				script.type_hash = registration.type_hash;
				script.type = registration.key;
				script.value = registration.make_default();
				scripts.scripts.push_back(std::move(script));
			}
			DrawItemTooltip(editor->options.description.c_str());
		}
		ImGui::EndMenu();
	}
	ImGui::EndPopup();
}

void DrawComponentDefinition(
	DemoEditorDrawContext& ui, ComponentDefinition& definition, bool removable,
	int* remove_index, int index
) {
	const auto* component{ ptgn::ComponentRegistry::Find(definition.type) };
	const auto* editor{ component ? FindComponentEditor(*component) : nullptr };
	const std::string label{
		component ? ResolveComponentEditor(*component).label : definition.type
	};

	ImGui::PushID(definition.type.c_str());
	bool open{ false };
	if (ImGui::BeginTable(
			"PrefabComponentHeader", removable ? 2 : 1,
			ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn(
			"Component", ImGuiTableColumnFlags_WidthStretch
		);
		if (removable) {
			ImGui::TableSetupColumn(
				"Remove", ImGuiTableColumnFlags_WidthFixed,
				ImGui::GetFrameHeight()
			);
		}
		ImGui::TableNextRow(
			ImGuiTableRowFlags_None, ImGui::GetFrameHeight()
		);
		ImGui::TableSetColumnIndex(0);
		open = ImGui::TreeNodeEx(
			"##Component",
			ImGuiTreeNodeFlags_DefaultOpen |
				ImGuiTreeNodeFlags_Framed |
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				(component && component->is_empty ? ImGuiTreeNodeFlags_Leaf
												  : ImGuiTreeNodeFlags_None),
			"%s", label.c_str()
		);
		if (component) {
			DrawItemTooltip(component->is_empty ? "Tag component" : component->name.data());
		}

		if (removable) {
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Button(
					"x",
					ImVec2{
						ImGui::GetFrameHeight(),
						ImGui::GetFrameHeight()
					}
				) && remove_index) {
				*remove_index = index;
			}
		}
		ImGui::EndTable();
	}

	if (open && component && editor && !component->is_empty) {
		if (definition.value.is_null() && component->make_default_json) {
			definition.value = component->make_default_json();
		}
		if (definition.value.is_null()) {
			definition.value = ptgn::json::object();
		}
		if (ptgn::editor::ComponentEditorRegistry::DrawJson(
				*component, definition.value
			)) {
			definition.apply_live = {};
		}
	}
	ImGui::PopID();
}

void DrawPrefabInspector(DemoEditorDrawContext& ui, PrefabDefinition& prefab) {
	ImGui::TextDisabled("Prefab Asset");
	if (ImGui::BeginTable("PrefabIdentity", 2, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthFixed, 160.0f);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
		ImGui::TableSetColumnIndex(0);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##PrefabName", "Display name", &prefab.name);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##PrefabTag", "Tag", &prefab.tag);
		ImGui::EndTable();
	}
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##PrefabKey", "Prefab key", &prefab.key);
	DrawItemTooltip("Stable asset key referenced by Spawn Entity Actions.");

	char label[64]{};
	std::snprintf(label, sizeof(label), "Components (%zu)", prefab.components.size());
	const bool open{ ImGui::TreeNodeEx(
		"##PrefabComponents", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_NoTreePushOnOpen,
		"%s", label
	) };
	if (!open) {
		return;
	}
	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(prefab.components.size()); ++i) {
		DrawComponentDefinition(ui, prefab.components[static_cast<std::size_t>(i)], true, &remove, i);
	}
	if (remove >= 0) {
		prefab.components.erase(prefab.components.begin() + remove);
	}
	if (ImGui::Button("+ Add Component", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddPrefabComponent");
	}
	if (ImGui::BeginPopup("AddPrefabComponent")) {
		std::string group;
		for (const auto& component : ptgn::ComponentRegistry::Components()) {
			if (!component.make_default_json || !HasComponentJsonEditor(component)) {
				continue;
			}

			auto options{ ResolveComponentEditor(component) };
			const bool already_added{ std::ranges::any_of(
				prefab.components,
				[&](const ComponentDefinition& definition) {
					return definition.type == component.name;
				}
			) };

			if (options.group != group) {
				if (!group.empty()) {
					ImGui::Separator();
				}
				if (!options.group.empty()) {
					ImGui::TextDisabled("%s", options.group.c_str());
				}
				group = options.group;
			}

			ImGui::BeginDisabled(already_added);
			if (ImGui::MenuItem(options.label.c_str())) {
				prefab.components.push_back(MakeComponentDefinition(component));
			}
			ImGui::EndDisabled();
		}
		ImGui::EndPopup();
	}
	ImGui::TextDisabled("Registry-backed values are copied into each spawned entity.");
}

void DrawPrefabs(DemoEditorDrawContext& ui) {
	ImGui::TextDisabled("Prefabs");
	ImGui::Separator();
	int delete_index{ -1 };
	int duplicate_index{ -1 };
	for (int i{ 0 }; i < static_cast<int>(ui.context.prefabs.definitions.size()); ++i) {
		auto& prefab{ ui.context.prefabs.definitions[static_cast<std::size_t>(i)] };
		ImGui::PushID(&prefab);
		if (ImGui::Selectable(prefab.name.c_str(), ui.state.inspect_prefab && ui.state.selected_prefab == i, 0, ImVec2{ 0.0f, 25.0f })) {
			ui.state.selected_prefab = i;
			ui.state.inspect_prefab = true;
		}
		DrawItemTooltip(prefab.key.c_str());
		if (ImGui::BeginPopupContextItem("PrefabContext")) {
			if (ImGui::MenuItem("Duplicate")) {
				duplicate_index = i;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete")) {
				delete_index = i;
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();
	}
	if (duplicate_index >= 0) {
		PrefabDefinition copy{ ui.context.prefabs.definitions[static_cast<std::size_t>(duplicate_index)] };
		copy.name += " Copy";
		copy.key += "_copy";
		ui.context.prefabs.definitions.insert(ui.context.prefabs.definitions.begin() + duplicate_index + 1, std::move(copy));
		ui.state.selected_prefab = duplicate_index + 1;
		ui.state.inspect_prefab = true;
	}
	if (delete_index >= 0) {
		ui.context.prefabs.definitions.erase(ui.context.prefabs.definitions.begin() + delete_index);
		ui.state.selected_prefab = ui.context.prefabs.definitions.empty() ? -1 : std::clamp(ui.state.selected_prefab, 0, static_cast<int>(ui.context.prefabs.definitions.size()) - 1);
		ui.state.inspect_prefab = ui.state.selected_prefab >= 0;
	}
	if (ImGui::Button("+ New Prefab", ImVec2{ -FLT_MIN, 0.0f })) {
		ui.context.prefabs.definitions.emplace_back();
		ui.state.selected_prefab = static_cast<int>(ui.context.prefabs.definitions.size()) - 1;
		ui.state.inspect_prefab = true;
	}
}

void DrawActivity(DemoEditorDrawContext& ui) {
	const auto activity{ ui.context.host.ActivityText() };
	char label[64]{};
	std::snprintf(label, sizeof(label), "Demo Activity (%zu)", activity.size());
	const bool open{ ImGui::TreeNodeEx(
		"##DemoActivity", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen,
		"%s", label
	) };
	if (!open) {
		return;
	}
	if (ImGui::BeginChild("ActivityLog", ImVec2{ 0.0f, 150.0f }, true)) {
		if (activity.empty()) {
			ImGui::TextDisabled("No activity yet.");
		}
		for (const auto& entry : activity) {
			ImGui::TextUnformatted(entry.c_str());
		}
	}
	ImGui::EndChild();
}

} // namespace editor

struct NameComponent {
	std::string value{ "Entity" };

	PTGN_REFLECT(NameComponent, value)
};

struct DemoVisual {
	ImVec4 color{ 0.35f, 0.43f, 0.57f, 1.0f };
	bool sensor{ false };

	PTGN_REFLECT(DemoVisual, color, sensor)
};

enum class ButtonState {
	Idle,
	Hovered,
	Pressed,
	Disabled
};
PTGN_REFLECT_ENUM(ButtonState);

struct ButtonData {
	bool enabled{ true };
	ButtonState state{ ButtonState::Idle };
	bool hovered{ false };
	bool pressed{ false };
	ptgn::Mouse pressed_button{ ptgn::Mouse::Left };

	PTGN_REFLECT(ButtonData, enabled, state, hovered, pressed, pressed_button)
};

struct ButtonStyle {
	ImVec4 idle{ 0.22f, 0.38f, 0.66f, 1.0f };
	ImVec4 hovered{ 0.30f, 0.49f, 0.82f, 1.0f };
	ImVec4 pressed{ 0.16f, 0.29f, 0.54f, 1.0f };
	ImVec4 disabled{ 0.28f, 0.29f, 0.32f, 1.0f };

	PTGN_REFLECT(ButtonStyle, idle, hovered, pressed, disabled)
};

struct PlayerMovementScript final : ptgn::Script {
	float speed{ 220.0f };
	void OnUpdate() override;

	PTGN_REFLECT(PlayerMovementScript, speed)
};

struct Health {
	float maximum{ 100.0f };
	float current{ 100.0f };

	PTGN_REFLECT(Health, maximum, current)
};

struct Zombie {
	PTGN_REFLECT_EMPTY(Zombie)
};

struct Damage {
	float amount{ 10.0f };
	std::string damage_type{ "Physical" };

	PTGN_REFLECT(Damage, amount, damage_type)
};

struct Lifetime {
	float duration_ms{ 3000.0f };

	PTGN_REFLECT(Lifetime, duration_ms)
};

struct MaskComponent {
	int value{ 1 };

	PTGN_REFLECT(MaskComponent, value)
};

struct ApplyDamageAction final : ScriptAction {
	float amount{ 10.0f };
	std::string damage_type{ "Physical" };
	bool critical{ false };

	ApplyDamageAction() = default;
	explicit ApplyDamageAction(
		float amount, std::string damage_type = "Physical", bool critical = false
	) : amount{ amount }, damage_type{ std::move(damage_type) }, critical{ critical } {}

	void OnStart() override;

	PTGN_REFLECT(ApplyDamageAction, amount, damage_type, critical)
};

template <typename T>
bool DrawRegisteredDemoContents(ptgn::Entity entity) {
	return ptgn::editor::inspector::DrawComponentContents(entity.Get<T>());
}

namespace editor::inspector {

template <>
struct Contents<DemoVisual> {
	static bool Draw(DemoVisual& visual) {
		bool changed{ ImGui::ColorEdit4("Color", &visual.color.x) };
		changed |= ImGui::Checkbox("Sensor", &visual.sensor);
		return changed;
	}
};

template <>
struct Contents<Health> {
	static bool Draw(Health& health) {
		bool changed{ ImGui::DragFloat("Maximum", &health.maximum, 1.0f, 0.0f) };
		changed |= ImGui::DragFloat(
			"Current", &health.current, 1.0f, 0.0f, health.maximum
		);
		return changed;
	}
};

template <>
struct Contents<Damage> {
	static bool Draw(Damage& damage) {
		bool changed{ ImGui::DragFloat("Amount", &damage.amount, 0.25f, 0.0f) };
		changed |= ImGui::InputText("Type", &damage.damage_type);
		return changed;
	}
};

template <>
struct Contents<Lifetime> {
	static bool Draw(Lifetime& lifetime) {
		return ImGui::DragFloat(
			"Duration", &lifetime.duration_ms, 10.0f, 0.0f,
			3600000.0f, "%.0f ms"
		);
	}
};

template <>
struct Contents<MaskComponent> {
	static bool Draw(MaskComponent& mask) {
		return ImGui::InputInt("Mask", &mask.value);
	}
};

template <>
struct Contents<ButtonData> {
	static bool Draw(ButtonData& button) {
		bool changed{ ImGui::Checkbox("Enabled", &button.enabled) };
		ImGui::BeginDisabled();
		std::string state{ magic_enum::enum_name(button.state) };
		ImGui::InputText("State", &state);
		ImGui::Checkbox("Hovered", &button.hovered);
		ImGui::Checkbox("Pressed", &button.pressed);
		ImGui::EndDisabled();
		return changed;
	}
};

template <>
struct Contents<ButtonStyle> {
	static bool Draw(ButtonStyle& style) {
		bool changed{ ImGui::ColorEdit4("Idle", &style.idle.x) };
		changed |= ImGui::ColorEdit4("Hovered", &style.hovered.x);
		changed |= ImGui::ColorEdit4("Pressed", &style.pressed.x);
		changed |= ImGui::ColorEdit4("Disabled", &style.disabled.x);
		return changed;
	}
};

} // namespace ptgn::editor::inspector

PTGN_REGISTER_COMPONENT(
	ptgn::Transform,
	{
		.label = "Transform",
		.group = "Core",
	}
);

PTGN_REGISTER_COMPONENT(
	DemoVisual,
	{
		.label = "Demo Visual",
		.group = "Graphics",
		.draw_contents = &DrawRegisteredDemoContents<DemoVisual>,
	}
);

PTGN_REGISTER_COMPONENT(
	Health,
	{
		.label = "Health",
		.group = "Gameplay",
		.draw_contents = &DrawRegisteredDemoContents<Health>,
	}
);

PTGN_REGISTER_COMPONENT(
	Zombie,
	{
		.label = "Zombie",
		.group = "Gameplay",
	}
);

PTGN_REGISTER_COMPONENT(
	Damage,
	{
		.label = "Damage",
		.group = "Gameplay",
		.draw_contents = &DrawRegisteredDemoContents<Damage>,
	}
);

PTGN_REGISTER_COMPONENT(
	Lifetime,
	{
		.label = "Lifetime",
		.group = "Gameplay",
		.draw_contents = &DrawRegisteredDemoContents<Lifetime>,
	}
);

PTGN_REGISTER_COMPONENT(
	MaskComponent,
	{
		.label = "Mask",
		.group = "Physics",
		.draw_contents = &DrawRegisteredDemoContents<MaskComponent>,
	}
);

PTGN_REGISTER_COMPONENT(
	ButtonData,
	{
		.label = "Button",
		.group = "UI",
		.draw_contents = &DrawRegisteredDemoContents<ButtonData>,
	}
);

PTGN_REGISTER_COMPONENT(
	ButtonStyle,
	{
		.label = "Button Style",
		.group = "UI",
		.draw_contents = &DrawRegisteredDemoContents<ButtonStyle>,
	}
);

void RegisterDemoEditorTypes();

class DemoScene final : public ptgn::Scene, public editor::EditorHost {
public:
	struct ActivityEntry {
		std::string text;
		float remaining_seconds{ 8.0f };
	};

	void OnEnter() override;
	void OnUpdate() override;

	[[nodiscard]] PrefabRegistry& GetPrefabs() override { return prefabs_; }
	[[nodiscard]] SharedScriptSequenceRegistry& GetSharedSequences() override {
		return shared_sequences_;
	}
	[[nodiscard]] std::vector<ptgn::Entity> Entities() const override {
		return ptgn::Scene::Entities().GetVector();
	}
	[[nodiscard]] std::vector<std::string> ActivityText() const override;
	[[nodiscard]] std::string_view Name(ptgn::Entity entity) const override;
	[[nodiscard]] editor::EditorVisual Visual(ptgn::Entity entity) const override;
	[[nodiscard]] std::string& EditableName(ptgn::Entity entity) override;
	[[nodiscard]] std::string& EditableTag(ptgn::Entity entity) override;
	ptgn::Entity CreateEntity(std::string name, std::string tag = {}) override;
	[[nodiscard]] ScriptSequence* Resolve(
		ptgn::Entity owner, ScriptSequence& binding
	) override {
		return script_runtime::Resolve(owner, binding);
	}
	[[nodiscard]] const ScriptSequence* Resolve(
		ptgn::Entity owner, const ScriptSequence& binding
	) const override {
		return script_runtime::Resolve(owner, binding);
	}
	void Start(ptgn::Entity owner, ScriptSequence& binding, bool force = false) override {
		(void)script_runtime::Start(owner, binding.id, force);
	}
	void Stop(ptgn::Entity owner, ScriptSequence& binding, bool) override {
		(void)script_runtime::Stop(owner, binding.id);
	}
	void SetPaused(ptgn::Entity owner, ScriptSequence& binding, bool paused) override {
		(void)script_runtime::SetPaused(owner, binding.id, paused);
	}
	[[nodiscard]] float Progress(
		ptgn::Entity owner, const ScriptSequence& binding
	) const override {
		return script_runtime::Progress(owner, binding.id);
	}
	void SubmitPointerFrame(editor::PointerFrame frame) override {
		pointer_frame_ = frame;
		pointer_frame_pending_ = true;
	}

	void Log(std::string text);
	void RequestDestroy(ptgn::Entity entity);
	[[nodiscard]] ptgn::Entity FindByTag(std::string_view tag) const;
	[[nodiscard]] int Mask(ptgn::Entity entity) const;
	[[nodiscard]] ptgn::Entity SpawnPrefab(
		const PrefabDefinition& prefab, ptgn::V2_float position
	);
	[[nodiscard]] ptgn::Entity SpawnPrefab(const PrefabSpawnRequest& request);

private:
	void CreatePrefabs();
	void CreateDemoScene();
	void UpdateButtonInteraction();
	void UpdateOverlapEvents();
	void ProcessPendingDestroy();
	[[nodiscard]] bool Overlap(ptgn::Entity a, ptgn::Entity b) const;

	PrefabRegistry prefabs_;
	SharedScriptSequenceRegistry shared_sequences_;
	std::vector<ActivityEntry> activity_;
	std::vector<ptgn::Entity> pending_destroy_;
	std::unique_ptr<editor::DemoEditor> editor_;

	editor::PointerFrame pointer_frame_{};
	bool pointer_frame_pending_{ false };
	ptgn::Entity hovered_button_;
	std::array<ptgn::Entity, 3> pressed_buttons_{};
	bool player_overlapping_sensor_{ false };
	bool player_overlapping_spawner_{ false };
};

namespace {

template <typename TEvent>
[[nodiscard]] ptgn::Entity OtherEntity(const TEvent& event) {
	if constexpr (requires { event.overlap_entity; }) {
		return event.overlap_entity;
	} else if constexpr (requires { event.collision.other_entity; }) {
		return event.collision.other_entity;
	} else if constexpr (requires { event.collision.other; }) {
		return event.collision.other;
	} else if constexpr (requires { event.collision.entity; }) {
		return event.collision.entity;
	} else if constexpr (requires { event.entity; }) {
		return event.entity;
	} else {
		return {};
	}
}

[[nodiscard]] std::string_view TrimFilterToken(std::string_view token) {
	while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front()))) {
		token.remove_prefix(1);
	}
	while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back()))) {
		token.remove_suffix(1);
	}
	return token;
}

template <typename F>
void ForEachFilterToken(std::string_view text, F&& fn) {
	while (true) {
		const std::size_t separator{ text.find(',') };
		std::string_view token{ TrimFilterToken(text.substr(0, separator)) };
		if (!token.empty()) {
			std::invoke(fn, token);
		}
		if (separator == std::string_view::npos) {
			break;
		}
		text.remove_prefix(separator + 1);
	}
}

[[nodiscard]] bool MatchesTagFilters(std::string_view filters, std::string_view tag) {
	bool has_include{ false };
	bool include_match{ false };
	bool excluded{ false };

	ForEachFilterToken(filters, [&](std::string_view token) {
		const bool exclude{ token.front() == '-' };
		if (exclude) {
			token = TrimFilterToken(token.substr(1));
		}
		if (token.empty()) {
			return;
		}
		if (exclude) {
			excluded |= tag == token;
		} else {
			has_include = true;
			include_match |= tag == token;
		}
	});

	return !excluded && (!has_include || include_match);
}

[[nodiscard]] bool MatchesMaskFilters(std::string_view filters, int mask) {
	bool has_include{ false };
	bool include_match{ false };
	bool excluded{ false };

	ForEachFilterToken(filters, [&](std::string_view token) {
		const bool exclude{ token.front() == '-' };
		if (exclude) {
			token = TrimFilterToken(token.substr(1));
		}
		if (!token.empty() && token.front() == '+') {
			token.remove_prefix(1);
		}
		if (token.empty()) {
			return;
		}

		int value{};
		const auto [end, error]{ std::from_chars(token.data(), token.data() + token.size(), value) };
		if (error != std::errc{} || end != token.data() + token.size() || value <= 0) {
			return;
		}

		const bool matches{ (mask & value) != 0 };
		if (exclude) {
			excluded |= matches;
		} else {
			has_include = true;
			include_match |= matches;
		}
	});

	return !excluded && (!has_include || include_match);
}

[[nodiscard]] std::string NormalizeKeyToken(std::string_view token) {
	std::string normalized;
	normalized.reserve(token.size());
	for (const unsigned char c : token) {
		if (std::isalnum(c)) {
			normalized.push_back(static_cast<char>(std::tolower(c)));
		}
	}
	return normalized;
}

[[nodiscard]] std::optional<ptgn::Key> ParseKeyToken(std::string_view token) {
	token = TrimFilterToken(token);
	if (token.empty()) {
		return std::nullopt;
	}

	std::string normalized{ NormalizeKeyToken(token) };
	if (normalized.empty()) {
		return std::nullopt;
	}

	const bool numeric_name{
		std::ranges::all_of(normalized, [](unsigned char c) {
			return std::isdigit(c) != 0;
		})
	};
	if (numeric_name) {
		// A single digit means the number-row key. Longer numbers are GLFW scancodes.
		if (normalized.size() == 1) {
			normalized.insert(normalized.begin(), 'k');
		} else {
			using KeyValue = std::underlying_type_t<ptgn::Key>;
			KeyValue scancode{};
			const auto [end, error]{ std::from_chars(
				normalized.data(), normalized.data() + normalized.size(), scancode
			) };
			if (error == std::errc{} && end == normalized.data() + normalized.size()) {
				if (const auto key{ magic_enum::enum_cast<ptgn::Key>(scancode) }) {
					return *key;
				}
			}
			return std::nullopt;
		}
	}

	// Common unsided modifier names imply the left modifier.
	if (normalized == "shift") {
		normalized = "leftshift";
	} else if (normalized == "ctrl" || normalized == "control") {
		normalized = "leftctrl";
	} else if (normalized == "alt" || normalized == "option") {
		normalized = "leftalt";
	} else if (normalized == "super" || normalized == "cmd" ||
		normalized == "command") {
		normalized = "leftsuper";
	} else if (normalized == "leftcontrol") {
		normalized = "leftctrl";
	} else if (normalized == "rightcontrol") {
		normalized = "rightctrl";
	}

	for (const auto [key, name] : magic_enum::enum_entries<ptgn::Key>()) {
		if (NormalizeKeyToken(name) == normalized) {
			return key;
		}
	}
	return std::nullopt;
}

struct ParsedKeyExpression {
	std::vector<std::vector<ptgn::Key>> alternatives;
	std::string error;

	[[nodiscard]] explicit operator bool() const {
		return error.empty() && !alternatives.empty();
	}
};

[[nodiscard]] ParsedKeyExpression ParseKeyExpression(std::string_view expression) {
	ParsedKeyExpression result;
	while (true) {
		const std::size_t comma{ expression.find(',') };
		std::string_view group{ TrimFilterToken(expression.substr(0, comma)) };
		if (group.empty()) {
			result.error = "Expected a key name or scancode between commas.";
			return result;
		}

		std::vector<ptgn::Key> keys;
		while (true) {
			const std::size_t plus{ group.find('+') };
			const std::string_view token{ TrimFilterToken(group.substr(0, plus)) };
			const auto key{ ParseKeyToken(token) };
			if (!key) {
				result.error =
					"Unknown key '" + std::string{ token } +
					"'. Use a Key enum name or numeric scancode.";
				return result;
			}
			if (!std::ranges::contains(keys, *key)) {
				keys.push_back(*key);
			}
			if (plus == std::string_view::npos) {
				break;
			}
			group.remove_prefix(plus + 1);
			if (TrimFilterToken(group).empty()) {
				result.error = "Expected a key after '+'.";
				return result;
			}
		}
		result.alternatives.push_back(std::move(keys));

		if (comma == std::string_view::npos) {
			break;
		}
		expression.remove_prefix(comma + 1);
		if (TrimFilterToken(expression).empty()) {
			result.error = "Expected a key after ','.";
			return result;
		}
	}
	return result;
}

enum class KeyExpressionEvent {
	Pressed,
	Held,
	Released
};

[[nodiscard]] int MouseButtonIndex(ptgn::Mouse button) {
	for (int i{ 0 }; i < static_cast<int>(kMouseButtons.size()); ++i) {
		if (kMouseButtons[static_cast<std::size_t>(i)] == button) {
			return i;
		}
	}
	return -1;
}

struct InputExpressionState {
	std::unordered_set<int> keys_down;
	std::unordered_map<int, double> key_down_since;
	std::array<bool, kMouseButtons.size()> mouse_down{};
	std::array<double, kMouseButtons.size()> mouse_down_since{};
	int observed_frame{ -1 };
	std::unordered_set<std::size_t> observed_events;

	void BeginFrame() {
		const int frame{ ImGui::GetFrameCount() };
		if (observed_frame != frame) {
			observed_frame = frame;
			observed_events.clear();
		}
	}

	template <typename TEvent>
	[[nodiscard]] bool MarkObserved(int value) {
		BeginFrame();
		const std::size_t signature{
			ptgn::Hash<TEvent>() ^
			(static_cast<std::size_t>(value) + 0x9e3779b97f4a7c15ULL +
				(ptgn::Hash<TEvent>() << 6U) + (ptgn::Hash<TEvent>() >> 2U))
		};
		return observed_events.insert(signature).second;
	}

	void PressKey(ptgn::Key key) {
		const int value{ static_cast<int>(key) };
		if (keys_down.insert(value).second) {
			key_down_since[value] = ImGui::GetTime();
		}
	}

	void ReleaseKey(ptgn::Key key) {
		const int value{ static_cast<int>(key) };
		keys_down.erase(value);
		key_down_since.erase(value);
	}

	void PressMouse(ptgn::Mouse button) {
		const int index{ MouseButtonIndex(button) };
		if (index < 0) {
			return;
		}
		auto& down{ mouse_down[static_cast<std::size_t>(index)] };
		if (!down) {
			down = true;
			mouse_down_since[static_cast<std::size_t>(index)] = ImGui::GetTime();
		}
	}

	void ReleaseMouse(ptgn::Mouse button) {
		const int index{ MouseButtonIndex(button) };
		if (index < 0) {
			return;
		}
		mouse_down[static_cast<std::size_t>(index)] = false;
		mouse_down_since[static_cast<std::size_t>(index)] = 0.0;
	}

	[[nodiscard]] bool IsKeyDown(ptgn::Key key) const {
		return keys_down.contains(static_cast<int>(key));
	}

	[[nodiscard]] float KeyChordHeldMilliseconds(
		const std::vector<ptgn::Key>& keys
	) const {
		double chord_start{};
		for (const auto key : keys) {
			const int value{ static_cast<int>(key) };
			const auto it{ key_down_since.find(value) };
			if (!keys_down.contains(value) || it == key_down_since.end()) {
				return 0.0f;
			}
			chord_start = std::max(chord_start, it->second);
		}
		return static_cast<float>(
			std::max(0.0, ImGui::GetTime() - chord_start) * 1000.0
		);
	}

	[[nodiscard]] float MouseHeldMilliseconds(ptgn::Mouse button) const {
		const int index{ MouseButtonIndex(button) };
		if (index < 0 || !mouse_down[static_cast<std::size_t>(index)]) {
			return 0.0f;
		}
		return static_cast<float>(
			std::max(
				0.0,
				ImGui::GetTime() -
					mouse_down_since[static_cast<std::size_t>(index)]
			) * 1000.0
		);
	}
};

[[nodiscard]] InputExpressionState& GetInputExpressionState() {
	static InputExpressionState state;
	return state;
}

void ObserveInputExpressionEvent(Event event) {
	auto& state{ GetInputExpressionState() };
	event.Dispatch<ptgn::event::KeyPressed>([&](const auto& input) {
		if (state.MarkObserved<ptgn::event::KeyPressed>(static_cast<int>(input.key))) {
			state.PressKey(input.key);
		}
	});
	event.Dispatch<ptgn::event::KeyHeld>([&](const auto& input) {
		if (state.MarkObserved<ptgn::event::KeyHeld>(static_cast<int>(input.key))) {
			state.PressKey(input.key);
		}
	});
	event.Dispatch<ptgn::event::KeyReleased>([&](const auto& input) {
		if (state.MarkObserved<ptgn::event::KeyReleased>(static_cast<int>(input.key))) {
			state.ReleaseKey(input.key);
		}
	});
	event.Dispatch<ptgn::event::MousePressed>([&](const auto& input) {
		const auto button{ EventMouse(input) };
		if (state.MarkObserved<ptgn::event::MousePressed>(static_cast<int>(button))) {
			state.PressMouse(button);
		}
	});
	event.Dispatch<ptgn::event::MouseHeld>([&](const auto& input) {
		const auto button{ EventMouse(input) };
		if (state.MarkObserved<ptgn::event::MouseHeld>(static_cast<int>(button))) {
			state.PressMouse(button);
		}
	});
	event.Dispatch<ptgn::event::MouseReleased>([&](const auto& input) {
		const auto button{ EventMouse(input) };
		if (state.MarkObserved<ptgn::event::MouseReleased>(static_cast<int>(button))) {
			state.ReleaseMouse(button);
		}
	});
}

[[nodiscard]] bool MatchesKeyExpression(
	std::string_view expression,
	ptgn::Key event_key,
	KeyExpressionEvent event_kind,
	float held_duration_ms = 0.0f
) {
	const ParsedKeyExpression parsed{ ParseKeyExpression(expression) };
	if (!parsed) {
		return false;
	}

	const auto& state{ GetInputExpressionState() };
	for (const auto& alternative : parsed.alternatives) {
		if (!std::ranges::contains(alternative, event_key)) {
			continue;
		}

		const bool complete{ std::ranges::all_of(
			alternative,
			[&](ptgn::Key key) {
				if (event_kind == KeyExpressionEvent::Released && key == event_key) {
					return true;
				}
				return state.IsKeyDown(key);
			}
		) };
		if (!complete) {
			continue;
		}

		if (event_kind != KeyExpressionEvent::Held ||
			state.KeyChordHeldMilliseconds(alternative) >=
				std::max(0.0f, held_duration_ms)) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] bool MatchesMouseHeld(
	ptgn::Mouse configured,
	ptgn::Mouse event_button,
	float duration_ms
) {
	return configured == event_button &&
		GetInputExpressionState().MouseHeldMilliseconds(configured) >=
			std::max(0.0f, duration_ms);
}

template <typename TScript>
	requires std::derived_from<TScript, ptgn::Script>
[[nodiscard]] ScriptEntry MakeLiveScriptEntry(TScript script) {
	return ScriptRegistry::Make<TScript>(std::move(script));
}

[[nodiscard]] ScriptEntry MakeSequenceEntry(ScriptSequence sequence) {
	SequenceScript script;
	script.sequence = std::move(sequence);
	return ScriptRegistry::Make(std::move(script));
}

void AttachSequence(ptgn::Entity entity, ScriptSequence sequence) {
	auto& scripts{ entity.TryAdd<ScriptsComponent>() };
	scripts.scripts.push_back(MakeSequenceEntry(std::move(sequence)));
	script_runtime::AttachEntry(entity, scripts.scripts.back());
}

template <typename TScript>
void AttachScript(ptgn::Entity entity, TScript script = {}) {
	auto& scripts{ entity.TryAdd<ScriptsComponent>() };
	scripts.scripts.push_back(MakeLiveScriptEntry(std::move(script)));
	script_runtime::AttachEntry(entity, scripts.scripts.back());
}

void RegisterDemoTypes() {
	static bool registered{ false };
	if (registered) {
		return;
	}
	registered = true;

	const auto always = [](ptgn::Entity, const ptgn::json&, const auto&) {
		return true;
	};
	const auto button_available{ editor::RequireComponents<ButtonData>() };
	const auto draggable_available{
		editor::RequireComponents<ptgn::impl::Draggable>()
	};
	const auto always_available = [](ptgn::Entity) { return true; };

	const auto draw_key = [](ptgn::json& value, bool with_duration) {
		std::string keys{ JsonValueOr<std::string>(value, "keys", "W") };
		float duration_ms{ JsonValueOr<float>(value, "duration_ms", 500.0f) };
		duration_ms = std::max(0.0f, duration_ms);

		const ParsedKeyExpression before{ ParseKeyExpression(keys) };
		const bool valid_before{ static_cast<bool>(before) };
		const float spacing{ ImGui::GetStyle().ItemSpacing.x };
		const float duration_width{ 92.0f };
		const float keys_width{
			with_duration
				? std::max(
					1.0f,
					ImGui::GetContentRegionAvail().x - duration_width - spacing
				)
				: -FLT_MIN
		};

		if (!valid_before) {
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4{ 0.90f, 0.25f, 0.25f, 1.0f });
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		}
		ImGui::SetNextItemWidth(keys_width);
		bool changed{ ImGui::InputTextWithHint(
			"##Keys", "Keys: W + Left Shift, Space", &keys
		) };
		if (!valid_before) {
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();
		}

		const ParsedKeyExpression parsed{ ParseKeyExpression(keys) };
		editor::DrawItemTooltip(
			parsed
				? "Use '+' for AND and ',' for OR. Names are matched case-insensitively against ptgn::Key; numeric key codes are also accepted."
				: parsed.error.c_str()
		);

		if (with_duration) {
			ImGui::SameLine(0.0f, spacing);
			ImGui::SetNextItemWidth(duration_width);
			changed |= ImGui::InputFloat(
				"##HeldDuration",
				&duration_ms,
				0.0f,
				0.0f,
				"%.0f ms",
				ImGuiInputTextFlags_CharsDecimal
			);
			duration_ms = std::max(0.0f, duration_ms);
			editor::DrawItemTooltip(
				"How long the complete key expression must remain held before matching."
			);
		}

		if (changed) {
			value["keys"] = std::move(keys);
			if (with_duration) {
				value["duration_ms"] = duration_ms;
			}
		}
		return changed;
	};

	const auto draw_mouse = [](ptgn::json& value, bool with_duration) {
		ptgn::Mouse button{
			JsonValueOr<ptgn::Mouse>(value, "button", ptgn::Mouse::Left)
		};
		float duration_ms{ JsonValueOr<float>(value, "duration_ms", 500.0f) };
		duration_ms = std::max(0.0f, duration_ms);

		const float spacing{ ImGui::GetStyle().ItemSpacing.x };
		const float duration_width{ 92.0f };
		const float button_width{
			with_duration
				? std::max(
					1.0f,
					ImGui::GetContentRegionAvail().x - duration_width - spacing
				)
				: -FLT_MIN
		};

		bool changed{ false };
		ImGui::SetNextItemWidth(button_width);
		if (ImGui::BeginCombo("##Mouse", MouseButtonLabel(button))) {
			for (const auto candidate : kMouseButtons) {
				if (ImGui::Selectable(
						MouseButtonLabel(candidate), candidate == button
					)) {
					button = candidate;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		editor::DrawItemTooltip("Only the selected mouse button matches this event.");

		if (with_duration) {
			ImGui::SameLine(0.0f, spacing);
			ImGui::SetNextItemWidth(duration_width);
			changed |= ImGui::InputFloat(
				"##HeldDuration",
				&duration_ms,
				0.0f,
				0.0f,
				"%.0f ms",
				ImGuiInputTextFlags_CharsDecimal
			);
			duration_ms = std::max(0.0f, duration_ms);
			editor::DrawItemTooltip(
				"How long the mouse button must remain held before matching."
			);
		}

		if (changed) {
			value["button"] = button;
			if (with_duration) {
				value["duration_ms"] = duration_ms;
			}
		}
		return changed;
	};

	const auto draw_entity_filters = [](ptgn::json& value) {
		std::string tags{ JsonValueOr<std::string>(value, "tags", "") };
		std::string masks{ JsonValueOr<std::string>(value, "masks", "") };
		const float spacing{ ImGui::GetStyle().ItemSpacing.x };
		const float width{ std::max(
			1.0f, (ImGui::GetContentRegionAvail().x - spacing) * 0.5f
		) };

		ImGui::SetNextItemWidth(width);
		bool changed{ ImGui::InputTextWithHint(
			"##Tags", "Tags: Player, -Enemy", &tags
		) };
		editor::DrawItemTooltip(
			"Comma-separated tag filters. Positive tags are included; tags prefixed with '-' are excluded."
		);

		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= ImGui::InputTextWithHint(
			"##Masks", "Masks: 1, -2", &masks
		);
		editor::DrawItemTooltip(
			"Comma-separated mask filters. Positive masks require overlapping bits; masks prefixed with '-' exclude overlapping bits."
		);

		if (changed) {
			value["tags"] = std::move(tags);
			value["masks"] = std::move(masks);
		}
		return changed;
	};

	const auto entity_filter_matches = [](
		ptgn::Entity,
		const ptgn::json& value,
		const auto& event
	) {
		const ptgn::Entity other{ OtherEntity(event) };
		if (!other) {
			return false;
		}
		return MatchesTagFilters(
				JsonValueOr<std::string>(value, "tags", ""),
				GetEntityTag(other)
			) &&
			MatchesMaskFilters(
				JsonValueOr<std::string>(value, "masks", ""),
				GetEntityMask(other)
			);
	};

#define PTGN_REGISTER_SIMPLE_KEY_EVENT(EventType, Key, Label, EventKind)        \
	PTGN_REGISTER_EVENT(                                                          \
		EventType,                                                                   \
		{                                                                            \
			.key = Key,                                                                  \
			.editor = {                                                                  \
				.label = Label,                                                              \
				.group = "Key",                                                              \
				.description = "Matches a key expression using '+' for AND and ',' for OR.",\
			},                                                                           \
			.default_value = ptgn::json{ { "keys", "W" } },                              \
			.inline_fields = 1,                                                          \
			.matches = [](                                                                \
				ptgn::Entity, const ptgn::json& value, const EventType& event               \
			) {                                                                           \
				return MatchesKeyExpression(                                                \
					JsonValueOr<std::string>(value, "keys", "W"),                             \
					event.key, EventKind                                                       \
				);                                                                          \
			},                                                                           \
			.draw = [draw_key](ptgn::json& value) { return draw_key(value, false); },     \
		}                                                                            \
	)

	PTGN_REGISTER_SIMPLE_KEY_EVENT(
		ptgn::event::KeyPressed,
		"ptgn.event.KeyPressed", "On Key Pressed", KeyExpressionEvent::Pressed
	);
	PTGN_REGISTER_EVENT(
		ptgn::event::KeyHeld,
		{
			.key = "ptgn.event.KeyHeld",
			.editor = {
				.label = "On Key Held",
				.group = "Key",
				.description = "Matches after a key expression remains held for the configured duration.",
			},
			.default_value = ptgn::json{
				{ "keys", "W" },
				{ "duration_ms", 500.0f },
			},
			.inline_fields = 2,
			.matches = [](
				ptgn::Entity,
				const ptgn::json& value,
				const ptgn::event::KeyHeld& event
			) {
				return MatchesKeyExpression(
					JsonValueOr<std::string>(value, "keys", "W"),
					event.key,
					KeyExpressionEvent::Held,
					JsonValueOr<float>(value, "duration_ms", 500.0f)
				);
			},
			.draw = [draw_key](ptgn::json& value) { return draw_key(value, true); },
		}
	);
	PTGN_REGISTER_SIMPLE_KEY_EVENT(
		ptgn::event::KeyReleased,
		"ptgn.event.KeyReleased", "On Key Released", KeyExpressionEvent::Released
	);
#undef PTGN_REGISTER_SIMPLE_KEY_EVENT

#define PTGN_REGISTER_SIMPLE_MOUSE_EVENT(EventType, Key, Label, Group, Available) \
	PTGN_REGISTER_EVENT(                                                            \
		EventType,                                                                     \
		{                                                                              \
			.key = Key,                                                                    \
			.editor = {                                                                    \
				.label = Label,                                                                \
				.group = Group,                                                                \
				.description = "Matches the selected mouse button.",                         \
			},                                                                             \
			.default_value = ptgn::json{ { "button", ptgn::Mouse::Left } },                 \
			.inline_fields = 1,                                                            \
			.matches = [](                                                                  \
				ptgn::Entity, const ptgn::json& value, const EventType& event                 \
			) {                                                                             \
				return EventMouse(event) ==                                                   \
					JsonValueOr<ptgn::Mouse>(value, "button", ptgn::Mouse::Left);               \
			},                                                                             \
			.draw = [draw_mouse](ptgn::json& value) { return draw_mouse(value, false); },   \
			.available = Available,                                                         \
		}                                                                              \
	)

	PTGN_REGISTER_SIMPLE_MOUSE_EVENT(
		ptgn::event::MousePressed,
		"ptgn.event.MousePressed", "On Mouse Pressed", "Mouse", always_available
	);
	PTGN_REGISTER_EVENT(
		ptgn::event::MouseHeld,
		{
			.key = "ptgn.event.MouseHeld",
			.editor = {
				.label = "On Mouse Held",
				.group = "Mouse",
				.description = "Matches after the selected mouse button remains held for the configured duration.",
			},
			.default_value = ptgn::json{
				{ "button", ptgn::Mouse::Left },
				{ "duration_ms", 500.0f },
			},
			.inline_fields = 2,
			.matches = [](
				ptgn::Entity,
				const ptgn::json& value,
				const ptgn::event::MouseHeld& event
			) {
				return MatchesMouseHeld(
					JsonValueOr<ptgn::Mouse>(
						value, "button", ptgn::Mouse::Left
					),
					EventMouse(event),
					JsonValueOr<float>(value, "duration_ms", 500.0f)
				);
			},
			.draw = [draw_mouse](ptgn::json& value) {
				return draw_mouse(value, true);
			},
		}
	);
	PTGN_REGISTER_SIMPLE_MOUSE_EVENT(
		ptgn::event::MouseReleased,
		"ptgn.event.MouseReleased", "On Mouse Released", "Mouse", always_available
	);

	PTGN_REGISTER_EVENT(
		MouseMoveOver,
		{
			.key = "demo.button.MouseEnter",
			.editor = {
				.label = "On Mouse Enter",
				.group = "Button",
				.description = "Available when the owner has ButtonData.",
			},
			.matches = always,
			.available = button_available,
		}
	);
	PTGN_REGISTER_EVENT(
		MouseMoveOut,
		{
			.key = "demo.button.MouseLeave",
			.editor = {
				.label = "On Mouse Leave",
				.group = "Button",
				.description = "Available when the owner has ButtonData.",
			},
			.matches = always,
			.available = button_available,
		}
	);
	PTGN_REGISTER_SIMPLE_MOUSE_EVENT(
		MousePressedOver,
		"demo.button.MousePressed", "On Mouse Pressed Over", "Button", button_available
	);
	PTGN_REGISTER_SIMPLE_MOUSE_EVENT(
		MouseReleasedOver,
		"demo.button.MouseReleased", "On Mouse Released", "Button", button_available
	);
	PTGN_REGISTER_SIMPLE_MOUSE_EVENT(
		ButtonPress,
		"demo.button.Press", "On Button Press", "Button", button_available
	);
#undef PTGN_REGISTER_SIMPLE_MOUSE_EVENT

#define PTGN_REGISTER_EMPTY_EVENT(EventType, Key, Label, Group, Description, Available) \
	PTGN_REGISTER_EVENT(                                                                  \
		EventType,                                                                           \
		{                                                                                    \
			.key = Key,                                                                          \
			.editor = {                                                                          \
				.label = Label,                                                                      \
				.group = Group,                                                                      \
				.description = Description,                                                          \
			},                                                                                   \
			.matches = always,                                                                   \
			.available = Available,                                                              \
		}                                                                                    \
	)

	PTGN_REGISTER_EMPTY_EVENT(
		DragStart, "demo.drag.Start", "On Drag Start", "Drag",
		"Available when the owner has Draggable.", draggable_available
	);
	PTGN_REGISTER_EMPTY_EVENT(
		Drag, "demo.drag.Update", "On Drag", "Drag",
		"Available when the owner has Draggable.", draggable_available
	);
	PTGN_REGISTER_EMPTY_EVENT(
		DragStop, "demo.drag.Stop", "On Drag Stop", "Drag",
		"Available when the owner has Draggable.", draggable_available
	);
#undef PTGN_REGISTER_EMPTY_EVENT

#define PTGN_REGISTER_ENTITY_FILTER_EVENT(EventType, Key, Label, Group)         \
	PTGN_REGISTER_EVENT(                                                          \
		EventType,                                                                   \
		{                                                                            \
			.key = Key,                                                                  \
			.editor = {                                                                  \
				.label = Label,                                                              \
				.group = Group,                                                              \
				.description = "Matches by comma-separated include/exclude tag and mask filters.", \
			},                                                                           \
			.default_value = ptgn::json{ { "tags", "" }, { "masks", "" } },            \
			.inline_fields = 2,                                                          \
			.matches = entity_filter_matches,                                           \
			.draw = draw_entity_filters,                                                \
		}                                                                            \
	)

	PTGN_REGISTER_ENTITY_FILTER_EVENT(
		ptgn::event::OverlapStart,
		"ptgn.event.OverlapStart", "On Overlap Start", "Overlap"
	);
	PTGN_REGISTER_ENTITY_FILTER_EVENT(
		ptgn::event::Overlap,
		"ptgn.event.Overlap", "On Overlap", "Overlap"
	);
	PTGN_REGISTER_ENTITY_FILTER_EVENT(
		ptgn::event::OverlapStop,
		"ptgn.event.OverlapStop", "On Overlap Stop", "Overlap"
	);
	PTGN_REGISTER_ENTITY_FILTER_EVENT(
		ptgn::event::Collision,
		"ptgn.event.Collision", "On Collision", "Collision"
	);
#undef PTGN_REGISTER_ENTITY_FILTER_EVENT

	PTGN_REGISTER_EVENT(
		Signal,
		{
			.key = "ptgn.event.Signal",
			.editor = {
				.label = "On Signal",
				.group = "",
				.description = "Matches the Signal name entered on the event row.",
			},
			.default_value = ptgn::json{ { "signal", "" } },
			.inline_fields = 1,
			.matches = [](
				ptgn::Entity,
				const ptgn::json& value,
				const Signal& event
			) {
				return event.key.value ==
					JsonValueOr<std::string>(value, "signal", "");
			},
			.draw = [](ptgn::json& value) {
				std::string signal{
					JsonValueOr<std::string>(value, "signal", "")
				};
				ImGui::SetNextItemWidth(-FLT_MIN);
				const bool changed{ ImGui::InputTextWithHint(
					"##Signal", "Signal name", &signal
				) };
				editor::DrawItemTooltip("Signal name matched exactly.");
				if (changed) {
					value["signal"] = std::move(signal);
				}
				return changed;
			},
		}
	);

	PTGN_REGISTER_ACTION(
		WaitAction,
		{
			.key = "engine.wait",
			.supports_timing = true,
			.requires_timing = true,
			.default_timing = ActionTiming{ .duration_ms = 250.0f },
			.editor = {
				.label = "Delay",
				.group = "Timing",
				.description = "Delay before continuing the sequence.",
			},
		}
	);
	PTGN_REGISTER_ACTION(
		MoveToAction,
		{
			.key = "engine.move_to",
			.supports_timing = true,
			.default_timing = ActionTiming{
				.duration_ms = 300.0f,
				.ease = ptgn::Ease::OutCubic,
			},
			.completion = ActionCompletion::Duration,
			.editor = {
				.label = "Move To",
				.group = "Transform",
				.description = "Move the owning entity.",
			},
		}
	);
	PTGN_REGISTER_ACTION(
		RotateToAction,
		{
			.key = "engine.rotate_to",
			.supports_timing = true,
			.default_timing = ActionTiming{ .duration_ms = 300.0f },
			.completion = ActionCompletion::Duration,
			.editor = {
				.label = "Rotate To",
				.group = "Transform",
				.description = "Rotate the owning entity.",
			},
		}
	);
	PTGN_REGISTER_ACTION(
		ScaleToAction,
		{
			.key = "engine.scale_to",
			.supports_timing = true,
			.default_timing = ActionTiming{
				.duration_ms = 180.0f,
				.ease = ptgn::Ease::OutBack,
			},
			.completion = ActionCompletion::Duration,
			.editor = {
				.label = "Scale To",
				.group = "Transform",
				.description = "Scale the owning entity.",
			},
		}
	);
	PTGN_REGISTER_ACTION(
		FollowTargetAction,
		{
			.key = "engine.follow_target",
			.completion = ActionCompletion::ActionControlled,
			.editor = {
				.label = "Follow Target",
				.group = "Transform",
				.description = "Move until the owner reaches a target entity.",
			},
		}
	);
	PTGN_REGISTER_ACTION(
		NativeAction,
		{
			.key = "engine.native",
			.serializable = false,
			.editor = {
				.label = "Native",
				.group = "Runtime",
				.description = "Runtime-only native callback action.",
			},
		}
	);
	PTGN_REGISTER_ACTION(
		SetVisibleAction,
		{
			.key = "engine.set_visible",
			.editor = {
				.label = "Set Visibility",
				.group = "Entity",
				.description = "Set the owner's visibility.",
			},
		}
	);
	PTGN_REGISTER_ACTION(
		PlayAudioAction,
		{
			.key = "engine.play_audio",
			.editor = {
				.label = "Play Audio",
				.group = "Audio",
				.description = "Play an audio asset.",
			},
		}
	);
	PTGN_REGISTER_ACTION(
		EmitSignalAction,
		{
			.key = "engine.emit_signal",
			.editor = {
				.label = "Emit Signal",
				.group = "Events",
				.description = "Emit a global Signal event.",
			},
		}
	);
	PTGN_REGISTER_ACTION(
		AddComponentsAction,
		{
			.key = "engine.add_components",
			.editor = {
				.label = "Add Components",
				.group = "Entity",
				.description = "Add registered components to the owner.",
				.menu_order = 1,
			},
		}
	);
	PTGN_REGISTER_ACTION(
		RemoveComponentsAction,
		{
			.key = "engine.remove_components",
			.editor = {
				.label = "Remove Components",
				.group = "Entity",
				.description = "Remove registered components from the owner.",
				.menu_order = 2,
				.separator_after = true,
			},
		}
	);
	PTGN_REGISTER_ACTION(
		SpawnEntityAction,
		{
			.key = "engine.spawn_entity",
			.editor = {
				.label = "Spawn Entity",
				.group = "Entity",
				.description = "Spawn one or more prefab instances.",
				.menu_order = 0,
			},
		}
	);
	PTGN_REGISTER_ACTION(
		ApplyDamageAction,
		{
			.key = "game.apply_damage",
			.editor = {
				.label = "Apply Damage",
				.group = "Game",
				.description = "Apply damage to the owning entity.",
			},
		}
	);

	PTGN_REGISTER_SCRIPT(
		SequenceScript,
		{
			.key = "engine.sequence",
			.editor = {
				.label = "Script Sequence",
				.group = "Sequence",
				.description = "Editor-authored Script built from registered Actions.",
			},
			.draw = [](SequenceScript&) { return false; },
		}
	);
	PTGN_REGISTER_SCRIPT(
		PlayerMovementScript,
		{
			.key = "game.player_movement",
			.editor = {
				.label = "Player Movement",
				.group = "Game",
				.description = "WASD movement implemented as a custom C++ Script.",
			},
		}
	);
}

} // namespace

void RegisterDemoEditorTypes() {
	PTGN_REGISTER(editor::ActionEditorRegistry::RegisterInline<ScaleToAction>(
		"engine.scale_to",
		{ .label = "Scale To", .group = "Transform", .description = "Scale the owning entity." },
		[](ScaleToAction& action, editor::EditorContextTemp&) {
			const float available{ ImGui::GetContentRegionAvail().x };
			const float spacing{ ImGui::GetStyle().ItemSpacing.x };
			const float mode_width{
				std::max(ImGui::CalcTextSize("Relative").x, ImGui::CalcTextSize("Absolute").x) +
				ImGui::GetStyle().FramePadding.x * 2.0f
			};
			const float field_width{
				std::max(36.0f, (available - mode_width - spacing * 2.0f) * 0.5f)
			};
			bool changed{ false };
			ImGui::SetNextItemWidth(field_width);
			changed |= ImGui::DragFloat(
				"##ScaleX", &action.scale.x, 0.01f, -100.0f, 100.0f, "X: %.2f"
			);
			ImGui::SameLine(0.0f, spacing);
			ImGui::SetNextItemWidth(field_width);
			changed |= ImGui::DragFloat(
				"##ScaleY", &action.scale.y, 0.01f, -100.0f, 100.0f, "Y: %.2f"
			);
			ImGui::SameLine(0.0f, spacing);
			if (ImGui::Button(
					action.relative ? "Relative" : "Absolute",
					ImVec2{ mode_width, ImGui::GetFrameHeight() }
				)) {
				action.relative = !action.relative;
				changed = true;
			}
			return changed;
		},
		[](ScaleToAction&, editor::EditorContextTemp&) { return false; }
	));

	PTGN_REGISTER(editor::ActionEditorRegistry::Register<FollowTargetAction>(
		"engine.follow_target",
		{ .label = "Follow Target", .group = "Transform", .description = "Move until the target is reached." },
		[](FollowTargetAction& action, editor::EditorContextTemp& context) {
			bool changed{ false };
			if (!ImGui::BeginTable(
					"FollowTargetParameters", 3,
					ImGuiTableFlags_SizingStretchProp
				)) {
				return false;
			}
			ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch, 1.35f);
			ImGui::TableSetupColumn("Speed", ImGuiTableColumnFlags_WidthStretch, 0.85f);
			ImGui::TableSetupColumn("StoppingDistance", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
			ImGui::TableSetColumnIndex(0);
			const std::string target_name{
				action.target ? std::string{ context.host.Name(action.target) } : "None"
			};
			const std::string preview{ "Target: " + target_name };
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##Target", preview.c_str())) {
				if (ImGui::Selectable("None", !action.target)) {
					action.target = {};
					changed = true;
				}
				for (ptgn::Entity entity : context.host.Entities()) {
					const std::string name{ context.host.Name(entity) };
					if (ImGui::Selectable(name.c_str(), entity == action.target)) {
						action.target = entity;
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			changed |= ImGui::DragFloat(
				"##Speed", &action.speed, 1.0f, 0.0f, 10000.0f, "Speed: %.0f"
			);
			ImGui::TableSetColumnIndex(2);
			ImGui::SetNextItemWidth(-FLT_MIN);
			changed |= ImGui::DragFloat(
				"##StoppingDistance", &action.stopping_distance,
				0.1f, 0.0f, 1000.0f, "Distance: %.1f"
			);
			ImGui::EndTable();
			return changed;
		}
	));

	PTGN_REGISTER(editor::ActionEditorRegistry::Register<ApplyDamageAction>(
		"game.apply_damage",
		{ .label = "Apply Damage", .group = "Game", .description = "Apply damage to the owner." },
		[](ApplyDamageAction& action, editor::EditorContextTemp&) {
			bool changed{ false };
			if (ImGui::BeginTable("DamageParams", 3, ImGuiTableFlags_SizingStretchProp)) {
				const float critical_width{
					ImGui::CalcTextSize("Critical").x + ImGui::GetFrameHeight() +
					ImGui::GetStyle().ItemInnerSpacing.x
				};
				ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthStretch, 0.75f);
				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.35f);
				ImGui::TableSetupColumn("Critical", ImGuiTableColumnFlags_WidthFixed, critical_width);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::DragFloat(
					"##Amount", &action.amount, 0.25f, 0.0f, 100000.0f, "%.2f"
				);
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::InputText("##DamageType", &action.damage_type);
				ImGui::TableSetColumnIndex(2);
				changed |= ImGui::Checkbox("Critical", &action.critical);
				ImGui::EndTable();
			}
			return changed;
		}
	));

	PTGN_REGISTER(editor::ScriptEditorRegistry::Register<PlayerMovementScript>(
		"game.player_movement",
		{ .label = "Player Movement", .group = "Game", .description = "WASD movement for the demo player." },
		[](PlayerMovementScript& script) {
			return ImGui::DragFloat("Speed", &script.speed, 1.0f, 0.0f, 2000.0f);
		}
	));
}

void PlayerMovementScript::OnUpdate() {
	if (!script_runtime::IsScriptEnabled(entity, this) || !entity || ImGui::GetIO().WantTextInput) {
		return;
	}

	ptgn::V2_float direction{};
	if (ImGui::IsKeyDown(ImGuiKey_A)) {
		direction.x -= 1.0f;
	}
	if (ImGui::IsKeyDown(ImGuiKey_D)) {
		direction.x += 1.0f;
	}
	if (ImGui::IsKeyDown(ImGuiKey_S)) {
		direction.y -= 1.0f;
	}
	if (ImGui::IsKeyDown(ImGuiKey_W)) {
		direction.y += 1.0f;
	}

	const float magnitude{ std::sqrt(direction.x * direction.x + direction.y * direction.y) };
	if (magnitude > 0.0f) {
		direction /= magnitude;
	}

	auto& transform{ entity.Get<ptgn::Transform>() };
	transform.position += direction * speed * script_runtime::DeltaSeconds();
	transform.position.x = std::clamp(transform.position.x, -430.0f, 430.0f);
	transform.position.y = std::clamp(transform.position.y, -250.0f, 250.0f);
}

void ApplyDamageAction::OnStart() {
	if (auto* health{ Owner().TryGet<Health>() }) {
		const float applied{ std::max(0.0f, amount) * (critical ? 2.0f : 1.0f) };
		health->current -= applied;
		LogScriptActivity(
			GetScene(),
			std::string{ GetEntityName(Owner()) } + " took " +
				std::to_string(static_cast<int>(applied)) + " " + damage_type + " damage"
		);
		if (health->current <= 0.0f) {
			static_cast<DemoScene&>(GetScene()).RequestDestroy(Owner());
		}
	}
}

namespace script_runtime {
namespace {

float current_delta_seconds{ 0.0f };

[[nodiscard]] ptgn::Script* EnsureInstance(
	ptgn::Entity owner, ScriptEntry& entry
) {
	if (!entry.instance) {
		const auto* registration{ ScriptRegistry::Find(entry.type_hash) };
		if (!registration) {
			return nullptr;
		}
		entry.instance = entry.attach_live
			? entry.attach_live(owner)
			: (registration->attach
				? registration->attach(owner, entry.value)
				: nullptr);
	}
	return entry.instance;
}

[[nodiscard]] SequenceScript* AsSequence(
	ptgn::Entity owner, ScriptEntry& entry
) {
	if (entry.type_hash != ptgn::Hash<SequenceScript>()) {
		return nullptr;
	}
	return dynamic_cast<SequenceScript*>(EnsureInstance(owner, entry));
}

[[nodiscard]] ScriptEntry* FindSequenceEntry(ptgn::Entity owner, SequenceId id) {
	if (!owner) {
		return nullptr;
	}
	auto* scripts{ owner.TryGet<ScriptsComponent>() };
	if (!scripts) {
		return nullptr;
	}

	auto find = [&](auto& entries) -> ScriptEntry* {
		for (auto& entry : entries) {
			if (auto* script{ AsSequence(owner, entry) };
				script && script->sequence.id == id) {
				return &entry;
			}
		}
		return nullptr;
	};

	if (auto* entry{ find(scripts->scripts) }) {
		return entry;
	}
	return find(scripts->pending_additions);
}

[[nodiscard]] ScriptSequence* FindBinding(ptgn::Entity owner, SequenceId id) {
	auto* entry{ FindSequenceEntry(owner, id) };
	if (!entry) {
		return nullptr;
	}
	auto* script{ AsSequence(owner, *entry) };
	return script ? &script->sequence : nullptr;
}

[[nodiscard]] SequenceChannelRuntime* FindChannel(
	ScriptsComponent& scripts, const SequenceChannelKey& key
) {
	const auto it{ std::ranges::find_if(
		scripts.channels,
		[&](const SequenceChannelRuntime& channel) {
			return channel.key == key;
		}
	) };
	return it == scripts.channels.end() ? nullptr : &*it;
}

[[nodiscard]] SequenceChannelRuntime& FindOrCreateChannel(
	ScriptsComponent& scripts, const SequenceChannelKey& key
) {
	if (auto* channel{ FindChannel(scripts, key) }) {
		return *channel;
	}
	scripts.channels.push_back(SequenceChannelRuntime{ .key = key });
	return scripts.channels.back();
}

void ExecuteInstant(ptgn::Entity owner, const Action& action);
void ProcessImmediateActions(ptgn::Entity owner, ScriptSequence& binding);
void CompleteSequence(ptgn::Entity owner, ScriptSequence& binding);
void UpdateSequence(ptgn::Entity owner, ScriptSequence& binding, float delta_seconds);

void InvokeLifecycle(
	ptgn::Entity owner,
	ScriptSequence& binding,
	SequenceLifecycle lifecycle
) {
	const auto* sequence{ Resolve(owner, binding) };
	if (!sequence) {
		return;
	}
	for (const auto& callback : sequence->lifecycle_actions) {
		if (callback.enabled && callback.lifecycle == lifecycle) {
			ExecuteInstant(owner, callback.action);
		}
	}
}

void PromoteNextInChannel(
	ptgn::Entity owner, SequenceChannelRuntime& channel
) {
	while (!channel.waiting.empty()) {
		const SequenceId id{ channel.waiting.front() };
		channel.waiting.pop_front();
		if (auto* binding{ FindBinding(owner, id) }) {
			binding->runtime.waiting_for_channel = false;
			if (Start(owner, id, true)) {
				return;
			}
		}
	}
}

void ReleaseChannel(
	ptgn::Entity owner,
	ScriptSequence& binding,
	bool promote
) {
	const auto* sequence{ Resolve(owner, binding) };
	if (!sequence || !sequence->channel || !owner.Has<ScriptsComponent>()) {
		return;
	}

	auto& scripts{ owner.Get<ScriptsComponent>() };
	auto* channel{ FindChannel(scripts, *sequence->channel) };
	if (!channel) {
		return;
	}
	if (channel->active == binding.id) {
		channel->active.reset();
	}
	std::erase(channel->waiting, binding.id);
	if (promote) {
		PromoteNextInChannel(owner, *channel);
	}
}

[[nodiscard]] bool CancelBinding(
	ptgn::Entity owner,
	ScriptSequence& binding,
	SequenceCancelReason reason,
	bool log,
	bool promote_channel,
	bool remove_transient = true
) {
	const auto* sequence{ Resolve(owner, binding) };
	const bool active{
		binding.runtime.running || binding.runtime.waiting_for_channel
	};

	if (binding.runtime.action_instance) {
		binding.runtime.action_instance->SetFrame(
			0.0f, 0.0f, 0.0f,
			binding.runtime.current_repeat,
			binding.runtime.currently_reversed
		);
		binding.runtime.action_instance->OnCancel(reason);
		InvokeLifecycle(owner, binding, SequenceLifecycle::ActionCancel);
	}
	if (active) {
		InvokeLifecycle(owner, binding, SequenceLifecycle::Stop);
	}
	if (log && sequence && active) {
		LogScriptActivity(
			owner.GetScene(),
			std::string{ GetEntityName(owner) } + " / " + sequence->name + " stopped"
		);
	}

	ReleaseChannel(owner, binding, promote_channel);
	const int completed_runs{ binding.runtime.completed_runs };
	binding.runtime = ScriptSequenceRuntime{};
	binding.runtime.completed_runs = completed_runs;

	if (remove_transient && sequence &&
		(sequence->transient || sequence->remove_binding_on_complete) &&
		reason != SequenceCancelReason::Reset &&
		reason != SequenceCancelReason::BindingRemoved &&
		reason != SequenceCancelReason::OwnerDestroyed &&
		owner.Has<ScriptsComponent>()) {
		owner.Get<ScriptsComponent>().RemoveDeferred(binding.id);
	}
	return active;
}

[[nodiscard]] bool StartBinding(
	ptgn::Entity owner,
	ScriptSequence& binding,
	bool force
) {
	const auto* sequence{ Resolve(owner, binding) };
	if (!sequence || !sequence->enabled || !binding.enabled) {
		return false;
	}

	auto& runtime{ binding.runtime };
	if (runtime.running) {
		const ReentryMode mode{ force ? ReentryMode::Restart : sequence->reentry };
		switch (mode) {
			case ReentryMode::IgnoreWhileRunning:
				return false;
			case ReentryMode::Restart:
				(void)CancelBinding(
					owner, binding, SequenceCancelReason::Replaced,
					false, false, false
				);
				break;
			case ReentryMode::Queue:
				++runtime.queued_runs;
				return true;
		}
	}

	if (sequence->channel) {
		auto& scripts{ owner.TryAdd<ScriptsComponent>() };
		auto& channel{ FindOrCreateChannel(scripts, *sequence->channel) };
		if (channel.active && *channel.active != binding.id) {
			const ReentryMode mode{ force ? ReentryMode::Restart : sequence->reentry };
			switch (mode) {
				case ReentryMode::IgnoreWhileRunning:
					return false;
				case ReentryMode::Restart:
					script_runtime::StopChannel(
						owner, *sequence->channel, SequenceStopMode::All
					);
					break;
				case ReentryMode::Queue:
					if (!std::ranges::contains(channel.waiting, binding.id)) {
						channel.waiting.push_back(binding.id);
					}
					runtime.waiting_for_channel = true;
					return true;
			}
		}
		channel.active = binding.id;
		runtime.waiting_for_channel = false;
	}

	const int completed_runs{ runtime.completed_runs };
	const int queued_runs{ runtime.queued_runs };
	runtime = ScriptSequenceRuntime{};
	runtime.completed_runs = completed_runs;
	runtime.queued_runs = queued_runs;
	runtime.running = true;

	LogScriptActivity(
		owner.GetScene(),
		std::string{ GetEntityName(owner) } + " / " + sequence->name + " started"
	);
	InvokeLifecycle(owner, binding, SequenceLifecycle::Start);
	ProcessImmediateActions(owner, binding);
	return true;
}

void ExecuteInstant(ptgn::Entity owner, const Action& action) {
	const auto* registration{ ActionRegistry::Find(action.type_hash) };
	if (!registration || !action.enabled) {
		return;
	}
	auto instance{
		action.runtime_factory
			? action.runtime_factory(owner)
			: registration->instantiate(owner, action.value)
	};
	if (!instance) {
		return;
	}
	instance->SetFrame(0.0f, 1.0f, 1.0f, 0, false);
	instance->OnStart();
	(void)instance->OnUpdate();
	instance->OnComplete();
}

void ProcessImmediateActions(
	ptgn::Entity owner, ScriptSequence& binding
) {
	const auto* sequence{ Resolve(owner, binding) };
	if (!sequence) {
		return;
	}
	auto& runtime{ binding.runtime };
	while (runtime.running && runtime.action_index < sequence->actions.size()) {
		const auto& action{ sequence->actions[runtime.action_index] };
		if (!action.enabled) {
			++runtime.action_index;
			continue;
		}
		const auto* registration{ ActionRegistry::Find(action.type_hash) };
		if (!registration) {
			++runtime.action_index;
			continue;
		}
		const ActionCompletion completion{
			action.completion.value_or(registration->completion)
		};
		if (completion != ActionCompletion::Instant) {
			return;
		}
		InvokeLifecycle(owner, binding, SequenceLifecycle::ActionStart);
		ExecuteInstant(owner, action);
		InvokeLifecycle(owner, binding, SequenceLifecycle::ActionComplete);
		++runtime.action_index;
	}
	if (runtime.running && runtime.action_index >= sequence->actions.size()) {
		CompleteSequence(owner, binding);
	}
}

void CompleteCurrentAction(
	ptgn::Entity owner, ScriptSequence& binding
) {
	auto& runtime{ binding.runtime };
	if (runtime.action_instance) {
		runtime.action_instance->SetFrame(
			0.0f, 1.0f, 1.0f,
			runtime.current_repeat,
			runtime.currently_reversed
		);
		runtime.action_instance->OnComplete();
	}
	InvokeLifecycle(owner, binding, SequenceLifecycle::ActionComplete);
	++runtime.action_index;
	runtime.ClearActiveAction();
	ProcessImmediateActions(owner, binding);
}

void CompleteSequence(
	ptgn::Entity owner, ScriptSequence& binding
) {
	const auto* sequence{ Resolve(owner, binding) };
	if (!sequence) {
		return;
	}
	const bool remove_binding{
		sequence->transient || sequence->remove_binding_on_complete
	};
	const bool destroy_owner{ sequence->destroy_owner_on_complete };
	const int queued_runs{ binding.runtime.queued_runs };
	const int completed_runs{ binding.runtime.completed_runs + 1 };

	binding.runtime.running = false;
	binding.runtime.paused = false;
	binding.runtime.completed = true;
	binding.runtime.completed_runs = completed_runs;
	binding.runtime.action_instance.reset();

	InvokeLifecycle(owner, binding, SequenceLifecycle::Complete);
	LogScriptActivity(
		owner.GetScene(),
		std::string{ GetEntityName(owner) } + " / " + sequence->name + " completed"
	);

	if (queued_runs > 0 && !destroy_owner) {
		binding.runtime.queued_runs = queued_runs - 1;
		(void)StartBinding(owner, binding, true);
		return;
	}
	ReleaseChannel(owner, binding, true);
	if (destroy_owner) {
		static_cast<DemoScene&>(owner.GetScene()).RequestDestroy(owner);
		return;
	}
	if (remove_binding && owner.Has<ScriptsComponent>()) {
		owner.Get<ScriptsComponent>().RemoveDeferred(binding.id);
	}
}

void UpdateSequence(
	ptgn::Entity owner,
	ScriptSequence& binding,
	float delta_seconds
) {
	auto& runtime{ binding.runtime };
	if (!runtime.running || runtime.paused || runtime.waiting_for_channel) {
		return;
	}
	const auto* sequence{ Resolve(owner, binding) };
	if (!sequence || runtime.action_index >= sequence->actions.size()) {
		CompleteSequence(owner, binding);
		return;
	}

	const auto& action{ sequence->actions[runtime.action_index] };
	if (!action.enabled) {
		++runtime.action_index;
		ProcessImmediateActions(owner, binding);
		return;
	}
	const auto* registration{ ActionRegistry::Find(action.type_hash) };
	if (!registration) {
		++runtime.action_index;
		ProcessImmediateActions(owner, binding);
		return;
	}
	const ActionCompletion completion{
		action.completion.value_or(registration->completion)
	};
	if (completion == ActionCompletion::Instant) {
		ProcessImmediateActions(owner, binding);
		return;
	}

	if (!runtime.action_instance) {
		runtime.action_instance = action.runtime_factory
			? action.runtime_factory(owner)
			: registration->instantiate(owner, action.value);
		if (!runtime.action_instance) {
			++runtime.action_index;
			ProcessImmediateActions(owner, binding);
			return;
		}
		runtime.currently_reversed = action.timing && action.timing->reversed;
		runtime.action_instance->SetFrame(
			0.0f, 0.0f, 0.0f, 0, runtime.currently_reversed
		);
		runtime.action_instance->OnStart();
		InvokeLifecycle(owner, binding, SequenceLifecycle::ActionStart);
	}

	const float delta_ms{ std::max(0.0f, delta_seconds) * 1000.0f };
	if (action.timing) {
		runtime.elapsed_ms += delta_ms;
	}
	float linear{ 0.0f };
	float progress{ 0.0f };
	if (action.timing) {
		const auto& timing{ *action.timing };
		linear = timing.duration_ms <= 0.0f
			? 1.0f
			: std::clamp(runtime.elapsed_ms / timing.duration_ms, 0.0f, 1.0f);
		const float directed{ runtime.currently_reversed ? 1.0f - linear : linear };
		progress = ptgn::ApplyEase(directed, timing.ease);
	}

	runtime.action_instance->SetFrame(
		delta_seconds, linear, progress,
		runtime.current_repeat, runtime.currently_reversed
	);
	const ActionStatus status{ runtime.action_instance->OnUpdate() };

	bool complete{ false };
	switch (completion) {
		case ActionCompletion::Instant:
			complete = true;
			break;
		case ActionCompletion::Duration:
			complete = status == ActionStatus::Complete || linear >= 1.0f;
			break;
		case ActionCompletion::ActionControlled:
			complete = status == ActionStatus::Complete;
			break;
		case ActionCompletion::Infinite:
			complete = false;
			break;
	}
	if (!complete) {
		return;
	}

	const bool repeat{
		action.timing &&
		(action.timing->infinite_repeats ||
		 runtime.current_repeat < action.timing->additional_repeats)
	};
	if (repeat) {
		++runtime.current_repeat;
		runtime.elapsed_ms = 0.0f;
		InvokeLifecycle(owner, binding, SequenceLifecycle::Repeat);
		if (action.timing->yoyo) {
			runtime.currently_reversed = !runtime.currently_reversed;
			InvokeLifecycle(owner, binding, SequenceLifecycle::Yoyo);
		}
		runtime.action_instance->SetFrame(
			0.0f, 0.0f, 0.0f,
			runtime.current_repeat, runtime.currently_reversed
		);
		runtime.action_instance->OnRepeat();
		return;
	}
	CompleteCurrentAction(owner, binding);
}

} // namespace

void AttachEntry(ptgn::Entity entity, ScriptEntry& entry) {
	if (!entity || !entry.type_hash) {
		return;
	}
	(void)EnsureInstance(entity, entry);
}

void AttachAll(ptgn::Entity entity) {
	if (!entity) {
		return;
	}
	auto* scripts{ entity.TryGet<ScriptsComponent>() };
	if (!scripts) {
		return;
	}
	for (auto& entry : scripts->scripts) {
		AttachEntry(entity, entry);
	}
	for (auto& entry : scripts->pending_additions) {
		AttachEntry(entity, entry);
	}
}

void ApplyPending(ptgn::Scene& scene) {
	const auto entities{ scene.EntitiesWith<ScriptsComponent>().GetVector() };
	for (ptgn::Entity entity : entities) {
		auto* scripts_ptr{ entity.TryGet<ScriptsComponent>() };
		if (!scripts_ptr) {
			continue;
		}
		auto& scripts{ *scripts_ptr };
		for (const SequenceId id : scripts.pending_removals) {
			ScriptEntry* target{ FindSequenceEntry(entity, id) };
			if (auto* binding{ FindBinding(entity, id) }) {
				(void)CancelBinding(
					entity, *binding, SequenceCancelReason::BindingRemoved,
					false, true, false
				);
			}
			if (target) {
				target->enabled = false;
				ptgn::Script* target_instance{ target->instance };
				if (auto* sequence{ dynamic_cast<SequenceScript*>(target_instance) }) {
					sequence->sequence.enabled = false;
				}
				std::erase_if(scripts.scripts, [target_instance](const ScriptEntry& entry) {
					return entry.instance == target_instance;
				});
			}
		}
		scripts.pending_removals.clear();
		for (auto& entry : scripts.pending_additions) {
			scripts.scripts.push_back(std::move(entry));
			AttachEntry(entity, scripts.scripts.back());
		}
		scripts.pending_additions.clear();
	}
}

void Update(ptgn::Scene& scene, float delta_seconds) {
	current_delta_seconds = std::max(0.0f, delta_seconds);
	ApplyPending(scene);
	const auto entities{ scene.EntitiesWith<ScriptsComponent>().GetVector() };
	for (ptgn::Entity entity : entities) {
		AttachAll(entity);
	}
}

float DeltaSeconds() {
	return current_delta_seconds;
}

ScriptSequence* Resolve(
	ptgn::Entity owner, ScriptSequence& binding
) {
	return binding.shared_reference
		? GetSharedScriptSequences(owner.GetScene()).Find(binding.shared_sequence_id)
		: &binding;
}

const ScriptSequence* Resolve(
	ptgn::Entity owner, const ScriptSequence& binding
) {
	return binding.shared_reference
		? GetSharedScriptSequences(owner.GetScene()).Find(binding.shared_sequence_id)
		: &binding;
}

SequenceHandle RunSequence(
	ptgn::Entity owner, ScriptSequence sequence
) {
	if (!owner) {
		return {};
	}
	auto& scripts{ owner.TryAdd<ScriptsComponent>() };
	const SequenceId id{ sequence.id };
	ScriptEntry entry{ MakeSequenceEntry(std::move(sequence)) };
	scripts.pending_additions.push_back(std::move(entry));
	AttachEntry(owner, scripts.pending_additions.back());
	return SequenceHandle{ .owner = owner, .binding_id = id };
}

SequenceHandle RunInChannel(
	ptgn::Entity owner,
	SequenceChannelKey channel,
	ScriptSequence sequence,
	ReentryMode reentry
) {
	sequence.channel = std::move(channel);
	sequence.reentry = reentry;
	return RunSequence(owner, std::move(sequence));
}

bool Start(ptgn::Entity owner, SequenceId id, bool force) {
	auto* binding{ FindBinding(owner, id) };
	return binding && StartBinding(owner, *binding, force);
}

void StopChannel(
	ptgn::Entity owner,
	SequenceChannelKey channel_key,
	SequenceStopMode mode
) {
	if (!owner || !owner.Has<ScriptsComponent>()) {
		return;
	}
	auto& scripts{ owner.Get<ScriptsComponent>() };
	auto* channel{ FindChannel(scripts, channel_key) };
	if (!channel) {
		return;
	}
	std::vector<SequenceId> ids;
	if (channel->active) {
		ids.push_back(*channel->active);
	}
	if (mode == SequenceStopMode::All) {
		ids.insert(ids.end(), channel->waiting.begin(), channel->waiting.end());
	}
	channel->active.reset();
	channel->waiting.clear();
	for (SequenceId id : ids) {
		if (auto* binding{ FindBinding(owner, id) }) {
			(void)CancelBinding(
				owner, *binding, SequenceCancelReason::Replaced,
				false, false
			);
		}
	}
}

bool Stop(ptgn::Entity owner, SequenceId id, SequenceCancelReason reason) {
	auto* binding{ FindBinding(owner, id) };
	return binding && CancelBinding(owner, *binding, reason, true, true);
}

bool Reset(ptgn::Entity owner, SequenceId id) {
	auto* binding{ FindBinding(owner, id) };
	if (!binding) {
		return false;
	}
	(void)CancelBinding(
		owner, *binding, SequenceCancelReason::Reset,
		false, true
	);
	InvokeLifecycle(owner, *binding, SequenceLifecycle::Reset);
	return true;
}

bool Clear(ptgn::Entity owner, SequenceId id) {
	auto* binding{ FindBinding(owner, id) };
	if (!binding) {
		return false;
	}
	(void)CancelBinding(
		owner, *binding, SequenceCancelReason::Cleared,
		false, true
	);
	if (binding->shared_reference || binding->transient) {
		owner.Get<ScriptsComponent>().RemoveDeferred(id);
	} else {
		binding->actions.clear();
		binding->start_events.clear();
		binding->stop_events.clear();
		binding->lifecycle_actions.clear();
	}
	return true;
}

bool Skip(ptgn::Entity owner, SequenceId id) {
	auto* binding{ FindBinding(owner, id) };
	if (!binding || !binding->runtime.running) {
		return false;
	}
	if (binding->runtime.action_instance) {
		binding->runtime.action_instance->OnCancel(SequenceCancelReason::Skipped);
		InvokeLifecycle(owner, *binding, SequenceLifecycle::ActionCancel);
	}
	++binding->runtime.action_index;
	binding->runtime.ClearActiveAction();
	ProcessImmediateActions(owner, *binding);
	return true;
}

bool Seek(ptgn::Entity owner, SequenceId id, float progress) {
	auto* binding{ FindBinding(owner, id) };
	if (!binding) {
		return false;
	}
	if (!binding->runtime.running && !StartBinding(owner, *binding, true)) {
		return false;
	}
	const auto* sequence{ Resolve(owner, *binding) };
	if (!sequence || binding->runtime.action_index >= sequence->actions.size()) {
		return false;
	}
	const auto& action{ sequence->actions[binding->runtime.action_index] };
	if (!action.timing) {
		return false;
	}
	binding->runtime.elapsed_ms =
		std::clamp(progress, 0.0f, 1.0f) *
		std::max(0.0f, action.timing->duration_ms);
	UpdateSequence(owner, *binding, 0.0f);
	return true;
}

bool SetPaused(ptgn::Entity owner, SequenceId id, bool paused) {
	auto* binding{ FindBinding(owner, id) };
	if (!binding || !binding->runtime.running || binding->runtime.paused == paused) {
		return false;
	}
	binding->runtime.paused = paused;
	InvokeLifecycle(
		owner, *binding,
		paused ? SequenceLifecycle::Pause : SequenceLifecycle::Resume
	);
	return true;
}

float Progress(ptgn::Entity owner, SequenceId id) {
	const auto* binding{ FindBinding(owner, id) };
	if (!binding) {
		return 0.0f;
	}
	const auto* sequence{ Resolve(owner, *binding) };
	if (!sequence || !binding->runtime.running ||
		binding->runtime.action_index >= sequence->actions.size()) {
		return binding->runtime.completed ? 1.0f : 0.0f;
	}
	const auto& action{ sequence->actions[binding->runtime.action_index] };
	if (!action.timing || action.timing->duration_ms <= 0.0f) {
		return 0.0f;
	}
	return std::clamp(
		binding->runtime.elapsed_ms / action.timing->duration_ms,
		0.0f, 1.0f
	);
}

bool IsRunning(ptgn::Entity owner, SequenceId id) {
	const auto* binding{ FindBinding(owner, id) };
	return binding && binding->runtime.running;
}

bool IsPaused(ptgn::Entity owner, SequenceId id) {
	const auto* binding{ FindBinding(owner, id) };
	return binding && binding->runtime.paused;
}

bool IsCompleted(ptgn::Entity owner, SequenceId id) {
	const auto* binding{ FindBinding(owner, id) };
	return binding && binding->runtime.completed;
}

} // namespace script_runtime

void SequenceScript::OnCreate() {}

void SequenceScript::OnUpdate() {
	if (!script_runtime::IsScriptEnabled(entity, this)) {
		return;
	}
	if (!initialized) {
		initialized = true;
		if (sequence.start_events.empty()) {
			(void)script_runtime::Start(entity, sequence.id);
		}
	}
	script_runtime::UpdateSequence(entity, sequence, script_runtime::DeltaSeconds());
}

void SequenceScript::OnEvent(Event event) {
	if (!script_runtime::IsScriptEnabled(entity, this)) {
		return;
	}
	ObserveInputExpressionEvent(event);
	const auto* definition{ script_runtime::Resolve(entity, sequence) };
	if (!definition) {
		return;
	}

	for (const auto& condition : definition->stop_events) {
		if (!condition.enabled) {
			continue;
		}
		const auto* registration{ SequenceEventRegistry::Find(condition.type_hash) };
		if (registration && registration->matches(
				entity, event, condition, condition.consume
			)) {
			(void)script_runtime::Stop(entity, sequence.id);
			return;
		}
	}

	for (const auto& condition : definition->start_events) {
		if (!condition.enabled) {
			continue;
		}
		const auto* registration{ SequenceEventRegistry::Find(condition.type_hash) };
		if (registration && registration->matches(
				entity, event, condition, condition.consume
			)) {
			(void)script_runtime::Start(entity, sequence.id);
			return;
		}
	}
}

bool SequenceHandle::Start(bool force) const {
	return *this && script_runtime::Start(owner, binding_id, force);
}
bool SequenceHandle::Stop() const {
	return *this && script_runtime::Stop(owner, binding_id);
}
bool SequenceHandle::Pause() const {
	return *this && script_runtime::SetPaused(owner, binding_id, true);
}
bool SequenceHandle::Resume() const {
	return *this && script_runtime::SetPaused(owner, binding_id, false);
}
bool SequenceHandle::TogglePaused() const {
	return *this && script_runtime::SetPaused(
		owner, binding_id, !script_runtime::IsPaused(owner, binding_id)
	);
}
bool SequenceHandle::Toggle() const {
	return IsRunning() ? Stop() : Start();
}
bool SequenceHandle::Reset() const {
	return *this && script_runtime::Reset(owner, binding_id);
}
bool SequenceHandle::Clear() const {
	return *this && script_runtime::Clear(owner, binding_id);
}
bool SequenceHandle::Skip() const {
	return *this && script_runtime::Skip(owner, binding_id);
}
bool SequenceHandle::Seek(float progress) const {
	return *this && script_runtime::Seek(owner, binding_id, progress);
}
bool SequenceHandle::IsRunning() const {
	return *this && script_runtime::IsRunning(owner, binding_id);
}
bool SequenceHandle::IsPaused() const {
	return *this && script_runtime::IsPaused(owner, binding_id);
}
bool SequenceHandle::IsCompleted() const {
	return *this && script_runtime::IsCompleted(owner, binding_id);
}
float SequenceHandle::Progress() const {
	return *this ? script_runtime::Progress(owner, binding_id) : 0.0f;
}

bool script_runtime::IsScriptEnabled(
	ptgn::Entity entity, const ptgn::Script* script
) {
	if (!entity || !script) {
		return false;
	}
	const auto* scripts{ entity.TryGet<ScriptsComponent>() };
	if (!scripts) {
		return false;
	}
	auto find_enabled = [script](const auto& entries) {
		const auto it{ std::ranges::find_if(entries, [script](const ScriptEntry& entry) {
			return entry.instance == script;
		}) };
		return it != entries.end() && it->enabled;
	};
	return find_enabled(scripts->scripts) || find_enabled(scripts->pending_additions);
}

std::vector<std::string> DemoScene::ActivityText() const {
	std::vector<std::string> result;
	result.reserve(activity_.size());
	for (const auto& entry : activity_) {
		result.push_back(entry.text);
	}
	return result;
}

std::string_view DemoScene::Name(ptgn::Entity entity) const {
	return GetEntityName(entity);
}

std::string& DemoScene::EditableName(ptgn::Entity entity) {
	return entity.TryAdd<NameComponent>().value;
}

std::string& DemoScene::EditableTag(ptgn::Entity entity) {
	return entity.TryAdd<ptgn::Tag>().value;
}

int DemoScene::Mask(ptgn::Entity entity) const {
	return GetEntityMask(entity);
}

editor::EditorVisual DemoScene::Visual(ptgn::Entity entity) const {
	editor::EditorVisual result;
	if (entity && entity.Has<DemoVisual>()) {
		const auto& visual{ entity.Get<DemoVisual>() };
		result.color = visual.color;
		result.sensor = visual.sensor;
	}
	if (entity && entity.Has<ButtonData>() && entity.Has<ButtonStyle>()) {
		const auto& button{ entity.Get<ButtonData>() };
		const auto& style{ entity.Get<ButtonStyle>() };
		switch (button.state) {
			case ButtonState::Idle: result.color = style.idle; break;
			case ButtonState::Hovered: result.color = style.hovered; break;
			case ButtonState::Pressed: result.color = style.pressed; break;
			case ButtonState::Disabled: result.color = style.disabled; break;
		}
	}
	if (entity && entity.Has<Health>()) {
		const auto& health{ entity.Get<Health>() };
		result.health_fraction = health.maximum > 0.0f
			? std::clamp(health.current / health.maximum, 0.0f, 1.0f)
			: 0.0f;
	}
	return result;
}

void DemoScene::Log(std::string text) {
	activity_.push_back(ActivityEntry{ .text = std::move(text) });
	constexpr std::size_t maximum{ 18 };
	if (activity_.size() > maximum) {
		activity_.erase(activity_.begin());
	}
}

void DemoScene::RequestDestroy(ptgn::Entity entity) {
	if (entity && !std::ranges::contains(pending_destroy_, entity)) {
		pending_destroy_.push_back(entity);
	}
}

ptgn::Entity DemoScene::FindByTag(std::string_view tag) const {
	return GetEntity(ptgn::Tag{ tag });
}

ptgn::Entity DemoScene::CreateEntity(std::string name, std::string tag) {
	ptgn::Entity entity{ ptgn::Scene::CreateEntity(ptgn::Tag{ tag }) };
	entity.TryAdd<NameComponent>().value = std::move(name);
	entity.TryAdd<ptgn::Transform>();
	entity.TryAdd<ptgn::Visible>(true);
	entity.TryAdd<DemoVisual>();
	entity.TryAdd<MaskComponent>();
	return entity;
}

ptgn::Entity DemoScene::SpawnPrefab(
	const PrefabDefinition& prefab,
	ptgn::V2_float position
) {
	ptgn::Entity entity{ CreateEntity(prefab.name, prefab.tag) };
	for (const auto& component : prefab.components) {
		if (component.apply_live) {
			component.apply_live(entity);
			continue;
		}
		const auto* registration{ ptgn::ComponentRegistry::Find(component.type) };
		if (!registration) {
			continue;
		}
		if (registration->is_empty && registration->add_default) {
			registration->add_default(entity);
		} else if (registration->deserialize && !component.value.is_null()) {
			registration->deserialize(component.value, entity);
		} else if (registration->add_default) {
			registration->add_default(entity);
		}
	}
	if (prefab.scripts) {
		entity.Add<ScriptsComponent>(*prefab.scripts);
		script_runtime::AttachAll(entity);
	}
	entity.Get<ptgn::Transform>().position = position;
	Log("Spawned prefab " + prefab.key);
	return entity;
}

ptgn::Entity DemoScene::SpawnPrefab(const PrefabSpawnRequest& request) {
	const auto* prefab{ prefabs_.Find(request.prefab_key) };
	if (!prefab) {
		return {};
	}
	ptgn::Entity entity{ SpawnPrefab(*prefab, request.position) };
	if (request.random_rotation && entity) {
		static std::mt19937 generator{ std::random_device{}() };
		std::uniform_real_distribution<float> angle{ -180.0f, 180.0f };
		entity.Get<ptgn::Transform>().rotation = ptgn::Degrees{ angle(generator) }.ToRad();
	}
	return entity;
}

void DemoScene::CreatePrefabs() {
	PrefabDefinition zombie;
	zombie.key = "prefabs/zombie";
	zombie.name = "Zombie";
	zombie.tag = "Zombie";
	zombie.components.push_back(
		MakeComponentDefinition(ptgn::Rect{ ptgn::V2_float{ 32.0f, 32.0f } })
	);
	zombie.components.push_back(
		MakeComponentDefinition(DemoVisual{
			.color = ImVec4{ 0.45f, 0.75f, 0.30f, 1.0f },
		})
	);
	zombie.components.push_back(MakeComponentDefinition(Health{}));
	zombie.components.push_back(MakeComponentDefinition(Zombie{}));
	prefabs_.definitions.push_back(std::move(zombie));

	PrefabDefinition circle;
	circle.key = "prefabs/recall_circle";
	circle.name = "Recall Circle";
	circle.tag = "SpawnedCircle";
	circle.components.push_back(MakeComponentDefinition(ptgn::Circle{ 13.0f }));
	circle.components.push_back(
		MakeComponentDefinition(DemoVisual{
			.color = ImVec4{ 0.78f, 0.42f, 0.88f, 1.0f },
		})
	);
	ScriptsComponent circle_scripts;
	ScriptSequence destroy_sequence{ "Destroy on Recall" };
	destroy_sequence
		.StartOn("ptgn.event.Signal", ptgn::json{ { "signal", "spawned_circles.destroy" } })
		.DestroyOwnerOnComplete();
	circle_scripts.scripts.push_back(MakeSequenceEntry(std::move(destroy_sequence)));
	circle.scripts = std::move(circle_scripts);
	prefabs_.definitions.push_back(std::move(circle));
}

void DemoScene::CreateDemoScene() {
	ScriptSequence opened_indicator{ "Door Opened Indicator" };
	opened_indicator
		.Reentry(ReentryMode::Restart)
		.StartOn("ptgn.event.Signal", ptgn::json{ { "signal", "door.opened" } })
		.StopOn("ptgn.event.Signal", ptgn::json{ { "signal", "door.closed" } })
		.During(350.0f, MoveToAction{ { 0.0f, 55.0f }, true })
		.Ease(ptgn::Ease::OutBack);
	const SequenceId opened_indicator_id{ opened_indicator.id };
	shared_sequences_.sequences.push_back(std::move(opened_indicator));

	ScriptSequence closed_indicator{ "Door Closed Indicator" };
	closed_indicator
		.Reentry(ReentryMode::Restart)
		.StartOn("ptgn.event.Signal", ptgn::json{ { "signal", "door.closed" } })
		.StopOn("ptgn.event.Signal", ptgn::json{ { "signal", "door.opened" } })
		.During(350.0f, MoveToAction{ { 300.0f, -110.0f }, false })
		.Ease(ptgn::Ease::OutCubic);
	const SequenceId closed_indicator_id{ closed_indicator.id };
	shared_sequences_.sequences.push_back(std::move(closed_indicator));

	ptgn::Entity player{ CreateEntity("Player", "Player") };
	player.Get<ptgn::Transform>().position = { -330.0f, 0.0f };
	player.Add<ptgn::Rect>(ptgn::V2_float{ 38.0f, 38.0f });
	player.Get<DemoVisual>().color = ImVec4{ 0.27f, 0.59f, 0.96f, 1.0f };
	AttachScript(player, PlayerMovementScript{});

	ptgn::Entity sensor{ CreateEntity("Door Sensor", "Door") };
	sensor.Get<ptgn::Transform>().position = { -70.0f, 0.0f };
	sensor.Add<ptgn::Rect>(ptgn::V2_float{ 110.0f, 170.0f });
	sensor.Get<DemoVisual>().color = ImVec4{ 0.23f, 0.75f, 0.45f, 0.30f };
	sensor.Get<DemoVisual>().sensor = true;
	ScriptSequence sensor_enter{ "Emit Door Opened" };
	sensor_enter
		.StartOn("ptgn.event.OverlapStart", ptgn::json{ { "tags", "Player" }, { "masks", "" } })
		.EmitSignal(SignalKey{ "door.opened" });
	AttachSequence(sensor, std::move(sensor_enter));
	ScriptSequence sensor_exit{ "Emit Door Closed" };
	sensor_exit
		.StartOn("ptgn.event.OverlapStop", ptgn::json{ { "tags", "Player" }, { "masks", "" } })
		.EmitSignal(SignalKey{ "door.closed" });
	AttachSequence(sensor, std::move(sensor_exit));

	ptgn::Entity panel{ CreateEntity("Sliding Panel", "MovingPanel") };
	panel.Get<ptgn::Transform>().position = { 70.0f, 0.0f };
	panel.Add<ptgn::Rect>(ptgn::V2_float{ 62.0f, 170.0f });
	panel.Get<DemoVisual>().color = ImVec4{ 0.88f, 0.57f, 0.24f, 1.0f };
	ScriptSequence open_sequence{ "Open Sliding Panel" };
	open_sequence
		.Reentry(ReentryMode::Restart)
		.StartOn("ptgn.event.Signal", ptgn::json{ { "signal", "door.opened" } })
		.StopOn("ptgn.event.Signal", ptgn::json{ { "signal", "door.closed" } })
		.During(500.0f, MoveToAction{ { 225.0f, 0.0f }, false })
		.Ease(ptgn::Ease::OutCubic);
	AttachSequence(panel, std::move(open_sequence));
	ScriptSequence close_sequence{ "Close Sliding Panel" };
	close_sequence
		.Reentry(ReentryMode::Restart)
		.StartOn("ptgn.event.Signal", ptgn::json{ { "signal", "door.closed" } })
		.StopOn("ptgn.event.Signal", ptgn::json{ { "signal", "door.opened" } })
		.During(500.0f, MoveToAction{ { 70.0f, 0.0f }, false })
		.Ease(ptgn::Ease::OutCubic);
	AttachSequence(panel, std::move(close_sequence));

	ptgn::Entity indicator{ CreateEntity("Event Indicator", "Indicator") };
	indicator.Get<ptgn::Transform>().position = { 300.0f, -110.0f };
	indicator.Add<ptgn::Circle>(21.0f);
	indicator.Get<DemoVisual>().color = ImVec4{ 0.92f, 0.80f, 0.27f, 1.0f };
	ScriptSequence opened_binding;
	opened_binding.shared_reference = true;
	opened_binding.shared_sequence_id = opened_indicator_id;
	AttachSequence(indicator, std::move(opened_binding));
	ScriptSequence closed_binding;
	closed_binding.shared_reference = true;
	closed_binding.shared_sequence_id = closed_indicator_id;
	AttachSequence(indicator, std::move(closed_binding));

	ptgn::Entity factory{ CreateEntity("Circle Spawner", "Spawner") };
	factory.Get<ptgn::Transform>().position = { -265.0f, -165.0f };
	factory.Add<ptgn::Rect>(ptgn::V2_float{ 130.0f, 72.0f });
	factory.Get<DemoVisual>().color = ImVec4{ 0.47f, 0.41f, 0.63f, 0.55f };
	factory.Get<DemoVisual>().sensor = true;
	SpawnEntityAction spawn_circles;
	spawn_circles.prefab_key = "prefabs/recall_circle";
	spawn_circles.count = 10;
	spawn_circles.origin = SpawnOrigin::OwnerEntity;
	spawn_circles.area = SpawnArea::Circle;
	spawn_circles.center = { 0.0f, 95.0f };
	spawn_circles.radius = 92.0f;
	spawn_circles.random_rotation = true;

	ScriptSequence spawn_sequence{ "Spawn Recall Circles" };
	spawn_sequence
		.Reentry(ReentryMode::IgnoreWhileRunning)
		.StartOn("ptgn.event.OverlapStart", ptgn::json{ { "tags", "Player" }, { "masks", "" } })
		.Then(std::move(spawn_circles));
	AttachSequence(factory, std::move(spawn_sequence));
	ScriptSequence recall_sequence{ "Recall Spawned Circles" };
	recall_sequence
		.StartOn("ptgn.event.OverlapStop", ptgn::json{ { "tags", "Player" }, { "masks", "" } })
		.EmitSignal(SignalKey{ "spawned_circles.destroy" });
	AttachSequence(factory, std::move(recall_sequence));

	ptgn::Entity damage_target{ CreateEntity("Damage Target", "DamageTarget") };
	damage_target.Get<ptgn::Transform>().position = { 315.0f, 105.0f };
	damage_target.Add<ptgn::Rect>(ptgn::V2_float{ 105.0f, 86.0f });
	damage_target.Get<DemoVisual>().color = ImVec4{ 0.78f, 0.27f, 0.29f, 1.0f };
	damage_target.Add<Health>(Health{ .maximum = 100.0f, .current = 100.0f });
	ScriptSequence damage_sequence{ "Overlap Damage Cooldown" };
	damage_sequence
		.Reentry(ReentryMode::IgnoreWhileRunning)
		.StartOn("ptgn.event.Overlap", ptgn::json{ { "tags", "Player" }, { "masks", "" } })
		.Then(ApplyDamageAction{ 25.0f })
		.Wait(3000.0f);
	AttachSequence(damage_target, std::move(damage_sequence));

	ptgn::Entity tween_button{ CreateEntity("Tween Button", "TweenButton") };
	tween_button.Get<ptgn::Transform>().position = { -80.0f, -210.0f };
	tween_button.Add<ptgn::Rect>(ptgn::V2_float{ 150.0f, 50.0f });
	tween_button.Add<ButtonData>();
	tween_button.Add<ButtonStyle>();
	ScriptSequence tween_button_sequence{ "Button Scale Pulse" };
	tween_button_sequence
		.Reentry(ReentryMode::Restart)
		.Channel(SequenceChannelKey{ "ui.press" })
		.StartOn("demo.button.Press", ptgn::json{ { "button", ptgn::Mouse::Left } })
		.During(105.0f, ScaleToAction{ { 1.12f, 1.12f }, false })
		.Ease(ptgn::Ease::OutBack)
		.During(105.0f, ScaleToAction{ { 1.0f, 1.0f }, false })
		.Ease(ptgn::Ease::OutBack)
		.EmitSignal(SignalKey{ "button.tween.clicked" });
	AttachSequence(tween_button, std::move(tween_button_sequence));

	ptgn::Entity signal_button{ CreateEntity("Global Event Button", "SignalButton") };
	signal_button.Get<ptgn::Transform>().position = { 115.0f, -210.0f };
	signal_button.Add<ptgn::Rect>(ptgn::V2_float{ 185.0f, 50.0f });
	signal_button.Add<ButtonData>();
	signal_button.Add<ButtonStyle>(ButtonStyle{
		.idle = ImVec4{ 0.47f, 0.29f, 0.62f, 1.0f },
		.hovered = ImVec4{ 0.61f, 0.39f, 0.78f, 1.0f },
		.pressed = ImVec4{ 0.35f, 0.20f, 0.49f, 1.0f },
	});
	ScriptSequence signal_button_sequence{ "Emit Global Button Signal" };
	signal_button_sequence
		.StartOn("demo.button.Press", ptgn::json{ { "button", ptgn::Mouse::Left } })
		.EmitSignal(SignalKey{ "button.global.clicked" });
	AttachSequence(signal_button, std::move(signal_button_sequence));

	ScriptSequence global_button_sequence{ "Global Button Indicator Spin" };
	global_button_sequence
		.Reentry(ReentryMode::Restart)
		.Channel(SequenceChannelKey{ "transform.rotation" })
		.StartOn("ptgn.event.Signal", ptgn::json{ { "signal", "button.global.clicked" } })
		.During(450.0f, RotateToAction{ 360.0f, false, true })
		.Ease(ptgn::Ease::OutBack);
	AttachSequence(indicator, std::move(global_button_sequence));

	ptgn::Entity follower{ CreateEntity("Action-Controlled Follower", "Follower") };
	follower.Get<ptgn::Transform>().position = { 350.0f, 205.0f };
	follower.Add<ptgn::Circle>(15.0f);
	follower.Get<DemoVisual>().color = ImVec4{ 0.35f, 0.86f, 0.82f, 1.0f };
	ScriptSequence follow_sequence{ "Follow Player Until Reached" };
	follow_sequence
		.Reentry(ReentryMode::Restart)
		.Channel(SequenceChannelKey{ "movement.follow" })
		.StartOn("ptgn.event.Signal", ptgn::json{ { "signal", "button.tween.clicked" } })
		.UntilComplete(FollowTargetAction{ player, 230.0f, 3.0f });
	AttachSequence(follower, std::move(follow_sequence));
}

bool DemoScene::Overlap(ptgn::Entity a, ptgn::Entity b) const {
	if (!a || !b || !a.Has<ptgn::Transform>() || !b.Has<ptgn::Transform>() ||
		!a.Has<ptgn::Rect>() || !b.Has<ptgn::Rect>()) {
		return false;
	}
	const auto& transform_a{ a.Get<ptgn::Transform>() };
	const auto& transform_b{ b.Get<ptgn::Transform>() };
	const auto size_a{ a.Get<ptgn::Rect>().GetSize(transform_a) };
	const auto size_b{ b.Get<ptgn::Rect>().GetSize(transform_b) };
	return std::abs(transform_a.position.x - transform_b.position.x) <=
			(size_a.x + size_b.x) * 0.5f &&
		std::abs(transform_a.position.y - transform_b.position.y) <=
			(size_a.y + size_b.y) * 0.5f;
}

void DemoScene::UpdateOverlapEvents() {
	const ptgn::Entity player{ FindByTag("Player") };
	if (!player) {
		return;
	}
	auto update_transition = [this, player](
		ptgn::Entity sensor, bool& previous, std::string_view label
	) {
		if (!sensor) {
			previous = false;
			return;
		}
		const bool overlapping{ Overlap(player, sensor) };
		if (overlapping == previous) {
			return;
		}
		previous = overlapping;
		if (overlapping) {
			ctx().event.Push<ptgn::event::OverlapStart>(
				sensor, ptgn::event::OverlapStart{ player }
			);
			Log("OverlapStart(" + std::string{ label } + ", Player)");
		} else {
			ctx().event.Push<ptgn::event::OverlapStop>(
				sensor, ptgn::event::OverlapStop{ player }
			);
			Log("OverlapStop(" + std::string{ label } + ", Player)");
		}
	};

	update_transition(FindByTag("Door"), player_overlapping_sensor_, "Door Sensor");
	update_transition(FindByTag("Spawner"), player_overlapping_spawner_, "Circle Spawner");
	const ptgn::Entity damage_target{ FindByTag("DamageTarget") };
	if (damage_target && Overlap(player, damage_target)) {
		ctx().event.Push<ptgn::event::Overlap>(
			damage_target, ptgn::event::Overlap{ player }
		);
	}
}

void DemoScene::UpdateButtonInteraction() {
	if (!pointer_frame_pending_) {
		return;
	}
	pointer_frame_pending_ = false;

	ptgn::Entity hovered;
	if (pointer_frame_.inside_scene) {
		auto entities{ Entities() };
		for (auto it{ entities.rbegin() }; it != entities.rend(); ++it) {
			ptgn::Entity entity{ *it };
			if (!entity || !entity.Has<ButtonData>() || !entity.Has<ptgn::Transform>() ||
				!entity.Has<ptgn::Rect>() || !ptgn::IsVisible(entity)) {
				continue;
			}
			auto& button{ entity.Get<ButtonData>() };
			if (!button.enabled) {
				button.hovered = false;
				button.pressed = false;
				button.state = ButtonState::Disabled;
				continue;
			}
			const auto& transform{ entity.Get<ptgn::Transform>() };
			const auto size{ entity.Get<ptgn::Rect>().GetSize(transform) };
			const auto offset{ pointer_frame_.world_position - transform.position };
			if (std::abs(offset.x) <= size.x * 0.5f &&
				std::abs(offset.y) <= size.y * 0.5f) {
				hovered = entity;
				break;
			}
		}
	}

	if (hovered != hovered_button_) {
		if (hovered_button_) {
			auto& button{ hovered_button_.Get<ButtonData>() };
			button.hovered = false;
			button.state = button.pressed ? ButtonState::Pressed : ButtonState::Idle;
			ctx().event.Push<MouseMoveOut>(hovered_button_, MouseMoveOut{});
		}
		hovered_button_ = hovered;
		if (hovered_button_) {
			auto& button{ hovered_button_.Get<ButtonData>() };
			button.hovered = true;
			button.state = button.pressed ? ButtonState::Pressed : ButtonState::Hovered;
			ctx().event.Push<MouseMoveOver>(hovered_button_, MouseMoveOver{});
		}
	}

	for (std::size_t i{ 0 }; i < kMouseButtons.size(); ++i) {
		const ptgn::Mouse mouse_button{ kMouseButtons[i] };
		if (pointer_frame_.pressed[i] && hovered_button_) {
			pressed_buttons_[i] = hovered_button_;
			auto& button{ pressed_buttons_[i].Get<ButtonData>() };
			button.pressed = true;
			button.pressed_button = mouse_button;
			button.state = ButtonState::Pressed;
			ctx().event.Push<MousePressedOver>(
				pressed_buttons_[i], MousePressedOver{ mouse_button }
			);
		}
		if (!pointer_frame_.released[i] || !pressed_buttons_[i]) {
			continue;
		}
		ptgn::Entity pressed{ pressed_buttons_[i] };
		const bool released_over{ pressed == hovered_button_ };
		auto& button{ pressed.Get<ButtonData>() };
		ctx().event.Push<MouseReleasedOver>(
			pressed,
			MouseReleasedOver{
				.button = mouse_button,
				.released_over = released_over,
			}
		);
		if (button.pressed && button.pressed_button == mouse_button && released_over) {
			ctx().event.Push<ButtonPress>(pressed, ButtonPress{ mouse_button });
		}
		button.pressed = false;
		button.state = button.hovered ? ButtonState::Hovered : ButtonState::Idle;
		pressed_buttons_[i] = {};
	}
}

void DemoScene::ProcessPendingDestroy() {
	for (ptgn::Entity entity : pending_destroy_) {
		if (!entity) {
			continue;
		}
		if (auto* scripts{ entity.TryGet<ScriptsComponent>() }) {
			for (auto& entry : scripts->scripts) {
				if (auto* sequence{ dynamic_cast<SequenceScript*>(entry.instance) }) {
					(void)script_runtime::Stop(
						entity, sequence->sequence.id, SequenceCancelReason::OwnerDestroyed
					);
				}
			}
		}
		if (hovered_button_ == entity) {
			hovered_button_ = {};
		}
		for (auto& pressed : pressed_buttons_) {
			if (pressed == entity) {
				pressed = {};
			}
		}
		Log("Destroyed " + std::string{ Name(entity) });
		entity.Destroy();
	}
	pending_destroy_.clear();
	Refresh();
}

void DemoScene::OnEnter() {
	RegisterDemoTypes();
	editor::RegisterEditorTypes();
	RegisterDemoEditorTypes();
	SetBackgroundColor(ptgn::Color{ 14, 15, 18, 255 });
	CreatePrefabs();
	CreateDemoScene();
	Refresh();
	for (auto [entity, _scripts] : EntitiesWith<ScriptsComponent>()) {
		script_runtime::AttachAll(entity);
	}
	editor_ = std::make_unique<editor::DemoEditor>(*this);
}

void DemoScene::OnUpdate() {
	const float delta_seconds{ std::max(0.0f, ImGui::GetIO().DeltaTime) };
	UpdateOverlapEvents();
	UpdateButtonInteraction();
	script_runtime::Update(*this, delta_seconds);
	ProcessPendingDestroy();
	for (auto& entry : activity_) {
		entry.remaining_seconds -= delta_seconds;
	}
	std::erase_if(activity_, [](const ActivityEntry& entry) {
		return entry.remaining_seconds <= 0.0f;
	});
	if (editor_) {
		editor_->Draw();
	}
}

void LogScriptActivity(ptgn::Scene& scene, std::string text) {
	static_cast<DemoScene&>(scene).Log(std::move(text));
}

std::string_view GetEntityName(ptgn::Entity entity) {
	if (entity && entity.Has<NameComponent>()) {
		return entity.Get<NameComponent>().value;
	}
	return "Entity";
}

std::string_view GetEntityTag(ptgn::Entity entity) {
	if (entity && entity.Has<ptgn::Tag>()) {
		return entity.Get<ptgn::Tag>().value;
	}
	return {};
}

int GetEntityMask(ptgn::Entity entity) {
	if (entity && entity.Has<MaskComponent>()) {
		return entity.Get<MaskComponent>().value;
	}
	return 0;
}

ptgn::Entity SpawnScriptPrefab(
	ptgn::Scene& scene,
	const PrefabSpawnRequest& request
) {
	return static_cast<DemoScene&>(scene).SpawnPrefab(request);
}

SharedScriptSequenceRegistry& GetSharedScriptSequences(ptgn::Scene& scene) {
	return static_cast<DemoScene&>(scene).GetSharedSequences();
}

} // namespace ptgn

int main() {
	ptgn::Application app{ "Protegon Registry-Driven Scripts" };
	PTGN_WITH_EDITOR(app, true);
	app.StartWith<ptgn::DemoScene>();
}
