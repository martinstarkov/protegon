#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/event/event.h"
#include "core/math/easing.h"
#include "core/util/concepts.h"
#include "core/util/hash.h"
#include "core/util/reflection.h"
#include "core/util/strong_string.h"
#include "core/util/type_info.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "serialization/json/json.h"

namespace ptgn {

class Scene;
class Script;
struct SignalKey;
struct SequenceHandle;

using SequenceId = std::uint64_t;
using TypeHashValue = std::size_t;

enum class ReentryMode {
	IgnoreWhileRunning,
	Restart,
	Queue
};
PTGN_REFLECT_ENUM(ReentryMode);

enum class ScriptStatus {
	Running,
	Complete
};

enum class ScriptCompletion {
	Instant,
	Duration,
	ScriptControlled,
	Infinite
};
PTGN_REFLECT_ENUM(ScriptCompletion);

enum class SequenceCancelReason {
	Stopped,
	Reset,
	Cleared,
	Skipped,
	Replaced,
	BindingRemoved,
	OwnerDestroyed
};
PTGN_REFLECT_ENUM(SequenceCancelReason);

enum class SequenceStopMode {
	Current,
	All
};
PTGN_REFLECT_ENUM(SequenceStopMode);

enum class SequenceLifecycle {
	Start,
	Complete,
	Reset,
	Stop,
	Pause,
	Resume,
	ScriptStart,
	ScriptComplete,
	ScriptCancel,
	Repeat,
	Yoyo
};
PTGN_REFLECT_ENUM(SequenceLifecycle);

struct ScriptTiming {
	float duration_ms{ 300.0f };
	Ease ease{ Ease::Linear };
	int additional_repeats{ 0 };
	bool infinite_repeats{ false };
	bool reversed{ false };
	bool yoyo{ false };

	PTGN_REFLECT(
		ScriptTiming, duration_ms, ease, additional_repeats, infinite_repeats, reversed, yoyo
	)
};

struct EventCondition {
	bool enabled{ true };
	bool consume{ false };
	TypeHashValue type_hash{ 0 };
	json value;

	PTGN_REFLECT(EventCondition, enabled, consume, type_hash, value)
};

struct ScriptStep {
	bool enabled{ true };
	TypeHashValue type_hash{ 0 };
	json value;
	std::optional<ScriptCompletion> completion;
	std::optional<ScriptTiming> timing;

	// C++-authored steps retain a typed construction path. Serialized/editor-authored steps use
	// the registry JSON construction path instead.
	std::function<std::unique_ptr<Script>()> runtime_factory;


	PTGN_REFLECT(ScriptStep, enabled, type_hash, value, completion, timing)
};

struct LifecycleScript {
	bool enabled{ true };
	SequenceLifecycle lifecycle{ SequenceLifecycle::Complete };
	ScriptStep action;

	PTGN_REFLECT(LifecycleScript, enabled, lifecycle, action)
};

struct SequenceChannelKey : StrongString<SequenceChannelKey> {
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
	std::size_t step_index{ 0 };
	float elapsed_ms{ 0.0f };
	int current_repeat{ 0 };
	bool currently_reversed{ false };
	std::unique_ptr<Script> script_instance;
	int completed_runs{ 0 };

	ScriptSequenceRuntime();
	~ScriptSequenceRuntime();
	ScriptSequenceRuntime(ScriptSequenceRuntime&&) noexcept;
	ScriptSequenceRuntime& operator=(ScriptSequenceRuntime&&) noexcept;
	ScriptSequenceRuntime(const ScriptSequenceRuntime&) = delete;
	ScriptSequenceRuntime& operator=(const ScriptSequenceRuntime&) = delete;

	void ClearActiveScript();
};

struct ScriptSequence {
private:
	[[nodiscard]] static SequenceId NextSequenceId();

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
	std::vector<ScriptStep> steps;
	std::vector<LifecycleScript> lifecycle_actions;
	ScriptSequenceRuntime runtime;

	ScriptSequence() = default;
	explicit ScriptSequence(std::string sequence_name) : name{ std::move(sequence_name) } {}
	ScriptSequence(ScriptSequence&&) noexcept = default;
	ScriptSequence& operator=(ScriptSequence&&) noexcept = default;

