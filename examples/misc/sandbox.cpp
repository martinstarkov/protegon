// script_sequence_old_ui_registry_demo_v18.cpp
//
// Old compact ImGui UI rebuilt on static registry-driven Events + Scripts + Script Sequences.
//
// Architecture boundaries in this file:
//   engine  - authored/runtime sequence data and static engine registries.
//   editor  - static editor registries and compact Dear ImGui inspectors.
//   demo    - replaceable demo world, scene setup, actions, and application loop.
//
// Important behavior rule:
//   Every registered Action affects its owning entity when applicable. Cross-entity behavior is
//   expressed by emitting an Event and attaching an Event-driven ScriptSequence to the other entity.
//
// The demo uses typed registry values for Events, Actions, and Scripts; the engine component
// registry stores detached prefab/action component values as JSON. It also uses double-buffered
// local/global event handlers, resident scripts, sequence handles/channels,
// duration/action-controlled/infinite Actions, old-style tween helpers, and script-driven Buttons.

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glad/gl.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <magic_enum/magic_enum.hpp>

#include "panels/inspector_fields.h"
#include "core/event/key_event.h"
#include "core/event/mouse_event.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/math/easing.h"
#include "core/util/hash.h"
#include "core/util/strong_string.h"
#include "core/util/type_info.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "panels/component_editor_registry.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/visible.h"
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
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ptgn {

using Id = std::uint64_t;

class IdGenerator {
public:
	static Id Next() {
		static Id next{ 1 };
		return next++;
	}
};

using TypeHashValue = std::size_t;

template <typename T>
[[nodiscard]] TypeHashValue TypeHash() {
	return ptgn::Hash<std::remove_cvref_t<T>>();
}

template <typename T>
[[nodiscard]] std::string_view TypeName() {
	return ptgn::type_name_without_namespaces<std::remove_cvref_t<T>>();
}

/// @brief Small cloneable typed value used by the registries and editor.
///
/// This keeps authored values strongly identified by Hash<T>() without relying on std::any or
/// unchecked casts. Copying a value invokes the stored type's copy constructor, which also means
/// copied ScriptsComponent values receive fresh runtime-only script/sequence state.
class TypedValue {
public:
	TypedValue() = default;

	template <typename T>
		requires (!std::same_as<std::remove_cvref_t<T>, TypedValue>)
	TypedValue(T&& value) :
		storage_{ std::make_unique<Storage<std::remove_cvref_t<T>>>(std::forward<T>(value)) } {}

	TypedValue(const TypedValue& other) :
		storage_{ other.storage_ ? other.storage_->Clone() : nullptr } {}

	TypedValue& operator=(const TypedValue& other) {
		if (this != &other) {
			storage_ = other.storage_ ? other.storage_->Clone() : nullptr;
		}
		return *this;
	}

	TypedValue(TypedValue&&) noexcept = default;
	TypedValue& operator=(TypedValue&&) noexcept = default;

	template <typename T>
	[[nodiscard]] bool Is() const {
		return storage_ && storage_->GetTypeHash() == TypeHash<T>() &&
			storage_->GetTypeName() == TypeName<T>();
	}

	template <typename T>
	[[nodiscard]] T* TryGet() {
		return Is<T>() ? static_cast<T*>(storage_->Get()) : nullptr;
	}

	template <typename T>
	[[nodiscard]] const T* TryGet() const {
		return Is<T>() ? static_cast<const T*>(storage_->Get()) : nullptr;
	}

	template <typename T>
	[[nodiscard]] T& Get() {
		auto* value{ TryGet<T>() };
		if (!value) {
			std::abort();
		}
		return *value;
	}

	template <typename T>
	[[nodiscard]] const T& Get() const {
		const auto* value{ TryGet<T>() };
		if (!value) {
			std::abort();
		}
		return *value;
	}

	[[nodiscard]] TypeHashValue GetTypeHash() const {
		return storage_ ? storage_->GetTypeHash() : 0;
	}

	[[nodiscard]] std::string_view GetTypeName() const {
		return storage_ ? storage_->GetTypeName() : std::string_view{};
	}

	[[nodiscard]] explicit operator bool() const {
		return storage_ != nullptr;
	}

private:
	class IStorage {
	public:
		virtual ~IStorage() = default;
		[[nodiscard]] virtual std::unique_ptr<IStorage> Clone() const = 0;
		[[nodiscard]] virtual TypeHashValue GetTypeHash() const = 0;
		[[nodiscard]] virtual std::string_view GetTypeName() const = 0;
		[[nodiscard]] virtual void* Get() = 0;
		[[nodiscard]] virtual const void* Get() const = 0;
	};

	template <typename T>
	class Storage final : public IStorage {
	public:
		template <typename U>
		explicit Storage(U&& value) : value_{ std::forward<U>(value) } {}

		[[nodiscard]] std::unique_ptr<IStorage> Clone() const override {
			return std::make_unique<Storage<T>>(value_);
		}

		[[nodiscard]] TypeHashValue GetTypeHash() const override {
			return TypeHash<T>();
		}

		[[nodiscard]] std::string_view GetTypeName() const override {
			return TypeName<T>();
		}

		[[nodiscard]] void* Get() override { return &value_; }
		[[nodiscard]] const void* Get() const override { return &value_; }

	private:
		T value_;
	};

	std::unique_ptr<IStorage> storage_;
};

enum class ReentryMode {
	IgnoreWhileRunning,
	Restart,
	Queue
};

enum class EventDelivery {
	Target,
	Broadcast
};

enum class EventResult {
	Continue,
	Handled
};

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

struct ActionTiming {
	float duration_ms{ 300.0f };
	ptgn::Ease ease{ ptgn::Ease::Linear };
	int additional_repeats{ 0 };
	bool infinite_repeats{ false };
	bool reversed{ false };
	bool yoyo{ false };
};

struct ComponentDefinition {
	Id id{ IdGenerator::Next() };
	std::string type;
	ptgn::json value;
};

struct EventCondition {
	Id id{ IdGenerator::Next() };
	bool enabled{ true };
	bool consume{ false };
	std::string type;
	TypedValue filter;
};

struct Action {
	Id id{ IdGenerator::Next() };
	bool enabled{ true };
	std::string type;
	TypedValue value;
	std::optional<ActionCompletion> completion;
	std::optional<ActionTiming> timing;
};

struct LifecycleAction {
	Id id{ IdGenerator::Next() };
	bool enabled{ true };
	SequenceLifecycle lifecycle{ SequenceLifecycle::Complete };
	Action action;
};

class RuntimeHost;
class EventView;
class TimedActionBuilder;
struct NoEventFilter;
struct SceneContext;
struct ScriptSequence;
struct SequenceHandle;
struct SequenceChannelKey;
struct SignalKey;

struct ActionContext {
	RuntimeHost& host;
	ptgn::Entity owner;
	SceneContext* scene{ nullptr };
	float delta_seconds{ 0.0f };
	float linear_progress{ 0.0f };
	float progress{ 0.0f };
	int repeat{ 0 };
	bool reversed{ false };
};

struct ScriptContext {
	RuntimeHost& host;
	ptgn::Entity owner;
	SceneContext* scene{ nullptr };
	float delta_seconds{ 0.0f };
};

class IActionInstance {
public:
	virtual ~IActionInstance() = default;
	virtual void Begin(ActionContext&) {}
	[[nodiscard]] virtual ActionStatus Update(ActionContext&) { return ActionStatus::Running; }
	virtual void Repeat(ActionContext&) {}
	virtual void Complete(ActionContext&) {}
	virtual void Cancel(ActionContext&, SequenceCancelReason) {}
};

class IScriptInstance {
public:
	virtual ~IScriptInstance() = default;
	virtual void OnCreate(ScriptContext&) {}
	virtual void OnUpdate(ScriptContext&) {}
	[[nodiscard]] virtual EventResult OnEvent(ScriptContext&, const EventView&) {
		return EventResult::Continue;
	}
};

template <typename T>
class TypedActionInstance final : public IActionInstance {
public:
	explicit TypedActionInstance(T value) : value_{ std::move(value) } {}

	void Begin(ActionContext& context) override {
		if constexpr (requires(T& value) { value.OnStart(context); }) {
			value_.OnStart(context);
		}
	}

	[[nodiscard]] ActionStatus Update(ActionContext& context) override {
		if constexpr (requires(T& value) {
			{ value.OnUpdate(context) } -> std::same_as<ActionStatus>;
		}) {
			return value_.OnUpdate(context);
		} else if constexpr (requires(T& value) { value.OnUpdate(context); }) {
			value_.OnUpdate(context);
		}
		return ActionStatus::Running;
	}

	void Repeat(ActionContext& context) override {
		if constexpr (requires(T& value) { value.OnRepeat(context); }) {
			value_.OnRepeat(context);
		}
	}

	void Complete(ActionContext& context) override {
		if constexpr (requires(T& value) { value.OnComplete(context); }) {
			value_.OnComplete(context);
		}
	}

	void Cancel(ActionContext& context, SequenceCancelReason reason) override {
		if constexpr (requires(T& value) { value.OnCancel(context, reason); }) {
			value_.OnCancel(context, reason);
		} else if constexpr (requires(T& value) { value.OnCancel(context); }) {
			value_.OnCancel(context);
		}
	}

private:
	T value_;
};

template <typename T>
class TypedScriptInstance final : public IScriptInstance {
public:
	explicit TypedScriptInstance(T value) : value_{ std::move(value) } {}

	void OnCreate(ScriptContext& context) override {
		if constexpr (requires(T& value) { value.OnCreate(context); }) {
			value_.OnCreate(context);
		}
	}

	void OnUpdate(ScriptContext& context) override {
		if constexpr (requires(T& value) { value.OnUpdate(context); }) {
			value_.OnUpdate(context);
		}
	}

	[[nodiscard]] EventResult OnEvent(
		ScriptContext& context,
		const EventView& event
	) override {
		if constexpr (requires(T& value) {
			{ value.OnEvent(context, event) } -> std::same_as<EventResult>;
		}) {
			return value_.OnEvent(context, event);
		} else if constexpr (requires(T& value) { value.OnEvent(context, event); }) {
			value_.OnEvent(context, event);
		}
		return EventResult::Continue;
	}

private:
	T value_;
};

[[nodiscard]] inline ComponentDefinition MakeComponentDefinition(
	const ptgn::RegisteredComponent& component
) {
	if (!component.make_default_json) {
		return {};
	}

	return ComponentDefinition{
		.id = IdGenerator::Next(),
		.type = std::string{ component.name },
		.value = component.make_default_json(),
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

	ptgn::json component_json;
	component_json = std::move(value);

	return ComponentDefinition{
		.id = IdGenerator::Next(),
		.type = std::string{ component->name },
		.value = std::move(component_json),
	};
}

struct EventMatchContext {
	RuntimeHost& host;
	ptgn::Entity owner;
	float held_duration_ms{ 0.0f };
};

struct EventEnvelope {
	TypeHashValue type_hash{ 0 };
	std::string type;
	TypedValue payload;
	std::optional<ptgn::Entity> target;
	ptgn::Entity source;
	EventDelivery delivery{ EventDelivery::Target };
	float held_duration_ms{ 0.0f };
};

class EventView {
public:
	explicit EventView(const EventEnvelope& event) : event_{ &event } {}

	template <typename T>
	[[nodiscard]] bool Is() const {
		return event_->type_hash == TypeHash<T>() && event_->payload.Is<T>();
	}

	template <typename T>
	[[nodiscard]] const T& Get() const {
		return event_->payload.Get<T>();
	}

	[[nodiscard]] std::string_view Type() const { return event_->type; }
	[[nodiscard]] TypeHashValue TypeHashCode() const { return event_->type_hash; }
	[[nodiscard]] ptgn::Entity Source() const { return event_->source; }
	[[nodiscard]] std::optional<ptgn::Entity> Target() const { return event_->target; }
	[[nodiscard]] float HeldDurationMs() const { return event_->held_duration_ms; }
	[[nodiscard]] EventDelivery Delivery() const { return event_->delivery; }

private:
	const EventEnvelope* event_{ nullptr };
};

/// @brief Narrow host interface required by the reusable sequence runtime.
///
/// The real engine can implement this with Scene/Application services. The standalone demo uses
/// A standalone host can implement it without coupling the engine model to the application loop.
struct PrefabSpawnRequest {
	std::string_view prefab_key;
	ptgn::Entity owner;
	ptgn::V2_float position{};
	bool parent_to_owner{ false };
	bool inherit_owner_rotation{ true };
	bool inherit_owner_scale{ true };
	bool random_rotation{ false };
};

class RuntimeHost {
public:
	virtual ~RuntimeHost() = default;

	virtual void Queue(EventEnvelope event) = 0;
	virtual void Log(std::string text) = 0;
	[[nodiscard]] virtual std::string_view Name(ptgn::Entity entity) const = 0;
	[[nodiscard]] virtual std::string_view Tag(ptgn::Entity entity) const = 0;
	[[nodiscard]] virtual int Mask(ptgn::Entity entity) const = 0;
	[[nodiscard]] virtual bool KeyDown(ptgn::Key key) const = 0;
	[[nodiscard]] virtual float KeyHoldDuration(ptgn::Key key) const = 0;
	[[nodiscard]] virtual bool MouseDown(ptgn::Mouse button) const = 0;
	[[nodiscard]] virtual float MouseHoldDuration(ptgn::Mouse button) const = 0;
	virtual ptgn::Entity SpawnPrefab(const PrefabSpawnRequest& request) = 0;
	virtual void Destroy(ptgn::Entity entity) = 0;

	virtual SequenceHandle RunSequence(ptgn::Entity owner, ScriptSequence sequence) = 0;
	virtual SequenceHandle RunInChannel(
		ptgn::Entity owner,
		SequenceChannelKey channel,
		ScriptSequence sequence,
		ReentryMode reentry
	) = 0;
	virtual bool StartSequence(ptgn::Entity owner, Id id, bool force = false) = 0;
	virtual void StopSequenceChannel(
		ptgn::Entity owner,
		SequenceChannelKey channel,
		SequenceStopMode mode
	) = 0;
	virtual bool StopSequence(
		ptgn::Entity owner,
		Id id,
		SequenceCancelReason reason = SequenceCancelReason::Stopped
	) = 0;
	virtual bool ResetSequence(ptgn::Entity owner, Id id) = 0;
	virtual bool ClearSequence(ptgn::Entity owner, Id id) = 0;
	virtual bool SkipSequenceAction(ptgn::Entity owner, Id id) = 0;
	virtual bool SeekSequence(ptgn::Entity owner, Id id, float progress) = 0;
	virtual bool SetSequencePaused(ptgn::Entity owner, Id id, bool paused) = 0;
	[[nodiscard]] virtual float SequenceProgress(ptgn::Entity owner, Id id) const = 0;
	[[nodiscard]] virtual bool SequenceRunning(ptgn::Entity owner, Id id) const = 0;
	[[nodiscard]] virtual bool SequencePaused(ptgn::Entity owner, Id id) const = 0;
	[[nodiscard]] virtual bool SequenceCompleted(ptgn::Entity owner, Id id) const = 0;
};

struct EventRegistration {
	TypeHashValue type_hash{ 0 };
	std::uint32_t schema_version{ 1 };
	std::string key;
	std::function<TypedValue()> make_default_filter;
	std::function<TypedValue()> make_default_payload;
	std::function<bool(const TypedValue&, const TypedValue&, EventMatchContext&)> matches;
	std::function<float(const TypedValue&)> start_delay_ms;
};

class EventRegistry {
public:
	template <typename TEvent, typename TFilter, typename F>
	static bool Register(std::string key, F&& matches) {
		auto& entries{ MutableEntries() };
		if (Find(key) || Find(TypeHash<TEvent>())) {
			return false;
		}
		entries.push_back(EventRegistration{
			.type_hash = TypeHash<TEvent>(),
			.key = std::move(key),
			.make_default_filter = [] { return TypedValue{ TFilter{} }; },
			.make_default_payload = [] { return TypedValue{ TEvent{} }; },
			.matches = [fn = std::forward<F>(matches)](
				const TypedValue& payload,
				const TypedValue& filter,
				EventMatchContext& context
			) mutable {
				return std::invoke(
					fn,
					payload.Get<TEvent>(),
					filter.Get<TFilter>(),
					context
				);
			},
			.start_delay_ms = [](const TypedValue& filter) {
				if constexpr (requires(const TFilter& value) { value.delay_ms; }) {
					return std::max(0.0f, filter.Get<TFilter>().delay_ms);
				}
				return 0.0f;
			},
		});
		return true;
	}

	template <typename TEvent>
	[[nodiscard]] static std::string_view Key() {
		const auto* entry{ Find(TypeHash<TEvent>()) };
		return entry ? std::string_view{ entry->key } : std::string_view{};
	}

	template <typename TEvent, typename TFilter>
	[[nodiscard]] static EventCondition MakeCondition(TFilter filter = {}) {
		const auto* entry{ Find(TypeHash<TEvent>()) };
		return entry ? EventCondition{
			.id = IdGenerator::Next(),
			.enabled = true,
			.consume = false,
			.type = entry->key,
			.filter = TypedValue{ std::move(filter) },
		} : EventCondition{};
	}

	template <typename TEvent>
	[[nodiscard]] static EventEnvelope MakeEvent(
		TEvent payload = {},
		std::optional<ptgn::Entity> target = std::nullopt,
		ptgn::Entity source = {},
		EventDelivery delivery = EventDelivery::Target
	) {
		const auto* entry{ Find(TypeHash<TEvent>()) };
		return entry ? EventEnvelope{
			.type_hash = entry->type_hash,
			.type = entry->key,
			.payload = TypedValue{ std::move(payload) },
			.target = target,
			.source = source,
			.delivery = delivery,
		} : EventEnvelope{};
	}

	[[nodiscard]] static const EventRegistration* Find(std::string_view key) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [key](const auto& entry) {
			return entry.key == key;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const EventRegistration* Find(TypeHashValue type_hash) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const std::vector<EventRegistration>& Entries() {
		return MutableEntries();
	}

private:
	[[nodiscard]] static std::vector<EventRegistration>& MutableEntries() {
		static std::vector<EventRegistration> entries;
		return entries;
	}
};

/// @brief Double-buffered event queue. Events pushed while dispatching are stored for the next pass.
class BufferedEventQueue {
public:
	void Push(EventEnvelope event) {
		if (!event.type.empty()) {
			pending_.push_back(std::move(event));
		}
	}

	void BeginDispatch() {
		if (dispatching_.empty()) {
			dispatching_.swap(pending_);
		}
	}

	[[nodiscard]] bool Empty() const { return dispatching_.empty(); }

	EventEnvelope Pop() {
		EventEnvelope event{ std::move(dispatching_.front()) };
		dispatching_.pop_front();
		return event;
	}

	void Clear() {
		pending_.clear();
		dispatching_.clear();
	}

private:
	std::deque<EventEnvelope> pending_;
	std::deque<EventEnvelope> dispatching_;
};

class LocalEventHandler {
public:
	template <typename TEvent>
	void Push(ptgn::Entity target, TEvent event = {}, ptgn::Entity source = {}) {
		queue_.Push(EventRegistry::MakeEvent<TEvent>(
			std::move(event), target, source, EventDelivery::Target
		));
	}

	void Push(EventEnvelope event) {
		event.delivery = EventDelivery::Target;
		queue_.Push(std::move(event));
	}

	void BeginDispatch() { queue_.BeginDispatch(); }
	[[nodiscard]] bool Empty() const { return queue_.Empty(); }
	EventEnvelope Pop() { return queue_.Pop(); }
	void Clear() { queue_.Clear(); }

private:
	BufferedEventQueue queue_;
};

class GlobalEventHandler {
public:
	template <typename TEvent>
	void Push(TEvent event = {}, ptgn::Entity source = {}) {
		queue_.Push(EventRegistry::MakeEvent<TEvent>(
			std::move(event), std::nullopt, source, EventDelivery::Broadcast
		));
	}

	void Push(EventEnvelope event) {
		event.target.reset();
		event.delivery = EventDelivery::Broadcast;
		queue_.Push(std::move(event));
	}

	void BeginDispatch() { queue_.BeginDispatch(); }
	[[nodiscard]] bool Empty() const { return queue_.Empty(); }
	EventEnvelope Pop() { return queue_.Pop(); }
	void Clear() { queue_.Clear(); }

private:
	BufferedEventQueue queue_;
};

struct PointerFrame {
	ptgn::V2_float world_position{};
	bool inside_scene{ false };
	std::array<bool, 3> pressed{};
	std::array<bool, 3> released{};
};

struct SceneContext {
	RuntimeHost* host{ nullptr };
	ptgn::Manager& manager;
	LocalEventHandler& local_events;
	GlobalEventHandler& global_events;

	template <typename TEvent>
	void PushLocal(ptgn::Entity target, TEvent event = {}, ptgn::Entity source = {}) {
		local_events.Push<TEvent>(target, std::move(event), source);
	}

	template <typename TEvent>
	void PushGlobal(TEvent event = {}, ptgn::Entity source = {}) {
		global_events.Push<TEvent>(std::move(event), source);
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
	std::function<TypedValue()> make_default;
	std::function<std::unique_ptr<IActionInstance>(const TypedValue&)> instantiate;
};

class ActionRegistry {
public:
	template <typename T>
	static bool Register(
		std::string key,
		bool supports_timing = false,
		bool requires_timing = false,
		std::optional<ActionTiming> default_timing = std::nullopt,
		ActionCompletion completion = ActionCompletion::Instant,
		bool serializable = true
	) {
		auto& entries{ MutableEntries() };
		if (Find(key) || Find(TypeHash<T>())) {
			return false;
		}
		if (requires_timing && completion == ActionCompletion::Instant) {
			completion = ActionCompletion::Duration;
		}

		entries.push_back(ActionRegistration{
			.type_hash = TypeHash<T>(),
			.key = std::move(key),
			.completion = completion,
			.supports_timing = supports_timing,
			.requires_timing = requires_timing,
			.serializable = serializable,
			.default_timing = default_timing,
			.make_default = [] { return TypedValue{ T{} }; },
			.instantiate = [](const TypedValue& value) {
				return std::make_unique<TypedActionInstance<T>>(
					value.Get<T>()
				);
			},
		});
		return true;
	}

	template <typename T>
	[[nodiscard]] static std::string_view Key() {
		const auto* entry{ Find(TypeHash<T>()) };
		return entry ? std::string_view{ entry->key } : std::string_view{};
	}

	template <typename T>
		requires (!std::convertible_to<std::remove_cvref_t<T>, std::string_view>)
	[[nodiscard]] static Action Make(T value = {}) {
		const auto* entry{ Find(TypeHash<T>()) };
		return entry ? Action{
			.id = IdGenerator::Next(),
			.enabled = true,
			.type = entry->key,
			.value = TypedValue{ std::move(value) },
			.timing = entry->default_timing,
		} : Action{};
	}

	[[nodiscard]] static Action Make(std::string_view key) {
		const auto* entry{ Find(key) };
		return entry ? Action{
			.id = IdGenerator::Next(),
			.enabled = true,
			.type = entry->key,
			.value = entry->make_default(),
			.timing = entry->default_timing,
		} : Action{};
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
	std::function<TypedValue()> make_default;
	std::function<std::unique_ptr<IScriptInstance>(const TypedValue&)> instantiate;
};

class ScriptRegistry {
public:
	template <typename T>
	static bool Register(std::string key) {
		auto& entries{ MutableEntries() };
		if (Find(key) || Find(TypeHash<T>())) {
			return false;
		}
		entries.push_back(ScriptRegistration{
			.type_hash = TypeHash<T>(),
			.key = std::move(key),
			.make_default = [] { return TypedValue{ T{} }; },
			.instantiate = [](const TypedValue& value) {
				return std::make_unique<TypedScriptInstance<T>>(
					value.Get<T>()
				);
			},
		});
		return true;
	}

	template <typename T>
	[[nodiscard]] static std::string_view Key() {
		const auto* entry{ Find(TypeHash<T>()) };
		return entry ? std::string_view{ entry->key } : std::string_view{};
	}

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

struct ScriptEntry {
	Id id{ IdGenerator::Next() };
	bool enabled{ true };
	std::string type;
	TypedValue value;

	// Runtime-only state. A copied ScriptsComponent receives a fresh script instance.
	std::unique_ptr<IScriptInstance> instance;
	bool created{ false };

	ScriptEntry() = default;
	ScriptEntry(ScriptEntry&&) noexcept = default;
	ScriptEntry& operator=(ScriptEntry&&) noexcept = default;

	ScriptEntry(const ScriptEntry& other) :
		id{ IdGenerator::Next() },
		enabled{ other.enabled },
		type{ other.type },
		value{ other.value } {}

	ScriptEntry& operator=(const ScriptEntry& other) {
		if (this == &other) {
			return *this;
		}

		ScriptEntry copy{ other };
		*this = std::move(copy);
		return *this;
	}
};

struct SequenceChannelKey : ptgn::StrongString<SequenceChannelKey> {
	using StrongString::StrongString;
	SequenceChannelKey() = default;
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
	float pending_start_delay_ms{ -1.0f };
	std::unique_ptr<IActionInstance> action_instance;
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
	Id id{ IdGenerator::Next() };
	bool enabled{ true };
	bool shared_reference{ false };
	Id shared_sequence_id{ 0 };
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

	template <typename TEvent, typename TFilter = NoEventFilter>
	ScriptSequence& StartOn(TFilter filter = {});

	template <typename TEvent, typename TFilter = NoEventFilter>
	ScriptSequence& StopOn(TFilter filter = {});

	template <typename TAction>
	ScriptSequence& Then(TAction action = {});

	template <typename TAction>
	ScriptSequence& UntilComplete(TAction action = {});

	template <typename TAction>
	ScriptSequence& Forever(TAction action = {});

	template <typename TAction>
	TimedActionBuilder During(float duration_ms, TAction action = {});

	ScriptSequence& Wait(float duration_ms);
	ScriptSequence& EmitSignal(SignalKey signal);

	ScriptSequence(const ScriptSequence& other) :
		id{ IdGenerator::Next() },
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
		lifecycle_actions{ other.lifecycle_actions } {
		for (auto& event : start_events) {
			event.id = IdGenerator::Next();
		}
		for (auto& event : stop_events) {
			event.id = IdGenerator::Next();
		}
		for (auto& action : actions) {
			action.id = IdGenerator::Next();
		}
		for (auto& lifecycle : lifecycle_actions) {
			lifecycle.id = IdGenerator::Next();
			lifecycle.action.id = IdGenerator::Next();
		}
	}

	ScriptSequence& operator=(const ScriptSequence& other) {
		if (this == &other) {
			return *this;
		}
		ScriptSequence copy{ other };
		*this = std::move(copy);
		return *this;
	}

private:
	friend class TimedActionBuilder;

	ActionTiming& FindTiming(Id action_id);
};

struct SequenceChannelRuntime {
	SequenceChannelKey key;
	std::optional<Id> active;
	std::deque<Id> waiting;
};

struct ScriptsComponent {
	std::vector<ScriptEntry> scripts;
	std::vector<ScriptSequence> sequences;
	std::vector<SequenceChannelRuntime> channels;

	// Mutations are applied outside the active script/sequence iteration pass.
	std::vector<ScriptEntry> pending_script_additions;
	std::vector<Id> pending_script_removals;
	std::vector<ScriptSequence> pending_sequence_additions;
	std::vector<Id> pending_sequence_removals;

	ScriptsComponent() = default;
	ScriptsComponent(ScriptsComponent&&) noexcept = default;
	ScriptsComponent& operator=(ScriptsComponent&&) noexcept = default;

	ScriptsComponent(const ScriptsComponent& other) :
		scripts{ other.scripts },
		sequences{ other.sequences } {}

	ScriptsComponent& operator=(const ScriptsComponent& other) {
		if (this != &other) {
			ScriptsComponent copy{ other };
			*this = std::move(copy);
		}
		return *this;
	}

	template <typename TScript>
	Id AddScriptDeferred(TScript value = {}) {
		const auto* registration{ ScriptRegistry::Find(TypeHash<TScript>()) };
		if (!registration) {
			return 0;
		}
		ScriptEntry entry;
		entry.enabled = true;
		entry.type = registration->key;
		entry.value = TypedValue{ std::move(value) };
		const Id id{ entry.id };
		pending_script_additions.push_back(std::move(entry));
		return id;
	}

	Id AddSequenceDeferred(ScriptSequence sequence) {
		const Id id{ sequence.id };
		pending_sequence_additions.push_back(std::move(sequence));
		return id;
	}

	void RemoveScriptDeferred(Id id) { pending_script_removals.push_back(id); }
	void RemoveSequenceDeferred(Id id) { pending_sequence_removals.push_back(id); }
};

struct SequenceHandle {
	RuntimeHost* host{ nullptr };
	ptgn::Entity owner;
	Id binding_id{ 0 };

	[[nodiscard]] explicit operator bool() const {
		return host && owner && binding_id != 0;
	}

	bool Start(bool force = false) const {
		return *this && host->StartSequence(owner, binding_id, force);
	}

	bool Stop() const {
		return *this && host->StopSequence(owner, binding_id);
	}

	bool Pause() const {
		return *this && host->SetSequencePaused(owner, binding_id, true);
	}

	bool Resume() const {
		return *this && host->SetSequencePaused(owner, binding_id, false);
	}

	bool TogglePaused() const {
		return *this && host->SetSequencePaused(
			owner, binding_id, !host->SequencePaused(owner, binding_id)
		);
	}

	bool Toggle() const {
		return IsRunning() ? Stop() : Start();
	}

	bool Reset() const {
		return *this && host->ResetSequence(owner, binding_id);
	}

	bool Clear() const {
		return *this && host->ClearSequence(owner, binding_id);
	}

	bool Skip() const {
		return *this && host->SkipSequenceAction(owner, binding_id);
	}

	bool Seek(float progress) const {
		return *this && host->SeekSequence(owner, binding_id, progress);
	}

	[[nodiscard]] bool IsRunning() const {
		return *this && host->SequenceRunning(owner, binding_id);
	}

	[[nodiscard]] bool IsPaused() const {
		return *this && host->SequencePaused(owner, binding_id);
	}

	[[nodiscard]] bool IsCompleted() const {
		return *this && host->SequenceCompleted(owner, binding_id);
	}

	[[nodiscard]] float Progress() const {
		return *this ? host->SequenceProgress(owner, binding_id) : 0.0f;
	}
};

static_assert(std::copy_constructible<ScriptEntry>);
static_assert(std::is_copy_assignable_v<ScriptEntry>);
static_assert(std::copy_constructible<ScriptSequence>);
static_assert(std::is_copy_assignable_v<ScriptSequence>);
static_assert(std::copy_constructible<ScriptsComponent>);
static_assert(std::is_copy_assignable_v<ScriptsComponent>);

struct SharedScriptSequenceRegistry {
	std::vector<ScriptSequence> sequences;

	[[nodiscard]] ScriptSequence* Find(Id id) {
		const auto it{ std::ranges::find_if(sequences, [id](const auto& sequence) {
			return sequence.id == id;
		}) };
		return it == sequences.end() ? nullptr : &*it;
	}

	[[nodiscard]] const ScriptSequence* Find(Id id) const {
		return const_cast<SharedScriptSequenceRegistry*>(this)->Find(id);
	}
};

struct NoEventFilter {};

struct MouseButtonFilter {
	ptgn::Mouse button{ ptgn::Mouse::Left };
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

struct SignalKey : ptgn::StrongString<SignalKey> {
	using StrongString::StrongString;
	SignalKey() = default;
};

struct Signal {
	SignalKey key;
};

struct MouseMoveOver {
	ptgn::Entity pointer;
};

struct MouseMoveOut {
	ptgn::Entity pointer;
};

struct MousePressedOver {
	ptgn::Mouse button{ ptgn::Mouse::Left };
};

struct MouseReleasedOver {
	ptgn::Mouse button{ ptgn::Mouse::Left };
	bool released_over{ false };
};

struct ButtonPress {
	ptgn::Mouse button{ ptgn::Mouse::Left };
};

struct OnCreate {
	ptgn::Entity entity;
};

struct OnCreateFilter {
	float delay_ms{ 0.0f };
};

struct KeyListFilter {
	std::string keys{ "W" };
	float hold_duration_ms{ 500.0f };
};

struct MouseListFilter {
	std::string buttons{ "Left" };
	float hold_duration_ms{ 500.0f };
};

struct EntityMaskFilter {
	std::string tags;
	std::string masks;
};

template <typename TEnum>
struct ParsedEnumExpression {
	std::vector<std::vector<TEnum>> alternatives;
	std::vector<std::string> invalid_tokens;

	[[nodiscard]] bool IsValid() const {
		return invalid_tokens.empty();
	}
};

inline std::string_view TrimView(std::string_view value) {
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
		value.remove_prefix(1);
	}
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
		value.remove_suffix(1);
	}
	return value;
}

inline std::string NormalizeEnumToken(std::string_view value) {
	std::string normalized;
	normalized.reserve(value.size());
	for (const char c : value) {
		if (c == ' ' || c == '_' || c == '-') {
			continue;
		}
		normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	}
	return normalized;
}

template <typename TEnum>
[[nodiscard]] bool IsValidNumericEnumValue(long long numeric) {
	using Underlying = std::underlying_type_t<TEnum>;
	if constexpr (std::same_as<TEnum, ptgn::Mouse>) {
		return numeric >= 0 &&
			numeric <= static_cast<long long>(static_cast<Underlying>(ptgn::Mouse::Last));
	}
	if constexpr (std::same_as<TEnum, ptgn::Key>) {
		return (numeric >= 32 && numeric <= 96) ||
			numeric == 161 || numeric == 162 ||
			(numeric >= 256 && numeric <= 348);
	}
	return magic_enum::enum_cast<TEnum>(static_cast<Underlying>(numeric)).has_value();
}

template <typename TEnum>
[[nodiscard]] std::optional<TEnum> ParseEnumToken(std::string_view token) {
	token = TrimView(token);
	if (token.empty()) {
		return std::nullopt;
	}

	using Underlying = std::underlying_type_t<TEnum>;
	long long numeric{};
	const auto* begin{ token.data() };
	const auto* end{ token.data() + token.size() };
	const auto [ptr, error]{ std::from_chars(begin, end, numeric) };
	if (error == std::errc{} && ptr == end) {
		if (!IsValidNumericEnumValue<TEnum>(numeric)) {
			return std::nullopt;
		}
		return static_cast<TEnum>(static_cast<Underlying>(numeric));
	}

	const std::string normalized{ NormalizeEnumToken(token) };
	if constexpr (std::same_as<TEnum, ptgn::Mouse>) {
		if (normalized == "left") {
			return ptgn::Mouse::Left;
		}
		if (normalized == "right") {
			return ptgn::Mouse::Right;
		}
		if (normalized == "middle") {
			return ptgn::Mouse::Middle;
		}
	}

	if (const auto value{ magic_enum::enum_cast<TEnum>(token, magic_enum::case_insensitive) }) {
		return value;
	}

	for (const auto& [value, name] : magic_enum::enum_entries<TEnum>()) {
		const std::string candidate{ NormalizeEnumToken(name) };
		if (normalized == candidate) {
			return value;
		}
		if (normalized.ends_with("arrow") &&
			normalized.substr(0, normalized.size() - std::string_view{ "arrow" }.size()) == candidate) {
			return value;
		}
		if (candidate.ends_with("arrow") &&
			candidate.substr(0, candidate.size() - std::string_view{ "arrow" }.size()) == normalized) {
			return value;
		}
	}
	return std::nullopt;
}

template <typename TEnum>
[[nodiscard]] ParsedEnumExpression<TEnum> ParseEnumExpression(std::string_view text) {
	ParsedEnumExpression<TEnum> result;
	std::size_t alternative_start{};
	while (alternative_start <= text.size()) {
		const std::size_t comma{ text.find(',', alternative_start) };
		const std::size_t alternative_finish{
			comma == std::string_view::npos ? text.size() : comma
		};
		const std::string_view alternative_text{
			TrimView(text.substr(alternative_start, alternative_finish - alternative_start))
		};

		std::vector<TEnum> combination;
		std::size_t token_start{};
		while (token_start <= alternative_text.size()) {
			const std::size_t plus{ alternative_text.find('+', token_start) };
			const std::size_t token_finish{
				plus == std::string_view::npos ? alternative_text.size() : plus
			};
			const std::string_view token{
				TrimView(alternative_text.substr(token_start, token_finish - token_start))
			};
			if (!token.empty()) {
				if (const auto value{ ParseEnumToken<TEnum>(token) }) {
					if (!std::ranges::contains(combination, *value)) {
						combination.push_back(*value);
					}
				} else {
					result.invalid_tokens.emplace_back(token);
				}
			}
			if (plus == std::string_view::npos) {
				break;
			}
			token_start = plus + 1;
		}

		if (!combination.empty()) {
			result.alternatives.push_back(std::move(combination));
		}
		if (comma == std::string_view::npos) {
			break;
		}
		alternative_start = comma + 1;
	}
	return result;
}

template <typename TEnum, typename TDown>
[[nodiscard]] bool MatchesPressedOrHeldCombination(
	TEnum event_value,
	const ParsedEnumExpression<TEnum>& expression,
	TDown&& is_down
) {
	return std::ranges::any_of(expression.alternatives, [&](const auto& combination) {
		return std::ranges::contains(combination, event_value) &&
			std::ranges::all_of(combination, [&](TEnum value) {
				return std::invoke(is_down, value);
			});
	});
}

template <typename TEnum, typename TDown>
[[nodiscard]] bool MatchesReleasedCombination(
	TEnum event_value,
	const ParsedEnumExpression<TEnum>& expression,
	TDown&& is_down
) {
	return std::ranges::any_of(expression.alternatives, [&](const auto& combination) {
		return std::ranges::contains(combination, event_value) &&
			std::ranges::none_of(combination, [&](TEnum value) {
				return std::invoke(is_down, value);
			});
	});
}

struct InclusionFilter {
	std::vector<std::string> included_tags;
	std::vector<std::string> excluded_tags;
	std::vector<int> included_masks;
	std::vector<int> excluded_masks;
	std::vector<std::string> invalid_masks;
};

inline void ParseTagTokens(
	std::string_view text,
	std::vector<std::string>& included,
	std::vector<std::string>& excluded
) {
	std::size_t start{};
	while (start <= text.size()) {
		const std::size_t comma{ text.find(',', start) };
		const std::size_t finish{ comma == std::string_view::npos ? text.size() : comma };
		std::string_view token{ TrimView(text.substr(start, finish - start)) };
		bool is_excluded{ false };
		if (!token.empty() && token.front() == '-') {
			is_excluded = true;
			token = TrimView(token.substr(1));
		}
		if (!token.empty()) {
			(is_excluded ? excluded : included).emplace_back(token);
		}
		if (comma == std::string_view::npos) {
			break;
		}
		start = comma + 1;
	}
}

inline void ParseMaskTokens(
	std::string_view text,
	std::vector<int>& included,
	std::vector<int>& excluded,
	std::vector<std::string>& invalid
) {
	std::size_t start{};
	while (start <= text.size()) {
		const std::size_t comma{ text.find(',', start) };
		const std::size_t finish{ comma == std::string_view::npos ? text.size() : comma };
		std::string_view token{ TrimView(text.substr(start, finish - start)) };
		bool is_excluded{ false };
		if (!token.empty() && token.front() == '-') {
			is_excluded = true;
			token = TrimView(token.substr(1));
		}
		if (!token.empty()) {
			int value{};
			const auto [ptr, error]{ std::from_chars(token.data(), token.data() + token.size(), value) };
			if (error == std::errc{} && ptr == token.data() + token.size()) {
				(is_excluded ? excluded : included).push_back(value);
			} else {
				invalid.emplace_back(token);
			}
		}
		if (comma == std::string_view::npos) {
			break;
		}
		start = comma + 1;
	}
}

[[nodiscard]] inline InclusionFilter ParseEntityMaskFilter(const EntityMaskFilter& filter) {
	InclusionFilter parsed;
	ParseTagTokens(filter.tags, parsed.included_tags, parsed.excluded_tags);
	ParseMaskTokens(filter.masks, parsed.included_masks, parsed.excluded_masks, parsed.invalid_masks);
	return parsed;
}

[[nodiscard]] inline bool MatchesEntityMaskFilter(
	ptgn::Entity other,
	const EntityMaskFilter& filter,
	const RuntimeHost& host
) {
	const InclusionFilter parsed{ ParseEntityMaskFilter(filter) };
	if (parsed.included_tags.empty() && parsed.excluded_tags.empty() &&
		parsed.included_masks.empty() && parsed.excluded_masks.empty()) {
		return false;
	}

	const std::string_view tag{ other ? host.Tag(other) : std::string_view{} };
	const int mask{ other ? host.Mask(other) : 0 };

	if (std::ranges::contains(parsed.excluded_tags, tag)) {
		return false;
	}
	for (const int excluded : parsed.excluded_masks) {
		if ((mask & excluded) != 0) {
			return false;
		}
	}

	const bool tag_matches{ parsed.included_tags.empty() ||
		std::ranges::contains(parsed.included_tags, tag) };
	const bool mask_matches{ parsed.included_masks.empty() ||
		std::ranges::any_of(parsed.included_masks, [mask](int included) {
			return (mask & included) != 0;
		}) };
	return tag_matches && mask_matches;
}

template <typename TEvent>
[[nodiscard]] ptgn::Mouse MouseFromEvent(const TEvent& event) {
	if constexpr (requires { event.button; }) {
		return static_cast<ptgn::Mouse>(event.button);
	} else if constexpr (requires { event.mouse; }) {
		return static_cast<ptgn::Mouse>(event.mouse);
	} else {
		return static_cast<ptgn::Mouse>(event);
	}
}

template <typename TEvent>
void SetMouseEvent(TEvent& event, ptgn::Mouse mouse) {
	if constexpr (requires { event.button = mouse; }) {
		event.button = mouse;
	} else if constexpr (requires { event.mouse = mouse; }) {
		event.mouse = mouse;
	}
}

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
	} else {
		return {};
	}
}

struct PrefabDefinition {
	Id id{ IdGenerator::Next() };
	std::string key{ "prefabs/new_entity" };
	std::string name{ "New Prefab" };
	std::string tag;
	std::vector<ComponentDefinition> components;
	std::optional<ScriptsComponent> scripts;
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

enum class SpawnOrigin {
	OwnerEntity,
	Position
};

enum class SpawnArea {
	Point,
	Rectangle,
	Circle
};

// Built-in Actions. Each Action affects ActionContext::owner when applicable.
struct WaitAction {};

struct MoveToAction {
	ptgn::V2_float destination{ 0.0f, 64.0f };
	bool relative{ true };

	MoveToAction() = default;
	MoveToAction(ptgn::V2_float destination, bool relative) :
		destination{ destination }, relative{ relative } {}

	void OnStart(ActionContext& context);
	void OnUpdate(ActionContext& context);
	void OnRepeat(ActionContext& context);

private:
	ptgn::V2_float start_{};
	ptgn::V2_float end_{};
};

struct RotateToAction {
	float degrees{ 90.0f };
	bool shortest_path{ true };
	bool relative{ false };

	RotateToAction() = default;
	RotateToAction(float degrees, bool shortest_path = true, bool relative = false) :
		degrees{ degrees }, shortest_path{ shortest_path }, relative{ relative } {}

	void OnStart(ActionContext& context);
	void OnUpdate(ActionContext& context);
	void OnRepeat(ActionContext& context);

private:
	float start_degrees_{ 0.0f };
	float delta_degrees_{ 0.0f };
};

struct ScaleToAction {
	ptgn::V2_float scale{ 1.0f, 1.0f };
	bool relative{ false };

	ScaleToAction() = default;
	ScaleToAction(ptgn::V2_float scale, bool relative = false) :
		scale{ scale }, relative{ relative } {}

	void OnStart(ActionContext& context);
	void OnUpdate(ActionContext& context);
	void OnRepeat(ActionContext& context);

private:
	ptgn::V2_float start_{};
	ptgn::V2_float end_{};
};

/// @brief Action-controlled example: no fixed duration is required.
struct FollowTargetAction {
	ptgn::Entity target;
	float speed{ 120.0f };
	float stopping_distance{ 2.0f };

	[[nodiscard]] ActionStatus OnUpdate(ActionContext& context);
};

struct NativeAction {
	std::function<void(ActionContext&)> on_start;
	std::function<ActionStatus(ActionContext&)> on_update;
	std::function<void(ActionContext&)> on_complete;
	std::function<void(ActionContext&, SequenceCancelReason)> on_cancel;

	void OnStart(ActionContext& context) {
		if (on_start) { on_start(context); }
	}

	[[nodiscard]] ActionStatus OnUpdate(ActionContext& context) {
		return on_update ? on_update(context) : ActionStatus::Complete;
	}

	void OnComplete(ActionContext& context) {
		if (on_complete) { on_complete(context); }
	}

	void OnCancel(ActionContext& context, SequenceCancelReason reason) {
		if (on_cancel) { on_cancel(context, reason); }
	}
};

struct SetVisibleAction {
	bool visible{ true };
	void OnStart(ActionContext& context);
};

struct PlayAudioAction {
	std::string asset{ "door_open" };
	float volume{ 1.0f };
	int loops{ 0 };
	void OnStart(ActionContext& context);
};

struct EmitSignalAction {
	SignalKey signal{ "sequence.completed" };
	void OnStart(ActionContext& context);
};

struct AddComponentsAction {
	std::vector<ComponentDefinition> components;
	void OnStart(ActionContext& context);
};

struct RemoveComponentsAction {
	std::vector<std::string> components;
	void OnStart(ActionContext& context);
};

struct SpawnEntityAction {
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
	void OnStart(ActionContext& context);
};

// Fluent timing modifier for the Action most recently added through ScriptSequence::During().
class TimedActionBuilder {
public:
	TimedActionBuilder& Ease(ptgn::Ease ease);
	TimedActionBuilder& Repeat(int additional_repeats);
	TimedActionBuilder& Infinite();
	TimedActionBuilder& Reversed(bool reversed = true);
	TimedActionBuilder& Yoyo(bool yoyo = true);
	ScriptSequence& End();

private:
	friend struct ScriptSequence;
	TimedActionBuilder(ScriptSequence& parent, Id action_id) :
		parent_{ &parent }, action_id_{ action_id } {}

	ActionTiming& Timing();
	ScriptSequence* parent_{ nullptr };
	Id action_id_{ 0 };
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

template <typename TEvent, typename TFilter>
ScriptSequence& ScriptSequence::StartOn(TFilter filter) {
	start_events.push_back(EventRegistry::MakeCondition<TEvent>(std::move(filter)));
	return *this;
}

template <typename TEvent, typename TFilter>
ScriptSequence& ScriptSequence::StopOn(TFilter filter) {
	stop_events.push_back(EventRegistry::MakeCondition<TEvent>(std::move(filter)));
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
TimedActionBuilder ScriptSequence::During(float duration_ms, TAction action) {
	Action definition{ ActionRegistry::Make<TAction>(std::move(action)) };
	definition.completion = ActionCompletion::Duration;
	definition.timing = definition.timing.value_or(ActionTiming{});
	definition.timing->duration_ms = std::max(0.0f, duration_ms);
	const Id action_id{ definition.id };
	actions.push_back(std::move(definition));
	return TimedActionBuilder{ *this, action_id };
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
	return Then(EmitSignalAction{ .signal = std::move(signal) });
}

inline ActionTiming& ScriptSequence::FindTiming(Id action_id) {
	auto it{ std::ranges::find_if(actions, [action_id](const auto& action) {
		return action.id == action_id;
	}) };
	if (it == actions.end() || !it->timing) {
		std::abort();
	}
	return *it->timing;
}

inline ActionTiming& TimedActionBuilder::Timing() {
	return parent_->FindTiming(action_id_);
}

inline TimedActionBuilder& TimedActionBuilder::Ease(ptgn::Ease ease) {
	Timing().ease = ease;
	return *this;
}

inline TimedActionBuilder& TimedActionBuilder::Repeat(int additional_repeats) {
	Timing().additional_repeats = std::max(0, additional_repeats);
	Timing().infinite_repeats = false;
	return *this;
}

inline TimedActionBuilder& TimedActionBuilder::Infinite() {
	Timing().infinite_repeats = true;
	return *this;
}

inline TimedActionBuilder& TimedActionBuilder::Reversed(bool reversed) {
	Timing().reversed = reversed;
	return *this;
}

inline TimedActionBuilder& TimedActionBuilder::Yoyo(bool yoyo) {
	Timing().yoyo = yoyo;
	return *this;
}

inline ScriptSequence& TimedActionBuilder::End() {
	return *parent_;
}

void MoveToAction::OnStart(ActionContext& context) {
	start_ = context.owner.Get<ptgn::Transform>().position;
	end_ = relative ? start_ + destination : destination;
}

void MoveToAction::OnUpdate(ActionContext& context) {
	context.owner.Get<ptgn::Transform>().position =
		start_ + (end_ - start_) * context.progress;
}

void MoveToAction::OnRepeat(ActionContext& context) {
	if (!context.reversed) {
		OnStart(context);
	}
}

void RotateToAction::OnStart(ActionContext& context) {
	start_degrees_ = context.owner.Get<ptgn::Transform>().rotation.ToDeg().value;
	const float end_degrees{ relative ? start_degrees_ + degrees : degrees };
	delta_degrees_ = end_degrees - start_degrees_;
	if (shortest_path) {
		delta_degrees_ = std::remainder(delta_degrees_, 360.0f);
	}
}

void RotateToAction::OnUpdate(ActionContext& context) {
	const float value{ start_degrees_ + delta_degrees_ * context.progress };
	context.owner.Get<ptgn::Transform>().rotation = ptgn::Degrees{ value }.ToRad();
}

void RotateToAction::OnRepeat(ActionContext& context) {
	if (!context.reversed) {
		OnStart(context);
	}
}

void ScaleToAction::OnStart(ActionContext& context) {
	start_ = context.owner.Get<ptgn::Transform>().scale;
	end_ = relative ? start_ * scale : scale;
}

void ScaleToAction::OnUpdate(ActionContext& context) {
	context.owner.Get<ptgn::Transform>().scale =
		start_ + (end_ - start_) * context.progress;
}

void ScaleToAction::OnRepeat(ActionContext& context) {
	if (!context.reversed) {
		OnStart(context);
	}
}

ActionStatus FollowTargetAction::OnUpdate(ActionContext& context) {
	if (!target || !target.Has<ptgn::Transform>() || !context.owner.Has<ptgn::Transform>()) {
		return ActionStatus::Complete;
	}

	auto& position{ context.owner.Get<ptgn::Transform>().position };
	const ptgn::V2_float offset{ target.Get<ptgn::Transform>().position - position };
	const float distance{ std::sqrt(offset.x * offset.x + offset.y * offset.y) };
	if (distance <= std::max(0.0f, stopping_distance)) {
		return ActionStatus::Complete;
	}

	const float step{ std::max(0.0f, speed) * std::max(0.0f, context.delta_seconds) };
	if (step >= distance) {
		position += offset;
		return ActionStatus::Complete;
	}

	position += offset * (step / distance);
	return ActionStatus::Running;
}

void SetVisibleAction::OnStart(ActionContext& context) {
	context.owner.TryAdd<ptgn::Visible>().visible = visible;
}

void PlayAudioAction::OnStart(ActionContext& context) {
	context.host.Log(
		std::string{ context.host.Name(context.owner) } + " played " + asset +
		" (volume " + std::to_string(volume) + ", loops " + std::to_string(loops) + ")"
	);
}

void EmitSignalAction::OnStart(ActionContext& context) {
	if (context.scene) {
		context.scene->PushGlobal<Signal>(Signal{ signal }, context.owner);
		return;
	}
	context.host.Queue(EventRegistry::MakeEvent<Signal>(
		Signal{ signal }, std::nullopt, context.owner, EventDelivery::Broadcast
	));
}

void AddComponentsAction::OnStart(ActionContext& context) {
	for (const auto& component : components) {
		const auto* registration{ ptgn::ComponentRegistry::Find(component.type) };
		if (registration && registration->deserialize) {
			registration->deserialize(component.value, context.owner);
		}
	}
}

void RemoveComponentsAction::OnStart(ActionContext& context) {
	for (const auto& name : components) {
		if (const auto* registration{ ptgn::ComponentRegistry::Find(name) }) {
			registration->remove(context.owner);
		}
	}
}

void SpawnEntityAction::OnStart(ActionContext& context) {
	static std::mt19937 generator{ std::random_device{}() };
	std::uniform_real_distribution<float> unit{ 0.0f, 1.0f };
	const ptgn::V2_float base{
		origin == SpawnOrigin::OwnerEntity
			? context.owner.Get<ptgn::Transform>().position + center
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

		context.host.SpawnPrefab(PrefabSpawnRequest{
			.prefab_key = prefab_key,
			.owner = context.owner,
			.position = position,
			.parent_to_owner = parent_to_owner,
			.inherit_owner_rotation = inherit_owner_rotation,
			.inherit_owner_scale = inherit_owner_scale,
			.random_rotation = random_rotation,
		});
	}
}

/// @brief Minimal scene shell that owns the ECS manager and scene-scoped services.
///
/// Script mutations and events are double buffered: changes generated during a pass are applied or
/// dispatched only after the active iteration finishes. The second dispatch pass allows Actions to
/// emit Events that are still observed in the same frame without mutating an active queue.
class Scene {
public:
	Scene() : context_{
		.host = nullptr,
		.manager = manager_,
		.local_events = local_events_,
		.global_events = global_events_,
	} {}
	virtual ~Scene() = default;

	void BindRuntimeHost(RuntimeHost& host) { context_.host = &host; }

	[[nodiscard]] SceneContext& Context() { return context_; }
	[[nodiscard]] const SceneContext& Context() const { return context_; }
	[[nodiscard]] ptgn::Manager& Manager() { return manager_; }

	void Update(float delta_seconds) {
		ApplyPendingScriptChanges();
		OnBeforeScriptUpdate(delta_seconds);

		BeginEventDispatch();
		DispatchBufferedEvents();

		UpdateResidentScripts(delta_seconds);
		UpdateScriptSequences(delta_seconds);

		ApplyPendingScriptChanges();
		BeginEventDispatch();
		DispatchBufferedEvents();

		OnAfterScriptUpdate(delta_seconds);
	}

protected:
	virtual void OnBeforeScriptUpdate(float) {}
	virtual void UpdateResidentScripts(float) = 0;
	virtual void UpdateScriptSequences(float) = 0;
	virtual void ApplyPendingScriptChanges() = 0;
	virtual void DispatchBufferedEvents() = 0;
	virtual void OnAfterScriptUpdate(float) {}

	void BeginEventDispatch() {
		local_events_.BeginDispatch();
		global_events_.BeginDispatch();
	}

	ptgn::Manager manager_;
	LocalEventHandler local_events_;
	GlobalEventHandler global_events_;
	SceneContext context_;
};

/// @brief Runtime-only replacement for the old generic TweenTo(getter, setter) helper.
///
/// The generated NativeAction is deliberately non-serializable, while concrete editor actions such
/// as MoveToAction and ScaleToAction remain registry-backed and serializable.
template <typename T, typename TGetter, typename TSetter>
SequenceHandle PropertyTo(
	SceneContext& context,
	ptgn::Entity entity,
	SequenceChannelKey channel,
	T target,
	float duration_ms,
	TGetter getter,
	TSetter setter,
	ptgn::Ease ease = ptgn::Ease::Linear,
	bool force = true
) {
	if (!context.host || !entity) {
		return {};
	}

	struct State {
		T start{};
		T target{};
	};
	auto state{ std::make_shared<State>() };
	state->target = std::move(target);

	NativeAction action{
		.on_start = [state, getter = std::move(getter)](ActionContext& action_context) mutable {
			state->start = std::invoke(getter, action_context.owner);
		},
		.on_update = [state, setter = std::move(setter)](ActionContext& action_context) mutable {
			std::invoke(
				setter,
				action_context.owner,
				state->start + (state->target - state->start) * action_context.progress
			);
			return action_context.linear_progress >= 1.0f
				? ActionStatus::Complete
				: ActionStatus::Running;
		},
	};

	ScriptSequence sequence{ "Property To" };
	sequence
		.Channel(channel)
		.Transient()
		.During(duration_ms, std::move(action))
		.Ease(ease);
	return context.host->RunInChannel(
		entity, std::move(channel), std::move(sequence),
		force ? ReentryMode::Restart : ReentryMode::Queue
	);
}

inline SequenceHandle TranslateTo(
	SceneContext& context,
	ptgn::Entity entity,
	ptgn::V2_float destination,
	float duration_ms,
	ptgn::Ease ease = ptgn::Ease::Linear,
	bool force = true,
	bool relative = false
) {
	if (!context.host || !entity) {
		return {};
	}
	ScriptSequence sequence{ "Translate To" };
	sequence
		.Channel(SequenceChannelKey{ "transform.position" })
		.Transient()
		.During(duration_ms, MoveToAction{ destination, relative })
		.Ease(ease);
	return context.host->RunInChannel(
		entity, *sequence.channel, std::move(sequence),
		force ? ReentryMode::Restart : ReentryMode::Queue
	);
}

inline SequenceHandle RotateTo(
	SceneContext& context,
	ptgn::Entity entity,
	float degrees,
	float duration_ms,
	ptgn::Ease ease = ptgn::Ease::Linear,
	bool force = true,
	bool shortest_path = true,
	bool relative = false
) {
	if (!context.host || !entity) {
		return {};
	}
	ScriptSequence sequence{ "Rotate To" };
	sequence
		.Channel(SequenceChannelKey{ "transform.rotation" })
		.Transient()
		.During(duration_ms, RotateToAction{ degrees, shortest_path, relative })
		.Ease(ease);
	return context.host->RunInChannel(
		entity, *sequence.channel, std::move(sequence),
		force ? ReentryMode::Restart : ReentryMode::Queue
	);
}

inline SequenceHandle ScaleTo(
	SceneContext& context,
	ptgn::Entity entity,
	ptgn::V2_float scale,
	float duration_ms,
	ptgn::Ease ease = ptgn::Ease::Linear,
	bool force = true,
	bool relative = false
) {
	if (!context.host || !entity) {
		return {};
	}
	ScriptSequence sequence{ "Scale To" };
	sequence
		.Channel(SequenceChannelKey{ "transform.scale" })
		.Transient()
		.During(duration_ms, ScaleToAction{ scale, relative })
		.Ease(ease);
	return context.host->RunInChannel(
		entity, *sequence.channel, std::move(sequence),
		force ? ReentryMode::Restart : ReentryMode::Queue
	);
}

inline SequenceHandle Follow(
	SceneContext& context,
	ptgn::Entity entity,
	ptgn::Entity target,
	float speed,
	float stopping_distance = 2.0f,
	bool force = true
) {
	if (!context.host || !entity) {
		return {};
	}
	ScriptSequence sequence{ "Follow Target" };
	sequence
		.Channel(SequenceChannelKey{ "movement.follow" })
		.Transient()
		.UntilComplete(FollowTargetAction{
			.target = target,
			.speed = speed,
			.stopping_distance = stopping_distance,
		});
	return context.host->RunInChannel(
		entity, *sequence.channel, std::move(sequence),
		force ? ReentryMode::Restart : ReentryMode::Queue
	);
}

template <std::ranges::input_range TRange>
std::vector<SequenceHandle> TranslateTo(
	SceneContext& context,
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
			context, entity, destination, duration_ms, ease, force, relative
		));
	}
	return handles;
}

template <std::ranges::input_range TRange>
std::vector<SequenceHandle> RotateTo(
	SceneContext& context,
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
			context, entity, degrees, duration_ms, ease, force, shortest_path, relative
		));
	}
	return handles;
}

