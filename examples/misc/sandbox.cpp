// script_sequence_old_ui_registry_demo_v7.cpp
//
// Old compact ImGui UI rebuilt on static registry-driven Events + Scripts + Script Sequences.
//
// Architecture boundaries in this file:
//   script_sequence_demo::engine  - authored/runtime sequence data and static engine registries.
//   script_sequence_demo::editor  - static editor registries and compact Dear ImGui inspectors.
//   script_sequence_demo::demo    - replaceable demo world, scene setup, actions, and application loop.
//
// Important behavior rule:
//   Every registered Action affects its owning entity when applicable. Cross-entity behavior is
//   expressed by emitting an Event and attaching an Event-driven ScriptSequence to the other entity.
//
// The demo uses WASD movement and tests door Signals, random prefab spawning and recall,
// configurable input Events, and an overlap-driven damage/cooldown sequence.

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glad/gl.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <magic_enum/magic_enum.hpp>

#include "core/event/key_event.h"
#include "core/event/mouse_event.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/math/easing.h"
#include "core/util/strong_string.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/visible.h"
#include "runtime/physics/collision_event.h"

#include <algorithm>
#include <any>
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

namespace script_sequence_demo {

namespace engine {

using Id = std::uint64_t;

class IdGenerator {
public:
	static Id Next() {
		static Id next{ 1 };
		return next++;
	}
};

template <typename T>
inline constexpr unsigned char kTypeIdStorage{ 0 };

template <typename T>
[[nodiscard]] constexpr const void* TypeId() {
	return &kTypeIdStorage<std::remove_cvref_t<T>>;
}

enum class ReentryMode {
	IgnoreWhileRunning,
	Restart,
	Queue
};

enum class EventDelivery {
	Target,
	Broadcast
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
	std::any value;
};

struct EventCondition {
	Id id{ IdGenerator::Next() };
	bool enabled{ true };
	std::string type;
	std::any filter;
};

struct Action {
	Id id{ IdGenerator::Next() };
	bool enabled{ true };
	std::string type;
	std::any value;
	std::optional<ActionTiming> timing;
};

struct LifecycleAction {
	Id id{ IdGenerator::Next() };
	bool enabled{ true };
	SequenceLifecycle lifecycle{ SequenceLifecycle::Complete };
	Action action;
};

class RuntimeHost;

struct ActionContext {
	RuntimeHost& host;
	ptgn::Entity owner;
	float delta_seconds{ 0.0f };
	float linear_progress{ 0.0f };
	float progress{ 0.0f };
	int repeat{ 0 };
	bool reversed{ false };
};

struct ScriptContext {
	RuntimeHost& host;
	ptgn::Entity owner;
	float delta_seconds{ 0.0f };
};

class IActionInstance {
public:
	virtual ~IActionInstance() = default;
	virtual void Begin(ActionContext&) {}
	virtual void Update(ActionContext&) {}
	virtual void Complete(ActionContext&) {}
	virtual void Cancel(ActionContext&) {}
};

class IScriptInstance {
public:
	virtual ~IScriptInstance() = default;
	virtual void OnCreate(ScriptContext&) {}
	virtual void OnUpdate(ScriptContext&) {}
	virtual void OnEvent(ScriptContext&, std::string_view, const std::any&) {}
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

	void Update(ActionContext& context) override {
		if constexpr (requires(T& value) { value.OnUpdate(context); }) {
			value_.OnUpdate(context);
		}
	}

	void Complete(ActionContext& context) override {
		if constexpr (requires(T& value) { value.OnComplete(context); }) {
			value_.OnComplete(context);
		}
	}