	ScriptSequence(const ScriptSequence& other);
	ScriptSequence& operator=(const ScriptSequence& other);

	ScriptSequence& Reentry(ReentryMode value);
	ScriptSequence& Channel(SequenceChannelKey value);
	ScriptSequence& Transient(bool remove_on_complete = true);
	ScriptSequence& DestroyOwnerOnComplete(bool value = true);

	template <typename TEvent>
	ScriptSequence& StartOn(json value = nullptr);

	template <typename TEvent>
	ScriptSequence& StopOn(json value = nullptr);

	template <typename TScript>
	ScriptSequence& Then(TScript script = {});

	template <typename TScript>
	ScriptSequence& UntilComplete(TScript script = {});

	template <typename TScript>
	ScriptSequence& Forever(TScript script = {});

	template <typename TScript>
	ScriptSequence& During(float duration_ms, TScript script = {});

	ScriptSequence& Ease(ptgn::Ease value);
	ScriptSequence& Repeat(int additional_repeats);
	ScriptSequence& Infinite();
	ScriptSequence& Reversed(bool value = true);
	ScriptSequence& Yoyo(bool value = true);
	ScriptSequence& Wait(float duration_ms);
	ScriptSequence& EmitSignal(SignalKey signal);
	[[nodiscard]] SequenceHandle Start(Entity owner, bool force = true) const;

	PTGN_REFLECT(
		ScriptSequence, id, enabled, shared_reference, shared_sequence_id, name, reentry, channel,
		transient, remove_binding_on_complete, destroy_owner_on_complete, start_events, stop_events,
		steps, lifecycle_actions
	)

private:
	ScriptTiming& LatestDuringTiming();
};

/// @brief A unit of executable behavior.
///
/// Root scripts are owned by impl::Scripts. Sequence steps instantiate the same Script types as
/// transient children. A Script may implement custom callbacks, use its internal sequence, or both.
class Script {
public:
	Script() = default;
	virtual ~Script() = default;
	Script(const Script& other) : sequence{ other.sequence } {}
	Script& operator=(const Script& other);
	Script(Script&&) noexcept = default;
	Script& operator=(Script&&) noexcept = default;

	virtual void OnCreate() { /* User implementation */ }
	virtual void OnStart() { /* User implementation */ }
	[[nodiscard]] virtual ScriptStatus OnUpdate() { return ScriptStatus::Running; }
	virtual void OnEvent(Event) { /* User implementation */ }
	virtual void OnRepeat() { /* User implementation */ }
	virtual void OnComplete() { /* User implementation */ }
	virtual void OnCancel(SequenceCancelReason) { /* User implementation */ }

	ScriptSequence sequence;

	PTGN_REFLECT(Script, sequence)

	[[nodiscard]] Entity Owner() const { return entity; }
	[[nodiscard]] Scene& GetScene() { return entity.GetScene(); }
	[[nodiscard]] const Scene& GetScene() const { return entity.GetScene(); }
	[[nodiscard]] float DeltaSeconds() const { return delta_seconds_; }
	[[nodiscard]] float LinearProgress() const { return linear_progress_; }
	[[nodiscard]] float Progress() const { return progress_; }
	[[nodiscard]] int RepeatIndex() const { return repeat_; }
	[[nodiscard]] bool IsReversed() const { return reversed_; }

protected:
	Entity entity;

	void Complete() { completion_requested_ = true; }
	void MoveOn() { Complete(); }

private:
	friend struct impl_ScriptAccess;