template <std::ranges::input_range TRange>
std::vector<SequenceHandle> ScaleTo(
	SceneContext& context,
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
			context, entity, scale, duration_ms, ease, force, relative
		));
	}
	return handles;
}

inline SequenceHandle After(
	SceneContext& context,
	ptgn::Entity owner,
	float delay_ms,
	std::function<void(ActionContext&)> callback
) {
	if (!context.host || !owner) {
		return {};
	}

	ScriptSequence sequence{ "After" };
	sequence
		.Transient()
		.Wait(delay_ms)
		.Then(NativeAction{ .on_start = std::move(callback) });
	return context.host->RunSequence(owner, std::move(sequence));
}

inline void StopChannel(
	RuntimeHost& host,
	ptgn::Entity entity,
	SequenceChannelKey channel,
	SequenceStopMode mode = SequenceStopMode::All
) {
	host.StopSequenceChannel(entity, std::move(channel), mode);
}

namespace editor {

struct EditorContext;

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
	std::string key;
	EventEditorOptions options;
	std::function<bool(TypedValue&)> draw_filter;
	std::function<bool(TypedValue&)> draw_payload;
};

class EventEditorRegistry {
public:
	template <typename TFilter, typename TEvent, typename FFilter, typename FPayload>
	static bool Register(
		std::string key,
		EventEditorOptions options,
		FFilter&& draw_filter,
		FPayload&& draw_payload
	) {
		auto& entries{ MutableEntries() };
		if (Find(key)) {
			return false;
		}
		entries.push_back(EventEditorRegistration{
			.key = std::move(key),
			.options = std::move(options),
			.draw_filter = [fn = std::forward<FFilter>(draw_filter)](TypedValue& value) mutable {
				return std::invoke(fn, value.Get<TFilter>());
			},
			.draw_payload = [fn = std::forward<FPayload>(draw_payload)](TypedValue& value) mutable {
				return std::invoke(fn, value.Get<TEvent>());
			},
		});
		return true;
	}