	void Cancel(ActionContext& context) override {
		if constexpr (requires(T& value) { value.OnCancel(context); }) {
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

	void OnEvent(
		ScriptContext& context,
		std::string_view event_type,
		const std::any& payload
	) override {
		if constexpr (requires(T& value) { value.OnEvent(context, event_type, payload); }) {
			value_.OnEvent(context, event_type, payload);
		}
	}

private:
	T value_;
};

struct ComponentRegistration {
	const void* type_id{ nullptr };
	std::string key;
	bool is_empty{ false };
	std::function<std::any()> make_default;
	std::function<bool(ptgn::Entity)> has;
	std::function<void(ptgn::Entity, const std::any&)> set;
	std::function<void(ptgn::Entity)> remove;
	std::function<std::any(ptgn::Entity)> capture;
};

class ComponentRegistry {
public:
	template <typename T>
	static bool Register(std::string key) {
		auto& entries{ MutableEntries() };
		if (Find(key) || Find(TypeId<T>())) {
			return false;
		}
		entries.push_back(ComponentRegistration{
			.type_id = TypeId<T>(),
			.key = std::move(key),
			.is_empty = std::is_empty_v<T>,
			.make_default = [] { return std::any{ T{} }; },
			.has = [](ptgn::Entity entity) { return entity.Has<T>(); },
			.set = [](ptgn::Entity entity, const std::any& value) {
				entity.Add<T>(std::any_cast<const T&>(value));
			},
			.remove = [](ptgn::Entity entity) {
				if (entity.Has<T>()) {
					entity.Remove<T>();
				}
			},
			.capture = [](ptgn::Entity entity) -> std::any {
				return entity.Has<T>() ? std::any{ entity.Get<T>() } : std::any{ T{} };
			},
		});
		return true;
	}

	[[nodiscard]] static const ComponentRegistration* Find(std::string_view key) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [key](const auto& entry) {
			return entry.key == key;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const ComponentRegistration* Find(const void* type_id) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [type_id](const auto& entry) {
			return entry.type_id == type_id;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const std::vector<ComponentRegistration>& Entries() {
		return MutableEntries();
	}

	[[nodiscard]] static ComponentDefinition Make(std::string_view key) {
		const auto* entry{ Find(key) };
		return entry ? ComponentDefinition{
			.id = IdGenerator::Next(),
			.type = entry->key,
			.value = entry->make_default(),
		} : ComponentDefinition{};
	}

private:
	[[nodiscard]] static std::vector<ComponentRegistration>& MutableEntries() {
		static std::vector<ComponentRegistration> entries;
		return entries;
	}
};

struct EventMatchContext {
	RuntimeHost& host;
	ptgn::Entity owner;
	float held_duration_ms{ 0.0f };
};

struct EventEnvelope {
	std::string type;
	std::any payload;
	std::optional<ptgn::Entity> target;
	ptgn::Entity source;
	EventDelivery delivery{ EventDelivery::Target };
	float held_duration_ms{ 0.0f };
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
};

struct EventRegistration {
	const void* type_id{ nullptr };
	std::string key;
	std::function<std::any()> make_default_filter;
	std::function<std::any()> make_default_payload;
	std::function<bool(const std::any&, const std::any&, EventMatchContext&)> matches;
	std::function<float(const std::any&)> start_delay_ms;
};

class EventRegistry {
public:
	template <typename TEvent, typename TFilter, typename F>
	static bool Register(std::string key, F&& matches) {
		auto& entries{ MutableEntries() };
		if (Find(key) || Find(TypeId<TEvent>())) {
			return false;
		}
		entries.push_back(EventRegistration{
			.type_id = TypeId<TEvent>(),
			.key = std::move(key),
			.make_default_filter = [] { return std::any{ TFilter{} }; },
			.make_default_payload = [] { return std::any{ TEvent{} }; },
			.matches = [fn = std::forward<F>(matches)](
				const std::any& payload,
				const std::any& filter,
				EventMatchContext& context
			) mutable {
				return std::invoke(
					fn,
					std::any_cast<const TEvent&>(payload),
					std::any_cast<const TFilter&>(filter),
					context
				);
			},
			.start_delay_ms = [](const std::any& filter) {
				if constexpr (requires(const TFilter& value) { value.delay_ms; }) {
					return std::max(0.0f, std::any_cast<const TFilter&>(filter).delay_ms);
				}
				return 0.0f;
			},
		});
		return true;
	}

	template <typename TEvent>
	[[nodiscard]] static std::string_view Key() {
		const auto* entry{ Find(TypeId<TEvent>()) };
		return entry ? std::string_view{ entry->key } : std::string_view{};
	}

	template <typename TEvent, typename TFilter>
	[[nodiscard]] static EventCondition MakeCondition(TFilter filter = {}) {
		const auto* entry{ Find(TypeId<TEvent>()) };
		return entry ? EventCondition{
			.id = IdGenerator::Next(),
			.enabled = true,
			.type = entry->key,
			.filter = std::any{ std::move(filter) },
		} : EventCondition{};
	}

	template <typename TEvent>
	[[nodiscard]] static EventEnvelope MakeEvent(
		TEvent payload = {},
		std::optional<ptgn::Entity> target = std::nullopt,
		ptgn::Entity source = {},
		EventDelivery delivery = EventDelivery::Target
	) {
		const auto* entry{ Find(TypeId<TEvent>()) };
		return entry ? EventEnvelope{
			.type = entry->key,
			.payload = std::any{ std::move(payload) },
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

	[[nodiscard]] static const EventRegistration* Find(const void* type_id) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [type_id](const auto& entry) {
			return entry.type_id == type_id;
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

struct ActionRegistration {
	const void* type_id{ nullptr };
	std::string key;
	bool supports_timing{ false };
	bool requires_timing{ false };
	std::optional<ActionTiming> default_timing;
	std::function<std::any()> make_default;
	std::function<std::unique_ptr<IActionInstance>(const std::any&)> instantiate;
};

class ActionRegistry {
public:
	template <typename T>
	static bool Register(
		std::string key,
		bool supports_timing = false,
		bool requires_timing = false,
		std::optional<ActionTiming> default_timing = std::nullopt
	) {
		auto& entries{ MutableEntries() };
		if (Find(key) || Find(TypeId<T>())) {
			return false;
		}
		entries.push_back(ActionRegistration{
			.type_id = TypeId<T>(),
			.key = std::move(key),
			.supports_timing = supports_timing,
			.requires_timing = requires_timing,
			.default_timing = default_timing,
			.make_default = [] { return std::any{ T{} }; },
			.instantiate = [](const std::any& value) {
				return std::make_unique<TypedActionInstance<T>>(
					std::any_cast<const T&>(value)
				);
			},
		});
		return true;
	}

	template <typename T>
	[[nodiscard]] static std::string_view Key() {
		const auto* entry{ Find(TypeId<T>()) };
		return entry ? std::string_view{ entry->key } : std::string_view{};
	}

	template <typename T>
		requires (!std::convertible_to<std::remove_cvref_t<T>, std::string_view>)
	[[nodiscard]] static Action Make(T value = {}) {
		const auto* entry{ Find(TypeId<T>()) };
		return entry ? Action{
			.id = IdGenerator::Next(),
			.enabled = true,
			.type = entry->key,
			.value = std::any{ std::move(value) },
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

	[[nodiscard]] static const ActionRegistration* Find(const void* type_id) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [type_id](const auto& entry) {
			return entry.type_id == type_id;
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
	const void* type_id{ nullptr };
	std::string key;
	std::function<std::any()> make_default;
	std::function<std::unique_ptr<IScriptInstance>(const std::any&)> instantiate;
};

class ScriptRegistry {
public:
	template <typename T>
	static bool Register(std::string key) {
		auto& entries{ MutableEntries() };
		if (Find(key) || Find(TypeId<T>())) {
			return false;
		}
		entries.push_back(ScriptRegistration{
			.type_id = TypeId<T>(),
			.key = std::move(key),
			.make_default = [] { return std::any{ T{} }; },
			.instantiate = [](const std::any& value) {
				return std::make_unique<TypedScriptInstance<T>>(
					std::any_cast<const T&>(value)
				);
			},
		});
		return true;
	}

	template <typename T>
	[[nodiscard]] static std::string_view Key() {
		const auto* entry{ Find(TypeId<T>()) };
		return entry ? std::string_view{ entry->key } : std::string_view{};
	}

	[[nodiscard]] static const ScriptRegistration* Find(std::string_view key) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [key](const auto& entry) {
			return entry.key == key;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const ScriptRegistration* Find(const void* type_id) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [type_id](const auto& entry) {
			return entry.type_id == type_id;
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
	std::any value;

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

struct ScriptSequenceRuntime {
	bool running{ false };
	bool paused{ false };
	bool completed{ false };
	bool queued{ false };
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
	bool destroy_on_complete{ false };
	std::vector<EventCondition> start_events;
	std::vector<EventCondition> stop_events;
	std::vector<Action> actions;
	std::vector<LifecycleAction> lifecycle_actions;
	ScriptSequenceRuntime runtime;

	ScriptSequence() = default;
	ScriptSequence(ScriptSequence&&) noexcept = default;
	ScriptSequence& operator=(ScriptSequence&&) noexcept = default;

	ScriptSequence(const ScriptSequence& other) :
		id{ IdGenerator::Next() },
		enabled{ other.enabled },
		shared_reference{ other.shared_reference },
		shared_sequence_id{ other.shared_sequence_id },
		name{ other.name },
		reentry{ other.reentry },
		destroy_on_complete{ other.destroy_on_complete },
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
};

struct ScriptsComponent {
	std::vector<ScriptEntry> scripts;
	std::vector<ScriptSequence> sequences;

	ScriptsComponent() = default;
	ScriptsComponent(const ScriptsComponent&) = default;
	ScriptsComponent& operator=(const ScriptsComponent&) = default;
	ScriptsComponent(ScriptsComponent&&) noexcept = default;
	ScriptsComponent& operator=(ScriptsComponent&&) noexcept = default;
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

struct SignalKey : ptgn::StrongString<SignalKey> {
	using StrongString::StrongString;
	SignalKey() = default;
};

struct Signal {
	SignalKey key;
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

private:
	ptgn::V2_float start_{};
	ptgn::V2_float end_{};
};

struct RotateToAction {
	float degrees{ 90.0f };
	bool shortest_path{ true };

	void OnStart(ActionContext& context);
	void OnUpdate(ActionContext& context);

private:
	float start_degrees_{ 0.0f };
	float delta_degrees_{ 0.0f };
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

// Fluent builder over the same ScriptSequence data edited by the UI.
class ScriptSequenceBuilder;

class TimedActionBuilder {
public:
	TimedActionBuilder& Ease(ptgn::Ease ease);
	TimedActionBuilder& Repeat(int additional_repeats);
	TimedActionBuilder& Infinite();
	TimedActionBuilder& Reversed(bool reversed = true);
	TimedActionBuilder& Yoyo(bool yoyo = true);
	ScriptSequenceBuilder& End();

private:
	friend class ScriptSequenceBuilder;
	TimedActionBuilder(ScriptSequenceBuilder& parent, Id action_id) :
		parent_{ &parent }, action_id_{ action_id } {}

	ActionTiming& Timing();
	ScriptSequenceBuilder* parent_{ nullptr };
	Id action_id_{ 0 };
};

class ScriptSequenceBuilder {
public:
	explicit ScriptSequenceBuilder(std::string name) {
		sequence_.name = std::move(name);
	}

	ScriptSequenceBuilder& Reentry(ReentryMode reentry) {
		sequence_.reentry = reentry;
		return *this;
	}

	template <typename TEvent, typename TFilter = NoEventFilter>
	ScriptSequenceBuilder& StartOn(TFilter filter = {}) {
		sequence_.start_events.push_back(
			EventRegistry::MakeCondition<TEvent>(std::move(filter))
		);
		return *this;
	}

	template <typename TEvent, typename TFilter = NoEventFilter>
	ScriptSequenceBuilder& StopOn(TFilter filter = {}) {
		sequence_.stop_events.push_back(
			EventRegistry::MakeCondition<TEvent>(std::move(filter))
		);
		return *this;
	}

	template <typename TAction>
	ScriptSequenceBuilder& Then(TAction action = {}) {
		sequence_.actions.push_back(ActionRegistry::Make<TAction>(std::move(action)));
		return *this;
	}

	template <typename TAction>
	TimedActionBuilder During(float duration_ms, TAction action = {}) {
		Action definition{ ActionRegistry::Make<TAction>(std::move(action)) };
		definition.timing = definition.timing.value_or(ActionTiming{});
		definition.timing->duration_ms = std::max(0.0f, duration_ms);
		const Id id{ definition.id };
		sequence_.actions.push_back(std::move(definition));
		return TimedActionBuilder{ *this, id };
	}

	ScriptSequenceBuilder& Wait(float duration_ms) {
		Action action{ ActionRegistry::Make<WaitAction>() };
		action.timing = action.timing.value_or(ActionTiming{});
		action.timing->duration_ms = std::max(0.0f, duration_ms);
		sequence_.actions.push_back(std::move(action));
		return *this;
	}

	ScriptSequenceBuilder& EmitSignal(SignalKey signal) {
		return Then(EmitSignalAction{ .signal = std::move(signal) });
	}

	ScriptSequence Build() {
		return std::move(sequence_);
	}

private:
	friend class TimedActionBuilder;

	ActionTiming& FindTiming(Id action_id) {
		auto it{ std::ranges::find_if(sequence_.actions, [action_id](const auto& action) {
			return action.id == action_id;
		}) };
		if (it == sequence_.actions.end() || !it->timing) {
			std::abort();
		}
		return *it->timing;
	}

	ScriptSequence sequence_;
};

ActionTiming& TimedActionBuilder::Timing() {
	return parent_->FindTiming(action_id_);
}

TimedActionBuilder& TimedActionBuilder::Ease(ptgn::Ease ease) {
	Timing().ease = ease;
	return *this;
}

TimedActionBuilder& TimedActionBuilder::Repeat(int additional_repeats) {
	Timing().additional_repeats = std::max(0, additional_repeats);
	Timing().infinite_repeats = false;
	return *this;
}

TimedActionBuilder& TimedActionBuilder::Infinite() {
	Timing().infinite_repeats = true;
	return *this;
}

TimedActionBuilder& TimedActionBuilder::Reversed(bool reversed) {
	Timing().reversed = reversed;
	return *this;
}

TimedActionBuilder& TimedActionBuilder::Yoyo(bool yoyo) {
	Timing().yoyo = yoyo;
	return *this;
}

ScriptSequenceBuilder& TimedActionBuilder::End() {
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

void RotateToAction::OnStart(ActionContext& context) {
	start_degrees_ = context.owner.Get<ptgn::Transform>().rotation.ToDeg().value;
	delta_degrees_ = degrees - start_degrees_;
	if (shortest_path) {
		delta_degrees_ = std::remainder(delta_degrees_, 360.0f);
	}
}

void RotateToAction::OnUpdate(ActionContext& context) {
	const float value{ start_degrees_ + delta_degrees_ * context.progress };
	context.owner.Get<ptgn::Transform>().rotation = ptgn::Degrees{ value }.ToRad();
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
	context.host.Queue(EventRegistry::MakeEvent<Signal>(
		Signal{ signal }, std::nullopt, context.owner, EventDelivery::Broadcast
	));
}

void AddComponentsAction::OnStart(ActionContext& context) {
	for (const auto& component : components) {
		if (const auto* registration{ ComponentRegistry::Find(component.type) }) {
			registration->set(context.owner, component.value);
		}
	}
}

void RemoveComponentsAction::OnStart(ActionContext& context) {
	for (const auto& key : components) {
		if (const auto* registration{ ComponentRegistry::Find(key) }) {
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

} // namespace engine

namespace editor {

struct EditorContext;

struct EditorVisual {
	ImVec4 color{ 0.35f, 0.43f, 0.57f, 1.0f };
	bool sensor{ false };
	float health_fraction{ -1.0f };
};

using namespace script_sequence_demo::engine;

struct ComponentEditorOptions {
	std::string label;
	std::string group;
	std::string description;
};

struct ComponentEditorRegistration {
	const void* type_id{ nullptr };
	ComponentEditorOptions options;
	std::function<bool(std::any&)> draw;
};

class ComponentEditorRegistry {
public:
	template <typename T, typename F>
	static bool Register(ComponentEditorOptions options, F&& draw) {
		auto& entries{ MutableEntries() };
		if (Find(TypeId<T>())) {
			return false;
		}
		entries.push_back(ComponentEditorRegistration{
			.type_id = TypeId<T>(),
			.options = std::move(options),
			.draw = [fn = std::forward<F>(draw)](std::any& value) mutable {
				return std::invoke(fn, std::any_cast<T&>(value));
			},
		});
		return true;
	}

	[[nodiscard]] static const ComponentEditorRegistration* Find(const void* type_id) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [type_id](const auto& entry) {
			return entry.type_id == type_id;
		}) };
		return it == entries.end() ? nullptr : &*it;
	}

	[[nodiscard]] static const std::vector<ComponentEditorRegistration>& Entries() {
		return MutableEntries();
	}

private:
	[[nodiscard]] static std::vector<ComponentEditorRegistration>& MutableEntries() {
		static std::vector<ComponentEditorRegistration> entries;
		return entries;
	}
};

using namespace script_sequence_demo::engine;

struct EventEditorOptions {
	std::string label;
	std::string group;
	std::string description;
};

struct EventEditorRegistration {
	std::string key;
	EventEditorOptions options;
	std::function<bool(std::any&)> draw_filter;
	std::function<bool(std::any&)> draw_payload;
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
			.draw_filter = [fn = std::forward<FFilter>(draw_filter)](std::any& value) mutable {
				return std::invoke(fn, std::any_cast<TFilter&>(value));
			},
			.draw_payload = [fn = std::forward<FPayload>(draw_payload)](std::any& value) mutable {
				return std::invoke(fn, std::any_cast<TEvent&>(value));
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

using namespace script_sequence_demo::engine;

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
	std::function<bool(std::any&, EditorContext&)> draw;
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
			.draw = [fn = std::forward<F>(draw)](
				std::any& value,
				EditorContext& context
			) mutable {
				return std::invoke(fn, std::any_cast<T&>(value), context);
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

using namespace script_sequence_demo::engine;

struct ScriptEditorOptions {
	std::string label;
	std::string group;
	std::string description;
};

struct ScriptEditorRegistration {
	std::string key;
	ScriptEditorOptions options;
	std::function<bool(std::any&)> draw;
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
			.draw = [fn = std::forward<F>(draw)](std::any& value) mutable {
				return std::invoke(fn, std::any_cast<T&>(value));
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
	virtual void RegisterEditorExtensions() = 0;
};

struct EditorContext {
	EditorHost& host;
	PrefabRegistry& prefabs;
};

using namespace script_sequence_demo::engine;
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
		TimedAction,
		Wait,
		EmitSignal
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
		"On Resume", "On Action Start", "On Action Complete", "On Repeat", "On Yoyo"
	};
	static constexpr std::array kActionFormLabels{
		"Action", "Timed Action", "Wait", "Emit Signal"
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
	static ActionForm GetActionForm(const Action& action);
	static void SetActionForm(Action& action, ActionForm form);
	static std::string ActionSummary(const Action& action);
	static void MoveAction(std::vector<Action>& actions, int from, int to);

	void DrawSidebar();
	void DrawScene();
	void DrawInspector();
	void DrawEntityComponents(ptgn::Entity entity);
	void DrawScripts(ptgn::Entity entity, ScriptsComponent& scripts);
	void DrawResidentScripts(ScriptsComponent& scripts);
	bool DrawSequence(ptgn::Entity owner, ScriptSequence& binding);
	void DrawRuntimeButtons(ptgn::Entity owner, ScriptSequence& binding);
	void DrawEventSection(
		const char* id, const char* label, std::vector<EventCondition>& events, bool stop_events
	);
	bool DrawEvent(EventCondition& event, bool stop_event);
	void DrawActions(ScriptSequence& sequence, ScriptSequence& binding);
	bool DrawAction(
		Action& action, int index, ScriptSequence* binding, bool& duplicate,
		int& move_from, int& move_to, bool lifecycle = false
	);
	void DrawActionPicker(Action& action, bool timed_only);
	void DrawActionParameters(Action& action, float left_screen_x);
	void DrawTimingOptions(ActionTiming& timing, float left_screen_x);
	void DrawEmitSignalCompact(EmitSignalAction& emit);
	void DrawLifecycle(ScriptSequence& sequence);
	void DrawComponentDefinition(ComponentDefinition& component, bool removable, int* remove_index = nullptr, int index = -1);
	void DrawPrefabs();
	void DrawPrefabInspector(PrefabDefinition& prefab);
	void DrawActivity();
	void PromoteToShared(ScriptSequence& binding);
	void DetachToLocal(ScriptSequence& binding);
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

float DemoEditor::GetCountControlWidth(const char* label) {
	constexpr float button_width{ 22.0f };
	constexpr float spacing{ 3.0f };
	const std::string widest{ std::string{ label } + ": 100" };
	return ImGui::CalcTextSize(widest.c_str()).x + button_width * 2.0f + spacing * 3.0f;
}

void DemoEditor::DrawCountControl(
	const char* label, int& value, int minimum, int maximum, bool disabled, const char* tooltip
) {
	value = std::clamp(value, minimum, maximum);
	constexpr float button_width{ 22.0f };
	constexpr float spacing{ 3.0f };
	const std::string widest{ std::string{ label } + ": 100" };
	const float text_width{ ImGui::CalcTextSize(widest.c_str()).x };
	const float start_x{ ImGui::GetCursorScreenPos().x };

	ImGui::PushID(label);
	ImGui::BeginDisabled(disabled);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s: %d", label, value);
	DrawItemTooltip(tooltip);
	ImGui::SameLine();
	ImGui::SetCursorScreenPos(ImVec2{ start_x + text_width + spacing, ImGui::GetCursorScreenPos().y });
	ImGui::BeginDisabled(value >= maximum);
	if (ImGui::Button("+", ImVec2{ button_width, 0.0f })) {
		++value;
	}
	ImGui::EndDisabled();
	ImGui::SameLine(0.0f, spacing);
	ImGui::BeginDisabled(value <= minimum);
	if (ImGui::Button("-", ImVec2{ button_width, 0.0f })) {
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

DemoEditor::ActionForm DemoEditor::GetActionForm(const Action& action) {
	if (action.type == ActionRegistry::Key<WaitAction>()) {
		return ActionForm::Wait;
	}
	if (action.type == ActionRegistry::Key<EmitSignalAction>()) {
		return ActionForm::EmitSignal;
	}
	return action.timing ? ActionForm::TimedAction : ActionForm::Action;
}

void DemoEditor::SetActionForm(Action& action, ActionForm form) {
	const Id id{ action.id };
	const bool enabled{ action.enabled };
	const auto* registration{ ActionRegistry::Find(action.type) };

	switch (form) {
		case ActionForm::Action:
			if (!registration || registration->requires_timing ||
				action.type == ActionRegistry::Key<WaitAction>() ||
				action.type == ActionRegistry::Key<EmitSignalAction>()) {
				action = ActionRegistry::Make<SetVisibleAction>();
			}
			action.timing.reset();
			break;
		case ActionForm::TimedAction:
			if (!registration || !registration->supports_timing ||
				action.type == ActionRegistry::Key<WaitAction>()) {
				action = ActionRegistry::Make<MoveToAction>();
			}
			registration = ActionRegistry::Find(action.type);
			action.timing = registration && registration->default_timing
				? registration->default_timing
				: std::optional<ActionTiming>{ ActionTiming{} };
			break;
		case ActionForm::Wait:
			action = ActionRegistry::Make<WaitAction>();
			break;
		case ActionForm::EmitSignal:
			action = ActionRegistry::Make<EmitSignalAction>();
			break;
	}

	action.id = id;
	action.enabled = enabled;
}

std::string DemoEditor::ActionSummary(const Action& action) {
	const auto* editor{ ActionEditorRegistry::Find(action.type) };
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

void DemoEditor::RegisterEditorTypes() {
	ComponentEditorRegistry::Register<ptgn::Transform>(
		{ .label = "Transform", .group = "Core", .description = "Position, rotation, and scale." },
		[](ptgn::Transform& transform) {
			bool changed{ ImGui::DragFloat2("Position", &transform.position.x, 1.0f) };
			float degrees{ transform.rotation.ToDeg().value };
			if (ImGui::DragFloat("Rotation", &degrees, 1.0f, -3600.0f, 3600.0f, "%.1f deg")) {
				transform.rotation = ptgn::Degrees{ degrees }.ToRad();
				changed = true;
			}
			changed |= ImGui::DragFloat2("Scale", &transform.scale.x, 0.01f, 0.001f, 10000.0f);
			return changed;
		}
	);
	ComponentEditorRegistry::Register<ptgn::Visible>(
		{ .label = "Visible", .group = "Core", .description = "Controls entity visibility." },
		[](ptgn::Visible& visible) { return ImGui::Checkbox("Visible", &visible.visible); }
	);
	ComponentEditorRegistry::Register<ptgn::Rect>(
		{ .label = "Rectangle", .group = "Shape", .description = "Rectangle geometry." },
		[](ptgn::Rect& rect) {
			auto size{ rect.GetSize() };
			if (!ImGui::DragFloat2("Size", &size.x, 1.0f, 1.0f, 2000.0f)) {
				return false;
			}
			size.x = std::max(1.0f, size.x);
			size.y = std::max(1.0f, size.y);
			rect = ptgn::Rect{ size };
			return true;
		}
	);
	ComponentEditorRegistry::Register<ptgn::Circle>(
		{ .label = "Circle", .group = "Shape", .description = "Circle geometry." },
		[](ptgn::Circle& circle) {
			return ImGui::DragFloat("Radius", &circle.radius, 1.0f, 1.0f, 1000.0f);
		}
	);

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
		return draw_entity_filter(filter, "Tags", "Masks");
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

	EventEditorRegistry::Register<OnCreateFilter, OnCreate>(
		"ptgn.event.OnCreate",
		{ .label = "On Create", .group = "", .description = "Fired after the owning entity is created." },
		draw_on_create_filter, draw_runtime_payload
	);
	EventEditorRegistry::Register<Signal, Signal>(
		"ptgn.event.Signal",
		{ .label = "Signal", .group = "", .description = "Data-driven broadcast event identified by a strong string key." },
		draw_signal, draw_signal
	);
	EventEditorRegistry::Register<KeyListFilter, ptgn::event::KeyPressed>(
		"ptgn.event.KeyPressed",
		{ .label = "Key Pressed", .group = "Key", .description = "Fired on the first pressed frame." },
		draw_key_filter, draw_key_payload
	);
	EventEditorRegistry::Register<KeyListFilter, ptgn::event::KeyHeld>(
		"ptgn.event.KeyHeld",
		{ .label = "Key Held", .group = "Key", .description = "Fired after a key combination remains held." },
		draw_key_held_filter, draw_key_payload
	);
	EventEditorRegistry::Register<KeyListFilter, ptgn::event::KeyReleased>(
		"ptgn.event.KeyReleased",
		{ .label = "Key Released", .group = "Key", .description = "Fired when a key is released." },
		draw_key_filter, draw_key_payload
	);
	EventEditorRegistry::Register<MouseListFilter, ptgn::event::MousePressed>(
		"ptgn.event.MousePressed",
		{ .label = "Mouse Pressed", .group = "Mouse", .description = "Fired on the first pressed frame." },
		draw_mouse_filter, draw_mouse_payload
	);
	EventEditorRegistry::Register<MouseListFilter, ptgn::event::MouseHeld>(
		"ptgn.event.MouseHeld",
		{ .label = "Mouse Held", .group = "Mouse", .description = "Fired after a mouse-button combination remains held." },
		draw_mouse_held_filter, draw_mouse_payload
	);
	EventEditorRegistry::Register<MouseListFilter, ptgn::event::MouseReleased>(
		"ptgn.event.MouseReleased",
		{ .label = "Mouse Released", .group = "Mouse", .description = "Fired when a mouse button is released." },
		draw_mouse_filter, draw_mouse_payload
	);
	EventEditorRegistry::Register<EntityMaskFilter, ptgn::event::OverlapStart>(
		"ptgn.event.OverlapStart",
		{ .label = "Overlap Start", .group = "Overlap", .description = "Fired when an overlap begins." },
		draw_overlap_filter, draw_runtime_payload
	);
	EventEditorRegistry::Register<EntityMaskFilter, ptgn::event::Overlap>(
		"ptgn.event.Overlap",
		{ .label = "Overlap", .group = "Overlap", .description = "Fired while an overlap continues." },
		draw_overlap_filter, draw_runtime_payload
	);
	EventEditorRegistry::Register<EntityMaskFilter, ptgn::event::OverlapStop>(
		"ptgn.event.OverlapStop",
		{ .label = "Overlap Stop", .group = "Overlap", .description = "Fired when an overlap ends." },
		draw_overlap_filter, draw_runtime_payload
	);
	EventEditorRegistry::Register<EntityMaskFilter, ptgn::event::Collision>(
		"ptgn.event.Collision",
		{ .label = "Collision", .group = "Collision", .description = "Fired for a collision." },
		draw_collision_filter, draw_runtime_payload
	);

	ActionEditorRegistry::Register<WaitAction>(
		"engine.wait",
		{ .label = "Wait", .group = "Timing", .description = "Wait without modifying the owner." },
		[](WaitAction&, EditorContext&) { return false; }
	);
	ActionEditorRegistry::Register<MoveToAction>(
		"engine.move_to",
		{ .label = "Move To", .group = "Transform", .description = "Move the owning entity." },
		[](MoveToAction& action, EditorContext&) {
			if (!ImGui::BeginTable("MoveToParams", 3, ImGuiTableFlags_SizingStretchProp)) {
				return false;
			}
			bool changed{ false };
			ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Relative", ImGuiTableColumnFlags_WidthFixed, 84.0f);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
			ImGui::TableSetColumnIndex(0);
			ImGui::SetNextItemWidth(-FLT_MIN);
			changed |= ImGui::DragFloat("##X", &action.destination.x, 1.0f, -100000.0f, 100000.0f, "X: %.0f");
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			changed |= ImGui::DragFloat("##Y", &action.destination.y, 1.0f, -100000.0f, 100000.0f, "Y: %.0f");
			ImGui::TableSetColumnIndex(2);
			changed |= ImGui::Checkbox("Relative", &action.relative);
			ImGui::EndTable();
			return changed;
		}
	);
	ActionEditorRegistry::Register<RotateToAction>(
		"engine.rotate_to",
		{ .label = "Rotate To", .group = "Transform", .description = "Rotate the owning entity to an angle." },
		[](RotateToAction& action, EditorContext&) {
			bool changed{ false };
			if (ImGui::BeginTable("RotateToParams", 3, ImGuiTableFlags_SizingStretchProp)) {
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 40.0f);
				ImGui::TableSetupColumn("Angle", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Shortest", ImGuiTableColumnFlags_WidthFixed, 82.0f);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("Angle");
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::DragFloat(
					"##Degrees", &action.degrees, 1.0f, -3600.0f, 3600.0f, "%.1f deg"
				);
				ImGui::TableSetColumnIndex(2);
				changed |= ImGui::Checkbox("Shortest", &action.shortest_path);
				DrawItemTooltip("Use the shortest rotational path to the target angle.");
				ImGui::EndTable();
			}
			return changed;
		}
	);
	ActionEditorRegistry::Register<SetVisibleAction>(
		"engine.set_visible",
		{ .label = "Set Visible", .group = "Entity", .description = "Set the owning entity visibility.", .menu_order = 3 },
		[](SetVisibleAction& action, EditorContext&) {
			return ImGui::Checkbox("Visible", &action.visible);
		}
	);
	ActionEditorRegistry::Register<PlayAudioAction>(
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
	ActionEditorRegistry::Register<EmitSignalAction>(
		"engine.emit_signal",
		{ .label = "Emit Signal", .group = "Event", .description = "Broadcast a Signal identified by a strong string key." },
		[](EmitSignalAction&, EditorContext&) { return false; }
	);
	ActionEditorRegistry::Register<AddComponentsAction>(
		"engine.add_components",
		{ .label = "Add Components", .group = "Entity", .description = "Add registered components to the owner.", .menu_order = 1 },
		[](AddComponentsAction& action, EditorContext&) {
			bool changed{ false };
			int remove{ -1 };
			for (int i{ 0 }; i < static_cast<int>(action.components.size()); ++i) {
				auto& component{ action.components[static_cast<std::size_t>(i)] };
				const auto* engine_registration{ ComponentRegistry::Find(component.type) };
				const auto* editor_registration{
					engine_registration ? ComponentEditorRegistry::Find(engine_registration->type_id) : nullptr
				};
				ImGui::PushID(static_cast<int>(component.id));
				if (ImGui::BeginTable("ComponentTitle", 2, ImGuiTableFlags_SizingStretchProp)) {
					ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
					ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(
						editor_registration ? editor_registration->options.label.c_str() : component.type.c_str()
					);
					ImGui::TableSetColumnIndex(1);
					if (ImGui::Button("x", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() })) {
						remove = i;
					}
					ImGui::EndTable();
				}
				if (editor_registration) {
					changed |= editor_registration->draw(component.value);
				}
				ImGui::PopID();
			}
			if (remove >= 0) {
				action.components.erase(action.components.begin() + remove);
				changed = true;
			}

			static std::unordered_map<ImGuiID, std::string> pending_components;
			const ImGuiID pending_id{ ImGui::GetID("##PendingComponent") };
			auto& pending_key{ pending_components[pending_id] };
			const ComponentRegistration* pending_registration{ ComponentRegistry::Find(pending_key) };
			const auto* pending_editor{
				pending_registration ? ComponentEditorRegistry::Find(pending_registration->type_id) : nullptr
			};
			const char* preview{
				pending_editor ? pending_editor->options.label.c_str() : "Select component"
			};
			const float button_size{ ImGui::GetFrameHeight() };
			const float combo_width{
				std::max(1.0f, ImGui::GetContentRegionAvail().x - button_size - ImGui::GetStyle().ItemSpacing.x)
			};
			ImGui::SetNextItemWidth(combo_width);
			if (ImGui::BeginCombo("##PendingComponent", preview)) {
				std::vector<std::string> groups;
				for (const auto& component_registration : ComponentRegistry::Entries()) {
					const auto* component_editor{ ComponentEditorRegistry::Find(component_registration.type_id) };
					if (component_editor && !component_editor->options.group.empty() &&
						!std::ranges::contains(groups, component_editor->options.group)) {
						groups.push_back(component_editor->options.group);
					}
				}
				for (const auto& group : groups) {
					if (!ImGui::BeginMenu(group.c_str())) {
						continue;
					}
					for (const auto& component_registration : ComponentRegistry::Entries()) {
						const auto* component_editor{ ComponentEditorRegistry::Find(component_registration.type_id) };
						if (!component_editor || component_editor->options.group != group) {
							continue;
						}
						const bool already_added{ std::ranges::any_of(action.components, [&](const auto& component) {
							return component.type == component_registration.key;
						}) };
						ImGui::BeginDisabled(already_added);
						if (ImGui::MenuItem(
								component_editor->options.label.c_str(), nullptr,
								pending_key == component_registration.key
							)) {
							pending_key = component_registration.key;
						}
						ImGui::EndDisabled();
						if (ImGui::IsItemHovered(already_added ? ImGuiHoveredFlags_AllowWhenDisabled : 0)) {
							ImGui::SetTooltip(
								already_added ? "Already selected.\n%s\n%s" : "%s\n%s",
								component_editor->options.description.c_str(), component_registration.key.c_str()
							);
						}
					}
					ImGui::EndMenu();
				}
				ImGui::EndCombo();
			}
			pending_registration = ComponentRegistry::Find(pending_key);
			ImGui::SameLine();
			const bool can_add{
				pending_registration &&
				!std::ranges::any_of(action.components, [&](const auto& component) {
					return component.type == pending_registration->key;
				})
			};
			ImGui::BeginDisabled(!can_add);
			if (ImGui::Button("+", ImVec2{ button_size, button_size })) {
				action.components.push_back(ComponentRegistry::Make(pending_registration->key));
				pending_key.clear();
				changed = true;
			}
			ImGui::EndDisabled();
			DrawItemTooltip(
				can_add ? "Add the selected registered component." : "Select a component to add."
			);
			return changed;
		}
	);
	ActionEditorRegistry::Register<RemoveComponentsAction>(
		"engine.remove_components",
		{ .label = "Remove Components", .group = "Entity", .description = "Remove selected registered components from the owner.", .menu_order = 2, .separator_after = true },
		[](RemoveComponentsAction& action, EditorContext&) {
			std::string preview;
			for (const auto& key : action.components) {
				const auto* component{ ComponentRegistry::Find(key) };
				const auto* component_editor{ component ? ComponentEditorRegistry::Find(component->type_id) : nullptr };
				if (!preview.empty()) {
					preview += ", ";
				}
				preview += component_editor ? component_editor->options.label : key;
			}
			if (preview.empty()) {
				preview = "Select components to remove";
			}
			bool changed{ false };
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##Components", preview.c_str())) {
				std::vector<std::string> groups;
				for (const auto& component : ComponentRegistry::Entries()) {
					const auto* component_editor{ ComponentEditorRegistry::Find(component.type_id) };
					if (component_editor && !component_editor->options.group.empty() &&
						!std::ranges::contains(groups, component_editor->options.group)) {
						groups.push_back(component_editor->options.group);
					}
				}
				for (const auto& group : groups) {
					if (!ImGui::BeginMenu(group.c_str())) {
						continue;
					}
					for (const auto& component : ComponentRegistry::Entries()) {
						const auto* component_editor{ ComponentEditorRegistry::Find(component.type_id) };
						if (!component_editor || component_editor->options.group != group) {
							continue;
						}
						bool selected{ std::ranges::contains(action.components, component.key) };
						if (ImGui::Checkbox(component_editor->options.label.c_str(), &selected)) {
							if (selected) {
								action.components.push_back(component.key);
							} else {
								std::erase(action.components, component.key);
							}
							changed = true;
						}
					}
					ImGui::EndMenu();
				}
				ImGui::EndCombo();
			}
			return changed;
		}
	);
	ActionEditorRegistry::Register<SpawnEntityAction>(
		"engine.spawn_entity",
		{ .label = "Spawn Entity", .group = "Entity", .description = "Spawn one or more prefab instances.", .menu_order = 0 },
		[](SpawnEntityAction& action, EditorContext& context) {
			action.count = std::clamp(action.count, 1, 100);
			action.rectangle_size.x = std::max(0.0f, action.rectangle_size.x);
			action.rectangle_size.y = std::max(0.0f, action.rectangle_size.y);
			action.radius = std::max(0.0f, action.radius);
			bool changed{ false };
			if (ImGui::BeginTable("SpawnEntityPrimaryRow", 2, ImGuiTableFlags_SizingStretchProp)) {
				ImGui::TableSetupColumn("Prefab", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, GetCountControlWidth("Count"));
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
				ImGui::TableSetColumnIndex(0);
				const auto* current{ context.prefabs.Find(action.prefab_key) };
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::BeginCombo("##Prefab", current ? current->name.c_str() : "Missing Prefab")) {
					for (const auto& prefab : context.prefabs.definitions) {
						if (ImGui::Selectable(prefab.name.c_str(), prefab.key == action.prefab_key)) {
							action.prefab_key = prefab.key;
							changed = true;
						}
					}
					ImGui::EndCombo();
				}
				ImGui::TableSetColumnIndex(1);
				const int previous_count{ action.count };
				DrawCountControl("Count", action.count, 1, 100, false, "Number of prefab instances to create.");
				changed |= previous_count != action.count;
				ImGui::EndTable();
			}

			auto draw_enum_combo = [](const char* id, auto& value) {
			using Enum = std::remove_cvref_t<decltype(value)>;
			auto label_for = [](Enum candidate) {
				if constexpr (std::same_as<Enum, SpawnOrigin>) {
					return std::string{ candidate == SpawnOrigin::OwnerEntity ? "Entity" : "Position" };
				}
				return std::string{ magic_enum::enum_name(candidate) };
			};
			const std::string preview{ label_for(value) };
			bool local_changed{ false };
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo(id, preview.c_str())) {
				for (const auto candidate : magic_enum::enum_values<Enum>()) {
					const std::string label{ label_for(candidate) };
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
			if (ImGui::BeginTable("SpawnEntityPlacementRow", columns, ImGuiTableFlags_SizingStretchProp)) {
				ImGui::TableSetupColumn("Origin", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				ImGui::TableSetupColumn("Shape", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				if (displayed_area == SpawnArea::Rectangle) {
					ImGui::TableSetupColumn("Width", ImGuiTableColumnFlags_WidthStretch, 1.0f);
					ImGui::TableSetupColumn("Height", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				} else if (displayed_area == SpawnArea::Circle) {
					ImGui::TableSetupColumn("Radius", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				}
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
				ImGui::TableSetColumnIndex(0);
				changed |= draw_enum_combo("##SpawnOrigin", action.origin);
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::DragFloat("##SpawnX", &action.center.x, 1.0f, -100000.0f, 100000.0f, "X: %.0f");
				ImGui::TableSetColumnIndex(2);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::DragFloat("##SpawnY", &action.center.y, 1.0f, -100000.0f, 100000.0f, "Y: %.0f");
				ImGui::TableSetColumnIndex(3);
				changed |= draw_enum_combo("##SpawnArea", action.area);
				if (displayed_area == SpawnArea::Rectangle) {
					ImGui::TableSetColumnIndex(4);
					ImGui::SetNextItemWidth(-FLT_MIN);
					changed |= ImGui::DragFloat("##SpawnWidth", &action.rectangle_size.x, 1.0f, 0.0f, 100000.0f, "W: %.0f");
					ImGui::TableSetColumnIndex(5);
					ImGui::SetNextItemWidth(-FLT_MIN);
					changed |= ImGui::DragFloat("##SpawnHeight", &action.rectangle_size.y, 1.0f, 0.0f, 100000.0f, "H: %.0f");
				} else if (displayed_area == SpawnArea::Circle) {
					ImGui::TableSetColumnIndex(4);
					ImGui::SetNextItemWidth(-FLT_MIN);
					changed |= ImGui::DragFloat("##SpawnRadius", &action.radius, 1.0f, 0.0f, 100000.0f, "R: %.0f");
				}
				ImGui::EndTable();
			}

			std::string options;
			auto append_option = [&options](std::string_view value) {
				if (!options.empty()) {
					options += ", ";
				}
				options += value;
			};
			if (action.parent_to_owner) append_option("Parent");
			if (action.inherit_owner_rotation) append_option("Rotation");
			if (action.inherit_owner_scale) append_option("Scale");
			if (action.random_rotation) append_option("Random Rotation");
			if (options.empty()) options = "Options";
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##SpawnOptions", options.c_str())) {
				changed |= ImGui::Checkbox("Parent to Owner", &action.parent_to_owner);
				if (ImGui::Checkbox("Inherit Rotation", &action.inherit_owner_rotation)) {
					changed = true;
					if (action.inherit_owner_rotation) {
						action.random_rotation = false;
					}
				}
				changed |= ImGui::Checkbox("Inherit Scale", &action.inherit_owner_scale);
				if (ImGui::Checkbox("Random Rotation", &action.random_rotation)) {
					changed = true;
					if (action.random_rotation) {
						action.inherit_owner_rotation = false;
					}
				}
				ImGui::EndCombo();
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
	ImGui::InvisibleButton("SceneCanvas", size);
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
		for (const auto& registration : ComponentRegistry::Entries()) {
			if (!registration.has(entity)) {
				continue;
			}
			const auto* editor{ ComponentEditorRegistry::Find(registration.type_id) };
			if (!editor) {
				continue;
			}
			ImGui::PushID(registration.key.c_str());
			bool component_open{ false };
			bool remove{ false };
			if (ImGui::BeginTable("ComponentHeader", 2, ImGuiTableFlags_SizingStretchProp)) {
				ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
				ImGui::TableSetColumnIndex(0);
				component_open = ImGui::TreeNodeEx(
					"##Component", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
						ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen,
					"%s", editor->options.label.c_str()
				);
				DrawItemTooltip(editor->options.description.c_str());
				ImGui::TableSetColumnIndex(1);
				if (ImGui::Button("x", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() })) {
					remove = true;
				}
				ImGui::EndTable();
			}
			if (component_open) {
				std::any value{ registration.capture(entity) };
				if (editor->draw(value)) {
					registration.set(entity, value);
				}
			}
			if (remove) {
				registration.remove(entity);
			}
			ImGui::PopID();
		}

		if (ImGui::Button("+ Add Component", ImVec2{ -FLT_MIN, 0.0f })) {
			ImGui::OpenPopup("AddEntityComponent");
		}
		if (ImGui::BeginPopup("AddEntityComponent")) {
			std::string current_group;
			for (const auto& registration : ComponentRegistry::Entries()) {
				const auto* editor{ ComponentEditorRegistry::Find(registration.type_id) };
				if (!editor || registration.has(entity)) {
					continue;
				}
				if (editor->options.group != current_group) {
					if (!current_group.empty()) {
						ImGui::Separator();
					}
					ImGui::TextDisabled("%s", editor->options.group.c_str());
					current_group = editor->options.group;
				}
				if (ImGui::MenuItem(editor->options.label.c_str())) {
					registration.set(entity, registration.make_default());
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
	const std::size_t total{ scripts.scripts.size() + scripts.sequences.size() };
	char header[96]{};
	std::snprintf(header, sizeof(header), "Scripts (%zu)", total);
	const bool open{ ImGui::TreeNodeEx(
		"##Scripts", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth,
		"%s", header
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

	const ImVec2 add_position{ ImGui::GetCursorScreenPos() };
	ImGui::SetCursorScreenPos(ImVec2{ add_position.x, add_position.y + 4.0f });
	if (ImGui::Button("+ Add Script Sequence", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddScriptSequencePopup");
	}
	DrawAddSequencePopup(scripts);

	const ImVec2 activity_position{ ImGui::GetCursorScreenPos() };
	ImGui::SetCursorScreenPos(ImVec2{ activity_position.x, activity_position.y + 4.0f });
	DrawActivity();
	ImGui::TreePop();
	ImGui::PopID();
}

void DemoEditor::DrawResidentScripts(ScriptsComponent& scripts) {
	char label[64]{};
	std::snprintf(label, sizeof(label), "Resident Scripts (%zu)", scripts.scripts.size());
	bool add_requested{ false };
	const bool open{ DrawAddableSectionHeader(
		"ResidentScripts", label, false, scripts.scripts.empty(),
		"Long-lived registered Scripts attached to this entity.", "No resident Scripts.",
		"Add a resident Script.", add_requested
	) };
	ImGui::PushID("ResidentScripts");
	if (add_requested) {
		ImGui::OpenPopup("AddResidentScript");
	}
	if (ImGui::BeginPopup("AddResidentScript")) {
		std::string group;
		for (const auto& registration : ScriptRegistry::Entries()) {
			const auto* editor{ ScriptEditorRegistry::Find(registration.key) };
			if (!editor) {
				continue;
			}
			if (editor->options.group != group) {
				if (!group.empty()) {
					ImGui::Separator();
				}
				ImGui::TextDisabled("%s", editor->options.group.c_str());
				group = editor->options.group;
			}
			if (ImGui::MenuItem(editor->options.label.c_str())) {
				ScriptEntry script;
				script.type = registration.key;
				script.value = registration.make_default();
				scripts.scripts.push_back(std::move(script));
			}
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
	if (!open) {
		return;
	}

	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(scripts.scripts.size()); ++i) {
		auto& script{ scripts.scripts[static_cast<std::size_t>(i)] };
		const auto* editor{ ScriptEditorRegistry::Find(script.type) };
		ImGui::PushID(static_cast<int>(script.id));
		if (ImGui::BeginTable("ResidentScriptRow", 3, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 21.0f);
			ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
			ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(editor ? editor->options.label.c_str() : script.type.c_str());
			if (editor) {
				DrawItemTooltip(editor->options.description.c_str());
			}
			ImGui::TableSetColumnIndex(1);
			ImGui::Checkbox("##Enabled", &script.enabled);
			ImGui::TableSetColumnIndex(2);
			if (ImGui::Button("x", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() })) {
				remove = i;
			}
			ImGui::EndTable();
		}
		if (editor && editor->draw(script.value)) {
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
	std::string header;
	if (binding.runtime.running) {
		header += "> ";
	}
	if (!binding.enabled) {
		header += "[Disabled] ";
	}
	if (binding.shared_reference) {
		header += "[Global] ";
	}
	header += sequence->name;
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{ 0.22f, 0.34f, 0.28f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4{ 0.28f, 0.43f, 0.35f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4{ 0.34f, 0.50f, 0.41f, 1.0f });
	const bool open{ ImGui::TreeNodeEx(
		"##ScriptSequence", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen,
		"%s", header.c_str()
	) };
	ImGui::PopStyleColor(3);
	if (ImGui::BeginPopupContextItem("ScriptSequenceContext")) {
		if (ImGui::MenuItem(binding.enabled ? "Disable" : "Enable")) {
			binding.enabled = !binding.enabled;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Delete")) {
			remove = true;
		}
		ImGui::EndPopup();
	}

	if (open) {
		if (ImGui::BeginTable("ScriptSequenceMainRow", 5, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 38.0f);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.6f);
			ImGui::TableSetupColumn("Play", ImGuiTableColumnFlags_WidthStretch, 0.6f);
			ImGui::TableSetupColumn("Pause", ImGuiTableColumnFlags_WidthStretch, 0.6f);
			ImGui::TableSetupColumn("Stop", ImGuiTableColumnFlags_WidthStretch, 0.6f);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled("Name");
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputText("##Name", &sequence->name);
			ImGui::TableSetColumnIndex(2);
			if (ImGui::Button(binding.runtime.running ? "Restart" : "Start", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
				world_.Start(owner, binding, true);
			}
			ImGui::TableSetColumnIndex(3);
			ImGui::BeginDisabled(!binding.runtime.running);
			if (ImGui::Button(binding.runtime.paused ? "Resume" : "Pause", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
				world_.SetPaused(owner, binding, !binding.runtime.paused);
			}
			ImGui::EndDisabled();
			ImGui::TableSetColumnIndex(4);
			if (ImGui::Button("Stop", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })) {
				world_.Stop(owner, binding);
			}
			ImGui::EndTable();
		}

		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("On Retrigger");
		ImGui::SameLine();
		const float global_width{ ImGui::CalcTextSize("Global").x + ImGui::GetFrameHeight() + 10.0f };
		const float destroy_width{ ImGui::CalcTextSize("Destroy on Complete").x + ImGui::GetFrameHeight() + 10.0f };
		const float reentry_width{ std::max(100.0f, ImGui::GetContentRegionAvail().x - global_width - destroy_width - 24.0f) };
		int reentry{ static_cast<int>(sequence->reentry) };
		ImGui::SetNextItemWidth(reentry_width);
		if (ImGui::Combo("##Reentry", &reentry, kReentryLabels.data(), static_cast<int>(kReentryLabels.size()))) {
			sequence->reentry = static_cast<ReentryMode>(reentry);
		}
		DrawItemTooltip("Controls what happens when the sequence receives a start Event while running.");
		ImGui::SameLine();
		bool global{ binding.shared_reference };
		if (ImGui::Checkbox("Global", &global)) {
			if (global) {
				PromoteToShared(binding);
			} else {
				DetachToLocal(binding);
			}
			sequence = world_.Resolve(binding);
		}
		DrawItemTooltip(binding.shared_reference ? "Shared definition with per-binding runtime." : "Definition owned by this entity.");
		ImGui::SameLine();
		ImGui::Checkbox("Destroy on Complete", &sequence->destroy_on_complete);
		DrawItemTooltip("Clear transient runtime state after completion while retaining the definition.");

		DrawLifecycle(*sequence);
		DrawEventSection("StartEvents", "Start Events", sequence->start_events, false);
		DrawEventSection("StopEvents", "Stop Events", sequence->stop_events, true);

		const ImVec2 action_position{ ImGui::GetCursorScreenPos() };
		ImGui::SetCursorScreenPos(ImVec2{ action_position.x, action_position.y + 2.0f });
		char action_label[64]{};
		std::snprintf(action_label, sizeof(action_label), "Actions (%zu)", sequence->actions.size());
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{ 0.31f, 0.24f, 0.34f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4{ 0.40f, 0.31f, 0.44f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4{ 0.47f, 0.36f, 0.51f, 1.0f });
		const bool actions_open{ ImGui::TreeNodeEx(
			"##Actions", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Framed,
			"%s", action_label
		) };
		ImGui::PopStyleColor(3);
		if (actions_open) {
			DrawActions(*sequence, binding);
		}
	}
	ImGui::PopID();
	return remove;
}

void DemoEditor::DrawEventSection(
	const char* id, const char* label, std::vector<EventCondition>& events, bool stop_events
) {
	char heading[64]{};
	std::snprintf(heading, sizeof(heading), "%s (%zu)", label, events.size());
	bool add_requested{ false };
	const bool open{ DrawAddableSectionHeader(
		id, heading, true, events.empty(),
		stop_events ? "Events that stop this sequence." : "Events that start this sequence.",
		stop_events ? "No stop Events: the sequence stops manually or on completion." : "No start Events: the sequence starts manually.",
		stop_events ? "Add a stop Event." : "Add a start Event.", add_requested
	) };
	ImGui::PushID(id);
	if (add_requested) {
		ImGui::OpenPopup("AddEvent");
	}
	if (ImGui::BeginPopup("AddEvent")) {
		auto add_candidate = [&](const EventEditorRegistration& candidate) {
			if (ImGui::MenuItem(candidate.options.label.c_str())) {
				if (const auto* registration{ EventRegistry::Find(candidate.key) }) {
					events.push_back(EventCondition{
						.id = IdGenerator::Next(), .enabled = true,
						.type = registration->key,
						.filter = registration->make_default_filter(),
					});
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s\n%s", candidate.options.description.c_str(), candidate.key.c_str());
			}
		};

		for (const auto& candidate : EventEditorRegistry::Entries()) {
			if (candidate.options.group.empty()) {
				add_candidate(candidate);
			}
		}
		std::vector<std::string> groups;
		for (const auto& candidate : EventEditorRegistry::Entries()) {
			if (!candidate.options.group.empty() && !std::ranges::contains(groups, candidate.options.group)) {
				groups.push_back(candidate.options.group);
			}
		}
		for (const auto& group : groups) {
			if (!ImGui::BeginMenu(group.c_str())) {
				continue;
			}
			for (const auto& candidate : EventEditorRegistry::Entries()) {
				if (candidate.options.group == group) {
					add_candidate(candidate);
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
	if (!open) {
		return;
	}
	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(events.size()); ++i) {
		if (DrawEvent(events[static_cast<std::size_t>(i)], stop_events)) {
			remove = i;
		}
	}
	if (remove >= 0) {
		events.erase(events.begin() + remove);
	}
}

bool DemoEditor::DrawEvent(EventCondition& event, bool stop_event) {
	bool remove{ false };
	ImGui::PushID(static_cast<int>(event.id));
	const float remove_width{ ImGui::GetFrameHeight() };
	const auto* selected{ EventEditorRegistry::Find(event.type) };
	if (ImGui::BeginTable("EventRow", 4, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthFixed, 130.0f);
		ImGui::TableSetupColumn("Filter", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 21.0f);
		ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, remove_width);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
		ImGui::TableSetColumnIndex(0);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo("##Event", selected ? selected->options.label.c_str() : "Missing Event")) {
			auto select_candidate = [&](const EventEditorRegistration& candidate) {
				if (ImGui::MenuItem(candidate.options.label.c_str(), nullptr, candidate.key == event.type)) {
					event.type = candidate.key;
					if (const auto* registration{ EventRegistry::Find(candidate.key) }) {
						event.filter = registration->make_default_filter();
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("%s\n%s", candidate.options.description.c_str(), candidate.key.c_str());
				}
			};
			for (const auto& candidate : EventEditorRegistry::Entries()) {
				if (candidate.options.group.empty()) {
					select_candidate(candidate);
				}
			}
			std::vector<std::string> groups;
			for (const auto& candidate : EventEditorRegistry::Entries()) {
				if (!candidate.options.group.empty() && !std::ranges::contains(groups, candidate.options.group)) {
					groups.push_back(candidate.options.group);
				}
			}
			for (const auto& group : groups) {
				if (!ImGui::BeginMenu(group.c_str())) {
					continue;
				}
				for (const auto& candidate : EventEditorRegistry::Entries()) {
					if (candidate.options.group == group) {
						select_candidate(candidate);
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndCombo();
		}
		ImGui::TableSetColumnIndex(1);
		selected = EventEditorRegistry::Find(event.type);
		if (selected) {
			selected->draw_filter(event.filter);
		} else {
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled(stop_event ? "Missing stop Event editor" : "Missing start Event editor");
		}
		ImGui::TableSetColumnIndex(2);
		ImGui::Checkbox("##Enabled", &event.enabled);
		ImGui::TableSetColumnIndex(3);
		if (ImGui::Button("x", ImVec2{ remove_width, ImGui::GetFrameHeight() })) {
			remove = true;
		}
		ImGui::EndTable();
	}
	ImGui::PopID();
	return remove;
}

void DemoEditor::DrawActionPicker(Action& action, bool timed_only) {
	const auto* current{ ActionEditorRegistry::Find(action.type) };
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (!ImGui::BeginCombo("##RegisteredAction", current ? current->options.label.c_str() : "Missing Action")) {
		return;
	}

	auto is_available = [&](const ActionEditorRegistration& candidate) {
		const auto* registration{ ActionRegistry::Find(candidate.key) };
		return registration && candidate.key != ActionRegistry::Key<WaitAction>() &&
			candidate.key != ActionRegistry::Key<EmitSignalAction>() &&
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
				action.timing = registration->default_timing.value_or(ActionTiming{});
			} else {
				action.timing.reset();
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s\n%s", candidate.options.description.c_str(), candidate.key.c_str());
		}
	};

	for (const auto& candidate : ActionEditorRegistry::Entries()) {
		if (is_available(candidate) && candidate.options.group.empty()) {
			select_candidate(candidate);
		}
	}
	std::vector<std::string> groups;
	for (const auto& candidate : ActionEditorRegistry::Entries()) {
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
		for (const auto& candidate : ActionEditorRegistry::Entries()) {
			if (is_available(candidate) && candidate.options.group == group) {
				candidates.push_back(&candidate);
			}
		}
		std::ranges::sort(candidates, {}, [](const auto* candidate) {
			return candidate->options.menu_order;
		});
		for (const auto* candidate : candidates) {
			select_candidate(*candidate);
			if (candidate->options.separator_after) {
				ImGui::Separator();
			}
		}
		ImGui::EndMenu();
	}
	ImGui::EndCombo();
}

void DemoEditor::DrawEmitSignalCompact(EmitSignalAction& emit) {
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##EmitSignal", "Signal name", &emit.signal.value);
	DrawItemTooltip("Broadcast Signal name.");
}

void DemoEditor::DrawTimingOptions(ActionTiming& timing, float left_screen_x) {
	const float right_screen_x{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
	const float width{ std::max(1.0f, right_screen_x - left_screen_x) };
	ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });
	if (!ImGui::BeginTable("TimedActionOptions", 2, ImGuiTableFlags_SizingStretchSame, ImVec2{ width, 0.0f })) {
		return;
	}
	ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
	ImGui::TableSetColumnIndex(0);
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
	ImGui::TableSetColumnIndex(1);
	std::string options;
	if (timing.infinite_repeats) options += "Infinite";
	if (timing.reversed) options += (options.empty() ? "" : ", ") + std::string{ "Reversed" };
	if (timing.yoyo) options += (options.empty() ? "" : ", ") + std::string{ "Yoyo" };
	if (options.empty()) options = "Options";
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::BeginCombo("##Options", options.c_str())) {
		ImGui::Checkbox("Infinite", &timing.infinite_repeats);
		DrawItemTooltip("Repeat this timed Action indefinitely. Duration still controls every cycle.");
		ImGui::Checkbox("Reversed", &timing.reversed);
		ImGui::Checkbox("Yoyo", &timing.yoyo);
		ImGui::EndCombo();
	}
	ImGui::EndTable();
}

void DemoEditor::DrawActionParameters(Action& action, float left_screen_x) {
	const auto* action_editor{ ActionEditorRegistry::Find(action.type) };
	if (!action_editor || action.type == ActionRegistry::Key<WaitAction>() ||
		action.type == ActionRegistry::Key<EmitSignalAction>()) {
		return;
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
	const float type_width{ 108.0f };
	const float duration_width{ 76.0f };
	const float repeats_width{ GetCountControlWidth("Repeats") };
	const float remove_width{ ImGui::GetFrameHeight() };
	ActionForm displayed_form{ GetActionForm(action) };
	ActionForm requested_form{ displayed_form };
	bool form_changed{ false };
	float parameter_left_screen_x{ ImGui::GetCursorScreenPos().x };

	int column_count{ lifecycle ? 4 : 4 };
	if (displayed_form == ActionForm::TimedAction) {
		column_count = lifecycle ? 4 : 6;
	}
	const int remove_column{ column_count - 1 };

	if (ImGui::BeginTable("ActionRow", column_count, ImGuiTableFlags_SizingStretchProp)) {
		if (!lifecycle) {
			ImGui::TableSetupColumn("Drag", ImGuiTableColumnFlags_WidthFixed, drag_width);
		}
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, type_width);
		if (displayed_form == ActionForm::TimedAction) {
			ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, duration_width);
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Repeats", ImGuiTableColumnFlags_WidthFixed, repeats_width);
		} else {
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		}
		ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, remove_width);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

		int column{ 0 };
		if (!lifecycle) {
			ImGui::TableSetColumnIndex(column++);
			if (!action.enabled) {
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
			}
			ImGui::Button("::", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() });
			if (!action.enabled) {
				ImGui::PopStyleVar();
			}
			DrawItemTooltip(action.enabled ? "Right-click for options. Drag to reorder." : "Disabled Action.");
			if (ImGui::BeginPopupContextItem("ActionContext")) {
				if (ImGui::MenuItem(action.enabled ? "Disable" : "Enable")) {
					action.enabled = !action.enabled;
				}
				if (ImGui::MenuItem("Duplicate")) {
					duplicate = true;
				}
				ImGui::EndPopup();
			}
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				const ActionDragPayload payload{ index };
				ImGui::SetDragDropPayload("PTGN_SCRIPT_ACTION", &payload, sizeof(payload));
				ImGui::Text("%d. %s", index + 1, ActionSummary(action).c_str());
				ImGui::EndDragDropSource();
			}
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("PTGN_SCRIPT_ACTION") }) {
					const auto* drag{ static_cast<const ActionDragPayload*>(payload->Data) };
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
		if (ImGui::BeginCombo("##Form", kActionFormLabels[static_cast<std::size_t>(displayed_form)])) {
			for (int i{ 0 }; i < static_cast<int>(kActionFormLabels.size()); ++i) {
				const auto candidate{ static_cast<ActionForm>(i) };
				if (lifecycle && (candidate == ActionForm::TimedAction || candidate == ActionForm::Wait)) {
					continue;
				}
				if (ImGui::Selectable(kActionFormLabels[static_cast<std::size_t>(i)], candidate == displayed_form)) {
					requested_form = candidate;
					form_changed = true;
				}
			}
			ImGui::EndCombo();
		}

		if (displayed_form == ActionForm::TimedAction) {
			ImGui::TableSetColumnIndex(column++);
			DrawDurationInput("##Duration", action.timing->duration_ms, -FLT_MIN, "Duration of each timed Action cycle.");
			ImGui::TableSetColumnIndex(column++);
			DrawActionPicker(action, true);
			ImGui::TableSetColumnIndex(column++);
			DrawCountControl(
				"Repeats", action.timing->additional_repeats, 0, 100,
				action.timing->infinite_repeats,
				"Additional full-duration cycles."
			);
		} else {
			ImGui::TableSetColumnIndex(column++);
			switch (displayed_form) {
				case ActionForm::Action:
					DrawActionPicker(action, false);
					break;
				case ActionForm::Wait:
					DrawDurationInput("##Duration", action.timing->duration_ms, -FLT_MIN, "Wait duration.");
					break;
				case ActionForm::EmitSignal:
					DrawEmitSignalCompact(std::any_cast<EmitSignalAction&>(action.value));
					break;
				case ActionForm::TimedAction:
					break;
			}
		}

		ImGui::TableSetColumnIndex(remove_column);
		if (ImGui::Button("x", ImVec2{ remove_width, ImGui::GetFrameHeight() })) {
			remove = true;
		}
		DrawItemTooltip("Delete this Action.");
		ImGui::EndTable();
	}

	if (form_changed) {
		SetActionForm(action, requested_form);
		displayed_form = requested_form;
	}
	if (displayed_form == ActionForm::TimedAction && action.timing) {
		DrawTimingOptions(*action.timing, parameter_left_screen_x);
	}
	if (displayed_form == ActionForm::Action || displayed_form == ActionForm::TimedAction) {
		DrawActionParameters(action, parameter_left_screen_x);
	}
	if (binding && binding->runtime.running &&
		binding->runtime.action_index == static_cast<std::size_t>(index)) {
		ImGui::ProgressBar(world_.Progress(*binding), ImVec2{ -FLT_MIN, 2.0f }, "");
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

	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float button_width{ (ImGui::GetContentRegionAvail().x - spacing * 3.0f) * 0.25f };
	if (ImGui::Button("+ Action", ImVec2{ button_width, 0.0f })) {
		sequence.actions.push_back(ActionRegistry::Make<SetVisibleAction>());
	}
	ImGui::SameLine();
	if (ImGui::Button("+ Timed Action", ImVec2{ button_width, 0.0f })) {
		sequence.actions.push_back(ActionRegistry::Make<MoveToAction>());
	}
	ImGui::SameLine();
	if (ImGui::Button("+ Wait", ImVec2{ button_width, 0.0f })) {
		sequence.actions.push_back(ActionRegistry::Make<WaitAction>());
	}
	ImGui::SameLine();
	if (ImGui::Button("+ Emit Signal", ImVec2{ button_width, 0.0f })) {
		sequence.actions.push_back(ActionRegistry::Make<EmitSignalAction>());
	}
}

void DemoEditor::DrawLifecycle(ScriptSequence& sequence) {
	char label[96]{};
	std::snprintf(label, sizeof(label), "Lifecycle (%zu)", sequence.lifecycle_actions.size());
	bool add_requested{ false };
	const bool open{ DrawAddableSectionHeader(
		"LifecycleSection", label, false, sequence.lifecycle_actions.empty(),
		"Optional lifecycle Actions and completion cleanup.", "No lifecycle Actions.",
		"Add a lifecycle Action.", add_requested
	) };
	if (add_requested) {
		sequence.lifecycle_actions.push_back(LifecycleAction{
			.id = IdGenerator::Next(), .enabled = true,
			.lifecycle = SequenceLifecycle::Complete,
			.action = ActionRegistry::Make<EmitSignalAction>(),
		});
	}
	if (!open) {
		return;
	}

	static constexpr std::array lifecycle_action_kinds{ "Action", "Emit Signal" };
	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(sequence.lifecycle_actions.size()); ++i) {
		auto& callback{ sequence.lifecycle_actions[static_cast<std::size_t>(i)] };
		ImGui::PushID(static_cast<int>(callback.id));
		const float lifecycle_width{ 145.0f };
		const float kind_width{ 108.0f };
		const float enabled_width{ 21.0f };
		const float remove_width{ ImGui::GetFrameHeight() };
		if (ImGui::BeginTable("LifecycleRow", 5, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Lifecycle", ImGuiTableColumnFlags_WidthFixed, lifecycle_width);
			ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, kind_width);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, enabled_width);
			ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, remove_width);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

			ImGui::TableSetColumnIndex(0);
			int lifecycle{ static_cast<int>(callback.lifecycle) };
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::Combo(
					"##Lifecycle", &lifecycle, kLifecycleLabels.data(),
					static_cast<int>(kLifecycleLabels.size())
				)) {
				callback.lifecycle = static_cast<SequenceLifecycle>(lifecycle);
			}

			int kind{ GetActionForm(callback.action) == ActionForm::EmitSignal ? 1 : 0 };
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::Combo(
					"##LifecycleActionKind", &kind, lifecycle_action_kinds.data(),
					static_cast<int>(lifecycle_action_kinds.size())
				)) {
				SetActionForm(
					callback.action, kind == 1 ? ActionForm::EmitSignal : ActionForm::Action
				);
			}

			ImGui::TableSetColumnIndex(2);
			if (GetActionForm(callback.action) == ActionForm::EmitSignal) {
				DrawEmitSignalCompact(std::any_cast<EmitSignalAction&>(callback.action.value));
			} else {
				DrawActionPicker(callback.action, false);
			}

			ImGui::TableSetColumnIndex(3);
			ImGui::Checkbox("##Enabled", &callback.enabled);
			ImGui::TableSetColumnIndex(4);
			if (ImGui::Button("x", ImVec2{ remove_width, ImGui::GetFrameHeight() })) {
				remove = i;
			}
			ImGui::EndTable();
		}
		if (GetActionForm(callback.action) != ActionForm::EmitSignal) {
			DrawActionParameters(callback.action, ImGui::GetCursorScreenPos().x);
		}
		ImGui::PopID();
	}
	if (remove >= 0) {
		sequence.lifecycle_actions.erase(sequence.lifecycle_actions.begin() + remove);
	}
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

void DemoEditor::DrawComponentDefinition(ComponentDefinition& component, bool removable, int* remove_index, int index) {
	const auto* engine{ ComponentRegistry::Find(component.type) };
	const auto* editor{ engine ? ComponentEditorRegistry::Find(engine->type_id) : nullptr };
	ImGui::PushID(static_cast<int>(component.id));
	bool open{ false };
	if (ImGui::BeginTable("PrefabComponentHeader", removable ? 2 : 1, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
		if (removable) {
			ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
		}
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
		ImGui::TableSetColumnIndex(0);
		open = ImGui::TreeNodeEx(
			"##Component", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen,
			"%s", editor ? editor->options.label.c_str() : component.type.c_str()
		);
		if (editor) {
			DrawItemTooltip(editor->options.description.c_str());
		}
		if (removable) {
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Button("x", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }) && remove_index) {
				*remove_index = index;
			}
		}
		ImGui::EndTable();
	}
	if (open && editor) {
		editor->draw(component.value);
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
		for (const auto& registration : ComponentRegistry::Entries()) {
			const auto* editor{ ComponentEditorRegistry::Find(registration.type_id) };
			if (!editor) {
				continue;
			}
			const bool already_added{ std::ranges::any_of(prefab.components, [&registration](const auto& component) {
				return component.type == registration.key;
			}) };
			if (editor->options.group != group) {
				if (!group.empty()) {
					ImGui::Separator();
				}
				ImGui::TextDisabled("%s", editor->options.group.c_str());
				group = editor->options.group;
			}
			ImGui::BeginDisabled(already_added);
			if (ImGui::MenuItem(editor->options.label.c_str())) {
				prefab.components.push_back(ComponentRegistry::Make(registration.key));
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

namespace demo {

using namespace script_sequence_demo::engine;
using namespace script_sequence_demo::editor;

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

class DemoWorld : public RuntimeHost, public script_sequence_demo::editor::EditorHost {
public:
	struct ActivityEntry {
		std::string text;
		float remaining_seconds{ 8.0f };
	};

	explicit DemoWorld(GLFWwindow* window) : window_{ window } {
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

	[[nodiscard]] script_sequence_demo::editor::EditorVisual Visual(ptgn::Entity entity) const override {
		script_sequence_demo::editor::EditorVisual result;
		if (entity && entity.Has<DemoVisual>()) {
			const auto& visual{ entity.Get<DemoVisual>() };
			result.color = visual.color;
			result.sensor = visual.sensor;
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
		if (!event.type.empty()) {
			pending_events_.push_back(std::move(event));
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
			if (const auto* registration{ ComponentRegistry::Find(component.type) }) {
				registration->set(entity, component.value);
			}
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
		UpdateInputEvents(delta_seconds);
		UpdateResidentScripts(delta_seconds);
		UpdateOverlapEvents();
		DispatchEvents();

		const auto sequence_entities{ entities_ };
		for (auto entity : sequence_entities) {
			if (auto* scripts{ entity.TryGet<ScriptsComponent>() }) {
				for (auto& sequence : scripts->sequences) {
					UpdateSequence(entity, sequence, delta_seconds);
				}
			}
		}

		DispatchEvents();
		ProcessPendingDestroy();
		for (auto& entry : activity_) {
			entry.remaining_seconds -= delta_seconds;
		}
		std::erase_if(activity_, [](const auto& entry) {
			return entry.remaining_seconds <= 0.0f;
		});
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
		const auto* sequence{ Resolve(binding) };
		if (!sequence || !binding.enabled || !sequence->enabled) {
			return;
		}

		auto& runtime{ binding.runtime };
		if (runtime.running && !force) {
			switch (sequence->reentry) {
				case ReentryMode::IgnoreWhileRunning:
					return;
				case ReentryMode::Restart:
					Stop(owner, binding, false);
					break;
				case ReentryMode::Queue:
					runtime.queued = true;
					return;
			}
		}

		runtime.pending_start_delay_ms = -1.0f;
		runtime.running = true;
		runtime.paused = false;
		runtime.completed = false;
		runtime.action_index = 0;
		runtime.ClearActiveAction();
		Log(std::string{ Name(owner) } + " / " + sequence->name + " started");
		InvokeLifecycle(owner, binding, SequenceLifecycle::Start);
		ProcessImmediateActions(owner, binding);
	}

	void Stop(ptgn::Entity owner, ScriptSequence& binding, bool log = true) override {
		const auto* sequence{ Resolve(binding) };
		if (!sequence) {
			binding.runtime = ScriptSequenceRuntime{};
			return;
		}

		if (binding.runtime.action_instance) {
			ActionContext context{ .host = *this, .owner = owner };
			binding.runtime.action_instance->Cancel(context);
		}
		InvokeLifecycle(owner, binding, SequenceLifecycle::Stop);
		if (log) {
			Log(std::string{ Name(owner) } + " / " + sequence->name + " stopped");
		}
		const int completed_runs{ binding.runtime.completed_runs };
		binding.runtime = ScriptSequenceRuntime{};
		binding.runtime.completed_runs = completed_runs;
	}

	void SetPaused(ptgn::Entity owner, ScriptSequence& binding, bool paused) override {
		if (!binding.runtime.running || binding.runtime.paused == paused) {
			return;
		}
		binding.runtime.paused = paused;
		InvokeLifecycle(
			owner,
			binding,
			paused ? SequenceLifecycle::Pause : SequenceLifecycle::Resume
		);
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
	friend class script_sequence_demo::editor::DemoEditor;

	void RegisterEngineTypes();
	void CreatePrefabs();
	void CreateDemoScene();
	void UpdateInputEvents(float delta_seconds);
	void UpdateResidentScripts(float delta_seconds);
	void ProcessPendingDestroy();
	void UpdateOverlapEvents();
	void DispatchEvents();
	void UpdateSequence(ptgn::Entity owner, ScriptSequence& binding, float delta_seconds);
	void ProcessImmediateActions(ptgn::Entity owner, ScriptSequence& binding);
	void CompleteCurrentAction(ptgn::Entity owner, ScriptSequence& binding);
	void CompleteSequence(ptgn::Entity owner, ScriptSequence& binding);
	void InvokeLifecycle(ptgn::Entity owner, ScriptSequence& binding, SequenceLifecycle lifecycle);
	void ExecuteInstant(ptgn::Entity owner, const Action& action);
	[[nodiscard]] bool Matches(
		const EventCondition& condition,
		const EventEnvelope& event,
		ptgn::Entity owner
	);
	[[nodiscard]] bool Overlap(ptgn::Entity a, ptgn::Entity b) const;

	GLFWwindow* window_{ nullptr };
	ptgn::Manager manager_;
	PrefabRegistry prefabs_;
	SharedScriptSequenceRegistry shared_sequences_;
	std::vector<ptgn::Entity> entities_;
	std::deque<EventEnvelope> pending_events_;
	std::vector<ActivityEntry> activity_;
	std::unordered_map<int, bool> previous_key_states_;
	std::unordered_map<int, float> key_hold_duration_ms_;
	std::unordered_map<int, bool> previous_mouse_states_;
	std::unordered_map<int, float> mouse_hold_duration_ms_;
	std::vector<ptgn::Entity> pending_destroy_;
	bool player_overlapping_sensor_{ false };
	bool player_overlapping_spawner_{ false };
};

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

using namespace script_sequence_demo::engine;
using namespace script_sequence_demo::editor;

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
	ComponentEditorRegistry::Register<DemoVisual>(
		{ .label = "Demo Visual", .group = "Graphics", .description = "Demo-only rectangle/circle color." },
		[](DemoVisual& visual) {
			bool changed{ ImGui::ColorEdit4("Color", &visual.color.x) };
			changed |= ImGui::Checkbox("Sensor", &visual.sensor);
			return changed;
		}
	);
	ComponentEditorRegistry::Register<Health>(
		{ .label = "Health", .group = "Gameplay", .description = "Current and maximum health." },
		[](Health& health) {
			bool changed{ ImGui::DragFloat("Maximum", &health.maximum, 1.0f, 0.0f) };
			changed |= ImGui::DragFloat("Current", &health.current, 1.0f, 0.0f, health.maximum);
			return changed;
		}
	);
	ComponentEditorRegistry::Register<Zombie>(
		{ .label = "Zombie", .group = "Gameplay", .description = "Example empty tag component." },
		[](Zombie&) { ImGui::TextDisabled("Tag component"); return false; }
	);
	ComponentEditorRegistry::Register<Damage>(
		{ .label = "Damage", .group = "Gameplay", .description = "Damage amount and type." },
		[](Damage& damage) {
			bool changed{ ImGui::DragFloat("Amount", &damage.amount, 0.25f, 0.0f) };
			changed |= ImGui::InputText("Type", &damage.damage_type);
			return changed;
		}
	);
	ComponentEditorRegistry::Register<Lifetime>(
		{ .label = "Lifetime", .group = "Gameplay", .description = "Runtime lifetime in milliseconds." },
		[](Lifetime& lifetime) {
			return ImGui::DragFloat("Duration", &lifetime.duration_ms, 10.0f, 0.0f, 3600000.0f, "%.0f ms");
		}
	);
	ComponentEditorRegistry::Register<MaskComponent>(
		{ .label = "Mask", .group = "Physics", .description = "Demo collision/overlap mask." },
		[](MaskComponent& mask) {
			return ImGui::InputInt("Mask", &mask.value);
		}
	);
	ActionEditorRegistry::Register<ApplyDamageAction>(
		"game.apply_damage",
		{ .label = "Apply Damage", .group = "Game", .description = "Apply damage to the owning entity." },
		[](ApplyDamageAction& action, EditorContext&) {
			bool changed{ false };
			if (ImGui::BeginTable("DamageParams", 3, ImGuiTableFlags_SizingStretchProp)) {
				ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed, 92.0f);
				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Critical", ImGuiTableColumnFlags_WidthFixed, 62.0f);
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
	ScriptEditorRegistry::Register<PlayerMovementScript>(
		"game.player_movement",
		{ .label = "Player Movement", .group = "Game", .description = "WASD movement for the demo player." },
		[](PlayerMovementScript& script) {
			return ImGui::DragFloat("Speed", &script.speed, 1.0f, 0.0f, 2000.0f);
		}
	);
}

void DemoWorld::RegisterEngineTypes() {
	// Engine component registrations.
	ComponentRegistry::Register<ptgn::Transform>("ptgn.Transform");
	ComponentRegistry::Register<ptgn::Visible>("ptgn.Visible");
	ComponentRegistry::Register<ptgn::Rect>("ptgn.Rect");
	ComponentRegistry::Register<ptgn::Circle>("ptgn.Circle");
	ComponentRegistry::Register<DemoVisual>("demo.Visual");
	ComponentRegistry::Register<Health>("game.Health");
	ComponentRegistry::Register<Zombie>("game.Zombie");
	ComponentRegistry::Register<Damage>("game.Damage");
	ComponentRegistry::Register<Lifetime>("game.Lifetime");
	ComponentRegistry::Register<MaskComponent>("demo.Mask");
	ComponentRegistry::Register<ScriptsComponent>("engine.Scripts");

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

	EventRegistry::Register<OnCreate, OnCreateFilter>(
		"ptgn.event.OnCreate", match_on_create
	);
	EventRegistry::Register<Signal, Signal>(
		"ptgn.event.Signal", match_signal
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
		ActionTiming{ .duration_ms = 300.0f, .ease = ptgn::Ease::OutCubic }
	);
	ActionRegistry::Register<RotateToAction>(
		"engine.rotate_to", true, false,
		ActionTiming{ .duration_ms = 300.0f }
	);
	ActionRegistry::Register<SetVisibleAction>("engine.set_visible");
	ActionRegistry::Register<PlayAudioAction>("engine.play_audio");
	ActionRegistry::Register<EmitSignalAction>("engine.emit_signal");
	ActionRegistry::Register<AddComponentsAction>("engine.add_components");
	ActionRegistry::Register<RemoveComponentsAction>("engine.remove_components");
	ActionRegistry::Register<SpawnEntityAction>("engine.spawn_entity");
	ActionRegistry::Register<ApplyDamageAction>("game.apply_damage");

	ScriptRegistry::Register<PlayerMovementScript>("game.player_movement");
}

void DemoWorld::CreatePrefabs() {
	PrefabDefinition zombie;
	zombie.key = "prefabs/zombie";
	zombie.name = "Zombie";
	zombie.tag = "Zombie";
	zombie.components.push_back(ComponentRegistry::Make("ptgn.Rect"));
	std::any_cast<ptgn::Rect&>(zombie.components.back().value) =
		ptgn::Rect{ ptgn::V2_float{ 32.0f, 32.0f } };
	zombie.components.push_back(ComponentRegistry::Make("demo.Visual"));
	std::any_cast<DemoVisual&>(zombie.components.back().value).color =
		ImVec4{ 0.45f, 0.75f, 0.30f, 1.0f };
	zombie.components.push_back(ComponentRegistry::Make("game.Health"));
	zombie.components.push_back(ComponentRegistry::Make("game.Zombie"));
	prefabs_.definitions.push_back(std::move(zombie));

	PrefabDefinition circle;
	circle.key = "prefabs/recall_circle";
	circle.name = "Recall Circle";
	circle.tag = "SpawnedCircle";
	circle.components.push_back(ComponentRegistry::Make("ptgn.Circle"));
	std::any_cast<ptgn::Circle&>(circle.components.back().value).radius = 13.0f;
	circle.components.push_back(ComponentRegistry::Make("demo.Visual"));
	std::any_cast<DemoVisual&>(circle.components.back().value).color =
		ImVec4{ 0.78f, 0.42f, 0.88f, 1.0f };

	ScriptsComponent circle_scripts;
	ScriptSequenceBuilder destroy_builder{ "Destroy on Recall" };
	destroy_builder.StartOn<Signal>(Signal{ SignalKey{ "spawned_circles.destroy" } });
	ScriptSequence destroy_sequence{ destroy_builder.Build() };
	destroy_sequence.destroy_on_complete = true;
	circle_scripts.sequences.push_back(std::move(destroy_sequence));
	circle.components.push_back(ComponentRegistry::Make("engine.Scripts"));
	std::any_cast<ScriptsComponent&>(circle.components.back().value) = std::move(circle_scripts);
	prefabs_.definitions.push_back(std::move(circle));
}

void DemoWorld::CreateDemoScene() {
	ScriptSequenceBuilder opened_indicator_builder{ "Door Opened Indicator" };
	opened_indicator_builder
		.Reentry(ReentryMode::Restart)
		.StartOn<Signal>(Signal{ SignalKey{ "door.opened" } })
		.StopOn<Signal>(Signal{ SignalKey{ "door.closed" } });
	opened_indicator_builder
		.During(350.0f, MoveToAction{ { 0.0f, 55.0f }, true })
		.Ease(ptgn::Ease::OutBack)
		.End();
	ScriptSequence opened_indicator{ opened_indicator_builder.Build() };
	const Id opened_indicator_id{ opened_indicator.id };
	shared_sequences_.sequences.push_back(std::move(opened_indicator));

	ScriptSequenceBuilder closed_indicator_builder{ "Door Closed Indicator" };
	closed_indicator_builder
		.Reentry(ReentryMode::Restart)
		.StartOn<Signal>(Signal{ SignalKey{ "door.closed" } })
		.StopOn<Signal>(Signal{ SignalKey{ "door.opened" } });
	closed_indicator_builder
		.During(350.0f, MoveToAction{ { 300.0f, -110.0f }, false })
		.Ease(ptgn::Ease::OutCubic)
		.End();
	ScriptSequence closed_indicator{ closed_indicator_builder.Build() };
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
	ScriptSequenceBuilder sensor_enter_builder{ "Emit Door Opened" };
	sensor_enter_builder
		.Reentry(ReentryMode::Restart)
		.StartOn<ptgn::event::OverlapStart>(EntityMaskFilter{ .tags = "Player" })
		.EmitSignal(SignalKey{ "door.opened" });
	sensor.Get<ScriptsComponent>().sequences.push_back(sensor_enter_builder.Build());

	ScriptSequenceBuilder sensor_exit_builder{ "Emit Door Closed" };
	sensor_exit_builder
		.Reentry(ReentryMode::Restart)
		.StartOn<ptgn::event::OverlapStop>(EntityMaskFilter{ .tags = "Player" })
		.EmitSignal(SignalKey{ "door.closed" });
	sensor.Get<ScriptsComponent>().sequences.push_back(sensor_exit_builder.Build());

	ptgn::Entity panel{ CreateEntity("Sliding Panel", "MovingPanel") };
	panel.Get<ptgn::Transform>().position = { 70.0f, 0.0f };
	panel.Add<ptgn::Rect>(ptgn::V2_float{ 62.0f, 170.0f });
	panel.Get<DemoVisual>().color = ImVec4{ 0.88f, 0.57f, 0.24f, 1.0f };
	panel.Add<ScriptsComponent>();

	// The panel owns the Actions that affect the panel.
	ScriptSequenceBuilder open_builder{ "Open Sliding Panel" };
	open_builder
		.Reentry(ReentryMode::Restart)
		.StartOn<Signal>(Signal{ SignalKey{ "door.opened" } })
		.StopOn<Signal>(Signal{ SignalKey{ "door.closed" } });
	open_builder
		.During(500.0f, MoveToAction{ { 225.0f, 0.0f }, false })
		.Ease(ptgn::Ease::OutCubic)
		.End();
	panel.Get<ScriptsComponent>().sequences.push_back(open_builder.Build());

	ScriptSequenceBuilder close_builder{ "Close Sliding Panel" };
	close_builder
		.Reentry(ReentryMode::Restart)
		.StartOn<Signal>(Signal{ SignalKey{ "door.closed" } })
		.StopOn<Signal>(Signal{ SignalKey{ "door.opened" } });
	close_builder
		.During(500.0f, MoveToAction{ { 70.0f, 0.0f }, false })
		.Ease(ptgn::Ease::OutCubic)
		.End();
	panel.Get<ScriptsComponent>().sequences.push_back(close_builder.Build());

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

	ScriptSequenceBuilder spawn_builder{ "Spawn Recall Circles" };
	spawn_builder
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
	factory.Get<ScriptsComponent>().sequences.push_back(spawn_builder.Build());

	ScriptSequenceBuilder recall_builder{ "Recall Spawned Circles" };
	recall_builder
		.Reentry(ReentryMode::Restart)
		.StartOn<ptgn::event::OverlapStop>(EntityMaskFilter{ .tags = "Player" })
		.EmitSignal(SignalKey{ "spawned_circles.destroy" });
	factory.Get<ScriptsComponent>().sequences.push_back(recall_builder.Build());

	ptgn::Entity damage_target{ CreateEntity("Damage Target", "DamageTarget") };
	damage_target.Get<ptgn::Transform>().position = { 315.0f, 105.0f };
	damage_target.Add<ptgn::Rect>(ptgn::V2_float{ 105.0f, 86.0f });
	damage_target.Get<DemoVisual>().color = ImVec4{ 0.78f, 0.27f, 0.29f, 1.0f };
	damage_target.Add<Health>(Health{ .maximum = 100.0f, .current = 100.0f });
	damage_target.Add<ScriptsComponent>();

	ScriptSequenceBuilder damage_builder{ "Overlap Damage Cooldown" };
	damage_builder
		.Reentry(ReentryMode::IgnoreWhileRunning)
		.StartOn<ptgn::event::Overlap>(EntityMaskFilter{ .tags = "Player" })
		.Then(ApplyDamageAction{ .amount = 25.0f })
		.Wait(3000.0f);
	damage_target.Get<ScriptsComponent>().sequences.push_back(damage_builder.Build());

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
		if (!is_key_event || condition.filter.type() != typeid(KeyListFilter)) {
			return;
		}
		const auto parsed{
			ParseEnumExpression<ptgn::Key>(std::any_cast<const KeyListFilter&>(condition.filter).keys)
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

void DemoWorld::ProcessPendingDestroy() {
	if (pending_destroy_.empty()) {
		return;
	}
	for (ptgn::Entity entity : pending_destroy_) {
		if (!entity) {
			continue;
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
			ScriptContext context{ .host = *this, .owner = entity, .delta_seconds = delta_seconds };
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
	while (!pending_events_.empty()) {
		EventEnvelope event{ std::move(pending_events_.front()) };
		pending_events_.pop_front();

		const auto event_entities{ entities_ };
		for (auto entity : event_entities) {
			if (event.delivery == EventDelivery::Target &&
				(!event.target || entity != *event.target)) {
				continue;
			}

			auto* scripts{ entity.TryGet<ScriptsComponent>() };
			if (!scripts) {
				continue;
			}

			for (auto& resident : scripts->scripts) {
				if (!resident.enabled || !resident.instance) {
					continue;
				}
				ScriptContext context{ .host = *this, .owner = entity };
				resident.instance->OnEvent(context, event.type, event.payload);
			}

			for (auto& binding : scripts->sequences) {
				const auto* sequence{ Resolve(binding) };
				if (!sequence) {
					continue;
				}
				const bool stop{ std::ranges::any_of(sequence->stop_events, [&](const auto& condition) {
					return Matches(condition, event, entity);
				}) };
				if (stop) {
					Stop(entity, binding);
					continue;
				}
				const auto start_it{ std::ranges::find_if(
					sequence->start_events,
					[&](const auto& condition) {
						return Matches(condition, event, entity);
					}
				) };
				if (start_it != sequence->start_events.end()) {
					const auto* registration{ EventRegistry::Find(start_it->type) };
					const float delay_ms{
						registration && registration->start_delay_ms
							? registration->start_delay_ms(start_it->filter)
							: 0.0f
					};
					if (delay_ms > 0.0f && !binding.runtime.running) {
						if (binding.runtime.pending_start_delay_ms < 0.0f) {
							binding.runtime.pending_start_delay_ms = delay_ms;
						}
					} else {
						Start(entity, binding);
					}
				}
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
		.linear_progress = 1.0f,
		.progress = 1.0f,
	};
	instance->Begin(context);
	instance->Update(context);
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
		if (action.timing) {
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
		ActionContext context{ .host = *this, .owner = owner, .linear_progress = 1.0f, .progress = 1.0f };
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
	const bool queued{ binding.runtime.queued };
	const int completed_runs{ binding.runtime.completed_runs + 1 };
	binding.runtime.running = false;
	binding.runtime.paused = false;
	binding.runtime.completed = true;
	binding.runtime.completed_runs = completed_runs;
	binding.runtime.action_instance.reset();
	InvokeLifecycle(owner, binding, SequenceLifecycle::Complete);
	Log(std::string{ Name(owner) } + " / " + sequence->name + " completed");
	if (sequence->destroy_on_complete) {
		Destroy(owner);
		return;
	}
	if (queued) {
		Start(owner, binding, true);
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
			Start(owner, binding, true);
		}
		return;
	}
	if (!runtime.running || runtime.paused) {
		return;
	}
	const auto* sequence{ Resolve(binding) };
	if (!sequence || runtime.action_index >= sequence->actions.size()) {
		CompleteSequence(owner, binding);
		return;
	}
	const auto& action{ sequence->actions[runtime.action_index] };
	if (!action.enabled || !action.timing) {
		ProcessImmediateActions(owner, binding);
		return;
	}
	const auto* registration{ ActionRegistry::Find(action.type) };
	if (!registration) {
		++runtime.action_index;
		ProcessImmediateActions(owner, binding);
		return;
	}
	if (!runtime.action_instance) {
		runtime.action_instance = registration->instantiate(action.value);
		runtime.currently_reversed = action.timing->reversed;
		ActionContext context{ .host = *this, .owner = owner };
		runtime.action_instance->Begin(context);
		InvokeLifecycle(owner, binding, SequenceLifecycle::ActionStart);
	}

	const auto& timing{ *action.timing };
	runtime.elapsed_ms += delta_seconds * 1000.0f;
	const float linear{
		timing.duration_ms <= 0.0f
			? 1.0f
			: std::clamp(runtime.elapsed_ms / timing.duration_ms, 0.0f, 1.0f)
	};
	const float directed{ runtime.currently_reversed ? 1.0f - linear : linear };
	ActionContext context{
		.host = *this,
		.owner = owner,
		.delta_seconds = delta_seconds,
		.linear_progress = linear,
		.progress = ptgn::ApplyEase(directed, timing.ease),
		.repeat = runtime.current_repeat,
		.reversed = runtime.currently_reversed,
	};
	runtime.action_instance->Update(context);
	if (linear < 1.0f) {
		return;
	}
	const bool repeat{
		timing.infinite_repeats || runtime.current_repeat < timing.additional_repeats
	};
	if (repeat) {
		++runtime.current_repeat;
		runtime.elapsed_ms = 0.0f;
		if (timing.yoyo) {
			runtime.currently_reversed = !runtime.currently_reversed;
			InvokeLifecycle(owner, binding, SequenceLifecycle::Yoyo);
		} else {
			InvokeLifecycle(owner, binding, SequenceLifecycle::Repeat);
		}
		return;
	}
	CompleteCurrentAction(owner, binding);
}

using namespace script_sequence_demo::engine;
using script_sequence_demo::editor::DemoEditor;

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
		DemoEditor editor{ world };

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

} // namespace demo

} // namespace script_sequence_demo

int main() {
	return script_sequence_demo::demo::DemoApplication{}.Run();
}