	float delta_seconds_{ 0.0f };
	float linear_progress_{ 0.0f };
	float progress_{ 0.0f };
	int repeat_{ 0 };
	bool reversed_{ false };
	bool completion_requested_{ false };
};

struct WaitScript : public Script {
	PTGN_REFLECT_EMPTY(WaitScript)
};

template <typename T>
concept ScriptClass = std::derived_from<T, Script>;

struct impl_ScriptAccess {
	static void Attach(Script& script, Entity owner) { script.entity = owner; }
	static void SetFrame(
		Script& script, float delta_seconds, float linear_progress, float progress, int repeat,
		bool reversed
	) {
		script.delta_seconds_ = delta_seconds;
		script.linear_progress_ = linear_progress;
		script.progress_ = progress;
		script.repeat_ = repeat;
		script.reversed_ = reversed;
	}
	static bool TakeCompletionRequest(Script& script) {
		return std::exchange(script.completion_requested_, false);
	}
};

template <typename T>
bool TryReadScriptJson(const json& input, T& output) {
	if constexpr (!requires(const json& value, T& result) { value.get_to(result); }) {
		return false;
	} else {
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
}

namespace impl {

/// @brief Ensures the translation unit containing the built-in script/event registrations is linked.
void EnsureEngineScriptsRegistered();

} // namespace impl

struct ScriptRegistrationOptions {
	std::uint32_t schema_version{ 1 };
	ScriptCompletion completion{ ScriptCompletion::ScriptControlled };
	bool supports_timing{ false };
	bool requires_timing{ false };
	bool serializable{ true };
	std::optional<ScriptTiming> default_timing;
};

struct ScriptRegistration {
	TypeHashValue type_hash{ 0 };
	std::string type;
	std::uint32_t schema_version{ 1 };
	ScriptCompletion completion{ ScriptCompletion::ScriptControlled };
	bool supports_timing{ false };
	bool requires_timing{ false };
	bool serializable{ true };
	std::optional<ScriptTiming> default_timing;
	std::function<json()> make_default;
	std::function<ScriptSequence()> make_default_sequence;
	std::function<std::unique_ptr<Script>(const json&)> instantiate;
	std::function<void(Script&, const json&)> apply;
};

class ScriptRegistry {
public:
	template <ScriptClass T>
	static bool Register() {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<T>() };
		if (std::ranges::any_of(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		})) {
			return false;
		}
		return Register<T>(ScriptRegistrationOptions{});
	}

	template <ScriptClass T>
	static bool Register(ScriptRegistrationOptions options) {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<T>() };

		if (options.requires_timing && options.completion == ScriptCompletion::Instant) {
			options.completion = ScriptCompletion::Duration;
		}

		ScriptRegistration registration{
			.type_hash = type_hash,
			.type = std::string{ type_name_without_namespaces<T>() },
			.schema_version = options.schema_version,
			.completion = options.completion,
			.supports_timing = options.supports_timing,
			.requires_timing = options.requires_timing,
			.serializable = options.serializable,
			.default_timing = std::move(options.default_timing),
			.make_default = [] {
				json output;
				if constexpr (std::default_initializable<T>) {
					T value{};
					if constexpr (requires(json& j, const T& v) { j = v; }) {
						try {
							output = value;
						} catch (...) {
							output = json::object();
						}
					}
				}
				return output;
			},
			.make_default_sequence = [] {
				if constexpr (std::default_initializable<T>) {
					T value{};
					return value.sequence;
				} else {
					return ScriptSequence{};
				}
			},
			.instantiate = [](const json& input) -> std::unique_ptr<Script> {
				if constexpr (std::default_initializable<T>) {
					auto script{ std::make_unique<T>() };
					if constexpr (requires(const json& j, T& v) { j.get_to(v); }) {
						TryReadScriptJson(input, *script);
					}
					return script;
				} else {
					return nullptr;
				}
			},
			.apply = []([[maybe_unused]] Script& script, [[maybe_unused]] const json& input) {
				if constexpr (requires(const json& j, T& v) { j.get_to(v); }) {
					TryReadScriptJson(input, static_cast<T&>(script));
				}
			},
		};

		const auto existing{ std::ranges::find_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };
		if (existing != entries.end()) {
			*existing = std::move(registration);
			return false;
		}