	[[nodiscard]] static const EventEditorRegistration* Find(std::string_view key) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [key](const auto& entry) {
			return entry.key == key;
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
	std::string key;
	ActionEditorOptions options;
	std::function<bool(TypedValue&, EditorContext&)> draw_inline;
	std::function<bool(TypedValue&, EditorContext&)> draw;
};

class ActionEditorRegistry {
public:
	template <typename T, typename F>
	static bool Register(std::string key, ActionEditorOptions options, F&& draw) {
		auto& entries{ MutableEntries() };
		if (Find(key)) {
			return false;
		}
		entries.push_back(ActionEditorRegistration{
			.key = std::move(key),
			.options = std::move(options),
			.draw_inline = {},
			.draw = [fn = std::forward<F>(draw)](
				TypedValue& value,
				EditorContext& context
			) mutable {
				return std::invoke(fn, value.Get<T>(), context);
			},
		});
		return true;
	}

	template <typename T, typename FInline, typename FDetails>
	static bool RegisterInline(
		std::string key,
		ActionEditorOptions options,
		FInline&& draw_inline,
		FDetails&& draw_details
	) {
		auto& entries{ MutableEntries() };
		if (Find(key)) {
			return false;
		}
		entries.push_back(ActionEditorRegistration{
			.key = std::move(key),
			.options = std::move(options),
			.draw_inline = [fn = std::forward<FInline>(draw_inline)](
				TypedValue& value,
				EditorContext& context
			) mutable {
				return std::invoke(fn, value.Get<T>(), context);
			},
			.draw = [fn = std::forward<FDetails>(draw_details)](
				TypedValue& value,
				EditorContext& context
			) mutable {
				return std::invoke(fn, value.Get<T>(), context);
			},
		});
		return true;
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
	std::string key;
	ScriptEditorOptions options;
	bool has_contents{ false };
	std::function<bool(TypedValue&)> draw;
};

class ScriptEditorRegistry {
public:
	template <typename T, typename F>
	static bool Register(std::string key, ScriptEditorOptions options, F&& draw) {
		auto& entries{ MutableEntries() };
		if (Find(key)) {
			return false;
		}
		entries.push_back(ScriptEditorRegistration{
			.key = std::move(key),
			.options = std::move(options),
			.has_contents = !std::is_empty_v<T>,
			.draw = [fn = std::forward<F>(draw)](TypedValue& value) mutable {
				return std::invoke(fn, value.Get<T>());
			},
		});
		return true;
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

class EditorHost {
public:
	virtual ~EditorHost() = default;

	[[nodiscard]] virtual PrefabRegistry& GetPrefabs() = 0;
	[[nodiscard]] virtual SharedScriptSequenceRegistry& GetSharedSequences() = 0;
	[[nodiscard]] virtual const std::vector<ptgn::Entity>& Entities() const = 0;
	[[nodiscard]] virtual std::vector<std::string> ActivityText() const = 0;
	[[nodiscard]] virtual std::string_view Name(ptgn::Entity entity) const = 0;
	[[nodiscard]] virtual EditorVisual Visual(ptgn::Entity entity) const = 0;
	[[nodiscard]] virtual std::string& EditableName(ptgn::Entity entity) = 0;
	[[nodiscard]] virtual std::string& EditableTag(ptgn::Entity entity) = 0;
	virtual ptgn::Entity CreateEntity(std::string name, std::string tag = {}) = 0;
	[[nodiscard]] virtual ScriptSequence* Resolve(ScriptSequence& binding) = 0;
	[[nodiscard]] virtual const ScriptSequence* Resolve(const ScriptSequence& binding) const = 0;
	virtual void Start(ptgn::Entity owner, ScriptSequence& binding, bool force = false) = 0;
	virtual void Stop(ptgn::Entity owner, ScriptSequence& binding, bool log = true) = 0;
	virtual void SetPaused(ptgn::Entity owner, ScriptSequence& binding, bool paused) = 0;
	[[nodiscard]] virtual float Progress(const ScriptSequence& binding) const = 0;
	virtual void SubmitPointerFrame(PointerFrame frame) = 0;
	virtual void RegisterEditorExtensions() = 0;
};

struct EditorContext {
	EditorHost& host;
	PrefabRegistry& prefabs;
};

class DemoEditor {
public:
	explicit DemoEditor(EditorHost& world) :
		world_{ world },
		context_{ .host = world, .prefabs = world.GetPrefabs() } {
		RegisterEditorTypes();
		world_.RegisterEditorExtensions();
	}

	void Draw() {
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
		ImGui::Checkbox("Runtime", &show_runtime_controls_);
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
			DrawSidebar();
			ImGui::EndChild();

			ImGui::TableNextColumn();
			ImGui::BeginChild("Scene");
			DrawScene();
			ImGui::EndChild();

			ImGui::TableNextColumn();
			ImGui::BeginChild("Inspector");
			DrawInspector();
			ImGui::EndChild();
			ImGui::EndTable();
		}
		ImGui::End();
	}

private:
	enum class ActionForm {
		Action,
		Tween,
		Delay
	};

	struct ActionDragPayload { int index; };
	struct DurationEditState {
		std::array<char, 32> buffer{};
		bool initialized{ false };
		bool was_active{ false };
	};

	static constexpr std::array kReentryLabels{ "Ignore", "Restart", "Queue" };
	static constexpr std::array kLifecycleLabels{
		"On Start", "On Complete", "On Reset", "On Stop", "On Pause",
		"On Resume", "On Action Start", "On Action Complete", "On Action Cancel",
		"On Repeat", "On Yoyo"
	};
	static constexpr std::array kActionFormLabels{
		"Action", "Tween", "Delay"
	};
	static constexpr std::array kEaseEntries{
		std::pair{ ptgn::Ease::Linear, "Linear" },
		std::pair{ ptgn::Ease::InQuad, "In Quad" },
		std::pair{ ptgn::Ease::OutQuad, "Out Quad" },
		std::pair{ ptgn::Ease::InOutQuad, "In-Out Quad" },
		std::pair{ ptgn::Ease::OutCubic, "Out Cubic" },
		std::pair{ ptgn::Ease::OutBack, "Out Back" },
	};

	static void RegisterEditorTypes();
	static void DrawItemTooltip(const char* text);
	static bool DrawDurationInput(
		const char* label, float& milliseconds, float width, const char* tooltip
	);
	static float CompactControlSpacing();
	static void SameLineControl();
	static float EnabledDeleteControlsWidth();
	static bool DrawEnabledDeleteControls(
		bool& enabled, const char* enabled_tooltip, const char* delete_tooltip
	);
	static bool DrawCenteredTextButton(
		const char* id, const char* text, ImVec2 size
	);
	static bool DrawToggleButton(
		const char* label, bool& value, ImVec2 size, const char* tooltip
	);
	static float GetHalfRowWidth();
	static float GetCountControlWidth(const char* label);
	static void DrawCountControl(
		const char* label, int& value, int minimum, int maximum = 100,
		bool disabled = false, const char* tooltip = nullptr
	);
	static bool DrawAddableSectionHeader(
		const char* id, const char* label, bool default_open, bool empty,
		const char* section_tooltip, const char* empty_tooltip,
		const char* add_tooltip, bool& add_requested
	);
	static bool DrawUnframedSectionHeader(
		const char* id, const char* label, bool default_open, bool empty,
		const char* tooltip, bool show_add_button,
		const char* add_tooltip, bool& add_requested
	);
	static void DrawSelectedItemsTooltip(std::span<const std::string> items);
	static ActionForm GetActionForm(const Action& action);
	static void SetActionForm(Action& action, ActionForm form);
	static std::string ActionSummary(const Action& action);
	static void MoveAction(std::vector<Action>& actions, int from, int to);
	static bool DrawAddComponentButton(AddComponentsAction& action, const char* popup_id);

	void DrawSidebar();
	void DrawScene();
	void DrawInspector();
	void DrawEntityComponents(ptgn::Entity entity);
	void DrawScripts(ptgn::Entity entity, ScriptsComponent& scripts);
	void DrawResidentScripts(ScriptsComponent& scripts);
	bool DrawSequence(ptgn::Entity owner, ScriptSequence& binding);
	void DrawRuntimeButtons(ptgn::Entity owner, ScriptSequence& binding);
	void DrawEvents(ScriptSequence& sequence);
	bool DrawEvent(EventCondition& event, bool stop_event, bool& switch_kind);
	void DrawLifecycleRows(ScriptSequence& sequence);
	void DrawActions(ScriptSequence& sequence, ScriptSequence& binding);
	bool DrawAction(
		Action& action, int index, ScriptSequence* binding, bool& duplicate,
		int& move_from, int& move_to, bool lifecycle = false
	);
	void DrawActionPicker(Action& action, bool timed_only, float width = -FLT_MIN);
	void DrawActionPickerWithInline(Action& action, bool timed_only);
	void DrawActionParameters(Action& action, float left_screen_x);
	void DrawTimingOptions(Action& action, ActionTiming& timing, float left_screen_x);
	void DrawEmitSignalCompact(EmitSignalAction& emit);
	void DrawComponentDefinition(ComponentDefinition& component, bool removable, int* remove_index = nullptr, int index = -1);
	void DrawPrefabs();
	void DrawPrefabInspector(PrefabDefinition& prefab);
	void DrawActivity();
	void PromoteToShared(ScriptSequence& binding);
	void DetachToLocal(ScriptSequence& binding);
	void DrawAddResidentScriptPopup(ScriptsComponent& scripts);
	void DrawAddSequencePopup(ScriptsComponent& scripts);

	[[nodiscard]] ptgn::Entity SelectedEntity() const {
		const auto& entities{ world_.Entities() };
		return selected_entity_ >= 0 && selected_entity_ < static_cast<int>(entities.size())
			? entities[static_cast<std::size_t>(selected_entity_)]
			: ptgn::Entity{};
	}

	EditorHost& world_;
	EditorContext context_;
	int selected_entity_{ 0 };
	int selected_prefab_{ -1 };
	bool inspect_prefab_{ false };
	bool show_runtime_controls_{ false };
	std::optional<Id> editing_sequence_name_;
	std::string editing_sequence_original_name_;
	std::unordered_map<Id, bool> sequence_open_states_;
};

void DemoEditor::DrawItemTooltip(const char* text) {
	if (text && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", text);
	}
}

bool DemoEditor::DrawDurationInput(
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

float DemoEditor::CompactControlSpacing() {
	return ImGui::GetStyle().ItemSpacing.x;
}

void DemoEditor::SameLineControl() {
	ImGui::SameLine(0.0f, CompactControlSpacing());
}

float DemoEditor::EnabledDeleteControlsWidth() {
	return ImGui::GetFrameHeight() * 2.0f + CompactControlSpacing();
}

bool DemoEditor::DrawEnabledDeleteControls(
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

bool DemoEditor::DrawCenteredTextButton(
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

bool DemoEditor::DrawToggleButton(
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

float DemoEditor::GetHalfRowWidth() {
	return std::max(
		1.0f,
		(ImGui::GetContentRegionAvail().x - CompactControlSpacing()) * 0.5f
	);
}

float DemoEditor::GetCountControlWidth(const char* label) {
	const float button_width{ ImGui::GetFrameHeight() };
	const float spacing{ CompactControlSpacing() };
	const std::string widest{ std::string{ label } + ": 100" };
	return ImGui::CalcTextSize(widest.c_str()).x +
		button_width * 2.0f + spacing * 2.0f;
}

void DemoEditor::DrawCountControl(
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

bool DemoEditor::DrawAddableSectionHeader(
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

bool DemoEditor::DrawUnframedSectionHeader(
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

void DemoEditor::DrawSelectedItemsTooltip(std::span<const std::string> items) {
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

DemoEditor::ActionForm DemoEditor::GetActionForm(const Action& action) {
	if (action.type == ActionRegistry::Key<WaitAction>()) {
		return ActionForm::Delay;
	}
	return action.timing ? ActionForm::Tween : ActionForm::Action;
}

void DemoEditor::SetActionForm(Action& action, ActionForm form) {
	const Id id{ action.id };
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

	action.id = id;
	action.enabled = enabled;
}

std::string DemoEditor::ActionSummary(const Action& action) {
	const auto* editor{ editor::ActionEditorRegistry::Find(action.type) };
	std::string result{ editor ? editor->options.label : action.type };
	if (action.timing) {
		result += " (" + std::to_string(static_cast<int>(action.timing->duration_ms)) + "ms)";
	}
	return result;
}

void DemoEditor::MoveAction(std::vector<Action>& actions, int from, int to) {
	if (from < 0 || to < 0 || from >= static_cast<int>(actions.size()) ||
		to >= static_cast<int>(actions.size()) || from == to) {
		return;
	}
	Action moved{ std::move(actions[static_cast<std::size_t>(from)]) };
	actions.erase(actions.begin() + from);
	actions.insert(actions.begin() + to, std::move(moved));
}

bool DemoEditor::DrawAddComponentButton(
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

void DemoEditor::RegisterEditorTypes() {
	if (!ptgn::editor::ComponentEditorRegistry::Find(
			ptgn::ComponentTypeId<ptgn::Transform>()
		)) {
		ptgn::editor::ComponentEditorRegistry::Register<ptgn::Transform>(
			ptgn::editor::MakeComponentEditorRegistration(
				ptgn::editor::ComponentEditorOptions{
					.label = "Transform",
					.group = "Core",
				}
			)
		);
	}

	auto draw_runtime_payload = [](auto&) {
		return false;
	};
	auto draw_enum_expression = []<typename TEnum>(
		const char* id,
		const char* hint,
		std::string& text,
		const char* noun,
		float width
	) {
		const auto parsed{ ParseEnumExpression<TEnum>(text) };
		const bool invalid{ !text.empty() && !parsed.IsValid() };
		if (invalid) {
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4{ 0.95f, 0.28f, 0.25f, 1.0f });
		}
		ImGui::SetNextItemWidth(width);
		const bool changed{ ImGui::InputTextWithHint(id, hint, &text) };
		const bool hovered{ ImGui::IsItemHovered() };
		if (invalid) {
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
		}
		if (hovered) {
			if (invalid) {
				std::string tooltip{ "Unrecognized " };
				tooltip += noun;
				tooltip += " name or scancode:";
				for (const auto& token : parsed.invalid_tokens) {
					tooltip += "\n- ";
					tooltip += token;
				}
				tooltip += "\nUse valid enum names or numeric scancodes.";
				ImGui::SetTooltip("%s", tooltip.c_str());
			} else {
				ImGui::SetTooltip(
					"Comma separates alternatives; + requires all %s entries.",
					noun
				);
			}
		}
		return changed;
	};
	auto draw_key_filter = [draw_enum_expression](KeyListFilter& filter) mutable {
		return draw_enum_expression.template operator()<ptgn::Key>(
			"##Keys", "W, UpArrow, 32", filter.keys, "key", -FLT_MIN
		);
	};
	auto draw_key_held_filter = [draw_enum_expression](KeyListFilter& filter) mutable {
		const float duration_width{ 78.0f };
		const float label_width{ ImGui::CalcTextSize("Hold:").x };
		const float spacing{ ImGui::GetStyle().ItemSpacing.x };
		const float key_width{ std::max(
			60.0f,
			ImGui::GetContentRegionAvail().x - duration_width - label_width - spacing * 2.0f
		) };
		bool changed{ draw_enum_expression.template operator()<ptgn::Key>(
			"##Keys", "W, UpArrow, 32", filter.keys, "key", key_width
		) };
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Hold:");
		ImGui::SameLine();
		changed |= DrawDurationInput(
			"##HoldDuration", filter.hold_duration_ms, duration_width,
			"Minimum time the key combination must remain held."
		);
		return changed;
	};
	auto draw_mouse_filter = [draw_enum_expression](MouseListFilter& filter) mutable {
		return draw_enum_expression.template operator()<ptgn::Mouse>(
			"##MouseButtons", "Left, Middle, 0", filter.buttons, "mouse button", -FLT_MIN
		);
	};
	auto draw_mouse_held_filter = [draw_enum_expression](MouseListFilter& filter) mutable {
		const float duration_width{ 78.0f };
		const float label_width{ ImGui::CalcTextSize("Hold:").x };
		const float spacing{ ImGui::GetStyle().ItemSpacing.x };
		const float button_width{ std::max(
			60.0f,
			ImGui::GetContentRegionAvail().x - duration_width - label_width - spacing * 2.0f
		) };
		bool changed{ draw_enum_expression.template operator()<ptgn::Mouse>(
			"##MouseButtons", "Left, Middle, 0", filter.buttons, "mouse button", button_width
		) };
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Hold:");
		ImGui::SameLine();
		changed |= DrawDurationInput(
			"##HoldDuration", filter.hold_duration_ms, duration_width,
			"Minimum time the mouse-button combination must remain held."
		);
		return changed;
	};
	auto draw_key_payload = [](auto& event) {
		std::string value{ std::string{ magic_enum::enum_name(event.key) } };
		if (value.empty()) {
			value = std::to_string(static_cast<int>(event.key));
		}
		if (!ImGui::InputText("Key", &value)) {
			return false;
		}
		if (const auto parsed{ ParseEnumToken<ptgn::Key>(value) }) {
			event.key = *parsed;
			return true;
		}
		return false;
	};
	auto draw_mouse_payload = [](auto& event) {
		const ptgn::Mouse mouse{ MouseFromEvent(event) };
		std::string value{ std::string{ magic_enum::enum_name(mouse) } };
		if (value.empty()) {
			value = std::to_string(static_cast<int>(mouse));
		}
		if (!ImGui::InputText("Button", &value)) {
			return false;
		}
		if (const auto parsed{ ParseEnumToken<ptgn::Mouse>(value) }) {
			SetMouseEvent(event, *parsed);
			return true;
		}
		return false;
	};
	auto draw_entity_filter = [](
		EntityMaskFilter& filter, const char* tags_hint, const char* masks_hint
	) {
		const float spacing{ ImGui::GetStyle().ItemSpacing.x };
		const float width{ std::max(1.0f, (ImGui::GetContentRegionAvail().x - spacing) * 0.5f) };
		bool changed{ false };
		ImGui::SetNextItemWidth(width);
		changed |= ImGui::InputTextWithHint("##Tags", tags_hint, &filter.tags);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Comma-separated tags; prefix excluded tags with -.");
		}
		ImGui::SameLine();
		const InclusionFilter parsed{ ParseEntityMaskFilter(filter) };
		const bool invalid{ !parsed.invalid_masks.empty() };
		if (invalid) {
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4{ 0.95f, 0.28f, 0.25f, 1.0f });
		}
		ImGui::SetNextItemWidth(width);
		changed |= ImGui::InputTextWithHint("##Masks", masks_hint, &filter.masks);
		const bool hovered{ ImGui::IsItemHovered() };
		if (invalid) {
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
		}
		if (hovered) {
			if (invalid) {
				std::string tooltip{ "Unrecognized integer mask:" };
				for (const auto& token : parsed.invalid_masks) {
					tooltip += "\n- ";
					tooltip += token;
				}
				ImGui::SetTooltip("%s", tooltip.c_str());
			} else {
				ImGui::SetTooltip("Comma-separated masks; prefix excluded masks with -.");
			}
		}
		return changed;
	};
	auto draw_overlap_filter = [draw_entity_filter](EntityMaskFilter& filter) {
		return draw_entity_filter(
			filter, "Tags: Player, -Enemy", "Masks: 1, -4"
		);
	};
	auto draw_collision_filter = [draw_entity_filter](EntityMaskFilter& filter) {
		return draw_entity_filter(filter, "Tags: Player, -Enemy", "Masks: 1, -4");
	};
	auto draw_on_create_filter = [](OnCreateFilter& filter) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Delay:");
		ImGui::SameLine();
		return DrawDurationInput(
			"##CreateDelay", filter.delay_ms, -FLT_MIN,
			"Delay after script creation before starting the sequence."
		);
	};
	auto draw_signal = [](Signal& signal) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::InputTextWithHint("##Signal", "Signal name", &signal.key.value);
	};

	editor::EventEditorRegistry::Register<OnCreateFilter, OnCreate>(
		"ptgn.event.OnCreate",
		{ .label = "On Create", .group = "", .description = "Fired after the owning entity is created." },
		draw_on_create_filter, draw_runtime_payload
	);
	editor::EventEditorRegistry::Register<Signal, Signal>(
		"ptgn.event.Signal",
		{ .label = "On Signal", .group = "", .description = "Data-driven broadcast event identified by a strong string key." },
		draw_signal, draw_signal
	);
	editor::EventEditorRegistry::Register<KeyListFilter, ptgn::event::KeyPressed>(
		"ptgn.event.KeyPressed",
		{ .label = "On Key Pressed", .group = "Key", .description = "Fired on the first pressed frame." },
		draw_key_filter, draw_key_payload
	);
	editor::EventEditorRegistry::Register<KeyListFilter, ptgn::event::KeyHeld>(
		"ptgn.event.KeyHeld",
		{ .label = "On Key Held", .group = "Key", .description = "Fired after a key combination remains held." },
		draw_key_held_filter, draw_key_payload
	);
	editor::EventEditorRegistry::Register<KeyListFilter, ptgn::event::KeyReleased>(
		"ptgn.event.KeyReleased",
		{ .label = "On Key Released", .group = "Key", .description = "Fired when a key is released." },
		draw_key_filter, draw_key_payload
	);
	editor::EventEditorRegistry::Register<MouseListFilter, ptgn::event::MousePressed>(
		"ptgn.event.MousePressed",
		{ .label = "On Mouse Pressed", .group = "Mouse", .description = "Fired on the first pressed frame." },
		draw_mouse_filter, draw_mouse_payload
	);
	editor::EventEditorRegistry::Register<MouseListFilter, ptgn::event::MouseHeld>(
		"ptgn.event.MouseHeld",
		{ .label = "On Mouse Held", .group = "Mouse", .description = "Fired after a mouse-button combination remains held." },
		draw_mouse_held_filter, draw_mouse_payload
	);
	editor::EventEditorRegistry::Register<MouseListFilter, ptgn::event::MouseReleased>(
		"ptgn.event.MouseReleased",
		{ .label = "On Mouse Released", .group = "Mouse", .description = "Fired when a mouse button is released." },
		draw_mouse_filter, draw_mouse_payload
	);
	editor::EventEditorRegistry::Register<EntityMaskFilter, ptgn::event::OverlapStart>(
		"ptgn.event.OverlapStart",
		{ .label = "On Overlap Start", .group = "Overlap", .description = "Fired when an overlap begins." },
		draw_overlap_filter, draw_runtime_payload
	);
	editor::EventEditorRegistry::Register<EntityMaskFilter, ptgn::event::Overlap>(
		"ptgn.event.Overlap",
		{ .label = "On Overlap", .group = "Overlap", .description = "Fired while an overlap continues." },
		draw_overlap_filter, draw_runtime_payload
	);
	editor::EventEditorRegistry::Register<EntityMaskFilter, ptgn::event::OverlapStop>(
		"ptgn.event.OverlapStop",
		{ .label = "On Overlap Stop", .group = "Overlap", .description = "Fired when an overlap ends." },
		draw_overlap_filter, draw_runtime_payload
	);
	editor::EventEditorRegistry::Register<EntityMaskFilter, ptgn::event::Collision>(
		"ptgn.event.Collision",
		{ .label = "On Collision", .group = "Collision", .description = "Fired for a collision." },
		draw_collision_filter, draw_runtime_payload
	);

	editor::ActionEditorRegistry::Register<WaitAction>(
		"engine.wait",
		{ .label = "Delay", .group = "Timing", .description = "Delay before continuing the sequence." },
		[](WaitAction&, EditorContext&) { return false; }
	);
	editor::ActionEditorRegistry::RegisterInline<MoveToAction>(
		"engine.move_to",
		{ .label = "Move To", .group = "Transform", .description = "Move the owning entity." },
		[](MoveToAction& action, EditorContext&) {
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
			DrawItemTooltip(
				action.relative
					? "Offset from the entity's current position."
					: "Use an absolute world position."
			);
			return changed;
		},
		[](MoveToAction&, EditorContext&) { return false; }
	);
	editor::ActionEditorRegistry::RegisterInline<RotateToAction>(
		"engine.rotate_to",
		{ .label = "Rotate To", .group = "Transform", .description = "Rotate the owning entity to an angle." },
		[](RotateToAction& action, EditorContext&) {
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
		[](RotateToAction&, EditorContext&) { return false; }
	);
	editor::ActionEditorRegistry::RegisterInline<SetVisibleAction>(
		"engine.set_visible",
		{ .label = "Set Visible", .group = "Entity", .description = "Set the owning entity visibility.", .menu_order = 3 },
		[](SetVisibleAction& action, EditorContext&) {
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
		[](SetVisibleAction&, EditorContext&) { return false; }
	);
	editor::ActionEditorRegistry::Register<PlayAudioAction>(
		"engine.play_audio",
		{ .label = "Play Audio", .group = "Audio", .description = "Play an audio asset." },
		[](PlayAudioAction& action, EditorContext&) {
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
	);
	editor::ActionEditorRegistry::RegisterInline<EmitSignalAction>(
		"engine.emit_signal",
		{ .label = "Emit Signal", .group = "", .description = "Broadcast a Signal identified by a strong string key." },
		[](EmitSignalAction& action, EditorContext&) {
			ImGui::SetNextItemWidth(-FLT_MIN);
			const bool changed{ ImGui::InputTextWithHint(
				"##SignalName", "Signal name", &action.signal.value
			) };
			DrawItemTooltip("Signal name to broadcast.");
			return changed;
		},
		[](EmitSignalAction&, EditorContext&) { return false; }
	);
	editor::ActionEditorRegistry::RegisterInline<AddComponentsAction>(
		"engine.add_components",
		{ .label = "Add Components", .group = "Entity", .description = "Add registered components to the owner.", .menu_order = 1 },
		[](AddComponentsAction& action, EditorContext&) {
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
		[](AddComponentsAction& action, EditorContext&) {
			bool changed{ false };
			int remove{ -1 };
			for (int i{ 0 }; i < static_cast<int>(action.components.size()); ++i) {
				auto& definition{ action.components[static_cast<std::size_t>(i)] };
				const auto* component{ ptgn::ComponentRegistry::Find(definition.type) };
				ImGui::PushID(static_cast<int>(definition.id));

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
					const std::string label{
						component ? ResolveComponentEditor(*component).label
								  : definition.type
					};
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
					changed |= ptgn::editor::ComponentEditorRegistry::DrawJson(
						*component, definition.value
					);
				}
				ImGui::PopID();
			}

			if (remove >= 0) {
				action.components.erase(action.components.begin() + remove);
				changed = true;
			}
			return changed;
		}
	);
	editor::ActionEditorRegistry::RegisterInline<RemoveComponentsAction>(
		"engine.remove_components",
		{ .label = "Remove Components", .group = "Entity", .description = "Remove selected registered components from the owner.", .menu_order = 2, .separator_after = true },
		[](RemoveComponentsAction& action, EditorContext&) {
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
		[](RemoveComponentsAction&, EditorContext&) { return false; }
	);
	editor::ActionEditorRegistry::Register<SpawnEntityAction>(
		"engine.spawn_entity",
		{ .label = "Spawn Entity", .group = "Entity", .description = "Spawn one or more prefab instances.", .menu_order = 0 },
		[](SpawnEntityAction& action, EditorContext& context) {
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
	);



}

void DemoEditor::DrawSidebar() {
	if (!ImGui::BeginTabBar("SidebarTabs")) {
		return;
	}
	if (ImGui::BeginTabItem("Scene")) {
		ImGui::TextDisabled("Scene Hierarchy");
		ImGui::Separator();
		for (int i{ 0 }; i < static_cast<int>(world_.Entities().size()); ++i) {
			const auto entity{ world_.Entities()[static_cast<std::size_t>(i)] };
			if (ImGui::Selectable(
					std::string{ world_.Name(entity) }.c_str(),
					!inspect_prefab_ && selected_entity_ == i,
					0, ImVec2{ 0.0f, 25.0f }
				)) {
				selected_entity_ = i;
				inspect_prefab_ = false;
			}
		}
		if (ImGui::Button("+ Entity", ImVec2{ -FLT_MIN, 0.0f })) {
			world_.CreateEntity("New Entity");
			selected_entity_ = static_cast<int>(world_.Entities().size()) - 1;
			inspect_prefab_ = false;
		}
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Prefabs")) {
		DrawPrefabs();
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
}

void DemoEditor::DrawScene() {
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

	for (int i{ 0 }; i < static_cast<int>(world_.Entities().size()); ++i) {
		const auto entity{ world_.Entities()[static_cast<std::size_t>(i)] };
		if (!entity.Has<ptgn::Transform>() ||
			(entity.Has<ptgn::Visible>() && !entity.Get<ptgn::Visible>().visible)) {
			continue;
		}
		const auto& transform{ entity.Get<ptgn::Transform>() };
		const auto visual{ world_.Visual(entity) };
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
		if (!inspect_prefab_ && i == selected_entity_) {
			draw->AddRect(
				ImVec2{ minimum.x - 3.0f, minimum.y - 3.0f },
				ImVec2{ maximum.x + 3.0f, maximum.y + 3.0f },
				ImGui::GetColorU32(ImGuiCol_ButtonHovered), 5.0f, 0, 2.0f
			);
		}
		const std::string name{ world_.Name(entity) };
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
		"Buttons receive local pointer Events in ButtonScript; one emits a global Signal."
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
	world_.SubmitPointerFrame(pointer_frame);
}

void DemoEditor::DrawInspector() {
	if (inspect_prefab_) {
		if (selected_prefab_ >= 0 &&
			selected_prefab_ < static_cast<int>(context_.prefabs.definitions.size())) {
			DrawPrefabInspector(context_.prefabs.definitions[static_cast<std::size_t>(selected_prefab_)]);
		}
		return;
	}

	ptgn::Entity entity{ SelectedEntity() };
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
		ImGui::InputTextWithHint("##Name", "Name", &world_.EditableName(entity));
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##Tag", "Tag", &world_.EditableTag(entity));
		ImGui::EndTable();
	}

	DrawEntityComponents(entity);
	if (auto* scripts{ entity.TryGet<ScriptsComponent>() }) {
		DrawScripts(entity, *scripts);
	} else if (ImGui::Button("+ Scripts Component", ImVec2{ -FLT_MIN, 0.0f })) {
		entity.Add<ScriptsComponent>();
	}
}

void DemoEditor::DrawEntityComponents(ptgn::Entity entity) {
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

void DemoEditor::DrawScripts(ptgn::Entity entity, ScriptsComponent& scripts) {
	ImGui::PushID("ScriptsComponent");
	const bool open{ ImGui::TreeNodeEx(
		"##Scripts", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
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

	const float half_width{ GetHalfRowWidth() };
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.20f, 0.34f, 0.33f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.26f, 0.43f, 0.41f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.31f, 0.49f, 0.47f, 1.0f });
	if (ImGui::Button("+ Script", ImVec2{ half_width, 0.0f })) {
		ImGui::OpenPopup("AddResidentScript");
	}
	ImGui::PopStyleColor(3);
	DrawItemTooltip("Add a script.");
	SameLineControl();
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.35f, 0.24f, 0.39f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.44f, 0.31f, 0.48f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.50f, 0.36f, 0.55f, 1.0f });
	if (ImGui::Button("+ Script Sequence", ImVec2{ half_width, 0.0f })) {
		ImGui::OpenPopup("AddScriptSequencePopup");
	}
	ImGui::PopStyleColor(3);
	DrawItemTooltip("Add a script sequence.");
	DrawAddResidentScriptPopup(scripts);
	DrawAddSequencePopup(scripts);

	DrawResidentScripts(scripts);
	int remove_sequence{ -1 };
	for (int i{ 0 }; i < static_cast<int>(scripts.sequences.size()); ++i) {
		if (DrawSequence(entity, scripts.sequences[static_cast<std::size_t>(i)])) {
			remove_sequence = i;
		}
	}
	if (remove_sequence >= 0) {
		scripts.sequences.erase(scripts.sequences.begin() + remove_sequence);
	}

	const ImVec2 activity_position{ ImGui::GetCursorScreenPos() };
	ImGui::SetCursorScreenPos(ImVec2{ activity_position.x, activity_position.y + 4.0f });
	DrawActivity();
	ImGui::TreePop();
	ImGui::PopID();
}

void DemoEditor::DrawResidentScripts(ScriptsComponent& scripts) {
	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(scripts.scripts.size()); ++i) {
		auto& script{ scripts.scripts[static_cast<std::size_t>(i)] };
		const auto* editor{ ScriptEditorRegistry::Find(script.type) };
		ImGui::PushID(static_cast<int>(script.id));
		bool open{ false };
		const float button_size{ ImGui::GetFrameHeight() };
		if (ImGui::BeginTable(
				"ResidentScriptRow", 2, ImGuiTableFlags_SizingStretchProp
			)) {
			ImGui::TableSetupColumn("Script", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(
				"Controls", ImGuiTableColumnFlags_WidthFixed,
				EnabledDeleteControlsWidth()
			);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);
			ImGui::TableSetColumnIndex(0);
			const ImVec4 header{ script.enabled ? ImVec4{ 0.20f, 0.34f, 0.33f, 1.0f }
												: ImVec4{ 0.25f, 0.25f, 0.25f, 1.0f } };
			const ImVec4 header_hovered{ script.enabled ? ImVec4{ 0.26f, 0.43f, 0.41f, 1.0f }
														: ImVec4{ 0.30f, 0.30f, 0.30f, 1.0f } };
			const ImVec4 header_active{ script.enabled ? ImVec4{ 0.31f, 0.49f, 0.47f, 1.0f }
													   : ImVec4{ 0.34f, 0.34f, 0.34f, 1.0f } };
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
				"##ResidentScript", flags, "%s",
				editor ? editor->options.label.c_str() : script.type.c_str()
			);
			ImGui::PopStyleColor(3);
			if (editor) {
				DrawItemTooltip(editor->options.description.c_str());
			}

			ImGui::TableSetColumnIndex(1);
			if (DrawEnabledDeleteControls(
					script.enabled, "Enable or disable this script.", "Remove this script."
				)) {
				remove = i;
			}
			ImGui::EndTable();
		}
		if (open && editor && editor->has_contents && editor->draw(script.value)) {
			script.instance.reset();
			script.created = false;
		}
		ImGui::PopID();
	}
	if (remove >= 0) {
		scripts.scripts.erase(scripts.scripts.begin() + remove);
	}
}

void DemoEditor::PromoteToShared(ScriptSequence& binding) {
	if (binding.shared_reference) {
		return;
	}
	ScriptSequence shared{ binding };
	shared.shared_reference = false;
	shared.shared_sequence_id = 0;
	shared.runtime = ScriptSequenceRuntime{};
	const Id shared_id{ shared.id };
	world_.GetSharedSequences().sequences.push_back(std::move(shared));
	binding.shared_reference = true;
	binding.shared_sequence_id = shared_id;
	binding.runtime = ScriptSequenceRuntime{};
}

void DemoEditor::DetachToLocal(ScriptSequence& binding) {
	if (!binding.shared_reference) {
		return;
	}
	const auto* shared{ world_.GetSharedSequences().Find(binding.shared_sequence_id) };
	if (!shared) {
		binding.shared_reference = false;
		binding.shared_sequence_id = 0;
		return;
	}
	ScriptSequence local{ *shared };
	const Id binding_id{ binding.id };
	const bool enabled{ binding.enabled };
	binding = std::move(local);
	binding.id = binding_id;
	binding.enabled = enabled;
	binding.shared_reference = false;
	binding.shared_sequence_id = 0;
	binding.runtime = ScriptSequenceRuntime{};
}

void DemoEditor::DrawRuntimeButtons(ptgn::Entity owner, ScriptSequence& binding) {
	if (!ImGui::BeginTable("RuntimeButtons", 3, ImGuiTableFlags_SizingStretchSame)) {
		return;
	}
	ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
	ImGui::TableSetColumnIndex(0);
	if (ImGui::Button(binding.runtime.running ? "Restart" : "Start", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
		world_.Start(owner, binding, true);
	}
	ImGui::TableSetColumnIndex(1);
	ImGui::BeginDisabled(!binding.runtime.running);
	if (ImGui::Button(binding.runtime.paused ? "Resume" : "Pause", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
		world_.SetPaused(owner, binding, !binding.runtime.paused);
	}
	ImGui::EndDisabled();
	ImGui::TableSetColumnIndex(2);
	if (ImGui::Button("Stop", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
		world_.Stop(owner, binding);
	}
	ImGui::EndTable();
}

bool DemoEditor::DrawSequence(ptgn::Entity owner, ScriptSequence& binding) {
	ScriptSequence* sequence{ world_.Resolve(binding) };
	if (!sequence) {
		ImGui::TextDisabled("Missing shared Script Sequence");
		return false;
	}

	bool remove{ false };
	ImGui::PushID(static_cast<int>(binding.id));
	const float button_size{ ImGui::GetFrameHeight() };
	const int runtime_columns{ show_runtime_controls_ ? 3 : 0 };
	const int column_count{ 3 + runtime_columns };
	const float available_width{ ImGui::GetContentRegionAvail().x };
	const float sequence_width{ std::max(
		1.0f, (available_width - CompactControlSpacing()) * 0.5f
	) };
	bool open{ sequence_open_states_.try_emplace(binding.id, true).first->second };
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
		if (show_runtime_controls_) {
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
		auto& stored_open{ sequence_open_states_[binding.id] };
		ImGui::SetNextItemOpen(stored_open, ImGuiCond_Always);
		const bool editing_before_draw{
			editing_sequence_name_ == binding.id
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
			editing_sequence_name_ = binding.id;
			editing_sequence_original_name_ = sequence->name;
			began_name_edit_this_frame = true;
		}

		if (!editing_before_draw && !begin_edit &&
			tree_clicked_left && mouse.x > name_hit_end_x) {
			open = !open;
			stored_open = open;
		}

		if (editing_sequence_name_ == binding.id) {
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
				sequence->name = editing_sequence_original_name_;
				editing_sequence_name_.reset();
			} else if (submitted) {
				editing_sequence_name_.reset();
			}
		}
		if (tree_hovered && editing_sequence_name_ != binding.id) {
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
					DetachToLocal(binding);
				} else {
					PromoteToShared(binding);
				}
				sequence = world_.Resolve(binding);
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
		if (show_runtime_controls_) {
			ImGui::TableSetColumnIndex(column++);
			if (DrawCenteredTextButton(
					"##Play", ">", ImVec2{ button_size, button_size }
				)) {
				world_.Start(owner, binding, true);
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
				world_.SetPaused(
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
				world_.Stop(owner, binding);
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

	if (editing_sequence_name_ == binding.id && name_input_drawn &&
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
			editing_sequence_name_.reset();
		}
	}

	if (open && !remove) {
		sequence = world_.Resolve(binding);
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
			DrawEvents(*sequence);

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
				DrawActions(*sequence, binding);
			}
		}
	}

	ImGui::PopID();
	return remove;
}

void DemoEditor::DrawEvents(ScriptSequence& sequence) {
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
			auto add_candidate = [&](const EventEditorRegistration& candidate) {
				if (ImGui::MenuItem(candidate.options.label.c_str())) {
					if (const auto* registration{ EventRegistry::Find(candidate.key) }) {
						sequence.start_events.push_back(EventCondition{
							.id = IdGenerator::Next(), .enabled = true,
							.type = registration->key,
							.filter = registration->make_default_filter(),
						});
					}
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
						.id = IdGenerator::Next(), .enabled = true,
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
				sequence.start_events[static_cast<std::size_t>(i)], false, switch_kind
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
				sequence.stop_events[static_cast<std::size_t>(i)], true, switch_kind
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

	DrawLifecycleRows(sequence);
}

bool DemoEditor::DrawEvent(
	EventCondition& event, bool stop_event, bool& switch_kind
) {
	bool remove{ false };
	ImGui::PushID(static_cast<int>(event.id));

	const float kind_width{
		std::max(
			ImGui::CalcTextSize("Start").x,
			ImGui::CalcTextSize("Stop").x
		) + ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const float consume_width{
		ImGui::CalcTextSize("Consume").x +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const bool has_filter{ !event.filter.Is<NoEventFilter>() };
	const int column_count{ has_filter ? 5 : 4 };
	const auto* selected{ editor::EventEditorRegistry::Find(event.type) };

	if (ImGui::BeginTable(
			"EventRow", column_count, ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn(
			"Kind", ImGuiTableColumnFlags_WidthFixed, kind_width
		);
		ImGui::TableSetupColumn(
			"Event", ImGuiTableColumnFlags_WidthFixed, 145.0f
		);
		if (has_filter) {
			ImGui::TableSetupColumn(
				"Filter", ImGuiTableColumnFlags_WidthStretch
			);
			ImGui::TableSetupColumn(
				"Consume", ImGuiTableColumnFlags_WidthFixed, consume_width
			);
		} else {
			ImGui::TableSetupColumn(
				"Consume", ImGuiTableColumnFlags_WidthStretch
			);
		}
		ImGui::TableSetupColumn(
			"Controls", ImGuiTableColumnFlags_WidthFixed,
			EnabledDeleteControlsWidth()
		);
		ImGui::TableNextRow(
			ImGuiTableRowFlags_None, ImGui::GetFrameHeight()
		);

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
			stop_event
				? "Change this to a start trigger."
				: "Change this to a stop trigger."
		);
		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(column++);
		ImGui::BeginDisabled(!event.enabled);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo(
				"##Event",
				selected ? selected->options.label.c_str() : "Missing Event"
			)) {
			auto select_candidate = [&](const EventEditorRegistration& candidate) {
				if (ImGui::MenuItem(
						candidate.options.label.c_str(), nullptr,
						candidate.key == event.type
					)) {
					event.type = candidate.key;
					if (const auto* registration{
							EventRegistry::Find(candidate.key)
						}) {
						event.filter = registration->make_default_filter();
					}
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
		ImGui::EndDisabled();

		if (has_filter) {
			ImGui::TableSetColumnIndex(column++);
			ImGui::BeginDisabled(!event.enabled);
			selected = editor::EventEditorRegistry::Find(event.type);
			if (selected) {
				selected->draw_filter(event.filter);
			} else {
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("Missing Trigger editor");
			}
			ImGui::EndDisabled();
		}

		ImGui::TableSetColumnIndex(column++);
		ImGui::BeginDisabled(!event.enabled);
		DrawToggleButton(
			"Consume", event.consume,
			ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() },
			"Stop propagation after this trigger matches. Targeted events stop on this entity; "
			"broadcast events stop before later entities."
		);
		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(column);
		remove = DrawEnabledDeleteControls(
			event.enabled,
			"Enable or disable this trigger.",
			"Remove this trigger."
		);
		ImGui::EndTable();
	}

	ImGui::PopID();
	return remove;
}

void DemoEditor::DrawActionPicker(Action& action, bool timed_only, float width) {
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
		return registration && candidate.key != ActionRegistry::Key<WaitAction>() &&
			(!timed_only || registration->supports_timing) &&
			(timed_only || !registration->requires_timing);
	};
	auto select_candidate = [&](const ActionEditorRegistration& candidate) {
		const auto* registration{ ActionRegistry::Find(candidate.key) };
		if (!registration) {
			return;
		}
		if (ImGui::MenuItem(candidate.options.label.c_str(), nullptr, candidate.key == action.type)) {
			const Id id{ action.id };
			const bool enabled{ action.enabled };
			action = ActionRegistry::Make(std::string_view{ candidate.key });
			action.id = id;
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

void DemoEditor::DrawActionPickerWithInline(Action& action, bool timed_only) {
	const auto* editor{ editor::ActionEditorRegistry::Find(action.type) };
	const bool has_inline_editor{
		!timed_only && editor && static_cast<bool>(editor->draw_inline)
	};
	if (!has_inline_editor) {
		DrawActionPicker(action, timed_only);
		return;
	}

	const float available{ ImGui::GetContentRegionAvail().x };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float picker_width{
		std::min(150.0f, std::max(110.0f, available * 0.32f))
	};
	DrawActionPicker(action, timed_only, picker_width);

	editor = editor::ActionEditorRegistry::Find(action.type);
	if (editor && editor->draw_inline) {
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(-FLT_MIN);
		editor->draw_inline(action.value, context_);
	}
}

void DemoEditor::DrawEmitSignalCompact(EmitSignalAction& emit) {
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##EmitSignal", "Signal name", &emit.signal.value);
	DrawItemTooltip("Broadcast Signal name.");
}

void DemoEditor::DrawTimingOptions(
	Action& action, ActionTiming& timing, float left_screen_x
) {
	const float right_screen_x{
		ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x
	};
	const float width{ std::max(1.0f, right_screen_x - left_screen_x) };
	ImGui::SetCursorScreenPos(
		ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y }
	);

	auto* move{ action.value.TryGet<MoveToAction>() };
	auto* rotate{ action.value.TryGet<RotateToAction>() };
	auto* scale{ action.value.TryGet<ScaleToAction>() };
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
}

void DemoEditor::DrawActionParameters(Action& action, float left_screen_x) {
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

	if (action.type == ActionRegistry::Key<AddComponentsAction>()) {
		const auto* add_components{ action.value.TryGet<AddComponentsAction>() };
		if (!add_components || add_components->components.empty()) {
			return;
		}
	}

	const float right_screen_x{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
	ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });
	if (ImGui::BeginChild(
			"ActionParameters", ImVec2{ std::max(1.0f, right_screen_x - left_screen_x), 0.0f },
			ImGuiChildFlags_AutoResizeY
		)) {
		action_editor->draw(action.value, context_);
	}
	ImGui::EndChild();
}

bool DemoEditor::DrawAction(
	Action& action, int index, ScriptSequence* binding, bool& duplicate,
	int& move_from, int& move_to, bool lifecycle
) {
	bool remove{ false };
	ImGui::PushID(static_cast<int>(action.id));
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
			DrawActionPicker(action, true);
		} else {
			ImGui::TableSetColumnIndex(column++);
			switch (displayed_form) {
				case ActionForm::Action:
					DrawActionPickerWithInline(action, false);
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
		DrawTimingOptions(action, *action.timing, parameter_left_screen_x);
	}
	if (displayed_form == ActionForm::Action ||
		displayed_form == ActionForm::Tween) {
		DrawActionParameters(action, parameter_left_screen_x);
	}
	if (binding && binding->runtime.running &&
		binding->runtime.action_index == static_cast<std::size_t>(index)) {
		ImGui::ProgressBar(
			world_.Progress(*binding), ImVec2{ -FLT_MIN, 2.0f }, ""
		);
	}
	ImGui::PopID();
	return remove;
}

void DemoEditor::DrawActions(ScriptSequence& sequence, ScriptSequence& binding) {
	int remove_index{ -1 };
	int duplicate_index{ -1 };
	int move_from{ -1 };
	int move_to{ -1 };
	for (int i{ 0 }; i < static_cast<int>(sequence.actions.size()); ++i) {
		bool duplicate{ false };
		if (DrawAction(
				sequence.actions[static_cast<std::size_t>(i)], i, &binding, duplicate,
				move_from, move_to
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
		copy.id = IdGenerator::Next();
		sequence.actions.insert(sequence.actions.begin() + duplicate_index + 1, std::move(copy));
	}
	if (remove_index >= 0) {
		sequence.actions.erase(sequence.actions.begin() + remove_index);
		binding.runtime = ScriptSequenceRuntime{};
	}
}

void DemoEditor::DrawLifecycleRows(ScriptSequence& sequence) {
	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(sequence.lifecycle_actions.size()); ++i) {
		auto& callback{ sequence.lifecycle_actions[static_cast<std::size_t>(i)] };
		ImGui::PushID(static_cast<int>(callback.id));

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
			DrawActionPickerWithInline(callback.action, false);
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
		DrawActionParameters(
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

void DemoEditor::DrawAddResidentScriptPopup(ScriptsComponent& scripts) {
	if (!ImGui::BeginPopup("AddResidentScript")) {
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

void DemoEditor::DrawAddSequencePopup(ScriptsComponent& scripts) {
	if (!ImGui::BeginPopup("AddScriptSequencePopup")) {
		return;
	}
	if (ImGui::MenuItem("Create New Script Sequence")) {
		ScriptSequence sequence;
		sequence.start_events.push_back(EventRegistry::MakeCondition<OnCreate, OnCreateFilter>());
		scripts.sequences.push_back(std::move(sequence));
	}
	if (ImGui::BeginMenu("Existing Script Sequence")) {
		if (world_.GetSharedSequences().sequences.empty()) {
			ImGui::TextDisabled("No global Script Sequences");
		}
		for (const auto& shared : world_.GetSharedSequences().sequences) {
			const bool attached{ std::ranges::any_of(scripts.sequences, [&shared](const auto& binding) {
				return binding.shared_reference && binding.shared_sequence_id == shared.id;
			}) };
			ImGui::BeginDisabled(attached);
			if (ImGui::MenuItem(shared.name.c_str())) {
				ScriptSequence binding;
				binding.shared_reference = true;
				binding.shared_sequence_id = shared.id;
				scripts.sequences.push_back(std::move(binding));
			}
			ImGui::EndDisabled();
			if (attached && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip("Already attached to this entity");
			}
		}
		ImGui::EndMenu();
	}
	ImGui::EndPopup();
}

void DemoEditor::DrawComponentDefinition(
	ComponentDefinition& definition, bool removable,
	int* remove_index, int index
) {
	const auto* component{ ptgn::ComponentRegistry::Find(definition.type) };
	const auto* editor{ component ? FindComponentEditor(*component) : nullptr };
	const std::string label{
		component ? ResolveComponentEditor(*component).label : definition.type
	};

	ImGui::PushID(static_cast<int>(definition.id));
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
		ptgn::editor::ComponentEditorRegistry::DrawJson(
			*component, definition.value
		);
	}
	ImGui::PopID();
}

void DemoEditor::DrawPrefabInspector(PrefabDefinition& prefab) {
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
		DrawComponentDefinition(prefab.components[static_cast<std::size_t>(i)], true, &remove, i);
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

void DemoEditor::DrawPrefabs() {
	ImGui::TextDisabled("Prefabs");
	ImGui::Separator();
	int delete_index{ -1 };
	int duplicate_index{ -1 };
	for (int i{ 0 }; i < static_cast<int>(context_.prefabs.definitions.size()); ++i) {
		auto& prefab{ context_.prefabs.definitions[static_cast<std::size_t>(i)] };
		ImGui::PushID(static_cast<int>(prefab.id));
		if (ImGui::Selectable(prefab.name.c_str(), inspect_prefab_ && selected_prefab_ == i, 0, ImVec2{ 0.0f, 25.0f })) {
			selected_prefab_ = i;
			inspect_prefab_ = true;
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
		PrefabDefinition copy{ context_.prefabs.definitions[static_cast<std::size_t>(duplicate_index)] };
		copy.id = IdGenerator::Next();
		copy.name += " Copy";
		copy.key += "_copy";
		for (auto& component : copy.components) {
			component.id = IdGenerator::Next();
		}
		context_.prefabs.definitions.insert(context_.prefabs.definitions.begin() + duplicate_index + 1, std::move(copy));
		selected_prefab_ = duplicate_index + 1;
		inspect_prefab_ = true;
	}
	if (delete_index >= 0) {
		context_.prefabs.definitions.erase(context_.prefabs.definitions.begin() + delete_index);
		selected_prefab_ = context_.prefabs.definitions.empty() ? -1 : std::clamp(selected_prefab_, 0, static_cast<int>(context_.prefabs.definitions.size()) - 1);
		inspect_prefab_ = selected_prefab_ >= 0;
	}
	if (ImGui::Button("+ New Prefab", ImVec2{ -FLT_MIN, 0.0f })) {
		context_.prefabs.definitions.emplace_back();
		selected_prefab_ = static_cast<int>(context_.prefabs.definitions.size()) - 1;
		inspect_prefab_ = true;
	}
}

void DemoEditor::DrawActivity() {
	const auto activity{ world_.ActivityText() };
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
};

struct TagComponent {
	std::string value;
};

struct DemoVisual {
	ImVec4 color{ 0.35f, 0.43f, 0.57f, 1.0f };
	bool sensor{ false };
};

enum class ButtonState {
	Idle,
	Hovered,
	Pressed,
	Disabled
};

struct ButtonData {
	bool enabled{ true };
	ButtonState state{ ButtonState::Idle };
	bool hovered{ false };
	bool pressed{ false };
	ptgn::Mouse pressed_button{ ptgn::Mouse::Left };
};

struct ButtonStyle {
	ImVec4 idle{ 0.22f, 0.38f, 0.66f, 1.0f };
	ImVec4 hovered{ 0.30f, 0.49f, 0.82f, 1.0f };
	ImVec4 pressed{ 0.16f, 0.29f, 0.54f, 1.0f };
	ImVec4 disabled{ 0.28f, 0.29f, 0.32f, 1.0f };
};

struct ButtonScript {
	[[nodiscard]] EventResult OnEvent(ScriptContext& context, const EventView& event);
};

struct PlayerMovementScript {
	float speed{ 220.0f };
	void OnUpdate(ScriptContext& context);
};

struct Health {
	float maximum{ 100.0f };
	float current{ 100.0f };
};

struct Zombie {};

struct Damage {
	float amount{ 10.0f };
	std::string damage_type{ "Physical" };
};

struct Lifetime {
	float duration_ms{ 3000.0f };
};

struct MaskComponent {
	int value{ 1 };
};

struct ApplyDamageAction {
	float amount{ 10.0f };
	std::string damage_type{ "Physical" };
	bool critical{ false };
	void OnStart(ActionContext& context);
};

inline void to_json(ptgn::json& output, const DemoVisual& visual) {
	output = ptgn::json{
		{ "color", { visual.color.x, visual.color.y, visual.color.z, visual.color.w } },
		{ "sensor", visual.sensor },
	};
}

inline void from_json(const ptgn::json& input, DemoVisual& visual) {
	const auto& color{ input.at("color") };
	visual.color = ImVec4{
		color.at(0).get<float>(),
		color.at(1).get<float>(),
		color.at(2).get<float>(),
		color.at(3).get<float>(),
	};
	visual.sensor = input.value("sensor", false);
}

inline void to_json(ptgn::json& output, const Health& health) {
	output = ptgn::json{
		{ "maximum", health.maximum },
		{ "current", health.current },
	};
}

inline void from_json(const ptgn::json& input, Health& health) {
	health.maximum = input.value("maximum", 100.0f);
	health.current = input.value("current", health.maximum);
}

inline void to_json(ptgn::json& output, const Zombie&) {
	output = ptgn::json::object();
}

inline void from_json(const ptgn::json&, Zombie&) {}

inline void to_json(ptgn::json& output, const Damage& damage) {
	output = ptgn::json{
		{ "amount", damage.amount },
		{ "damage_type", damage.damage_type },
	};
}

inline void from_json(const ptgn::json& input, Damage& damage) {
	damage.amount = input.value("amount", 10.0f);
	damage.damage_type = input.value("damage_type", std::string{ "Physical" });
}

inline void to_json(ptgn::json& output, const Lifetime& lifetime) {
	output = ptgn::json{ { "duration_ms", lifetime.duration_ms } };
}

inline void from_json(const ptgn::json& input, Lifetime& lifetime) {
	lifetime.duration_ms = input.value("duration_ms", 3000.0f);
}

inline void to_json(ptgn::json& output, const MaskComponent& mask) {
	output = ptgn::json{ { "value", mask.value } };
}

inline void from_json(const ptgn::json& input, MaskComponent& mask) {
	mask.value = input.value("value", 1);
}

inline void to_json(ptgn::json& output, const ButtonData& button) {
	output = ptgn::json{
		{ "enabled", button.enabled },
		{ "state", static_cast<int>(button.state) },
		{ "hovered", button.hovered },
		{ "pressed", button.pressed },
		{ "pressed_button", static_cast<int>(button.pressed_button) },
	};
}

inline void from_json(const ptgn::json& input, ButtonData& button) {
	button.enabled = input.value("enabled", true);
	button.state = static_cast<ButtonState>(
		input.value("state", static_cast<int>(ButtonState::Idle))
	);
	button.hovered = input.value("hovered", false);
	button.pressed = input.value("pressed", false);
	button.pressed_button = static_cast<ptgn::Mouse>(
		input.value("pressed_button", static_cast<int>(ptgn::Mouse::Left))
	);
}

inline void to_json(ptgn::json& output, const ButtonStyle& style) {
	auto color = [](const ImVec4& value) {
		return ptgn::json{ value.x, value.y, value.z, value.w };
	};
	output = ptgn::json{
		{ "idle", color(style.idle) },
		{ "hovered", color(style.hovered) },
		{ "pressed", color(style.pressed) },
		{ "disabled", color(style.disabled) },
	};
}

inline void from_json(const ptgn::json& input, ButtonStyle& style) {
	auto color = [](const ptgn::json& value) {
		return ImVec4{
			value.at(0).get<float>(),
			value.at(1).get<float>(),
			value.at(2).get<float>(),
			value.at(3).get<float>(),
		};
	};
	style.idle = color(input.at("idle"));
	style.hovered = color(input.at("hovered"));
	style.pressed = color(input.at("pressed"));
	style.disabled = color(input.at("disabled"));
}

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

class DemoWorld :
	public Scene,
	public RuntimeHost,
	public editor::EditorHost {
public:
	struct ActivityEntry {
		std::string text;
		float remaining_seconds{ 8.0f };
	};

	explicit DemoWorld(GLFWwindow* window) : window_{ window } {
		BindRuntimeHost(*this);
		RegisterEngineTypes();
		CreatePrefabs();
		CreateDemoScene();
	}

	void RegisterEditorExtensions() override;

	[[nodiscard]] PrefabRegistry& GetPrefabs() override { return prefabs_; }
	[[nodiscard]] SharedScriptSequenceRegistry& GetSharedSequences() override { return shared_sequences_; }
	[[nodiscard]] const std::vector<ptgn::Entity>& Entities() const override { return entities_; }
	[[nodiscard]] std::vector<ptgn::Entity>& Entities() { return entities_; }
	[[nodiscard]] const std::vector<ActivityEntry>& Activity() const { return activity_; }
	[[nodiscard]] std::vector<std::string> ActivityText() const override {
		std::vector<std::string> result;
		result.reserve(activity_.size());
		for (const auto& entry : activity_) {
			result.push_back(entry.text);
		}
		return result;
	}

	[[nodiscard]] ptgn::Entity FindByTag(std::string_view tag) const {
		for (auto entity : entities_) {
			if (const auto* component{ entity.TryGet<TagComponent>() };
				component && component->value == tag) {
				return entity;
			}
		}
		return {};
	}

	[[nodiscard]] std::string_view Name(ptgn::Entity entity) const override {
		if (entity && entity.Has<NameComponent>()) {
			return entity.Get<NameComponent>().value;
		}
		return "Entity";
	}

	[[nodiscard]] std::string& EditableName(ptgn::Entity entity) override {
		return entity.TryAdd<NameComponent>().value;
	}

	[[nodiscard]] std::string& EditableTag(ptgn::Entity entity) override {
		return entity.TryAdd<TagComponent>().value;
	}

	[[nodiscard]] std::string_view Tag(ptgn::Entity entity) const override {
		if (entity && entity.Has<TagComponent>()) {
			return entity.Get<TagComponent>().value;
		}
		return {};
	}

	[[nodiscard]] int Mask(ptgn::Entity entity) const override {
		if (entity && entity.Has<MaskComponent>()) {
			return entity.Get<MaskComponent>().value;
		}
		return 0;
	}

	[[nodiscard]] editor::EditorVisual Visual(ptgn::Entity entity) const override {
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

	void Log(std::string text) override {
		activity_.push_back(ActivityEntry{ .text = std::move(text) });
		constexpr std::size_t maximum{ 18 };
		if (activity_.size() > maximum) {
			activity_.erase(activity_.begin());
		}
	}

	void Queue(EventEnvelope event) override {
		if (event.type.empty()) {
			return;
		}
		if (event.delivery == EventDelivery::Target) {
			Context().local_events.Push(std::move(event));
		} else {
			Context().global_events.Push(std::move(event));
		}
	}

	ptgn::Entity CreateEntity(std::string name, std::string tag = {}) override {
		ptgn::Entity entity{ manager_.CreateEntity() };
		entity.Add<NameComponent>(NameComponent{ std::move(name) });
		entity.Add<TagComponent>(TagComponent{ std::move(tag) });
		entity.Add<ptgn::Transform>();
		entity.Add<ptgn::Visible>();
		entity.Add<DemoVisual>();
		entity.Add<MaskComponent>();
		entities_.push_back(entity);
		manager_.Refresh();
		return entity;
	}

	ptgn::Entity SpawnPrefab(const PrefabDefinition& prefab, ptgn::V2_float position) {
		ptgn::Entity entity{ CreateEntity(prefab.name, prefab.tag) };
		for (const auto& component : prefab.components) {
			const auto* registration{
				ptgn::ComponentRegistry::Find(component.type)
			};
			if (registration && registration->deserialize) {
				registration->deserialize(component.value, entity);
			}
		}
		if (prefab.scripts.has_value()) {
			entity.Add<ScriptsComponent>(prefab.scripts.value());
		}
		entity.Get<ptgn::Transform>().position = position;
		Log("Spawned prefab " + prefab.key);
		return entity;
	}

	ptgn::Entity SpawnPrefab(const PrefabSpawnRequest& request) override {
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

	[[nodiscard]] bool KeyDown(ptgn::Key key) const override {
		return glfwGetKey(window_, static_cast<int>(key)) == GLFW_PRESS;
	}

	[[nodiscard]] float KeyHoldDuration(ptgn::Key key) const override {
		const auto it{ key_hold_duration_ms_.find(static_cast<int>(key)) };
		return it == key_hold_duration_ms_.end() ? 0.0f : it->second;
	}

	[[nodiscard]] bool MouseDown(ptgn::Mouse button) const override {
		return glfwGetMouseButton(window_, static_cast<int>(button)) == GLFW_PRESS;
	}

	[[nodiscard]] float MouseHoldDuration(ptgn::Mouse button) const override {
		const auto it{ mouse_hold_duration_ms_.find(static_cast<int>(button)) };
		return it == mouse_hold_duration_ms_.end() ? 0.0f : it->second;
	}

	void Destroy(ptgn::Entity entity) override {
		if (entity && !std::ranges::contains(pending_destroy_, entity)) {
			pending_destroy_.push_back(entity);
		}
	}

	void Update(float delta_seconds) {
		Scene::Update(delta_seconds);
	}

	[[nodiscard]] const ScriptSequence* Resolve(const ScriptSequence& binding) const override {
		return binding.shared_reference
			? shared_sequences_.Find(binding.shared_sequence_id)
			: &binding;
	}

	[[nodiscard]] ScriptSequence* Resolve(ScriptSequence& binding) override {
		return binding.shared_reference
			? shared_sequences_.Find(binding.shared_sequence_id)
			: &binding;
	}

	void Start(ptgn::Entity owner, ScriptSequence& binding, bool force = false) override {
		(void)StartBinding(owner, binding, force);
	}

	void Stop(ptgn::Entity owner, ScriptSequence& binding, bool log = true) override {
		(void)CancelBinding(owner, binding, SequenceCancelReason::Stopped, log, true);
	}

	void SetPaused(ptgn::Entity owner, ScriptSequence& binding, bool paused) override {
		(void)SetBindingPaused(owner, binding, paused);
	}

	[[nodiscard]] float Progress(const ScriptSequence& binding) const override {
		const auto* sequence{ Resolve(binding) };
		if (!sequence || !binding.runtime.running ||
			binding.runtime.action_index >= sequence->actions.size()) {
			return binding.runtime.completed ? 1.0f : 0.0f;
		}
		const auto& action{ sequence->actions[binding.runtime.action_index] };
		if (!action.timing || action.timing->duration_ms <= 0.0f) {
			return 0.0f;
		}
		return std::clamp(
			binding.runtime.elapsed_ms / action.timing->duration_ms,
			0.0f,
			1.0f
		);
	}

	SequenceHandle RunSequence(ptgn::Entity owner, ScriptSequence sequence) override {
		if (!owner) {
			return {};
		}
		auto& scripts{ owner.TryAdd<ScriptsComponent>() };
		const Id id{ scripts.AddSequenceDeferred(std::move(sequence)) };
		(void)StartSequence(owner, id, true);
		return SequenceHandle{ .host = this, .owner = owner, .binding_id = id };
	}

	SequenceHandle RunInChannel(
		ptgn::Entity owner,
		SequenceChannelKey channel,
		ScriptSequence sequence,
		ReentryMode reentry
	) override {
		if (!owner) {
			return {};
		}

		auto& scripts{ owner.TryAdd<ScriptsComponent>() };
		auto& channel_runtime{ FindOrCreateChannel(scripts, channel) };
		if (channel_runtime.active) {
			if (reentry == ReentryMode::IgnoreWhileRunning) {
				return SequenceHandle{
					.host = this,
					.owner = owner,
					.binding_id = *channel_runtime.active,
				};
			}
			if (reentry == ReentryMode::Restart) {
				StopSequenceChannel(owner, channel, SequenceStopMode::All);
			}
		}

		sequence.channel = channel;
		sequence.reentry = reentry;
		sequence.transient = true;
		sequence.remove_binding_on_complete = true;
		const Id id{ scripts.AddSequenceDeferred(std::move(sequence)) };

		if (reentry == ReentryMode::Queue && channel_runtime.active) {
			channel_runtime.waiting.push_back(id);
			if (auto* binding{ FindBinding(owner, id) }) {
				binding->runtime.waiting_for_channel = true;
			}
		} else {
			channel_runtime.active = id;
			(void)StartSequence(owner, id, true);
		}

		return SequenceHandle{ .host = this, .owner = owner, .binding_id = id };
	}

	bool StartSequence(ptgn::Entity owner, Id id, bool force = false) override {
		auto* binding{ FindBinding(owner, id) };
		return binding && StartBinding(owner, *binding, force);
	}

	void StopSequenceChannel(
		ptgn::Entity owner,
		SequenceChannelKey channel,
		SequenceStopMode mode
	) override {
		auto* scripts{ owner.TryGet<ScriptsComponent>() };
		if (!scripts) {
			return;
		}
		auto* runtime{ FindChannel(*scripts, channel) };
		if (!runtime) {
			return;
		}

		const std::optional<Id> active{ runtime->active };
		std::vector<Id> waiting{ runtime->waiting.begin(), runtime->waiting.end() };
		if (mode == SequenceStopMode::All) {
			runtime->active.reset();
			runtime->waiting.clear();
		}

		if (active) {
			if (auto* binding{ FindBinding(owner, *active) }) {
				(void)CancelBinding(
					owner, *binding, SequenceCancelReason::Replaced, false,
					mode == SequenceStopMode::Current
				);
			}
		}
		if (mode == SequenceStopMode::All) {
			for (const Id id : waiting) {
				if (auto* binding{ FindBinding(owner, id) }) {
					(void)CancelBinding(
						owner, *binding, SequenceCancelReason::Replaced, false, false
					);
				}
			}
		}
	}

	bool StopSequence(
		ptgn::Entity owner,
		Id id,
		SequenceCancelReason reason = SequenceCancelReason::Stopped
	) override {
		auto* binding{ FindBinding(owner, id) };
		return binding && CancelBinding(owner, *binding, reason, true, true);
	}

	bool ResetSequence(ptgn::Entity owner, Id id) override {
		auto* binding{ FindBinding(owner, id) };
		if (!binding) {
			return false;
		}
		(void)CancelBinding(owner, *binding, SequenceCancelReason::Reset, false, true);
		InvokeLifecycle(owner, *binding, SequenceLifecycle::Reset);
		return true;
	}

	bool ClearSequence(ptgn::Entity owner, Id id) override {
		auto* binding{ FindBinding(owner, id) };
		if (!binding) {
			return false;
		}
		(void)CancelBinding(owner, *binding, SequenceCancelReason::Cleared, false, true);
		if (binding->shared_reference || binding->transient) {
			owner.Get<ScriptsComponent>().RemoveSequenceDeferred(id);
		} else {
			binding->actions.clear();
			binding->start_events.clear();
			binding->stop_events.clear();
			binding->lifecycle_actions.clear();
		}
		return true;
	}

	bool SkipSequenceAction(ptgn::Entity owner, Id id) override {
		auto* binding{ FindBinding(owner, id) };
		if (!binding || !binding->runtime.running) {
			return false;
		}
		if (binding->runtime.action_instance) {
			ActionContext context{ .host = *this, .owner = owner, .scene = &Context() };
			binding->runtime.action_instance->Cancel(context, SequenceCancelReason::Skipped);
			InvokeLifecycle(owner, *binding, SequenceLifecycle::ActionCancel);
		}
		++binding->runtime.action_index;
		binding->runtime.ClearActiveAction();
		ProcessImmediateActions(owner, *binding);
		return true;
	}

	bool SeekSequence(ptgn::Entity owner, Id id, float progress) override {
		auto* binding{ FindBinding(owner, id) };
		if (!binding) {
			return false;
		}
		if (!binding->runtime.running && !StartBinding(owner, *binding, true)) {
			return false;
		}
		const auto* sequence{ Resolve(*binding) };
		if (!sequence || binding->runtime.action_index >= sequence->actions.size()) {
			return false;
		}
		const auto& action{ sequence->actions[binding->runtime.action_index] };
		if (!action.timing) {
			return false;
		}
		binding->runtime.elapsed_ms =
			std::clamp(progress, 0.0f, 1.0f) * std::max(0.0f, action.timing->duration_ms);
		UpdateSequence(owner, *binding, 0.0f);
		return true;
	}

	bool SetSequencePaused(ptgn::Entity owner, Id id, bool paused) override {
		auto* binding{ FindBinding(owner, id) };
		return binding && SetBindingPaused(owner, *binding, paused);
	}

	[[nodiscard]] float SequenceProgress(ptgn::Entity owner, Id id) const override {
		const auto* binding{ FindBinding(owner, id) };
		return binding ? Progress(*binding) : 0.0f;
	}

	[[nodiscard]] bool SequenceRunning(ptgn::Entity owner, Id id) const override {
		const auto* binding{ FindBinding(owner, id) };
		return binding && binding->runtime.running;
	}

	[[nodiscard]] bool SequencePaused(ptgn::Entity owner, Id id) const override {
		const auto* binding{ FindBinding(owner, id) };
		return binding && binding->runtime.paused;
	}

	[[nodiscard]] bool SequenceCompleted(ptgn::Entity owner, Id id) const override {
		const auto* binding{ FindBinding(owner, id) };
		return binding && binding->runtime.completed;
	}

	void SubmitPointerFrame(PointerFrame frame) override {
		pointer_frame_ = frame;
		pointer_frame_pending_ = true;
	}

private:
	friend struct PlayerMovementScript;
	friend struct MoveToAction;
	friend struct RotateToAction;
	friend struct SetVisibleAction;
	friend struct PlayAudioAction;
	friend struct EmitSignalAction;
	friend struct AddComponentsAction;
	friend struct RemoveComponentsAction;
	friend struct SpawnEntityAction;
	friend struct ApplyDamageAction;
	friend class editor::DemoEditor;

	void RegisterEngineTypes();
	void CreatePrefabs();
	void CreateDemoScene();
	void UpdateInputEvents(float delta_seconds);
	void UpdateButtonInteraction();
	void ProcessPendingDestroy();
	void UpdateOverlapEvents();

	void OnBeforeScriptUpdate(float delta_seconds) override;
	void UpdateResidentScripts(float delta_seconds) override;
	void UpdateScriptSequences(float delta_seconds) override;
	void ApplyPendingScriptChanges() override;
	void DispatchBufferedEvents() override;
	void OnAfterScriptUpdate(float delta_seconds) override;

	void DispatchEvents();
	void UpdateSequence(ptgn::Entity owner, ScriptSequence& binding, float delta_seconds);
	void ProcessImmediateActions(ptgn::Entity owner, ScriptSequence& binding);
	void CompleteCurrentAction(ptgn::Entity owner, ScriptSequence& binding);
	void CompleteSequence(ptgn::Entity owner, ScriptSequence& binding);
	void InvokeLifecycle(ptgn::Entity owner, ScriptSequence& binding, SequenceLifecycle lifecycle);
	void ExecuteInstant(ptgn::Entity owner, const Action& action);
	[[nodiscard]] ScriptSequence* FindBinding(ptgn::Entity owner, Id id);
	[[nodiscard]] const ScriptSequence* FindBinding(ptgn::Entity owner, Id id) const;
	[[nodiscard]] SequenceChannelRuntime* FindChannel(
		ScriptsComponent& scripts, const SequenceChannelKey& key
	);
	[[nodiscard]] SequenceChannelRuntime& FindOrCreateChannel(
		ScriptsComponent& scripts, const SequenceChannelKey& key
	);
	[[nodiscard]] bool StartBinding(ptgn::Entity owner, ScriptSequence& binding, bool force);
	[[nodiscard]] bool CancelBinding(
		ptgn::Entity owner,
		ScriptSequence& binding,
		SequenceCancelReason reason,
		bool log,
		bool promote_channel,
		bool remove_transient = true
	);
	[[nodiscard]] bool SetBindingPaused(
		ptgn::Entity owner, ScriptSequence& binding, bool paused
	);
	void ReleaseChannel(ptgn::Entity owner, ScriptSequence& binding, bool promote);
	void PromoteNextInChannel(ptgn::Entity owner, SequenceChannelRuntime& channel);
	[[nodiscard]] bool Matches(
		const EventCondition& condition,
		const EventEnvelope& event,
		ptgn::Entity owner
	);
	[[nodiscard]] bool Overlap(ptgn::Entity a, ptgn::Entity b) const;

	GLFWwindow* window_{ nullptr };
	PrefabRegistry prefabs_;
	SharedScriptSequenceRegistry shared_sequences_;
	std::vector<ptgn::Entity> entities_;
	std::vector<ActivityEntry> activity_;
	std::unordered_map<int, bool> previous_key_states_;
	std::unordered_map<int, float> key_hold_duration_ms_;
	std::unordered_map<int, bool> previous_mouse_states_;
	std::unordered_map<int, float> mouse_hold_duration_ms_;
	std::vector<ptgn::Entity> pending_destroy_;
	bool player_overlapping_sensor_{ false };
	bool player_overlapping_spawner_{ false };
	PointerFrame pointer_frame_{};
	bool pointer_frame_pending_{ false };
	ptgn::Entity hovered_button_;
	std::array<ptgn::Entity, 3> pressed_buttons_{};
};

EventResult ButtonScript::OnEvent(ScriptContext& context, const EventView& event) {
	auto* button{ context.owner.TryGet<ButtonData>() };
	if (!button) {
		return EventResult::Continue;
	}

	if (!button->enabled) {
		button->hovered = false;
		button->pressed = false;
		button->state = ButtonState::Disabled;
		return EventResult::Continue;
	}

	if (event.Is<MouseMoveOver>()) {
		button->hovered = true;
		button->state = button->pressed ? ButtonState::Pressed : ButtonState::Hovered;
		return EventResult::Continue;
	}

	if (event.Is<MouseMoveOut>()) {
		button->hovered = false;
		button->state = button->pressed ? ButtonState::Pressed : ButtonState::Idle;
		return EventResult::Continue;
	}

	if (event.Is<MousePressedOver>()) {
		const auto& pressed{ event.Get<MousePressedOver>() };
		button->pressed = true;
		button->pressed_button = pressed.button;
		button->state = ButtonState::Pressed;
		return EventResult::Continue;
	}

	if (event.Is<MouseReleasedOver>()) {
		const auto& released{ event.Get<MouseReleasedOver>() };
		if (!button->pressed || released.button != button->pressed_button) {
			return EventResult::Continue;
		}

		const bool activate{ released.released_over };
		button->pressed = false;
		button->state = button->hovered ? ButtonState::Hovered : ButtonState::Idle;

		if (activate) {
			if (context.scene) {
				context.scene->PushLocal<ButtonPress>(
					context.owner, ButtonPress{ released.button }, context.owner
				);
			} else {
				context.host.Queue(EventRegistry::MakeEvent<ButtonPress>(
					ButtonPress{ released.button }, context.owner, context.owner,
					EventDelivery::Target
				));
			}
		}
		return EventResult::Continue;
	}

	return EventResult::Continue;
}

void PlayerMovementScript::OnUpdate(ScriptContext& context) {
	if (ImGui::GetIO().WantTextInput) {
		return;
	}

	ptgn::V2_float direction{};
	if (context.host.KeyDown(static_cast<ptgn::Key>(GLFW_KEY_A))) {
		direction.x -= 1.0f;
	}
	if (context.host.KeyDown(static_cast<ptgn::Key>(GLFW_KEY_D))) {
		direction.x += 1.0f;
	}
	if (context.host.KeyDown(static_cast<ptgn::Key>(GLFW_KEY_S))) {
		direction.y -= 1.0f;
	}
	if (context.host.KeyDown(static_cast<ptgn::Key>(GLFW_KEY_W))) {
		direction.y += 1.0f;
	}

	const float magnitude{ std::sqrt(direction.x * direction.x + direction.y * direction.y) };
	if (magnitude > 0.0f) {
		direction /= magnitude;
	}

	auto& transform{ context.owner.Get<ptgn::Transform>() };
	transform.position += direction * speed * context.delta_seconds;
	transform.position.x = std::clamp(transform.position.x, -430.0f, 430.0f);
	transform.position.y = std::clamp(transform.position.y, -250.0f, 250.0f);
}

void ApplyDamageAction::OnStart(ActionContext& context) {
	if (auto* health{ context.owner.TryGet<Health>() }) {
		const float applied{ std::max(0.0f, amount) * (critical ? 2.0f : 1.0f) };
		health->current -= applied;
		context.host.Log(
			std::string{ context.host.Name(context.owner) } + " took " +
			std::to_string(static_cast<int>(applied)) + " " + damage_type + " damage"
		);
		if (health->current <= 0.0f) {
			context.host.Destroy(context.owner);
		}
	}
}

void DemoWorld::RegisterEditorExtensions() {
	ptgn::editor::ComponentEditorRegistry::Register<DemoVisual>(
		ptgn::editor::MakeComponentEditorRegistration(
			ptgn::editor::ComponentEditorOptions{
				.label = "Demo Visual",
				.group = "Graphics",
				.draw_contents = &DrawRegisteredDemoContents<DemoVisual>,
			}
		)
	);
	ptgn::editor::ComponentEditorRegistry::Register<Health>(
		ptgn::editor::MakeComponentEditorRegistration(
			ptgn::editor::ComponentEditorOptions{
				.label = "Health",
				.group = "Gameplay",
				.draw_contents = &DrawRegisteredDemoContents<Health>,
			}
		)
	);
	ptgn::editor::ComponentEditorRegistry::Register<Zombie>(
		ptgn::editor::MakeComponentEditorRegistration(
			ptgn::editor::ComponentEditorOptions{
				.label = "Zombie",
				.group = "Gameplay",
			}
		)
	);
	ptgn::editor::ComponentEditorRegistry::Register<Damage>(
		ptgn::editor::MakeComponentEditorRegistration(
			ptgn::editor::ComponentEditorOptions{
				.label = "Damage",
				.group = "Gameplay",
				.draw_contents = &DrawRegisteredDemoContents<Damage>,
			}
		)
	);
	ptgn::editor::ComponentEditorRegistry::Register<Lifetime>(
		ptgn::editor::MakeComponentEditorRegistration(
			ptgn::editor::ComponentEditorOptions{
				.label = "Lifetime",
				.group = "Gameplay",
				.draw_contents = &DrawRegisteredDemoContents<Lifetime>,
			}
		)
	);
	ptgn::editor::ComponentEditorRegistry::Register<MaskComponent>(
		ptgn::editor::MakeComponentEditorRegistration(
			ptgn::editor::ComponentEditorOptions{
				.label = "Mask",
				.group = "Physics",
				.draw_contents = &DrawRegisteredDemoContents<MaskComponent>,
			}
		)
	);
	ptgn::editor::ComponentEditorRegistry::Register<ButtonData>(
		ptgn::editor::MakeComponentEditorRegistration(
			ptgn::editor::ComponentEditorOptions{
				.label = "Button",
				.group = "UI",
				.draw_contents = &DrawRegisteredDemoContents<ButtonData>,
			}
		)
	);
	ptgn::editor::ComponentEditorRegistry::Register<ButtonStyle>(
		ptgn::editor::MakeComponentEditorRegistration(
			ptgn::editor::ComponentEditorOptions{
				.label = "Button Style",
				.group = "UI",
				.draw_contents = &DrawRegisteredDemoContents<ButtonStyle>,
			}
		)
	);

	auto draw_no_event_filter = [](NoEventFilter&) {
		return false;
	};
	auto draw_mouse_button_filter = [](MouseButtonFilter& filter) {
		const char* preview{ MouseButtonLabel(filter.button) };
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (!ImGui::BeginCombo("##MouseButton", preview)) {
			return false;
		}

		bool changed{ false };
		for (ptgn::Mouse button : kMouseButtons) {
			if (ImGui::Selectable(
					MouseButtonLabel(button), button == filter.button
				)) {
				filter.button = button;
				changed = true;
			}
		}
		ImGui::EndCombo();
		return changed;
	};
	auto draw_runtime_event_payload = [](auto&) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("Runtime payload");
		return false;
	};
	auto draw_button_payload = [](auto& event) {
		const char* preview{ MouseButtonLabel(event.button) };
		bool changed{ false };
		if (ImGui::BeginCombo("Button", preview)) {
			for (ptgn::Mouse button : kMouseButtons) {
				if (ImGui::Selectable(
						MouseButtonLabel(button), button == event.button
					)) {
					event.button = button;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		if constexpr (requires { event.released_over; }) {
			changed |= ImGui::Checkbox("Released Over", &event.released_over);
		}
		return changed;
	};
	editor::EventEditorRegistry::Register<NoEventFilter, MouseMoveOver>(
		"ptgn.event.MouseMoveOver",
		{ .label = "On Mouse Enter", .group = "Button", .description = "Targeted local event fired when the pointer enters a button." },
		draw_no_event_filter, draw_runtime_event_payload
	);
	editor::EventEditorRegistry::Register<NoEventFilter, MouseMoveOut>(
		"ptgn.event.MouseMoveOut",
		{ .label = "On Mouse Leave", .group = "Button", .description = "Targeted local event fired when the pointer leaves a button." },
		draw_no_event_filter, draw_runtime_event_payload
	);
	editor::EventEditorRegistry::Register<MouseButtonFilter, MousePressedOver>(
		"ptgn.event.MousePressedOver",
		{ .label = "On Mouse Pressed Over", .group = "Button", .description = "Targeted local event fired when a mouse button is pressed over the entity." },
		draw_mouse_button_filter, draw_button_payload
	);
	editor::EventEditorRegistry::Register<MouseButtonFilter, MouseReleasedOver>(
		"ptgn.event.MouseReleasedOver",
		{ .label = "On Mouse Released", .group = "Button", .description = "Targeted local event fired when the active mouse press is released." },
		draw_mouse_button_filter, draw_button_payload
	);
	editor::EventEditorRegistry::Register<MouseButtonFilter, ButtonPress>(
		"ptgn.event.ButtonPress",
		{ .label = "On Button Press", .group = "Button", .description = "Semantic event emitted by ButtonScript after a valid click." },
		draw_mouse_button_filter, draw_button_payload
	);

	editor::ActionEditorRegistry::RegisterInline<ScaleToAction>(
		"engine.scale_to",
		{ .label = "Scale To", .group = "Transform", .description = "Scale the owning entity." },
		[](ScaleToAction& action, editor::EditorContext&) {
			const float available{ ImGui::GetContentRegionAvail().x };
			const float spacing{ ImGui::GetStyle().ItemSpacing.x };
			const float mode_width{
				std::max(ImGui::CalcTextSize("Relative").x, ImGui::CalcTextSize("Absolute").x) +
				ImGui::GetStyle().FramePadding.x * 2.0f
			};
			const float field_width{ std::max(36.0f, (available - mode_width - spacing * 2.0f) * 0.5f) };
			bool changed{ false };
			ImGui::SetNextItemWidth(field_width);
			changed |= ImGui::DragFloat("##ScaleX", &action.scale.x, 0.01f, -100.0f, 100.0f, "X: %.2f");
			ImGui::SameLine(0.0f, spacing);
			ImGui::SetNextItemWidth(field_width);
			changed |= ImGui::DragFloat("##ScaleY", &action.scale.y, 0.01f, -100.0f, 100.0f, "Y: %.2f");
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
		[](ScaleToAction&, editor::EditorContext&) { return false; }
	);
	editor::ActionEditorRegistry::Register<FollowTargetAction>(
		"engine.follow_target",
		{ .label = "Follow Target", .group = "Transform", .description = "Action-controlled movement that completes when the owner reaches its target." },
		[](FollowTargetAction& action, editor::EditorContext& context) {
			bool changed{ false };
			if (!ImGui::BeginTable(
					"FollowTargetParameters", 3,
					ImGuiTableFlags_SizingStretchProp
				)) {
				return false;
			}

			ImGui::TableSetupColumn(
				"Target", ImGuiTableColumnFlags_WidthStretch, 1.35f
			);
			ImGui::TableSetupColumn(
				"Speed", ImGuiTableColumnFlags_WidthStretch, 0.85f
			);
			ImGui::TableSetupColumn(
				"StoppingDistance", ImGuiTableColumnFlags_WidthStretch, 1.0f
			);
			ImGui::TableNextRow(
				ImGuiTableRowFlags_None, ImGui::GetFrameHeight()
			);

			ImGui::TableSetColumnIndex(0);
			const std::string target_name{
				action.target
					? std::string{ context.host.Name(action.target) }
					: std::string{ "None" }
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
					if (ImGui::Selectable(
							name.c_str(), entity == action.target
						)) {
						action.target = entity;
						changed = true;
					}
				}
				ImGui::EndCombo();
			}

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			changed |= ImGui::DragFloat(
				"##Speed", &action.speed, 1.0f, 0.0f, 10000.0f,
				"Speed: %.0f"
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
	);
	editor::ActionEditorRegistry::Register<ApplyDamageAction>(
		"game.apply_damage",
		{ .label = "Apply Damage", .group = "Game", .description = "Apply damage to the owning entity." },
		[](ApplyDamageAction& action, editor::EditorContext&) {
			bool changed{ false };
			if (ImGui::BeginTable("DamageParams", 3, ImGuiTableFlags_SizingStretchProp)) {
				const float critical_width{
					ImGui::CalcTextSize("Critical").x + ImGui::GetFrameHeight() +
					ImGui::GetStyle().ItemInnerSpacing.x
				};
				ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthStretch, 0.75f);
				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.35f);
				ImGui::TableSetupColumn(
					"Critical", ImGuiTableColumnFlags_WidthFixed, critical_width
				);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::DragFloat("##Amount", &action.amount, 0.25f, 0.0f, 100000.0f, "%.2f");
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::InputText("##DamageType", &action.damage_type);
				ImGui::TableSetColumnIndex(2);
				changed |= ImGui::Checkbox("Critical", &action.critical);
				ImGui::EndTable();
			}
			return changed;
		}
	);
	editor::ScriptEditorRegistry::Register<PlayerMovementScript>(
		"game.player_movement",
		{ .label = "Player Movement", .group = "Game", .description = "WASD movement for the demo player." },
		[](PlayerMovementScript& script) {
			return ImGui::DragFloat("Speed", &script.speed, 1.0f, 0.0f, 2000.0f);
		}
	);
	editor::ScriptEditorRegistry::Register<ButtonScript>(
		"ui.button",
		{ .label = "Button", .group = "UI", .description = "Consumes typed local pointer events and emits ButtonPress." },
		[](ButtonScript&) { return false; }
	);
}

void DemoWorld::RegisterEngineTypes() {
	// Reuse the engine component registry for live entities and detached JSON values.
	ptgn::ComponentRegistry::Register<ptgn::Transform>("Transform");
	ptgn::ComponentRegistry::Register<DemoVisual>("DemoVisual");
	ptgn::ComponentRegistry::Register<Health>("DemoHealth");
	ptgn::ComponentRegistry::Register<Zombie>("DemoZombie");
	ptgn::ComponentRegistry::Register<Damage>("DemoDamage");
	ptgn::ComponentRegistry::Register<Lifetime>("DemoLifetime");
	ptgn::ComponentRegistry::Register<MaskComponent>("DemoMask");
	ptgn::ComponentRegistry::Register<ButtonData>("DemoButtonData");
	ptgn::ComponentRegistry::Register<ButtonStyle>("DemoButtonStyle");

	// Existing engine Events plus the data-driven Signal Event.
	auto match_key_pressed = [](const auto& event, const KeyListFilter& filter, EventMatchContext& context) {
		const auto parsed{ ParseEnumExpression<ptgn::Key>(filter.keys) };
		return parsed.IsValid() && MatchesPressedOrHeldCombination(
			event.key, parsed, [&](ptgn::Key key) { return context.host.KeyDown(key); }
		);
	};
	auto match_key_held = [](const auto& event, const KeyListFilter& filter, EventMatchContext& context) {
		const auto parsed{ ParseEnumExpression<ptgn::Key>(filter.keys) };
		const float required_duration{ std::max(0.0f, filter.hold_duration_ms) };
		return parsed.IsValid() && std::ranges::any_of(
			parsed.alternatives,
			[&](const auto& combination) {
				return std::ranges::contains(combination, event.key) &&
					std::ranges::all_of(combination, [&](ptgn::Key key) {
						return context.host.KeyDown(key) &&
							context.host.KeyHoldDuration(key) >= required_duration;
					});
			}
		);
	};
	auto match_key_released = [](const auto& event, const KeyListFilter& filter, EventMatchContext& context) {
		const auto parsed{ ParseEnumExpression<ptgn::Key>(filter.keys) };
		return parsed.IsValid() && MatchesReleasedCombination(
			event.key, parsed, [&](ptgn::Key key) { return context.host.KeyDown(key); }
		);
	};
	auto match_mouse_pressed = [](const auto& event, const MouseListFilter& filter, EventMatchContext& context) {
		const auto parsed{ ParseEnumExpression<ptgn::Mouse>(filter.buttons) };
		return parsed.IsValid() && MatchesPressedOrHeldCombination(
			MouseFromEvent(event), parsed,
			[&](ptgn::Mouse button) { return context.host.MouseDown(button); }
		);
	};
	auto match_mouse_held = [](const auto& event, const MouseListFilter& filter, EventMatchContext& context) {
		const auto parsed{ ParseEnumExpression<ptgn::Mouse>(filter.buttons) };
		const ptgn::Mouse event_button{ MouseFromEvent(event) };
		const float required_duration{ std::max(0.0f, filter.hold_duration_ms) };
		return parsed.IsValid() && std::ranges::any_of(
			parsed.alternatives,
			[&](const auto& combination) {
				return std::ranges::contains(combination, event_button) &&
					std::ranges::all_of(combination, [&](ptgn::Mouse button) {
						return context.host.MouseDown(button) &&
							context.host.MouseHoldDuration(button) >= required_duration;
					});
			}
		);
	};
	auto match_mouse_released = [](const auto& event, const MouseListFilter& filter, EventMatchContext& context) {
		const auto parsed{ ParseEnumExpression<ptgn::Mouse>(filter.buttons) };
		return parsed.IsValid() && MatchesReleasedCombination(
			MouseFromEvent(event), parsed,
			[&](ptgn::Mouse button) { return context.host.MouseDown(button); }
		);
	};
	auto match_entity = [](const auto& event, const EntityMaskFilter& filter, EventMatchContext& context) {
		return MatchesEntityMaskFilter(OtherEntity(event), filter, context.host);
	};
	auto match_on_create = [](const OnCreate&, const OnCreateFilter&, EventMatchContext&) {
		return true;
	};
	auto match_signal = [](const Signal& event, const Signal& filter, EventMatchContext&) {
		return event.key == filter.key;
	};
	auto match_all = [](const auto&, const NoEventFilter&, EventMatchContext&) {
		return true;
	};
	auto match_mouse_button = [](
		const auto& event,
		const MouseButtonFilter& filter,
		EventMatchContext&
	) {
		return event.button == filter.button;
	};

	EventRegistry::Register<OnCreate, OnCreateFilter>(
		"ptgn.event.OnCreate", match_on_create
	);
	EventRegistry::Register<Signal, Signal>(
		"ptgn.event.Signal", match_signal
	);
	EventRegistry::Register<MouseMoveOver, NoEventFilter>(
		"ptgn.event.MouseMoveOver", match_all
	);
	EventRegistry::Register<MouseMoveOut, NoEventFilter>(
		"ptgn.event.MouseMoveOut", match_all
	);
	EventRegistry::Register<MousePressedOver, MouseButtonFilter>(
		"ptgn.event.MousePressedOver", match_mouse_button
	);
	EventRegistry::Register<MouseReleasedOver, MouseButtonFilter>(
		"ptgn.event.MouseReleasedOver", match_mouse_button
	);
	EventRegistry::Register<ButtonPress, MouseButtonFilter>(
		"ptgn.event.ButtonPress", match_mouse_button
	);
	EventRegistry::Register<ptgn::event::KeyPressed, KeyListFilter>(
		"ptgn.event.KeyPressed", match_key_pressed
	);
	EventRegistry::Register<ptgn::event::KeyHeld, KeyListFilter>(
		"ptgn.event.KeyHeld", match_key_held
	);
	EventRegistry::Register<ptgn::event::KeyReleased, KeyListFilter>(
		"ptgn.event.KeyReleased", match_key_released
	);
	EventRegistry::Register<ptgn::event::MousePressed, MouseListFilter>(
		"ptgn.event.MousePressed", match_mouse_pressed
	);
	EventRegistry::Register<ptgn::event::MouseHeld, MouseListFilter>(
		"ptgn.event.MouseHeld", match_mouse_held
	);
	EventRegistry::Register<ptgn::event::MouseReleased, MouseListFilter>(
		"ptgn.event.MouseReleased", match_mouse_released
	);
	EventRegistry::Register<ptgn::event::OverlapStart, EntityMaskFilter>(
		"ptgn.event.OverlapStart", match_entity
	);
	EventRegistry::Register<ptgn::event::Overlap, EntityMaskFilter>(
		"ptgn.event.Overlap", match_entity
	);
	EventRegistry::Register<ptgn::event::OverlapStop, EntityMaskFilter>(
		"ptgn.event.OverlapStop", match_entity
	);
	EventRegistry::Register<ptgn::event::Collision, EntityMaskFilter>(
		"ptgn.event.Collision", match_entity
	);

	// Engine Action registrations.
	ActionRegistry::Register<WaitAction>(
		"engine.wait", true, true,
		ActionTiming{ .duration_ms = 250.0f }
	);
	ActionRegistry::Register<MoveToAction>(
		"engine.move_to", true, false,
		ActionTiming{ .duration_ms = 300.0f, .ease = ptgn::Ease::OutCubic },
		ActionCompletion::Duration
	);
	ActionRegistry::Register<RotateToAction>(
		"engine.rotate_to", true, false,
		ActionTiming{ .duration_ms = 300.0f },
		ActionCompletion::Duration
	);
	ActionRegistry::Register<ScaleToAction>(
		"engine.scale_to", true, false,
		ActionTiming{ .duration_ms = 180.0f, .ease = ptgn::Ease::OutBack },
		ActionCompletion::Duration
	);
	ActionRegistry::Register<FollowTargetAction>(
		"engine.follow_target", false, false, std::nullopt,
		ActionCompletion::ActionControlled
	);
	ActionRegistry::Register<NativeAction>(
		"engine.native", false, false, std::nullopt,
		ActionCompletion::Instant, false
	);
	ActionRegistry::Register<SetVisibleAction>("engine.set_visible");
	ActionRegistry::Register<PlayAudioAction>("engine.play_audio");
	ActionRegistry::Register<EmitSignalAction>("engine.emit_signal");
	ActionRegistry::Register<AddComponentsAction>("engine.add_components");
	ActionRegistry::Register<RemoveComponentsAction>("engine.remove_components");
	ActionRegistry::Register<SpawnEntityAction>("engine.spawn_entity");
	ActionRegistry::Register<ApplyDamageAction>("game.apply_damage");

	ScriptRegistry::Register<PlayerMovementScript>("game.player_movement");
	ScriptRegistry::Register<ButtonScript>("ui.button");
}

void DemoWorld::CreatePrefabs() {
	PrefabDefinition zombie;
	zombie.key = "prefabs/zombie";
	zombie.name = "Zombie";
	zombie.tag = "Zombie";
	zombie.components.push_back(
		MakeComponentDefinition(
			ptgn::Rect{ ptgn::V2_float{ 32.0f, 32.0f } }
		)
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
	circle.components.push_back(
		MakeComponentDefinition(ptgn::Circle{ 13.0f })
	);
	circle.components.push_back(
		MakeComponentDefinition(DemoVisual{
			.color = ImVec4{ 0.78f, 0.42f, 0.88f, 1.0f },
		})
	);

	ScriptsComponent circle_scripts;
	ScriptSequence destroy_sequence{ "Destroy on Recall" };
	destroy_sequence
		.StartOn<Signal>(Signal{ SignalKey{ "spawned_circles.destroy" } })
		.DestroyOwnerOnComplete();
	circle_scripts.sequences.push_back(std::move(destroy_sequence));
	circle.scripts = std::move(circle_scripts);
	prefabs_.definitions.push_back(std::move(circle));
}

void DemoWorld::CreateDemoScene() {
	ScriptSequence opened_indicator{ "Door Opened Indicator" };
	opened_indicator
		.Reentry(ReentryMode::Restart)
		.StartOn<Signal>(Signal{ SignalKey{ "door.opened" } })
		.StopOn<Signal>(Signal{ SignalKey{ "door.closed" } });
	opened_indicator
		.During(350.0f, MoveToAction{ { 0.0f, 55.0f }, true })
		.Ease(ptgn::Ease::OutBack)
		.End();
	const Id opened_indicator_id{ opened_indicator.id };
	shared_sequences_.sequences.push_back(std::move(opened_indicator));

	ScriptSequence closed_indicator{ "Door Closed Indicator" };
	closed_indicator
		.Reentry(ReentryMode::Restart)
		.StartOn<Signal>(Signal{ SignalKey{ "door.closed" } })
		.StopOn<Signal>(Signal{ SignalKey{ "door.opened" } });
	closed_indicator
		.During(350.0f, MoveToAction{ { 300.0f, -110.0f }, false })
		.Ease(ptgn::Ease::OutCubic)
		.End();
	const Id closed_indicator_id{ closed_indicator.id };
	shared_sequences_.sequences.push_back(std::move(closed_indicator));

	ptgn::Entity player{ CreateEntity("Player", "Player") };
	player.Get<ptgn::Transform>().position = { -330.0f, 0.0f };
	player.Add<ptgn::Rect>(ptgn::V2_float{ 38.0f, 38.0f });
	player.Get<DemoVisual>().color = ImVec4{ 0.27f, 0.59f, 0.96f, 1.0f };
	player.Add<ScriptsComponent>();
	{
		ScriptEntry script;
		script.type = std::string{ ScriptRegistry::Key<PlayerMovementScript>() };
		script.value = PlayerMovementScript{};
		player.Get<ScriptsComponent>().scripts.push_back(std::move(script));
	}

	ptgn::Entity sensor{ CreateEntity("Door Sensor", "Door") };
	sensor.Get<ptgn::Transform>().position = { -70.0f, 0.0f };
	sensor.Add<ptgn::Rect>(ptgn::V2_float{ 110.0f, 170.0f });
	sensor.Get<DemoVisual>().color = ImVec4{ 0.23f, 0.75f, 0.45f, 0.30f };
	sensor.Get<DemoVisual>().sensor = true;
	sensor.Add<ScriptsComponent>();

	// The sensor only emits Events. It never reaches across to mutate the panel.
	ScriptSequence sensor_enter_sequence{ "Emit Door Opened" };
	sensor_enter_sequence
		.Reentry(ReentryMode::Restart)
		.StartOn<ptgn::event::OverlapStart>(EntityMaskFilter{ .tags = "Player" })
		.EmitSignal(SignalKey{ "door.opened" });
	sensor.Get<ScriptsComponent>().sequences.push_back(std::move(sensor_enter_sequence));

	ScriptSequence sensor_exit_sequence{ "Emit Door Closed" };
	sensor_exit_sequence
		.Reentry(ReentryMode::Restart)
		.StartOn<ptgn::event::OverlapStop>(EntityMaskFilter{ .tags = "Player" })
		.EmitSignal(SignalKey{ "door.closed" });
	sensor.Get<ScriptsComponent>().sequences.push_back(std::move(sensor_exit_sequence));

	ptgn::Entity panel{ CreateEntity("Sliding Panel", "MovingPanel") };
	panel.Get<ptgn::Transform>().position = { 70.0f, 0.0f };
	panel.Add<ptgn::Rect>(ptgn::V2_float{ 62.0f, 170.0f });
	panel.Get<DemoVisual>().color = ImVec4{ 0.88f, 0.57f, 0.24f, 1.0f };
	panel.Add<ScriptsComponent>();

	// The panel owns the Actions that affect the panel.
	ScriptSequence open_sequence{ "Open Sliding Panel" };
	open_sequence
		.Reentry(ReentryMode::Restart)
		.StartOn<Signal>(Signal{ SignalKey{ "door.opened" } })
		.StopOn<Signal>(Signal{ SignalKey{ "door.closed" } });
	open_sequence
		.During(500.0f, MoveToAction{ { 225.0f, 0.0f }, false })
		.Ease(ptgn::Ease::OutCubic)
		.End();
	panel.Get<ScriptsComponent>().sequences.push_back(std::move(open_sequence));

	ScriptSequence close_sequence{ "Close Sliding Panel" };
	close_sequence
		.Reentry(ReentryMode::Restart)
		.StartOn<Signal>(Signal{ SignalKey{ "door.closed" } })
		.StopOn<Signal>(Signal{ SignalKey{ "door.opened" } });
	close_sequence
		.During(500.0f, MoveToAction{ { 70.0f, 0.0f }, false })
		.Ease(ptgn::Ease::OutCubic)
		.End();
	panel.Get<ScriptsComponent>().sequences.push_back(std::move(close_sequence));

	ptgn::Entity indicator{ CreateEntity("Event Indicator", "Indicator") };
	indicator.Get<ptgn::Transform>().position = { 300.0f, -110.0f };
	indicator.Add<ptgn::Circle>(21.0f);
	indicator.Get<DemoVisual>().color = ImVec4{ 0.92f, 0.80f, 0.27f, 1.0f };
	indicator.Add<ScriptsComponent>();
	{
		ScriptSequence opened_binding;
		opened_binding.shared_reference = true;
		opened_binding.shared_sequence_id = opened_indicator_id;
		indicator.Get<ScriptsComponent>().sequences.push_back(std::move(opened_binding));

		ScriptSequence closed_binding;
		closed_binding.shared_reference = true;
		closed_binding.shared_sequence_id = closed_indicator_id;
		indicator.Get<ScriptsComponent>().sequences.push_back(std::move(closed_binding));
	}

	ptgn::Entity factory{ CreateEntity("Circle Spawner", "Spawner") };
	factory.Get<ptgn::Transform>().position = { -265.0f, -165.0f };
	factory.Add<ptgn::Rect>(ptgn::V2_float{ 130.0f, 72.0f });
	factory.Get<DemoVisual>().color = ImVec4{ 0.47f, 0.41f, 0.63f, 0.55f };
	factory.Get<DemoVisual>().sensor = true;
	factory.Add<ScriptsComponent>();

	ScriptSequence spawn_sequence{ "Spawn Recall Circles" };
	spawn_sequence
		.Reentry(ReentryMode::IgnoreWhileRunning)
		.StartOn<ptgn::event::OverlapStart>(EntityMaskFilter{ .tags = "Player" })
		.Then(SpawnEntityAction{
			.prefab_key = "prefabs/recall_circle",
			.count = 10,
			.origin = SpawnOrigin::OwnerEntity,
			.area = SpawnArea::Circle,
			.center = { 0.0f, 95.0f },
			.radius = 92.0f,
			.random_rotation = true,
		});
	factory.Get<ScriptsComponent>().sequences.push_back(std::move(spawn_sequence));

	ScriptSequence recall_sequence{ "Recall Spawned Circles" };
	recall_sequence
		.Reentry(ReentryMode::Restart)
		.StartOn<ptgn::event::OverlapStop>(EntityMaskFilter{ .tags = "Player" })
		.EmitSignal(SignalKey{ "spawned_circles.destroy" });
	factory.Get<ScriptsComponent>().sequences.push_back(std::move(recall_sequence));

	ptgn::Entity damage_target{ CreateEntity("Damage Target", "DamageTarget") };
	damage_target.Get<ptgn::Transform>().position = { 315.0f, 105.0f };
	damage_target.Add<ptgn::Rect>(ptgn::V2_float{ 105.0f, 86.0f });
	damage_target.Get<DemoVisual>().color = ImVec4{ 0.78f, 0.27f, 0.29f, 1.0f };
	damage_target.Add<Health>(Health{ .maximum = 100.0f, .current = 100.0f });
	damage_target.Add<ScriptsComponent>();

	ScriptSequence damage_sequence{ "Overlap Damage Cooldown" };
	damage_sequence
		.Reentry(ReentryMode::IgnoreWhileRunning)
		.StartOn<ptgn::event::Overlap>(EntityMaskFilter{ .tags = "Player" })
		.Then(ApplyDamageAction{ .amount = 25.0f })
		.Wait(3000.0f);
	damage_target.Get<ScriptsComponent>().sequences.push_back(std::move(damage_sequence));

	// Two basic Buttons. The editor-side pointer hit test only pushes typed local Events;
	// ButtonScript owns the state machine and emits ButtonPress back through the local handler.
	ptgn::Entity tween_button{ CreateEntity("Tween Button", "TweenButton") };
	tween_button.Get<ptgn::Transform>().position = { -80.0f, -210.0f };
	tween_button.Add<ptgn::Rect>(ptgn::V2_float{ 150.0f, 50.0f });
	tween_button.Add<ButtonData>();
	tween_button.Add<ButtonStyle>();
	tween_button.Add<ScriptsComponent>();
	{
		ScriptEntry script;
		script.type = std::string{ ScriptRegistry::Key<ButtonScript>() };
		script.value = ButtonScript{};
		tween_button.Get<ScriptsComponent>().scripts.push_back(std::move(script));
	}
	ScriptSequence tween_button_sequence{ "Button Scale Pulse" };
	tween_button_sequence
		.Reentry(ReentryMode::Restart)
		.Channel(SequenceChannelKey{ "ui.press" })
		.StartOn<ButtonPress>(MouseButtonFilter{ ptgn::Mouse::Left });
	tween_button_sequence
		.During(105.0f, ScaleToAction{ { 1.12f, 1.12f }, false })
		.Ease(ptgn::Ease::OutBack)
		.Repeat(1)
		.Yoyo()
		.End()
		.EmitSignal(SignalKey{ "button.tween.clicked" });
	tween_button.Get<ScriptsComponent>().sequences.push_back(std::move(tween_button_sequence));

	ptgn::Entity signal_button{ CreateEntity("Global Event Button", "SignalButton") };
	signal_button.Get<ptgn::Transform>().position = { 115.0f, -210.0f };
	signal_button.Add<ptgn::Rect>(ptgn::V2_float{ 185.0f, 50.0f });
	signal_button.Add<ButtonData>();
	signal_button.Add<ButtonStyle>(ButtonStyle{
		.idle = ImVec4{ 0.47f, 0.29f, 0.62f, 1.0f },
		.hovered = ImVec4{ 0.61f, 0.39f, 0.78f, 1.0f },
		.pressed = ImVec4{ 0.35f, 0.20f, 0.49f, 1.0f },
	});
	signal_button.Add<ScriptsComponent>();
	{
		ScriptEntry script;
		script.type = std::string{ ScriptRegistry::Key<ButtonScript>() };
		script.value = ButtonScript{};
		signal_button.Get<ScriptsComponent>().scripts.push_back(std::move(script));
	}
	ScriptSequence signal_button_sequence{ "Emit Global Button Signal" };
	signal_button_sequence
		.Reentry(ReentryMode::Restart)
		.StartOn<ButtonPress>(MouseButtonFilter{ ptgn::Mouse::Left })
		.EmitSignal(SignalKey{ "button.global.clicked" });
	signal_button.Get<ScriptsComponent>().sequences.push_back(std::move(signal_button_sequence));

	ScriptSequence global_button_sequence{ "Global Button Indicator Spin" };
	global_button_sequence
		.Reentry(ReentryMode::Restart)
		.Channel(SequenceChannelKey{ "transform.rotation" })
		.StartOn<Signal>(Signal{ SignalKey{ "button.global.clicked" } });
	global_button_sequence
		.During(450.0f, RotateToAction{ 360.0f, false, true })
		.Ease(ptgn::Ease::OutBack)
		.End();
	indicator.Get<ScriptsComponent>().sequences.push_back(std::move(global_button_sequence));

	// ActionControlled example: the action decides when it has reached the Player.
	ptgn::Entity follower{ CreateEntity("Action-Controlled Follower", "Follower") };
	follower.Get<ptgn::Transform>().position = { 350.0f, 205.0f };
	follower.Add<ptgn::Circle>(15.0f);
	follower.Get<DemoVisual>().color = ImVec4{ 0.35f, 0.86f, 0.82f, 1.0f };
	follower.Add<ScriptsComponent>();
	ScriptSequence follow_sequence{ "Follow Player Until Reached" };
	follow_sequence
		.Reentry(ReentryMode::Restart)
		.Channel(SequenceChannelKey{ "movement.follow" })
		.StartOn<Signal>(Signal{ SignalKey{ "button.tween.clicked" } })
		.UntilComplete(FollowTargetAction{
			.target = player,
			.speed = 230.0f,
			.stopping_distance = 3.0f,
		});
	follower.Get<ScriptsComponent>().sequences.push_back(std::move(follow_sequence));

	manager_.Refresh();
	for (auto entity : entities_) {
		Queue(EventRegistry::MakeEvent<OnCreate>(
			OnCreate{ entity }, entity, entity
		));
	}
}

void DemoWorld::UpdateInputEvents(float delta_seconds) {
	std::vector<int> key_codes{
		GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_SPACE,
		GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT,
		GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT,
		GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL,
		GLFW_KEY_ENTER, GLFW_KEY_ESCAPE
	};

	const auto collect_condition_keys = [&](const EventCondition& condition) {
		const bool is_key_event{
			condition.type == EventRegistry::Key<ptgn::event::KeyPressed>() ||
			condition.type == EventRegistry::Key<ptgn::event::KeyHeld>() ||
			condition.type == EventRegistry::Key<ptgn::event::KeyReleased>()
		};
		if (!is_key_event || !condition.filter.Is<KeyListFilter>()) {
			return;
		}
		const auto parsed{
			ParseEnumExpression<ptgn::Key>(condition.filter.Get<KeyListFilter>().keys)
		};
		for (const auto& combination : parsed.alternatives) {
			for (const ptgn::Key key : combination) {
				const int key_code{ static_cast<int>(key) };
				if (!std::ranges::contains(key_codes, key_code)) {
					key_codes.push_back(key_code);
				}
			}
		}
	};

	for (const auto& record : entities_) {
		const auto* scripts{ record.TryGet<ScriptsComponent>() };
		if (!scripts) {
			continue;
		}
		for (const auto& binding : scripts->sequences) {
			const auto* sequence{ Resolve(binding) };
			if (!sequence) {
				continue;
			}
			for (const auto& condition : sequence->start_events) {
				collect_condition_keys(condition);
			}
			for (const auto& condition : sequence->stop_events) {
				collect_condition_keys(condition);
			}
		}
	}

	const float delta_ms{ std::max(0.0f, delta_seconds) * 1000.0f };
	for (const int key_code : key_codes) {
		const bool down{ glfwGetKey(window_, key_code) == GLFW_PRESS };
		const bool previous{ previous_key_states_[key_code] };
		auto& hold_duration{ key_hold_duration_ms_[key_code] };
		const auto key{ static_cast<ptgn::Key>(key_code) };

		if (down) {
			hold_duration = previous ? hold_duration + delta_ms : delta_ms;
		} else if (previous) {
			const float released_duration{ hold_duration };
			auto event{ EventRegistry::MakeEvent<ptgn::event::KeyReleased>(
				ptgn::event::KeyReleased{ key }, std::nullopt, {}, EventDelivery::Broadcast
			) };
			event.held_duration_ms = released_duration;
			Queue(std::move(event));
			hold_duration = 0.0f;
		}

		if (down && !previous) {
			auto event{ EventRegistry::MakeEvent<ptgn::event::KeyPressed>(
				ptgn::event::KeyPressed{ key }, std::nullopt, {}, EventDelivery::Broadcast
			) };
			event.held_duration_ms = hold_duration;
			Queue(std::move(event));
		}
		if (down) {
			auto event{ EventRegistry::MakeEvent<ptgn::event::KeyHeld>(
				ptgn::event::KeyHeld{ key }, std::nullopt, {}, EventDelivery::Broadcast
			) };
			event.held_duration_ms = hold_duration;
			Queue(std::move(event));
		}
		previous_key_states_[key_code] = down;
	}

	for (int button_code{ 0 }; button_code <= static_cast<int>(ptgn::Mouse::Last); ++button_code) {
		const bool down{ glfwGetMouseButton(window_, button_code) == GLFW_PRESS };
		const bool previous{ previous_mouse_states_[button_code] };
		auto& hold_duration{ mouse_hold_duration_ms_[button_code] };
		const auto button{ static_cast<ptgn::Mouse>(button_code) };

		if (down) {
			hold_duration = previous ? hold_duration + delta_ms : delta_ms;
		} else if (previous) {
			const float released_duration{ hold_duration };
			auto event{ EventRegistry::MakeEvent<ptgn::event::MouseReleased>(
				ptgn::event::MouseReleased{ button }, std::nullopt, {}, EventDelivery::Broadcast
			) };
			event.held_duration_ms = released_duration;
			Queue(std::move(event));
			hold_duration = 0.0f;
		}

		if (down && !previous) {
			auto event{ EventRegistry::MakeEvent<ptgn::event::MousePressed>(
				ptgn::event::MousePressed{ button }, std::nullopt, {}, EventDelivery::Broadcast
			) };
			event.held_duration_ms = hold_duration;
			Queue(std::move(event));
		}
		if (down) {
			auto event{ EventRegistry::MakeEvent<ptgn::event::MouseHeld>(
				ptgn::event::MouseHeld{ button }, std::nullopt, {}, EventDelivery::Broadcast
			) };
			event.held_duration_ms = hold_duration;
			Queue(std::move(event));
		}
		previous_mouse_states_[button_code] = down;
	}
}

void DemoWorld::UpdateButtonInteraction() {
	if (!pointer_frame_pending_) {
		return;
	}
	pointer_frame_pending_ = false;

	ptgn::Entity hovered;
	if (pointer_frame_.inside_scene) {
		for (auto it{ entities_.rbegin() }; it != entities_.rend(); ++it) {
			ptgn::Entity entity{ *it };
			if (!entity || !entity.Has<ButtonData>() || !entity.Has<ptgn::Transform>() ||
				!entity.Has<ptgn::Rect>() ||
				(entity.Has<ptgn::Visible>() && !entity.Get<ptgn::Visible>().visible)) {
				continue;
			}
			auto& button{ entity.Get<ButtonData>() };
			if (!button.enabled) {
				button.hovered = false;
				button.pressed = false;
				button.state = ButtonState::Disabled;
				continue;
			}
			if (button.state == ButtonState::Disabled) {
				button.state = ButtonState::Idle;
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
			Context().PushLocal<MouseMoveOut>(
				hovered_button_, MouseMoveOut{}, {}
			);
		}
		hovered_button_ = hovered;
		if (hovered_button_) {
			Context().PushLocal<MouseMoveOver>(
				hovered_button_, MouseMoveOver{}, {}
			);
		}
	}

	for (std::size_t i{ 0 }; i < kMouseButtons.size(); ++i) {
		const ptgn::Mouse mouse_button{ kMouseButtons[i] };

		if (pointer_frame_.pressed[i] && hovered_button_) {
			pressed_buttons_[i] = hovered_button_;
			Context().PushLocal<MousePressedOver>(
				pressed_buttons_[i], MousePressedOver{ mouse_button }, {}
			);
		}

		if (pointer_frame_.released[i] && pressed_buttons_[i]) {
			Context().PushLocal<MouseReleasedOver>(
				pressed_buttons_[i],
				MouseReleasedOver{
					.button = mouse_button,
					.released_over = pressed_buttons_[i] == hovered_button_,
				},
				{}
			);
			pressed_buttons_[i] = {};
		}
	}
}

void DemoWorld::OnBeforeScriptUpdate(float delta_seconds) {
	UpdateInputEvents(delta_seconds);
	UpdateOverlapEvents();
	UpdateButtonInteraction();
}

void DemoWorld::UpdateScriptSequences(float delta_seconds) {
	const auto sequence_entities{ entities_ };
	for (auto entity : sequence_entities) {
		if (auto* scripts{ entity.TryGet<ScriptsComponent>() }) {
			for (auto& sequence : scripts->sequences) {
				UpdateSequence(entity, sequence, delta_seconds);
			}
		}
	}
}

void DemoWorld::ApplyPendingScriptChanges() {
	const auto script_entities{ entities_ };
	for (auto entity : script_entities) {
		auto* scripts{ entity.TryGet<ScriptsComponent>() };
		if (!scripts) {
			continue;
		}

		for (const Id id : scripts->pending_script_removals) {
			std::erase_if(scripts->scripts, [id](const ScriptEntry& entry) {
				return entry.id == id;
			});
			std::erase_if(scripts->pending_script_additions, [id](const ScriptEntry& entry) {
				return entry.id == id;
			});
		}
		scripts->pending_script_removals.clear();

		for (const Id id : scripts->pending_sequence_removals) {
			auto it{ std::ranges::find_if(scripts->sequences, [id](const ScriptSequence& sequence) {
				return sequence.id == id;
			}) };
			if (it != scripts->sequences.end()) {
				(void)CancelBinding(
					entity, *it, SequenceCancelReason::BindingRemoved, false, true
				);
				scripts->sequences.erase(it);
			}
			std::erase_if(
				scripts->pending_sequence_additions,
				[id](const ScriptSequence& sequence) { return sequence.id == id; }
			);
			for (auto& channel : scripts->channels) {
				const bool released_active{ channel.active == id };
				if (released_active) {
					channel.active.reset();
				}
				std::erase(channel.waiting, id);
				if (released_active) {
					PromoteNextInChannel(entity, channel);
				}
			}
		}
		scripts->pending_sequence_removals.clear();

		std::ranges::move(
			scripts->pending_script_additions,
			std::back_inserter(scripts->scripts)
		);
		scripts->pending_script_additions.clear();

		std::ranges::move(
			scripts->pending_sequence_additions,
			std::back_inserter(scripts->sequences)
		);
		scripts->pending_sequence_additions.clear();
	}
}

void DemoWorld::DispatchBufferedEvents() {
	DispatchEvents();
}

void DemoWorld::OnAfterScriptUpdate(float delta_seconds) {
	ProcessPendingDestroy();
	for (auto& entry : activity_) {
		entry.remaining_seconds -= delta_seconds;
	}
	std::erase_if(activity_, [](const auto& entry) {
		return entry.remaining_seconds <= 0.0f;
	});
}

void DemoWorld::ProcessPendingDestroy() {
	if (pending_destroy_.empty()) {
		return;
	}
	for (ptgn::Entity entity : pending_destroy_) {
		if (!entity) {
			continue;
		}
		if (auto* scripts{ entity.TryGet<ScriptsComponent>() }) {
			for (auto& binding : scripts->sequences) {
				(void)CancelBinding(
					entity, binding, SequenceCancelReason::OwnerDestroyed, false, false
				);
			}
		}
		if (hovered_button_ == entity) {
			hovered_button_ = {};
		}
		for (auto& pressed_button : pressed_buttons_) {
			if (pressed_button == entity) {
				pressed_button = {};
			}
		}
		Log("Destroyed " + std::string{ Name(entity) });
		entity.Destroy();
	}
	std::erase_if(entities_, [](ptgn::Entity entity) {
		return !entity;
	});
	pending_destroy_.clear();
	manager_.Refresh();
}

void DemoWorld::UpdateResidentScripts(float delta_seconds) {
	const auto script_entities{ entities_ };
	for (auto entity : script_entities) {
		auto* scripts{ entity.TryGet<ScriptsComponent>() };
		if (!scripts) {
			continue;
		}
		for (auto& script : scripts->scripts) {
			if (!script.enabled) {
				continue;
			}
			const auto* registration{ ScriptRegistry::Find(script.type) };
			if (!registration) {
				continue;
			}
			if (!script.instance) {
				script.instance = registration->instantiate(script.value);
			}
			ScriptContext context{ .host = *this, .owner = entity, .scene = &Context(), .delta_seconds = delta_seconds };
			if (!script.created) {
				script.instance->OnCreate(context);
				script.created = true;
			}
			script.instance->OnUpdate(context);
		}
	}
}

bool DemoWorld::Overlap(ptgn::Entity a, ptgn::Entity b) const {
	if (!a || !b || !a.Has<ptgn::Transform>() || !b.Has<ptgn::Transform>() ||
		!a.Has<ptgn::Rect>() || !b.Has<ptgn::Rect>()) {
		return false;
	}
	const auto& transform_a{ a.Get<ptgn::Transform>() };
	const auto& transform_b{ b.Get<ptgn::Transform>() };
	const auto size_a{ a.Get<ptgn::Rect>().GetSize(transform_a) };
	const auto size_b{ b.Get<ptgn::Rect>().GetSize(transform_b) };
	return std::abs(transform_a.position.x - transform_b.position.x) <= (size_a.x + size_b.x) * 0.5f &&
		std::abs(transform_a.position.y - transform_b.position.y) <= (size_a.y + size_b.y) * 0.5f;
}

void DemoWorld::UpdateOverlapEvents() {
	const ptgn::Entity player{ FindByTag("Player") };
	if (!player) {
		return;
	}

	auto update_transition = [&](ptgn::Entity sensor, bool& previous, std::string_view label) {
		if (!sensor) {
			previous = false;
			return;
		}
		const bool overlapping{ Overlap(player, sensor) };
		if (overlapping != previous) {
			previous = overlapping;
			if (overlapping) {
				Queue(EventRegistry::MakeEvent<ptgn::event::OverlapStart>(
					ptgn::event::OverlapStart{ player }, sensor, sensor
				));
				Log("OverlapStart(" + std::string{ label } + ", Player)");
			} else {
				Queue(EventRegistry::MakeEvent<ptgn::event::OverlapStop>(
					ptgn::event::OverlapStop{ player }, sensor, sensor
				));
				Log("OverlapStop(" + std::string{ label } + ", Player)");
			}
		}
	};

	update_transition(FindByTag("Door"), player_overlapping_sensor_, "Door Sensor");
	update_transition(FindByTag("Spawner"), player_overlapping_spawner_, "Circle Spawner");

	const ptgn::Entity damage_target{ FindByTag("DamageTarget") };
	if (damage_target && Overlap(player, damage_target)) {
		Queue(EventRegistry::MakeEvent<ptgn::event::Overlap>(
			ptgn::event::Overlap{ player }, damage_target, damage_target
		));
	}
}

ScriptSequence* DemoWorld::FindBinding(ptgn::Entity owner, Id id) {
	if (!owner) {
		return nullptr;
	}
	auto* scripts{ owner.TryGet<ScriptsComponent>() };
	if (!scripts) {
		return nullptr;
	}
	auto find = [id](auto& sequences) -> ScriptSequence* {
		auto it{ std::ranges::find_if(sequences, [id](const ScriptSequence& sequence) {
			return sequence.id == id;
		}) };
		return it == sequences.end() ? nullptr : &*it;
	};
	if (auto* binding{ find(scripts->sequences) }) {
		return binding;
	}
	return find(scripts->pending_sequence_additions);
}

const ScriptSequence* DemoWorld::FindBinding(ptgn::Entity owner, Id id) const {
	return const_cast<DemoWorld*>(this)->FindBinding(owner, id);
}

SequenceChannelRuntime* DemoWorld::FindChannel(
	ScriptsComponent& scripts,
	const SequenceChannelKey& key
) {
	auto it{ std::ranges::find_if(scripts.channels, [&](const SequenceChannelRuntime& channel) {
		return channel.key == key;
	}) };
	return it == scripts.channels.end() ? nullptr : &*it;
}

SequenceChannelRuntime& DemoWorld::FindOrCreateChannel(
	ScriptsComponent& scripts,
	const SequenceChannelKey& key
) {
	if (auto* channel{ FindChannel(scripts, key) }) {
		return *channel;
	}
	scripts.channels.push_back(SequenceChannelRuntime{ .key = key });
	return scripts.channels.back();
}

bool DemoWorld::StartBinding(ptgn::Entity owner, ScriptSequence& binding, bool force) {
	const auto* sequence{ Resolve(binding) };
	if (!sequence || !binding.enabled || !sequence->enabled) {
		return false;
	}

	auto& runtime{ binding.runtime };
	if (runtime.running && !force) {
		switch (sequence->reentry) {
			case ReentryMode::IgnoreWhileRunning:
				return false;
			case ReentryMode::Restart:
				(void)CancelBinding(
					owner, binding, SequenceCancelReason::Replaced, false, false, false
				);
				break;
			case ReentryMode::Queue:
				++runtime.queued_runs;
				return true;
		}
	} else if (runtime.running && force) {
		(void)CancelBinding(
			owner, binding, SequenceCancelReason::Replaced, false, false, false
		);
	}

	if (sequence->channel) {
		auto& scripts{ owner.TryAdd<ScriptsComponent>() };
		auto& channel{ FindOrCreateChannel(scripts, *sequence->channel) };
		if (channel.active && *channel.active != binding.id) {
			const ReentryMode channel_reentry{
				force ? ReentryMode::Restart : sequence->reentry
			};
			switch (channel_reentry) {
				case ReentryMode::IgnoreWhileRunning:
					return false;
				case ReentryMode::Restart:
					StopSequenceChannel(owner, *sequence->channel, SequenceStopMode::All);
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
	runtime.pending_start_delay_ms = -1.0f;
	Log(std::string{ Name(owner) } + " / " + sequence->name + " started");
	InvokeLifecycle(owner, binding, SequenceLifecycle::Start);
	ProcessImmediateActions(owner, binding);
	return true;
}

bool DemoWorld::CancelBinding(
	ptgn::Entity owner,
	ScriptSequence& binding,
	SequenceCancelReason reason,
	bool log,
	bool promote_channel,
	bool remove_transient
) {
	const auto* sequence{ Resolve(binding) };
	const bool active{
		binding.runtime.running || binding.runtime.waiting_for_channel ||
		binding.runtime.pending_start_delay_ms >= 0.0f
	};
	if (binding.runtime.action_instance) {
		ActionContext context{ .host = *this, .owner = owner, .scene = &Context() };
		binding.runtime.action_instance->Cancel(context, reason);
		InvokeLifecycle(owner, binding, SequenceLifecycle::ActionCancel);
	}
	if (active) {
		InvokeLifecycle(owner, binding, SequenceLifecycle::Stop);
	}
	if (log && sequence && active) {
		Log(std::string{ Name(owner) } + " / " + sequence->name + " stopped");
	}
	ReleaseChannel(owner, binding, promote_channel);
	const int completed_runs{ binding.runtime.completed_runs };
	binding.runtime = ScriptSequenceRuntime{};
	binding.runtime.completed_runs = completed_runs;
	if (remove_transient && sequence &&
		(sequence->transient || sequence->remove_binding_on_complete) &&
		reason != SequenceCancelReason::Reset &&
		reason != SequenceCancelReason::BindingRemoved &&
		reason != SequenceCancelReason::OwnerDestroyed && owner.Has<ScriptsComponent>()) {
		owner.Get<ScriptsComponent>().RemoveSequenceDeferred(binding.id);
	}
	return active;
}

bool DemoWorld::SetBindingPaused(
	ptgn::Entity owner,
	ScriptSequence& binding,
	bool paused
) {
	if (!binding.runtime.running || binding.runtime.paused == paused) {
		return false;
	}
	binding.runtime.paused = paused;
	InvokeLifecycle(owner, binding, paused ? SequenceLifecycle::Pause : SequenceLifecycle::Resume);
	return true;
}

void DemoWorld::ReleaseChannel(
	ptgn::Entity owner,
	ScriptSequence& binding,
	bool promote
) {
	const auto* sequence{ Resolve(binding) };
	if (!sequence || !sequence->channel || !owner.Has<ScriptsComponent>()) {
		return;
	}
	auto& scripts{ owner.Get<ScriptsComponent>() };
	auto* channel{ FindChannel(scripts, *sequence->channel) };
	if (!channel) {
		return;
	}
	std::erase(channel->waiting, binding.id);
	if (channel->active == binding.id) {
		channel->active.reset();
		if (promote) {
			PromoteNextInChannel(owner, *channel);
		}
	}
}

void DemoWorld::PromoteNextInChannel(
	ptgn::Entity owner,
	SequenceChannelRuntime& channel
) {
	while (!channel.waiting.empty()) {
		const Id next_id{ channel.waiting.front() };
		channel.waiting.pop_front();
		auto* next{ FindBinding(owner, next_id) };
		if (!next) {
			continue;
		}
		channel.active = next_id;
		next->runtime.waiting_for_channel = false;
		(void)StartBinding(owner, *next, true);
		return;
	}
}

bool DemoWorld::Matches(
	const EventCondition& condition,
	const EventEnvelope& event,
	ptgn::Entity owner
) {
	if (!condition.enabled || condition.type != event.type) {
		return false;
	}
	const auto* registration{ EventRegistry::Find(condition.type) };
	if (!registration) {
		return false;
	}
	EventMatchContext context{ .host = *this, .owner = owner, .held_duration_ms = event.held_duration_ms };
	return registration->matches(event.payload, condition.filter, context);
}

void DemoWorld::DispatchEvents() {
	auto dispatch_to_entity = [&](ptgn::Entity entity, const EventEnvelope& event) {
		auto* scripts{ entity.TryGet<ScriptsComponent>() };
		if (!scripts) {
			return EventResult::Continue;
		}

		const EventView view{ event };
		for (auto& resident : scripts->scripts) {
			if (!resident.enabled) {
				continue;
			}
			const auto* registration{ ScriptRegistry::Find(resident.type) };
			if (!registration) {
				continue;
			}
			if (!resident.instance) {
				resident.instance = registration->instantiate(resident.value);
			}
			ScriptContext context{ .host = *this, .owner = entity, .scene = &Context() };
			if (!resident.created) {
				resident.instance->OnCreate(context);
				resident.created = true;
			}
			if (resident.instance->OnEvent(context, view) == EventResult::Handled) {
				return EventResult::Handled;
			}
		}

		for (auto& binding : scripts->sequences) {
			const auto* sequence{ Resolve(binding) };
			if (!sequence) {
				continue;
			}

			const auto stop_it{ std::ranges::find_if(sequence->stop_events, [&](const auto& condition) {
				return Matches(condition, event, entity);
			}) };
			if (stop_it != sequence->stop_events.end()) {
				Stop(entity, binding);
				if (stop_it->consume) {
					return EventResult::Handled;
				}
				continue;
			}

			const auto start_it{ std::ranges::find_if(sequence->start_events, [&](const auto& condition) {
				return Matches(condition, event, entity);
			}) };
			if (start_it == sequence->start_events.end()) {
				continue;
			}

			const auto* registration{ EventRegistry::Find(start_it->type) };
			const float delay_ms{
				registration && registration->start_delay_ms
					? registration->start_delay_ms(start_it->filter)
					: 0.0f
			};
			if (delay_ms > 0.0f && !binding.runtime.running) {
				if (binding.runtime.pending_start_delay_ms < 0.0f) {
					binding.runtime.pending_start_delay_ms = delay_ms;
				} else {
					switch (sequence->reentry) {
						case ReentryMode::IgnoreWhileRunning:
							break;
						case ReentryMode::Restart:
							binding.runtime.pending_start_delay_ms = delay_ms;
							break;
						case ReentryMode::Queue:
							++binding.runtime.queued_runs;
							break;
					}
				}
			} else {
				Start(entity, binding);
			}
			if (start_it->consume) {
				return EventResult::Handled;
			}
		}
		return EventResult::Continue;
	};

	while (!Context().local_events.Empty()) {
		EventEnvelope event{ Context().local_events.Pop() };
		if (event.target && *event.target) {
			dispatch_to_entity(*event.target, event);
		}
	}

	while (!Context().global_events.Empty()) {
		EventEnvelope event{ Context().global_events.Pop() };
		const auto event_entities{ entities_ };
		for (auto entity : event_entities) {
			if (dispatch_to_entity(entity, event) == EventResult::Handled) {
				break;
			}
		}
	}
}

void DemoWorld::ExecuteInstant(ptgn::Entity owner, const Action& action) {
	const auto* registration{ ActionRegistry::Find(action.type) };
	if (!registration || !action.enabled) {
		return;
	}
	auto instance{ registration->instantiate(action.value) };
	ActionContext context{
		.host = *this,
		.owner = owner,
		.scene = &Context(),
		.linear_progress = 1.0f,
		.progress = 1.0f,
	};
	instance->Begin(context);
	(void)instance->Update(context);
	instance->Complete(context);
}

void DemoWorld::InvokeLifecycle(
	ptgn::Entity owner,
	ScriptSequence& binding,
	SequenceLifecycle lifecycle
) {
	const auto* sequence{ Resolve(binding) };
	if (!sequence) {
		return;
	}
	for (const auto& callback : sequence->lifecycle_actions) {
		if (callback.enabled && callback.lifecycle == lifecycle) {
			ExecuteInstant(owner, callback.action);
		}
	}
}

void DemoWorld::ProcessImmediateActions(ptgn::Entity owner, ScriptSequence& binding) {
	const auto* sequence{ Resolve(binding) };
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

		const auto* registration{ ActionRegistry::Find(action.type) };
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

void DemoWorld::CompleteCurrentAction(ptgn::Entity owner, ScriptSequence& binding) {
	auto& runtime{ binding.runtime };
	if (runtime.action_instance) {
		ActionContext context{
			.host = *this,
			.owner = owner,
			.scene = &Context(),
			.linear_progress = 1.0f,
			.progress = 1.0f,
			.repeat = runtime.current_repeat,
			.reversed = runtime.currently_reversed,
		};
		runtime.action_instance->Complete(context);
	}
	InvokeLifecycle(owner, binding, SequenceLifecycle::ActionComplete);
	++runtime.action_index;
	runtime.ClearActiveAction();
	ProcessImmediateActions(owner, binding);
}

void DemoWorld::CompleteSequence(ptgn::Entity owner, ScriptSequence& binding) {
	const auto* sequence{ Resolve(binding) };
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
	Log(std::string{ Name(owner) } + " / " + sequence->name + " completed");

	// Re-triggers queued on this same binding run before another binding waiting in the channel.
	if (queued_runs > 0 && !destroy_owner) {
		binding.runtime.queued_runs = queued_runs - 1;
		(void)StartBinding(owner, binding, true);
		return;
	}

	ReleaseChannel(owner, binding, true);
	if (destroy_owner) {
		Destroy(owner);
		return;
	}
	if (remove_binding && owner.Has<ScriptsComponent>()) {
		owner.Get<ScriptsComponent>().RemoveSequenceDeferred(binding.id);
	}
}

void DemoWorld::UpdateSequence(
	ptgn::Entity owner,
	ScriptSequence& binding,
	float delta_seconds
) {
	auto& runtime{ binding.runtime };
	if (!runtime.running && runtime.pending_start_delay_ms >= 0.0f) {
		runtime.pending_start_delay_ms -= std::max(0.0f, delta_seconds) * 1000.0f;
		if (runtime.pending_start_delay_ms <= 0.0f) {
			runtime.pending_start_delay_ms = -1.0f;
			(void)StartBinding(owner, binding, false);
		}
		return;
	}
	if (!runtime.running || runtime.paused || runtime.waiting_for_channel) {
		return;
	}

	const auto* sequence{ Resolve(binding) };
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

	const auto* registration{ ActionRegistry::Find(action.type) };
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
		runtime.action_instance = registration->instantiate(action.value);
		runtime.currently_reversed = action.timing && action.timing->reversed;
		ActionContext context{
			.host = *this,
			.owner = owner,
			.scene = &Context(),
			.reversed = runtime.currently_reversed,
		};
		runtime.action_instance->Begin(context);
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

	ActionContext context{
		.host = *this,
		.owner = owner,
		.scene = &Context(),
		.delta_seconds = delta_seconds,
		.linear_progress = linear,
		.progress = progress,
		.repeat = runtime.current_repeat,
		.reversed = runtime.currently_reversed,
	};
	const ActionStatus status{ runtime.action_instance->Update(context) };

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

		ActionContext repeat_context{
			.host = *this,
			.owner = owner,
			.scene = &Context(),
			.repeat = runtime.current_repeat,
			.reversed = runtime.currently_reversed,
		};
		runtime.action_instance->Repeat(repeat_context);
		return;
	}

	CompleteCurrentAction(owner, binding);
}

class DemoApplication {
public:
	static void GlfwErrorCallback(int error, const char* description) {
		std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
	}

	int Run() {
		glfwSetErrorCallback(GlfwErrorCallback);
		if (!glfwInit()) {
			return 1;
		}

#if defined(__APPLE__)
		const char* glsl_version{ "#version 150" };
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
		const char* glsl_version{ "#version 330" };
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

		GLFWwindow* window{ glfwCreateWindow(
			1650, 950, "Protegon Registry Driven Script Sequences", nullptr, nullptr
		) };
		if (!window) {
			glfwTerminate();
			return 1;
		}
		glfwMakeContextCurrent(window);
		glfwSwapInterval(1);
		if (!gladLoadGL(glfwGetProcAddress)) {
			glfwDestroyWindow(window);
			glfwTerminate();
			return 1;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();
		ImGui::GetStyle().WindowRounding = 0.0f;
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init(glsl_version);

		DemoWorld world{ window };
		editor::DemoEditor editor{ world };

		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			world.Update(ImGui::GetIO().DeltaTime);
			editor.Draw();

			ImGui::Render();
			int width{ 0 };
			int height{ 0 };
			glfwGetFramebufferSize(window, &width, &height);
			glViewport(0, 0, width, height);
			glClearColor(0.055f, 0.060f, 0.070f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			glfwSwapBuffers(window);
		}

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		glfwDestroyWindow(window);
		glfwTerminate();
		return 0;
	}
};

} // namespace ptgn

int main() {
	return ptgn::DemoApplication{}.Run();
}