		entries.push_back(std::move(registration));
		return true;
	}

	/// @brief Compatibility overload for direct registrations that have not migrated to options.
	template <ScriptClass T>
	static bool Register(
		bool supports_timing, bool requires_timing = false,
		std::optional<ScriptTiming> default_timing = std::nullopt,
		ScriptCompletion completion = ScriptCompletion::ScriptControlled, bool serializable = true
	) {
		return Register<T>(ScriptRegistrationOptions{
			.completion = completion,
			.supports_timing = supports_timing,
			.requires_timing = requires_timing,
			.serializable = serializable,
			.default_timing = std::move(default_timing),
		});
	}

	template <ScriptClass T>
	static void EnsureRegistered() {
		if (!Find(Hash<T>())) {
			// Script identity is serializable independently of whether T has reflected/custom JSON
			// payload data. A default-constructible stateless Script can be restored from its type.
			Register<T>(ScriptRegistrationOptions{
				.serializable = std::default_initializable<T>,
			});
		}
	}

	template <ScriptClass T>
	[[nodiscard]] static ScriptStep MakeStep(T value = {});

	[[nodiscard]] static ScriptStep MakeStep(TypeHashValue type_hash);

	[[nodiscard]] static const ScriptRegistration* Find(TypeHashValue type_hash);
	[[nodiscard]] static const ScriptRegistration* Find(std::string_view type);
	[[nodiscard]] static const std::vector<ScriptRegistration>& Entries();

private:
	[[nodiscard]] static std::vector<ScriptRegistration>& MutableEntries();
};

struct ScriptEntry {
	bool enabled{ true };
	TypeHashValue type_hash{ 0 };
	json value;
	ScriptSequence sequence;

	std::unique_ptr<Script> instance;
	std::function<std::unique_ptr<Script>()> runtime_factory;
	bool initialized{ false };

	ScriptEntry() = default;
	ScriptEntry(ScriptEntry&&) noexcept = default;
	ScriptEntry& operator=(ScriptEntry&&) noexcept = default;
	ScriptEntry(const ScriptEntry& other) :
		enabled{ other.enabled },
		type_hash{ other.type_hash },
		value{ other.value },
		sequence{ other.sequence },
		runtime_factory{ other.runtime_factory } {}

	ScriptEntry& operator=(const ScriptEntry& other) {
		if (this != &other) {
			ScriptEntry copy{ other };
			*this = std::move(copy);
		}
		return *this;
	}

	PTGN_REFLECT(ScriptEntry, enabled, type_hash, value, sequence)
};

struct SequenceChannelRuntime {
	SequenceChannelKey key;
	std::optional<SequenceId> active;
	std::deque<SequenceId> waiting;
};

struct SharedScriptSequenceRegistry {
	std::vector<ScriptSequence> sequences;

	[[nodiscard]] ScriptSequence* Find(SequenceId id);
	[[nodiscard]] const ScriptSequence* Find(SequenceId id) const;

	PTGN_REFLECT(SharedScriptSequenceRegistry, sequences)
};

template <typename TEvent>
struct SequenceEventRegistrationOptions {
	std::uint32_t schema_version{ 1 };
	json default_value;
	std::function<bool(Entity, const json&, const TEvent&)> matches;
	std::function<bool(Entity)> available{ [](Entity) { return true; } };
};

struct SequenceEventRegistration {
	TypeHashValue type_hash{ 0 };
	std::uint32_t schema_version{ 1 };
	std::function<void(EventCondition&)> set_defaults;
	std::function<bool(Entity, Event, const EventCondition&, bool consume)> matches;
	std::function<bool(Entity)> available;
};

class SequenceEventRegistry {
public:
	/// @brief Registers an event that matches every dispatched event of TEvent.
	///
	/// A default registration never replaces an existing explicit matcher.
	template <typename TEvent>
	static bool Register() {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<TEvent>() };
		if (std::ranges::any_of(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		})) {
			return false;
		}
		return Register<TEvent>(SequenceEventRegistrationOptions<TEvent>{});
	}

	template <typename TEvent>
	static bool Register(SequenceEventRegistrationOptions<TEvent> options) {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<TEvent>() };

		if (options.default_value.is_null()) {
			options.default_value = json::object();
		}
		if (!options.matches) {
			options.matches = [](Entity, const json&, const TEvent&) {
				return true;
			};
		}
		if (!options.available) {
			options.available = [](Entity) { return true; };
		}

		SequenceEventRegistration registration{
			.type_hash = type_hash,
			.schema_version = options.schema_version,
			.set_defaults = [value = std::move(options.default_value)](EventCondition& output) {
				output.value = value;
			},
			.matches = [fn = std::move(options.matches)](
				Entity owner, Event event, const EventCondition& input, bool consume
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
			.available = std::move(options.available),
		};

		const auto existing{ std::ranges::find_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };
		if (existing != entries.end()) {
			*existing = std::move(registration);
			return false;
		}

		entries.push_back(std::move(registration));
		return true;
	}

	template <typename TEvent, typename FMatch, typename FAvailable>
	static bool Register(json default_value, FMatch&& matches, FAvailable&& available) {
		return Register<TEvent>(SequenceEventRegistrationOptions<TEvent>{
			.default_value = std::move(default_value),
			.matches = std::forward<FMatch>(matches),
			.available = std::forward<FAvailable>(available),
		});
	}

	template <typename TEvent, typename FMatch>
	static bool Register(json default_value, FMatch&& matches) {
		return Register<TEvent>(SequenceEventRegistrationOptions<TEvent>{
			.default_value = std::move(default_value),
			.matches = std::forward<FMatch>(matches),
		});
	}

	[[nodiscard]] static EventCondition MakeCondition(
		TypeHashValue type_hash, json value = nullptr
	);

	template <typename TEvent>
	[[nodiscard]] static EventCondition MakeCondition(json value = nullptr) {
		return MakeCondition(Hash<TEvent>(), std::move(value));
	}

	[[nodiscard]] static const SequenceEventRegistration* Find(TypeHashValue type_hash);
	[[nodiscard]] static const std::vector<SequenceEventRegistration>& Entries();

private:
	[[nodiscard]] static std::vector<SequenceEventRegistration>& MutableEntries();
};

namespace impl {

class Scripts {
public:
	Scripts() = default;
	~Scripts() noexcept = default;
	Scripts(const Scripts& other) : scripts{ other.scripts } {}
	Scripts& operator=(const Scripts& other) {
		if (this != &other) {
			Scripts copy{ other };
			*this = std::move(copy);
		}
		return *this;
	}
	Scripts(Scripts&&) noexcept = default;
	Scripts& operator=(Scripts&&) noexcept = default;


	template <ScriptClass T, typename... TArgs>
		requires BraceConstructible<T, TArgs...>
	T& Add(Entity owner, TArgs&&... constructor_args);

	template <ScriptClass T>
	void Remove();

	template <ScriptClass T>
	[[nodiscard]] bool Has() const;

	void AddEntryDeferred(ScriptEntry entry);
	void RemoveDeferred(SequenceId sequence_id);
	void Attach(Entity owner);
	void ApplyPending();
	void CancelAll(SequenceCancelReason reason = SequenceCancelReason::OwnerDestroyed);
	void OnEvent(Event event);
	void OnEvent(Event event) const { const_cast<Scripts*>(this)->OnEvent(event); }

	PTGN_REFLECT_EMPTY(Scripts)

	friend std::ostream& operator<<(std::ostream& os, const Scripts& scripts_component) {
		os << "{ script_count: " << scripts_component.scripts.size() << " }";
		return os;
	}

	std::vector<ScriptEntry> scripts;
	std::vector<SequenceChannelRuntime> channels;
	std::vector<ScriptEntry> pending_additions;
	std::vector<SequenceId> pending_removals;

private:
	Entity owner_;
};

void from_json(const json& j, Scripts& scripts);
void to_json(json& j, const Scripts& scripts);

} // namespace impl

struct SequenceHandle {
	Entity owner;
	SequenceId binding_id{ 0 };

	[[nodiscard]] explicit operator bool() const { return owner && binding_id != 0; }

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

namespace script_runtime {

void AttachEntry(Entity entity, ScriptEntry& entry);
void AttachAll(Entity entity);
void ApplyPending(Scene& scene);
void Update(Scene& scene, secondsf delta_time);
bool DispatchEvent(Entity entity, Event event);
bool DispatchGlobalEvent(Scene& scene, Event event);

template <typename T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
bool Dispatch(Entity entity, TArgs&&... args) {
	auto data{ impl::EventData::Create<T>(std::forward<TArgs>(args)...) };
	return DispatchEvent(entity, Event{ data });
}

template <typename T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
bool DispatchGlobal(Scene& scene, TArgs&&... args) {
	auto data{ impl::EventData::Create<T>(std::forward<TArgs>(args)...) };
	return DispatchGlobalEvent(scene, Event{ data });
}

[[nodiscard]] ScriptSequence* Resolve(Entity owner, ScriptSequence& binding);
[[nodiscard]] const ScriptSequence* Resolve(Entity owner, const ScriptSequence& binding);
SequenceHandle RunSequence(Entity owner, ScriptSequence sequence);
SequenceHandle RunInChannel(
	Entity owner, SequenceChannelKey channel, ScriptSequence sequence, ReentryMode reentry
);
bool Start(Entity owner, SequenceId id, bool force = false);
void StopChannel(Entity owner, SequenceChannelKey channel, SequenceStopMode mode);
bool Stop(
	Entity owner, SequenceId id, SequenceCancelReason reason = SequenceCancelReason::Stopped
);
bool Reset(Entity owner, SequenceId id);
bool Clear(Entity owner, SequenceId id);
bool Skip(Entity owner, SequenceId id);
bool Seek(Entity owner, SequenceId id, float progress);
bool SetPaused(Entity owner, SequenceId id, bool paused);
[[nodiscard]] float Progress(Entity owner, SequenceId id);
[[nodiscard]] bool IsRunning(Entity owner, SequenceId id);
[[nodiscard]] bool IsPaused(Entity owner, SequenceId id);
[[nodiscard]] bool IsCompleted(Entity owner, SequenceId id);

} // namespace script_runtime

/// @brief Adds an instance of a script of type T to the entity.
template <ScriptClass T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
T& AddScript(Entity entity, TArgs&&... constructor_args) {
	auto& scripts{ entity.TryAdd<impl::Scripts>() };
	return scripts.Add<T>(entity, std::forward<TArgs>(constructor_args)...);
}

/// @brief Removes all root script instances of type T from the entity.
template <ScriptClass T>
void RemoveScript(Entity entity) {
	if (auto* scripts{ entity.TryGet<impl::Scripts>() }) {
		scripts->Attach(entity);
		scripts->Remove<T>();
	}
}

/// @return True if the entity has a root script instance of type T.
template <ScriptClass T>
[[nodiscard]] bool HasScript(Entity entity) {
	if (auto* scripts{ entity.TryGet<impl::Scripts>() }) {
		return scripts->Has<T>();
	}
	return false;
}

template <ScriptClass T>
ScriptStep ScriptRegistry::MakeStep(T value) {
	EnsureRegistered<T>();
	const auto* registration{ Find(Hash<T>()) };
	if (!registration) {
		return {};
	}

	json script_json;
	try {
		script_json = value;
	} catch (...) {
		script_json = json::object();
	}

	auto prototype{ std::make_shared<T>(std::move(value)) };
	return ScriptStep{
		.enabled = true,
		.type_hash = registration->type_hash,
		.value = std::move(script_json),
		.timing = registration->default_timing,
		.runtime_factory = [prototype] { return std::make_unique<T>(*prototype); },
	};
}

template <typename TEvent>
ScriptSequence& ScriptSequence::StartOn(json value) {
	start_events.push_back(SequenceEventRegistry::MakeCondition<TEvent>(std::move(value)));
	return *this;
}

template <typename TEvent>
ScriptSequence& ScriptSequence::StopOn(json value) {
	stop_events.push_back(SequenceEventRegistry::MakeCondition<TEvent>(std::move(value)));
	return *this;
}

template <typename TScript>
ScriptSequence& ScriptSequence::Then(TScript script) {
	static_assert(ScriptClass<TScript>);
	auto step{ ScriptRegistry::MakeStep<TScript>(std::move(script)) };
	step.completion = ScriptCompletion::Instant;
	step.timing.reset();
	steps.push_back(std::move(step));
	return *this;
}

template <typename TScript>
ScriptSequence& ScriptSequence::UntilComplete(TScript script) {
	static_assert(ScriptClass<TScript>);
	auto step{ ScriptRegistry::MakeStep<TScript>(std::move(script)) };
	step.completion = ScriptCompletion::ScriptControlled;
	steps.push_back(std::move(step));
	return *this;
}

template <typename TScript>
ScriptSequence& ScriptSequence::Forever(TScript script) {
	static_assert(ScriptClass<TScript>);
	auto step{ ScriptRegistry::MakeStep<TScript>(std::move(script)) };
	step.completion = ScriptCompletion::Infinite;
	if (!step.timing) {
		step.timing = ScriptTiming{};
	}
	steps.push_back(std::move(step));
	return *this;
}

template <typename TScript>
ScriptSequence& ScriptSequence::During(float duration_ms, TScript script) {
	static_assert(ScriptClass<TScript>);
	auto step{ ScriptRegistry::MakeStep<TScript>(std::move(script)) };
	step.completion = ScriptCompletion::Duration;
	step.timing = step.timing.value_or(ScriptTiming{});
	step.timing->duration_ms = std::max(0.0f, duration_ms);
	steps.push_back(std::move(step));
	return *this;
}

template <ScriptClass T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
T& impl::Scripts::Add(Entity owner, TArgs&&... constructor_args) {
	Attach(owner);
	ScriptRegistry::EnsureRegistered<T>();
	const auto* registration{ ScriptRegistry::Find(Hash<T>()) };

	auto instance{ std::make_unique<T>(std::forward<TArgs>(constructor_args)...) };
	impl_ScriptAccess::Attach(*instance, owner);
	auto* raw{ instance.get() };

	json snapshot;
	if constexpr (requires(json& j, const T& value) { j = value; }) {
		try {
			snapshot = *instance;
		} catch (...) {
			snapshot = json::object();
		}
	}

	ScriptEntry entry;
	entry.type_hash = Hash<T>();
	entry.value = std::move(snapshot);
	const SequenceId sequence_id{ instance->sequence.id };
	entry.sequence = instance->sequence;
	entry.sequence.id = sequence_id;
	if constexpr (std::copy_constructible<T>) {
		auto prototype{ std::make_shared<T>(*instance) };
		entry.runtime_factory = [prototype] { return std::make_unique<T>(*prototype); };
	}
	entry.instance = std::move(instance);
	entry.initialized = false;
	if (registration && registration->default_timing && entry.sequence.steps.empty()) {
		// Registration metadata applies to sequence steps, not root scripts. Intentionally empty.
	}
	pending_additions.push_back(std::move(entry));
	return *raw;
}

template <ScriptClass T>
void impl::Scripts::Remove() {
	const auto hash{ Hash<T>() };
	const auto queue = [&](const auto& entries) {
		for (const auto& entry : entries) {
			const SequenceId id{
				entry.instance ? entry.instance->sequence.id : entry.sequence.id
			};
			if (entry.type_hash == hash && !std::ranges::contains(pending_removals, id)) {
				pending_removals.push_back(id);
			}
		}
	};
	queue(scripts);
	queue(pending_additions);
}

template <ScriptClass T>
[[nodiscard]] bool impl::Scripts::Has() const {
	const auto hash{ Hash<T>() };
	const auto contains = [&](const auto& entries) {
		return std::ranges::any_of(entries, [&](const auto& entry) {
			return entry.type_hash == hash &&
				!std::ranges::contains(pending_removals, entry.sequence.id);
		});
	};
	return contains(scripts) || contains(pending_additions);
}

static_assert(std::copy_constructible<ScriptStep>);
static_assert(std::is_copy_assignable_v<ScriptStep>);
static_assert(std::copy_constructible<ScriptEntry>);
static_assert(std::is_copy_assignable_v<ScriptEntry>);
static_assert(std::copy_constructible<ScriptSequence>);
static_assert(std::is_copy_assignable_v<ScriptSequence>);
static_assert(std::copy_constructible<impl::Scripts>);
static_assert(std::is_copy_assignable_v<impl::Scripts>);

namespace impl {

template <typename TEvent>
struct EventScript : public Script {
	EventScript() = default;

	explicit EventScript(EventCallback<TEvent> callback) :
		callback_{ std::move(callback) } {}

	void OnEvent(Event event) override {
		event.DispatchVariant<TEvent>(callback_);
	}

private:
	EventCallback<TEvent> callback_;
};

} // namespace impl

} // namespace ptgn
