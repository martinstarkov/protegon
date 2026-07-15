// behavior_component_inspector_demo.cpp
//
// Standalone Dear ImGui mock-up for a compact Behaviors entity component.
//
// Flat sequence entry types:
//   - Action
//   - Timed Action
//   - Wait
//   - Emit Signal
//
// Behaviors can be local to an entity component or references to shared global
// definitions. Runtime state remains per entity binding even for global behaviors.
//
// Expected dependencies: Dear ImGui, GLFW, GLAD/OpenGL, and the standard
// imgui_impl_glfw / imgui_impl_opengl3 backends.

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glad/gl.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace demo {

using Id = std::uint64_t;

Id NextId() {
	static Id next{ 1 };
	return next++;
}

template <std::size_t N>
struct TextBuffer {
	std::array<char, N> data{};

	TextBuffer() = default;
	explicit TextBuffer(std::string_view text) { Assign(text); }

	void Assign(std::string_view text) {
		const auto count{ std::min(text.size(), N - 1) };
		std::memcpy(data.data(), text.data(), count);
		data[count] = '\0';
		if (count + 1 < N) {
			std::fill(data.begin() + static_cast<std::ptrdiff_t>(count + 1), data.end(), '\0');
		}
	}

	char* Data() { return data.data(); }
	const char* Data() const { return data.data(); }
	constexpr std::size_t Size() const { return N; }
	bool Empty() const { return data[0] == '\0'; }
	std::string_view View() const { return Data(); }
};

enum class TriggerKind {
	OnCreate,
	KeyPressed,
	OverlapStart,
	Signal,
	Timer,
	Manual
};

enum class ReentryMode {
	IgnoreWhileRunning,
	Restart,
	Queue,
	Parallel
};

enum class SequenceItemKind {
	Action,
	TimedAction,
	Wait,
	EmitSignal
};

enum class Ease {
	Linear,
	InQuad,
	OutQuad,
	InOutQuad,
	OutCubic,
	OutBack
};

enum class ActionKind {
	SetVisible,
	MoveTo,
	RotateTo,
	PlayAudio,
	SetColliderMode,
	ApplyDamage
};

constexpr std::array kTriggerNames{
	"On Create", "Key Pressed", "Overlap Start", "Signal", "Timer", "Manual"
};
constexpr std::array kReentryNames{
	"Ignore While Running", "Restart", "Queue", "Parallel"
};
constexpr std::array kSequenceItemNames{
	"Action", "Timed Action", "Wait", "Emit Signal"
};
constexpr std::array kEaseNames{
	"Linear", "In Quad", "Out Quad", "In-Out Quad", "Out Cubic", "Out Back"
};
constexpr std::array kColliderModeNames{
	"None", "Overlap", "Discrete", "Continuous"
};

template <typename TEnum, std::size_t N>
bool DrawEnumCombo(
	const char* label,
	TEnum& value,
	const std::array<const char*, N>& names,
	float width = -FLT_MIN
) {
	if (width != 0.0f) {
		ImGui::SetNextItemWidth(width);
	}

	bool changed{ false };
	const int current{ static_cast<int>(value) };

	if (ImGui::BeginCombo(label, names[static_cast<std::size_t>(current)])) {
		for (int i{ 0 }; i < static_cast<int>(N); ++i) {
			const bool selected{ i == current };
			if (ImGui::Selectable(names[static_cast<std::size_t>(i)], selected)) {
				value = static_cast<TEnum>(i);
				changed = true;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	return changed;
}

struct TriggerDefinition {
	Id id{ NextId() };
	TriggerKind kind{ TriggerKind::Manual };
	bool enabled{ true };
	TextBuffer<48> key{ "Space" };
	TextBuffer<64> other_tag{ "Player" };
	TextBuffer<80> signal{ "door.opened" };
	float timer_seconds{ 1.0f };
};

struct SetVisibleParams { bool visible{ true }; };
struct MoveToParams { float destination[2]{ 0.0f, 64.0f }; bool relative{ true }; };
struct RotateToParams { float degrees{ 90.0f }; bool shortest_path{ true }; };
struct PlayAudioParams { TextBuffer<80> asset{ "door_open" }; float volume{ 1.0f }; bool loop{ false }; };
struct SetColliderModeParams { int mode{ 0 }; };
struct ApplyDamageParams { float amount{ 10.0f }; TextBuffer<48> damage_type{ "Physical" }; bool critical{ false }; };

using ActionParameters = std::variant<
	SetVisibleParams,
	MoveToParams,
	RotateToParams,
	PlayAudioParams,
	SetColliderModeParams,
	ApplyDamageParams
>;

struct ActionDefinition {
	ActionKind kind{ ActionKind::SetVisible };
	ActionParameters parameters{ SetVisibleParams{} };
};

struct ActionDescriptor {
	ActionKind kind;
	const char* key;
	const char* label;
	const char* group;
	const char* description;
	bool supports_timed;
};

constexpr std::array kActionRegistry{
	ActionDescriptor{ ActionKind::SetVisible, "engine.set_visible", "Set Visible", "Entity", "Changes entity visibility.", false },
	ActionDescriptor{ ActionKind::MoveTo, "engine.move_to", "Move To", "Transform", "Moves an entity to a target position.", true },
	ActionDescriptor{ ActionKind::RotateTo, "engine.rotate_to", "Rotate To", "Transform", "Rotates an entity to a target angle.", true },
	ActionDescriptor{ ActionKind::PlayAudio, "engine.play_audio", "Play Audio", "Audio", "Plays an audio asset.", false },
	ActionDescriptor{ ActionKind::SetColliderMode, "engine.set_collider_mode", "Set Collider Mode", "Physics", "Changes the collider mode.", false },
	ActionDescriptor{ ActionKind::ApplyDamage, "game.apply_damage", "Apply Damage", "Game", "Example user-registered action.", false },
};

const ActionDescriptor& GetActionDescriptor(ActionKind kind) {
	const auto it{ std::ranges::find_if(kActionRegistry, [kind](const auto& descriptor) {
		return descriptor.kind == kind;
	}) };
	return it != kActionRegistry.end() ? *it : kActionRegistry.front();
}

ActionDefinition MakeAction(ActionKind kind) {
	ActionDefinition action;
	action.kind = kind;

	switch (kind) {
		case ActionKind::SetVisible: action.parameters = SetVisibleParams{}; break;
		case ActionKind::MoveTo: action.parameters = MoveToParams{}; break;
		case ActionKind::RotateTo: action.parameters = RotateToParams{}; break;
		case ActionKind::PlayAudio: action.parameters = PlayAudioParams{}; break;
		case ActionKind::SetColliderMode: action.parameters = SetColliderModeParams{}; break;
		case ActionKind::ApplyDamage: action.parameters = ApplyDamageParams{}; break;
	}

	return action;
}

struct ActionItem { ActionDefinition action; };
struct TimedActionItem {
	ActionDefinition action{ MakeAction(ActionKind::MoveTo) };
	float duration_ms{ 300.0f };
	Ease ease{ Ease::Linear };
	int additional_repeats{ 0 };
	bool infinite_repeats{ false };
	bool reversed{ false };
	bool yoyo{ false };
};
struct WaitItem { float duration_ms{ 250.0f }; };
struct EmitSignalItem { TextBuffer<80> signal{ "door.opened" }; };

using SequenceItemData = std::variant<ActionItem, TimedActionItem, WaitItem, EmitSignalItem>;

struct SequenceItem {
	Id id{ NextId() };
	bool enabled{ true };
	SequenceItemKind kind{ SequenceItemKind::Action };
	SequenceItemData data{ ActionItem{} };
};

SequenceItem MakeSequenceItem(SequenceItemKind kind) {
	SequenceItem item;
	item.kind = kind;

	switch (kind) {
		case SequenceItemKind::Action: item.data = ActionItem{}; break;
		case SequenceItemKind::TimedAction: item.data = TimedActionItem{}; break;
		case SequenceItemKind::Wait: item.data = WaitItem{}; break;
		case SequenceItemKind::EmitSignal: item.data = EmitSignalItem{}; break;
	}

	return item;
}

void SetSequenceItemKind(SequenceItem& item, SequenceItemKind kind) {
	if (item.kind == kind) {
		return;
	}
	const Id id{ item.id };
	const bool enabled{ item.enabled };
	item = MakeSequenceItem(kind);
	item.id = id;
	item.enabled = enabled;
}

struct BehaviorDefinition {
	Id id{ NextId() };
	TextBuffer<80> name{ "New Behavior" };
	ReentryMode reentry{ ReentryMode::IgnoreWhileRunning };
	std::vector<TriggerDefinition> triggers;
	std::vector<SequenceItem> sequence;
};

struct RuntimeState {
	bool running{ false };
	bool paused{ false };
	bool completed{ false };
	bool queued{ false };
	std::size_t item_index{ 0 };
	float elapsed_ms{ 0.0f };
	int completed_runs{ 0 };
};

struct BehaviorBinding {
	Id id{ NextId() };
	bool enabled{ true };
	bool global_reference{ false };
	Id global_behavior_id{ 0 };
	BehaviorDefinition local_definition;
	RuntimeState runtime;
};

struct BehaviorsComponent { std::vector<BehaviorBinding> bindings; };

struct EntityData {
	Id id{ NextId() };
	TextBuffer<64> name{ "Entity" };
	float position[2]{ 0.0f, 0.0f };
	float rotation{ 0.0f };
	float scale[2]{ 1.0f, 1.0f };
	bool visible{ true };
	std::optional<BehaviorsComponent> behaviors;
};

struct GlobalBehaviorRegistry {
	std::vector<BehaviorDefinition> definitions;

	BehaviorDefinition* Find(Id id) {
		const auto it{ std::ranges::find_if(definitions, [id](const auto& definition) {
			return definition.id == id;
		}) };
		return it != definitions.end() ? &*it : nullptr;
	}

	const BehaviorDefinition* Find(Id id) const {
		return const_cast<GlobalBehaviorRegistry*>(this)->Find(id);
	}
};

BehaviorDefinition* ResolveBehavior(BehaviorBinding& binding, GlobalBehaviorRegistry& registry) {
	return binding.global_reference ? registry.Find(binding.global_behavior_id) : &binding.local_definition;
}

const BehaviorDefinition* ResolveBehavior(const BehaviorBinding& binding, const GlobalBehaviorRegistry& registry) {
	return binding.global_reference ? registry.Find(binding.global_behavior_id) : &binding.local_definition;
}

BehaviorDefinition CloneBehavior(const BehaviorDefinition& source) {
	BehaviorDefinition copy{ source };
	copy.id = NextId();
	for (auto& trigger : copy.triggers) {
		trigger.id = NextId();
	}
	for (auto& item : copy.sequence) {
		item.id = NextId();
	}
	return copy;
}

struct SignalEvent {
	std::string name;
	Id source_entity{ 0 };
	Id source_behavior{ 0 };
};

struct ActivityEntry {
	std::string text;
	float remaining_seconds{ 7.0f };
};

struct DemoRuntimeContext {
	std::deque<SignalEvent> pending_signals;
	std::vector<ActivityEntry> activity;
};

void AddActivity(DemoRuntimeContext& context, std::string text) {
	context.activity.push_back({ std::move(text), 7.0f });
	constexpr std::size_t kMaxEntries{ 10 };
	if (context.activity.size() > kMaxEntries) {
		context.activity.erase(context.activity.begin());
	}
}

template <typename T>
void MoveItem(std::vector<T>& items, int from, int to) {
	if (from < 0 || to < 0 || from >= static_cast<int>(items.size()) ||
		to >= static_cast<int>(items.size()) || from == to) {
		return;
	}
	T moved{ std::move(items[static_cast<std::size_t>(from)]) };
	items.erase(items.begin() + from);
	items.insert(items.begin() + to, std::move(moved));
}

void ExecuteAction(
	const ActionDefinition& action,
	const EntityData& entity,
	const BehaviorDefinition& behavior,
	DemoRuntimeContext& context,
	bool timed
) {
	AddActivity(
		context,
		std::string{ entity.name.Data() } + " / " + behavior.name.Data() +
			(timed ? " timed: " : " action: ") + GetActionDescriptor(action.kind).label
	);
}

void EmitSignal(
	const EmitSignalItem& emit,
	const EntityData& entity,
	const BehaviorDefinition& behavior,
	DemoRuntimeContext& context
) {
	if (emit.signal.Empty()) {
		return;
	}
	context.pending_signals.push_back({ std::string{ emit.signal.Data() }, entity.id, behavior.id });
	AddActivity(
		context,
		std::string{ entity.name.Data() } + " / " + behavior.name.Data() +
			" emitted \"" + emit.signal.Data() + "\""
	);
}

bool IsTimedItem(const SequenceItem& item) {
	return item.kind == SequenceItemKind::Wait || item.kind == SequenceItemKind::TimedAction;
}

float GetItemDuration(const SequenceItem& item) {
	switch (item.kind) {
		case SequenceItemKind::Wait:
			return std::max(0.0f, std::get<WaitItem>(item.data).duration_ms);
		case SequenceItemKind::TimedAction:
			return std::max(0.0f, std::get<TimedActionItem>(item.data).duration_ms);
		case SequenceItemKind::Action:
		case SequenceItemKind::EmitSignal:
			return 0.0f;
	}
	return 0.0f;
}

void StartBehaviorRuntime(
	EntityData& entity,
	BehaviorBinding& binding,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context,
	bool force_restart = true
);

void FinishBehavior(
	EntityData& entity,
	BehaviorBinding& binding,
	const BehaviorDefinition& behavior,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	auto& runtime{ binding.runtime };
	runtime.running = false;
	runtime.paused = false;
	runtime.completed = true;
	++runtime.completed_runs;
	AddActivity(context, std::string{ entity.name.Data() } + " / " + behavior.name.Data() + " completed");
	if (runtime.queued) {
		runtime.queued = false;
		StartBehaviorRuntime(entity, binding, registry, context);
	}
}

void ProcessImmediateItems(
	EntityData& entity,
	BehaviorBinding& binding,
	const BehaviorDefinition& behavior,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	auto& runtime{ binding.runtime };

	while (runtime.running && runtime.item_index < behavior.sequence.size()) {
		const auto& item{ behavior.sequence[runtime.item_index] };
		if (!item.enabled) {
			++runtime.item_index;
			continue;
		}

		if (IsTimedItem(item)) {
			if (GetItemDuration(item) <= 0.0f) {
				if (item.kind == SequenceItemKind::TimedAction) {
					ExecuteAction(std::get<TimedActionItem>(item.data).action, entity, behavior, context, true);
				}
				++runtime.item_index;
				runtime.elapsed_ms = 0.0f;
				continue;
			}
			break;
		}

		switch (item.kind) {
			case SequenceItemKind::Action:
				ExecuteAction(std::get<ActionItem>(item.data).action, entity, behavior, context, false);
				break;
			case SequenceItemKind::EmitSignal:
				EmitSignal(std::get<EmitSignalItem>(item.data), entity, behavior, context);
				break;
			case SequenceItemKind::TimedAction:
			case SequenceItemKind::Wait:
				break;
		}

		++runtime.item_index;
		runtime.elapsed_ms = 0.0f;
	}

	if (runtime.running && runtime.item_index >= behavior.sequence.size()) {
		FinishBehavior(entity, binding, behavior, registry, context);
	}
}

void StartBehaviorRuntime(
	EntityData& entity,
	BehaviorBinding& binding,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context,
	bool force_restart
) {
	if (!binding.enabled) {
		return;
	}
	const auto* behavior{ ResolveBehavior(binding, registry) };
	if (!behavior) {
		return;
	}

	auto& runtime{ binding.runtime };
	if (runtime.running && !force_restart) {
		switch (behavior->reentry) {
			case ReentryMode::IgnoreWhileRunning: return;
			case ReentryMode::Restart: break;
			case ReentryMode::Queue: runtime.queued = true; return;
			case ReentryMode::Parallel: break;
		}
	}

	runtime.running = !behavior->sequence.empty();
	runtime.paused = false;
	runtime.completed = behavior->sequence.empty();
	runtime.item_index = 0;
	runtime.elapsed_ms = 0.0f;
	AddActivity(context, std::string{ entity.name.Data() } + " / " + behavior->name.Data() + " started");
	if (runtime.running) {
		ProcessImmediateItems(entity, binding, *behavior, registry, context);
	}
}

void UpdateBehaviorRuntime(
	EntityData& entity,
	BehaviorBinding& binding,
	float delta_seconds,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	auto& runtime{ binding.runtime };
	if (!runtime.running || runtime.paused) {
		return;
	}

	const auto* behavior{ ResolveBehavior(binding, registry) };
	if (!behavior || runtime.item_index >= behavior->sequence.size()) {
		if (behavior) {
			FinishBehavior(entity, binding, *behavior, registry, context);
		}
		return;
	}

	const auto& item{ behavior->sequence[runtime.item_index] };
	if (!item.enabled || !IsTimedItem(item)) {
		ProcessImmediateItems(entity, binding, *behavior, registry, context);
		return;
	}

	runtime.elapsed_ms += delta_seconds * 1000.0f;
	if (runtime.elapsed_ms < GetItemDuration(item)) {
		return;
	}

	if (item.kind == SequenceItemKind::TimedAction) {
		ExecuteAction(std::get<TimedActionItem>(item.data).action, entity, *behavior, context, true);
	}
	++runtime.item_index;
	runtime.elapsed_ms = 0.0f;
	ProcessImmediateItems(entity, binding, *behavior, registry, context);
}

bool MatchesSignalTrigger(const BehaviorDefinition& behavior, std::string_view signal) {
	return std::ranges::any_of(behavior.triggers, [signal](const auto& trigger) {
		return trigger.enabled && trigger.kind == TriggerKind::Signal && trigger.signal.View() == signal;
	});
}

void DispatchSignals(
	std::vector<EntityData>& entities,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	while (!context.pending_signals.empty()) {
		SignalEvent event{ std::move(context.pending_signals.front()) };
		context.pending_signals.pop_front();

		for (auto& entity : entities) {
			if (!entity.behaviors) {
				continue;
			}
			for (auto& binding : entity.behaviors->bindings) {
				const auto* behavior{ ResolveBehavior(binding, registry) };
				if (!behavior || !MatchesSignalTrigger(*behavior, event.name)) {
					continue;
				}
				AddActivity(
					context,
					std::string{ entity.name.Data() } + " / " + behavior->name.Data() +
						" received \"" + event.name + "\""
				);
				StartBehaviorRuntime(entity, binding, registry, context, false);
			}
		}
	}
}

void UpdateActivity(DemoRuntimeContext& context, float delta_seconds) {
	for (auto& entry : context.activity) {
		entry.remaining_seconds -= delta_seconds;
	}
	std::erase_if(context.activity, [](const auto& entry) {
		return entry.remaining_seconds <= 0.0f;
	});
}

float GetRuntimeProgress(const BehaviorBinding& binding, const BehaviorDefinition& behavior) {
	const auto& runtime{ binding.runtime };
	if (runtime.completed) {
		return 1.0f;
	}
	if (!runtime.running || runtime.item_index >= behavior.sequence.size()) {
		return 0.0f;
	}
	const float duration{ GetItemDuration(behavior.sequence[runtime.item_index]) };
	return duration > 0.0f ? std::clamp(runtime.elapsed_ms / duration, 0.0f, 1.0f) : 0.0f;
}

std::string TriggerSummary(const TriggerDefinition& trigger) {
	switch (trigger.kind) {
		case TriggerKind::OnCreate: return "On Create";
		case TriggerKind::KeyPressed: return std::string{ "Key: " } + trigger.key.Data();
		case TriggerKind::OverlapStart: return std::string{ "Overlap: " } + trigger.other_tag.Data();
		case TriggerKind::Signal: return std::string{ "Signal: " } + trigger.signal.Data();
		case TriggerKind::Timer: {
			char buffer[64]{};
			std::snprintf(buffer, sizeof(buffer), "Timer: %.2fs", trigger.timer_seconds);
			return buffer;
		}
		case TriggerKind::Manual: return "Manual";
	}
	return {};
}

std::string SequenceItemSummary(const SequenceItem& item) {
	char buffer[160]{};
	switch (item.kind) {
		case SequenceItemKind::Action:
			return GetActionDescriptor(std::get<ActionItem>(item.data).action.kind).label;
		case SequenceItemKind::TimedAction: {
			const auto& timed{ std::get<TimedActionItem>(item.data) };
			std::snprintf(buffer, sizeof(buffer), "%s  %.0fms", GetActionDescriptor(timed.action.kind).label, timed.duration_ms);
			return buffer;
		}
		case SequenceItemKind::Wait:
			std::snprintf(buffer, sizeof(buffer), "%.0fms", std::get<WaitItem>(item.data).duration_ms);
			return buffer;
		case SequenceItemKind::EmitSignal:
			return std::get<EmitSignalItem>(item.data).signal.Data();
	}
	return {};
}


bool DrawDisclosureRow(
	const char* id,
	const char* label,
	bool default_open = true
) {
	ImGui::PushID(id);

	ImGuiStorage* storage{ ImGui::GetStateStorage() };
	const ImGuiID open_id{ ImGui::GetID("Open") };
	bool open{ storage->GetBool(open_id, default_open) };

	if (ImGui::SmallButton(open ? "v" : ">")) {
		open = !open;
		storage->SetBool(open_id, open);
	}

	ImGui::SameLine();

	if (ImGui::Selectable(
			label,
			false
		)) {
		open = !open;
		storage->SetBool(open_id, open);
	}

	ImGui::PopID();
	return open;
}

bool DrawFramedDisclosureHeader(
	const char* id,
	const char* label,
	bool* enabled,
	bool& remove,
	bool default_open = true
) {
	ImGui::PushID(id);

	ImGuiStorage* storage{ ImGui::GetStateStorage() };
	const ImGuiID open_id{ ImGui::GetID("Open") };
	bool open{ storage->GetBool(open_id, default_open) };

	const int column_count{ enabled ? 4 : 3 };

	if (ImGui::BeginTable(
			"Header",
			column_count,
			ImGuiTableFlags_SizingStretchProp |
				ImGuiTableFlags_BordersOuterH |
				ImGuiTableFlags_RowBg
		)) {
		ImGui::TableSetupColumn("Arrow", ImGuiTableColumnFlags_WidthFixed, 23.0f);
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);

		if (enabled) {
			ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 21.0f);
		}

		ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, 21.0f);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);

		if (ImGui::SmallButton(open ? "v" : ">")) {
			open = !open;
			storage->SetBool(open_id, open);
		}

		ImGui::TableSetColumnIndex(1);

		if (ImGui::Selectable(
				label,
				false
			)) {
			open = !open;
			storage->SetBool(open_id, open);
		}

		if (enabled) {
			ImGui::TableSetColumnIndex(2);
			ImGui::Checkbox("##Enabled", enabled);
		}

		ImGui::TableSetColumnIndex(column_count - 1);

		if (ImGui::SmallButton("x")) {
			remove = true;
		}

		ImGui::EndTable();
	}

	ImGui::PopID();
	return open;
}

void DrawActionParametersCompact(ActionDefinition& action) {
	ImGui::Indent(24.0f);

	switch (action.kind) {
		case ActionKind::SetVisible: {
			auto& p{ std::get<SetVisibleParams>(action.parameters) };
			ImGui::Checkbox("Visible", &p.visible);
			break;
		}

		case ActionKind::MoveTo: {
			auto& p{ std::get<MoveToParams>(action.parameters) };

			ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - 90.0f));
			ImGui::DragFloat2(
				"##Destination",
				p.destination,
				1.0f,
				-100000.0f,
				100000.0f,
				"%.0f"
			);

			ImGui::SameLine();
			ImGui::Checkbox("Relative", &p.relative);
			break;
		}

		case ActionKind::RotateTo: {
			auto& p{ std::get<RotateToParams>(action.parameters) };

			ImGui::SetNextItemWidth(std::max(100.0f, ImGui::GetContentRegionAvail().x - 115.0f));
			ImGui::DragFloat(
				"##Degrees",
				&p.degrees,
				1.0f,
				-3600.0f,
				3600.0f,
				"%.1f deg"
			);

			ImGui::SameLine();
			ImGui::Checkbox("Shortest", &p.shortest_path);
			break;
		}

		case ActionKind::PlayAudio: {
			auto& p{ std::get<PlayAudioParams>(action.parameters) };

			if (ImGui::BeginTable(
					"AudioParams",
					3,
					ImGuiTableFlags_SizingStretchProp
				)) {
				ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthFixed, 92.0f);
				ImGui::TableSetupColumn("Loop", ImGuiTableColumnFlags_WidthFixed, 48.0f);
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##Audio", p.asset.Data(), p.asset.Size());

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::SliderFloat("##Volume", &p.volume, 0.0f, 1.0f, "%.2f");

				ImGui::TableSetColumnIndex(2);
				ImGui::Checkbox("Loop", &p.loop);

				ImGui::EndTable();
			}
			break;
		}

		case ActionKind::SetColliderMode: {
			auto& p{ std::get<SetColliderModeParams>(action.parameters) };
			ImGui::SetNextItemWidth(-FLT_MIN);

			if (ImGui::BeginCombo(
					"##ColliderMode",
					kColliderModeNames[static_cast<std::size_t>(p.mode)]
				)) {
				for (int i{ 0 }; i < static_cast<int>(kColliderModeNames.size()); ++i) {
					const bool selected{ p.mode == i };

					if (ImGui::Selectable(
							kColliderModeNames[static_cast<std::size_t>(i)],
							selected
						)) {
						p.mode = i;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}
			break;
		}

		case ActionKind::ApplyDamage: {
			auto& p{ std::get<ApplyDamageParams>(action.parameters) };

			if (ImGui::BeginTable(
					"DamageParams",
					3,
					ImGuiTableFlags_SizingStretchProp
				)) {
				ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed, 92.0f);
				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Critical", ImGuiTableColumnFlags_WidthFixed, 62.0f);
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat(
					"##Amount",
					&p.amount,
					0.25f,
					0.0f,
					100000.0f,
					"%.2f"
				);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText(
					"##DamageType",
					p.damage_type.Data(),
					p.damage_type.Size()
				);

				ImGui::TableSetColumnIndex(2);
				ImGui::Checkbox("Critical", &p.critical);

				ImGui::EndTable();
			}
			break;
		}
	}

	ImGui::Unindent(24.0f);
}

void DrawActionParameters(ActionDefinition& action) {
	DrawActionParametersCompact(action);
}

bool DrawActionPicker(
	const char* label,
	ActionDefinition& action,
	bool timed_only,
	float width = -FLT_MIN
) {
	bool changed{ false };
	ImGui::SetNextItemWidth(width);

	if (ImGui::BeginCombo(label, GetActionDescriptor(action.kind).label)) {
		const char* previous_group{ nullptr };

		for (const auto& descriptor : kActionRegistry) {
			if (timed_only && !descriptor.supports_timed) {
				continue;
			}

			if (!previous_group || std::strcmp(previous_group, descriptor.group) != 0) {
				if (previous_group) {
					ImGui::Separator();
				}

				ImGui::TextDisabled("%s", descriptor.group);
				previous_group = descriptor.group;
			}

			const bool selected{ action.kind == descriptor.kind };

			if (ImGui::Selectable(descriptor.label, selected)) {
				action = MakeAction(descriptor.kind);
				changed = true;
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s\n%s", descriptor.description, descriptor.key);
			}
		}

		ImGui::EndCombo();
	}

	return changed;
}

bool DrawTriggerCompact(TriggerDefinition& trigger) {
	bool remove{ false };

	ImGui::PushID(static_cast<int>(trigger.id));

	if (ImGui::BeginTable(
			"TriggerRow",
			4,
			ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 122.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 21.0f);
		ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, 21.0f);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		DrawEnumCombo("##Kind", trigger.kind, kTriggerNames);

		ImGui::TableSetColumnIndex(1);

		switch (trigger.kind) {
			case TriggerKind::OnCreate:
				ImGui::TextDisabled("Entity created");
				break;

			case TriggerKind::KeyPressed:
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##Key", trigger.key.Data(), trigger.key.Size());
				break;

			case TriggerKind::OverlapStart:
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText(
					"##OtherTag",
					trigger.other_tag.Data(),
					trigger.other_tag.Size()
				);
				break;

			case TriggerKind::Signal:
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##Signal", trigger.signal.Data(), trigger.signal.Size());
				break;

			case TriggerKind::Timer:
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat(
					"##After",
					&trigger.timer_seconds,
					0.05f,
					0.0f,
					3600.0f,
					"%.2fs",
					ImGuiSliderFlags_AlwaysClamp
				);
				break;

			case TriggerKind::Manual:
				ImGui::TextDisabled("Manual");
				break;
		}

		ImGui::TableSetColumnIndex(2);
		ImGui::Checkbox("##Enabled", &trigger.enabled);

		ImGui::TableSetColumnIndex(3);

		if (ImGui::SmallButton("x")) {
			remove = true;
		}

		ImGui::EndTable();
	}

	ImGui::PopID();
	return remove;
}

struct SequenceDragPayload { int index; };

bool DrawSequenceItemCompact(
	SequenceItem& item,
	int index,
	bool active,
	float progress,
	bool& duplicate,
	int& move_from,
	int& move_to
) {
	bool remove{ false };

	ImGui::PushID(static_cast<int>(item.id));

	const int column_count{
		item.kind == SequenceItemKind::TimedAction ? 6 : 5
	};

	if (ImGui::BeginTable(
			"SequenceRow",
			column_count,
			ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn("Drag", ImGuiTableColumnFlags_WidthFixed, 23.0f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 108.0f);

		if (item.kind == SequenceItemKind::TimedAction) {
			ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 88.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		} else {
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		}

		ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 21.0f);
		ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, 21.0f);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::SmallButton("::");

		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Drag to reorder");
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			const SequenceDragPayload payload{ index };
			ImGui::SetDragDropPayload(
				"PTGN_SEQUENCE_ITEM",
				&payload,
				sizeof(payload)
			);
			ImGui::Text("%d. %s", index + 1, SequenceItemSummary(item).c_str());
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload{
					ImGui::AcceptDragDropPayload("PTGN_SEQUENCE_ITEM")
				}) {
				const auto* drag{
					static_cast<const SequenceDragPayload*>(payload->Data)
				};

				if (drag) {
					move_from = drag->index;
					move_to = index;
				}
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::TableSetColumnIndex(1);

		SequenceItemKind new_kind{ item.kind };

		if (DrawEnumCombo("##Type", new_kind, kSequenceItemNames)) {
			SetSequenceItemKind(item, new_kind);
		}

		switch (item.kind) {
			case SequenceItemKind::Action: {
				ImGui::TableSetColumnIndex(2);
				auto& action{ std::get<ActionItem>(item.data).action };
				DrawActionPicker("##Action", action, false);
				break;
			}

			case SequenceItemKind::TimedAction: {
				auto& timed{ std::get<TimedActionItem>(item.data) };

				ImGui::TableSetColumnIndex(2);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat(
					"##Duration",
					&timed.duration_ms,
					10.0f,
					0.0f,
					3600000.0f,
					"%.0fms",
					ImGuiSliderFlags_AlwaysClamp
				);

				ImGui::TableSetColumnIndex(3);
				DrawActionPicker("##Action", timed.action, true);
				break;
			}

			case SequenceItemKind::Wait: {
				ImGui::TableSetColumnIndex(2);
				auto& wait{ std::get<WaitItem>(item.data) };
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat(
					"##Duration",
					&wait.duration_ms,
					10.0f,
					0.0f,
					3600000.0f,
					"%.0fms",
					ImGuiSliderFlags_AlwaysClamp
				);
				break;
			}

			case SequenceItemKind::EmitSignal: {
				ImGui::TableSetColumnIndex(2);
				auto& emit{ std::get<EmitSignalItem>(item.data) };
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::InputText("##Signal", emit.signal.Data(), emit.signal.Size());
				break;
			}
		}

		ImGui::TableSetColumnIndex(column_count - 2);
		ImGui::Checkbox("##Enabled", &item.enabled);

		ImGui::TableSetColumnIndex(column_count - 1);

		if (ImGui::SmallButton("x")) {
			remove = true;
		}

		ImGui::EndTable();
	}

	if (ImGui::BeginPopupContextItem("ItemMenu")) {
		if (ImGui::MenuItem(item.enabled ? "Disable" : "Enable")) {
			item.enabled = !item.enabled;
		}

		if (ImGui::MenuItem("Duplicate")) {
			duplicate = true;
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Remove")) {
			remove = true;
		}

		ImGui::EndPopup();
	}

	if (active) {
		ImGui::ProgressBar(progress, ImVec2{ -FLT_MIN, 2.0f }, "");
	}

	if (item.kind == SequenceItemKind::TimedAction) {
		auto& timed{ std::get<TimedActionItem>(item.data) };

		ImGui::Indent(24.0f);

		if (ImGui::BeginTable(
				"TimedOptions",
				5,
				ImGuiTableFlags_SizingStretchProp
			)) {
			ImGui::TableSetupColumn("Ease", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Infinite", ImGuiTableColumnFlags_WidthFixed, 65.0f);
			ImGui::TableSetupColumn("Repeats", ImGuiTableColumnFlags_WidthFixed, 112.0f);
			ImGui::TableSetupColumn("Reversed", ImGuiTableColumnFlags_WidthFixed, 76.0f);
			ImGui::TableSetupColumn("Yoyo", ImGuiTableColumnFlags_WidthFixed, 52.0f);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			DrawEnumCombo("##Ease", timed.ease, kEaseNames);

			ImGui::TableSetColumnIndex(1);
			ImGui::Checkbox("Infinite", &timed.infinite_repeats);

			ImGui::TableSetColumnIndex(2);
			ImGui::TextDisabled("Repeats");
			ImGui::SameLine();
			ImGui::BeginDisabled(timed.infinite_repeats);
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputInt("##Repeats", &timed.additional_repeats);
			timed.additional_repeats = std::max(0, timed.additional_repeats);
			ImGui::EndDisabled();

			ImGui::TableSetColumnIndex(3);
			ImGui::Checkbox("Reversed", &timed.reversed);

			ImGui::TableSetColumnIndex(4);
			ImGui::Checkbox("Yoyo", &timed.yoyo);

			ImGui::EndTable();
		}

		ImGui::Unindent(24.0f);
		DrawActionParametersCompact(timed.action);
	} else if (item.kind == SequenceItemKind::Action) {
		DrawActionParametersCompact(std::get<ActionItem>(item.data).action);
	}

	ImGui::PopID();
	return remove;
}

void DrawBehaviorSequence(
	BehaviorDefinition& behavior,
	BehaviorBinding& binding
) {
	int remove_index{ -1 };
	int duplicate_index{ -1 };
	int move_from{ -1 };
	int move_to{ -1 };

	for (int i{ 0 }; i < static_cast<int>(behavior.sequence.size()); ++i) {
		auto& item{ behavior.sequence[static_cast<std::size_t>(i)] };
		const bool active{
			binding.runtime.running &&
			binding.runtime.item_index == static_cast<std::size_t>(i)
		};
		bool duplicate{ false };

		if (DrawSequenceItemCompact(
				item,
				i,
				active,
				active ? GetRuntimeProgress(binding, behavior) : 0.0f,
				duplicate,
				move_from,
				move_to
			)) {
			remove_index = i;
		}

		if (duplicate) {
			duplicate_index = i;
		}
	}

	if (move_from >= 0 && move_to >= 0) {
		MoveItem(behavior.sequence, move_from, move_to);
	}

	if (duplicate_index >= 0) {
		auto copy{ behavior.sequence[static_cast<std::size_t>(duplicate_index)] };
		copy.id = NextId();
		behavior.sequence.insert(
			behavior.sequence.begin() + duplicate_index + 1,
			std::move(copy)
		);
	}

	if (remove_index >= 0) {
		behavior.sequence.erase(behavior.sequence.begin() + remove_index);
		binding.runtime = {};
	}

	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float button_width{
		(ImGui::GetContentRegionAvail().x - spacing * 3.0f) * 0.25f
	};

	if (ImGui::Button("+ Action", ImVec2{ button_width, 0.0f })) {
		behavior.sequence.push_back(MakeSequenceItem(SequenceItemKind::Action));
	}

	ImGui::SameLine();

	if (ImGui::Button("+ Timed Action", ImVec2{ button_width, 0.0f })) {
		behavior.sequence.push_back(MakeSequenceItem(SequenceItemKind::TimedAction));
	}

	ImGui::SameLine();

	if (ImGui::Button("+ Wait", ImVec2{ button_width, 0.0f })) {
		behavior.sequence.push_back(MakeSequenceItem(SequenceItemKind::Wait));
	}

	ImGui::SameLine();

	if (ImGui::Button("+ Emit Signal", ImVec2{ button_width, 0.0f })) {
		behavior.sequence.push_back(MakeSequenceItem(SequenceItemKind::EmitSignal));
	}
}

void PromoteBindingToGlobal(BehaviorBinding& binding, GlobalBehaviorRegistry& registry) {
	if (binding.global_reference) {
		return;
	}
	BehaviorDefinition global{ std::move(binding.local_definition) };
	const Id id{ global.id };
	registry.definitions.push_back(std::move(global));
	binding.global_reference = true;
	binding.global_behavior_id = id;
	binding.local_definition = {};
	binding.runtime = {};
}

void DetachBindingToLocal(BehaviorBinding& binding, GlobalBehaviorRegistry& registry) {
	if (!binding.global_reference) {
		return;
	}
	const auto* global{ registry.Find(binding.global_behavior_id) };
	binding.local_definition = global ? CloneBehavior(*global) : BehaviorDefinition{};
	if (global) {
		binding.local_definition.name.Assign(std::string{ global->name.Data() } + " Local");
	}
	binding.global_reference = false;
	binding.global_behavior_id = 0;
	binding.runtime = {};
}

void DrawRuntimeButtons(
	EntityData& entity,
	BehaviorBinding& binding,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	if (ImGui::SmallButton(binding.runtime.running ? "Restart" : "Play")) {
		StartBehaviorRuntime(entity, binding, registry, context);
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!binding.runtime.running);
	if (ImGui::SmallButton(binding.runtime.paused ? "Resume" : "Pause")) {
		binding.runtime.paused = !binding.runtime.paused;
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::SmallButton("Stop")) {
		binding.runtime = {};
	}
}

bool DrawBehaviorBinding(
	EntityData& entity,
	BehaviorBinding& binding,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	auto* behavior{ ResolveBehavior(binding, registry) };

	if (!behavior) {
		ImGui::TextDisabled("Missing global behavior");
		ImGui::SameLine();
		return ImGui::SmallButton("Remove");
	}

	bool remove{ false };

	ImGui::PushID(static_cast<int>(binding.id));

	char header[180]{};
	std::snprintf(
		header,
		sizeof(header),
		"%s%s%s",
		binding.runtime.running ? "> " : "",
		binding.global_reference ? "[Global] " : "",
		behavior->name.Data()
	);

	const bool open{
		DrawFramedDisclosureHeader(
			"BehaviorHeader",
			header,
			&binding.enabled,
			remove,
			true
		)
	};

	if (open) {
		ImGui::Indent(12.0f);

		if (ImGui::BeginTable(
				"BehaviorSettings",
				3,
				ImGuiTableFlags_SizingStretchProp
			)) {
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Global", ImGuiTableColumnFlags_WidthFixed, 60.0f);
			ImGui::TableSetupColumn("Reentry", ImGuiTableColumnFlags_WidthFixed, 185.0f);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputText("##Name", behavior->name.Data(), behavior->name.Size());

			ImGui::TableSetColumnIndex(1);
			bool global{ binding.global_reference };

			if (ImGui::Checkbox("Global", &global)) {
				if (global) {
					PromoteBindingToGlobal(binding, registry);
				} else {
					DetachBindingToLocal(binding, registry);
				}

				behavior = ResolveBehavior(binding, registry);
			}

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					binding.global_reference
						? "Shared definition. Untick to make a local copy."
						: "Move this definition into the global registry."
				);
			}

			ImGui::TableSetColumnIndex(2);
			DrawEnumCombo(
				"##Reentry",
				behavior->reentry,
				kReentryNames
			);

			ImGui::EndTable();
		}

		DrawRuntimeButtons(entity, binding, registry, context);

		char trigger_label[64]{};
		std::snprintf(
			trigger_label,
			sizeof(trigger_label),
			"Triggers (%zu)",
			behavior->triggers.size()
		);

		if (DrawDisclosureRow("TriggersSection", trigger_label, true)) {
			ImGui::Indent(12.0f);

			int remove_trigger{ -1 };

			for (int i{ 0 }; i < static_cast<int>(behavior->triggers.size()); ++i) {
				if (DrawTriggerCompact(
						behavior->triggers[static_cast<std::size_t>(i)]
					)) {
					remove_trigger = i;
				}
			}

			if (remove_trigger >= 0) {
				behavior->triggers.erase(
					behavior->triggers.begin() + remove_trigger
				);
			}

			if (ImGui::SmallButton("+ Trigger")) {
				behavior->triggers.emplace_back();
			}

			ImGui::Unindent(12.0f);
		}

		char sequence_label[64]{};
		std::snprintf(
			sequence_label,
			sizeof(sequence_label),
			"Sequence (%zu)",
			behavior->sequence.size()
		);

		if (DrawDisclosureRow("SequenceSection", sequence_label, true)) {
			ImGui::Indent(12.0f);
			DrawBehaviorSequence(*behavior, binding);
			ImGui::Unindent(12.0f);
		}

		ImGui::Unindent(12.0f);
	}

	ImGui::PopID();
	return remove;
}

BehaviorBinding MakeLocalBinding() {
	BehaviorBinding binding;
	binding.local_definition.triggers.emplace_back();
	binding.local_definition.sequence.push_back(MakeSequenceItem(SequenceItemKind::Action));
	return binding;
}

BehaviorBinding MakeGlobalBinding(Id id) {
	BehaviorBinding binding;
	binding.global_reference = true;
	binding.global_behavior_id = id;
	return binding;
}

void DrawAddBehaviorPopup(
	BehaviorsComponent& component,
	GlobalBehaviorRegistry& registry
) {
	if (!ImGui::BeginPopup("AddBehaviorPopup")) {
		return;
	}

	if (ImGui::MenuItem("Create New Behavior")) {
		component.bindings.push_back(MakeLocalBinding());
	}

	ImGui::SeparatorText("Existing Global Behaviors");

	if (registry.definitions.empty()) {
		ImGui::TextDisabled("No global behaviors");
	}

	for (const auto& definition : registry.definitions) {
		const bool attached{
			std::ranges::any_of(
				component.bindings,
				[&definition](const auto& binding) {
					return binding.global_reference &&
						   binding.global_behavior_id == definition.id;
				}
			)
		};

		ImGui::BeginDisabled(attached);

		if (ImGui::MenuItem(definition.name.Data())) {
			component.bindings.push_back(MakeGlobalBinding(definition.id));
		}

		ImGui::EndDisabled();

		if (attached && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("Already attached to this entity");
		}
	}

	ImGui::EndPopup();
}

void DrawBehaviorsComponent(
	EntityData& entity,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	auto& component{ *entity.behaviors };

	ImGui::PushID("BehaviorsComponent");

	bool remove_component{ false };

	char header[96]{};
	std::snprintf(
		header,
		sizeof(header),
		"Behaviors (%zu)",
		component.bindings.size()
	);

	const bool open{
		DrawFramedDisclosureHeader(
			"ComponentHeader",
			header,
			nullptr,
			remove_component,
			true
		)
	};

	if (remove_component) {
		entity.behaviors.reset();
		ImGui::PopID();
		return;
	}

	if (open) {
		ImGui::Indent(8.0f);

		int remove_index{ -1 };

		for (int i{ 0 }; i < static_cast<int>(component.bindings.size()); ++i) {
			if (DrawBehaviorBinding(
					entity,
					component.bindings[static_cast<std::size_t>(i)],
					registry,
					context
				)) {
				remove_index = i;
			}
		}

		if (remove_index >= 0) {
			component.bindings.erase(
				component.bindings.begin() + remove_index
			);
		}

		if (ImGui::Button("+ Add Behavior", ImVec2{ -FLT_MIN, 0.0f })) {
			ImGui::OpenPopup("AddBehaviorPopup");
		}

		DrawAddBehaviorPopup(component, registry);

		if (DrawDisclosureRow("ActivitySection", "Demo Activity", false)) {
			if (context.activity.empty()) {
				ImGui::TextDisabled("No activity yet.");
			}

			for (auto it{ context.activity.rbegin() }; it != context.activity.rend(); ++it) {
				ImGui::BulletText("%s", it->text.c_str());
			}
		}

		ImGui::Unindent(8.0f);
	}

	ImGui::PopID();
}

void DrawInspector(EntityData& entity, GlobalBehaviorRegistry& registry, DemoRuntimeContext& context) {
	ImGui::TextDisabled("Entity");
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputText("##Name", entity.name.Data(), entity.name.Size());

	if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
		ImGui::DragFloat2("Position", entity.position, 1.0f, -100000.0f, 100000.0f, "%.0f");
		ImGui::DragFloat("Rotation", &entity.rotation, 1.0f, -3600.0f, 3600.0f, "%.1f deg");
		ImGui::DragFloat2("Scale", entity.scale, 0.01f, -1000.0f, 1000.0f, "%.2f");
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Visible", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
		ImGui::Checkbox("Value", &entity.visible);
		ImGui::TreePop();
	}
	if (entity.behaviors) {
		DrawBehaviorsComponent(entity, registry, context);
	}

	if (ImGui::Button("+ Add Component", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddComponentPopup");
	}
	if (ImGui::BeginPopup("AddComponentPopup")) {
		ImGui::BeginDisabled(entity.behaviors.has_value());
		if (ImGui::MenuItem("Behaviors")) {
			entity.behaviors.emplace();
		}
		ImGui::EndDisabled();
		ImGui::EndPopup();
	}
}

std::vector<EntityData> MakeDemoEntities(GlobalBehaviorRegistry& registry) {
	BehaviorDefinition celebration;
	celebration.name.Assign("Door Celebration");
	celebration.reentry = ReentryMode::Restart;
	TriggerDefinition signal_trigger;
	signal_trigger.kind = TriggerKind::Signal;
	signal_trigger.signal.Assign("door.opened");
	celebration.triggers.push_back(std::move(signal_trigger));

	auto celebration_audio{ MakeSequenceItem(SequenceItemKind::Action) };
	auto& audio_action{ std::get<ActionItem>(celebration_audio.data).action };
	audio_action = MakeAction(ActionKind::PlayAudio);
	std::get<PlayAudioParams>(audio_action.parameters).asset.Assign("success_chime");
	celebration.sequence.push_back(std::move(celebration_audio));

	auto celebration_rotate{ MakeSequenceItem(SequenceItemKind::TimedAction) };
	auto& rotate{ std::get<TimedActionItem>(celebration_rotate.data) };
	rotate.action = MakeAction(ActionKind::RotateTo);
	std::get<RotateToParams>(rotate.action.parameters).degrees = 360.0f;
	rotate.duration_ms = 750.0f;
	rotate.ease = Ease::OutBack;
	celebration.sequence.push_back(std::move(celebration_rotate));

	auto celebration_wait{ MakeSequenceItem(SequenceItemKind::Wait) };
	std::get<WaitItem>(celebration_wait.data).duration_ms = 250.0f;
	celebration.sequence.push_back(std::move(celebration_wait));

	const Id celebration_id{ celebration.id };
	registry.definitions.push_back(std::move(celebration));

	BehaviorDefinition damage_flash;
	damage_flash.name.Assign("Damage Flash");
	damage_flash.reentry = ReentryMode::Restart;
	TriggerDefinition damaged;
	damaged.kind = TriggerKind::Signal;
	damaged.signal.Assign("player.damaged");
	damage_flash.triggers.push_back(std::move(damaged));
	auto damage_action_item{ MakeSequenceItem(SequenceItemKind::Action) };
	auto& damage_action{ std::get<ActionItem>(damage_action_item.data).action };
	damage_action = MakeAction(ActionKind::ApplyDamage);
	std::get<ApplyDamageParams>(damage_action.parameters).amount = 25.0f;
	damage_flash.sequence.push_back(std::move(damage_action_item));
	const Id damage_id{ damage_flash.id };
	registry.definitions.push_back(std::move(damage_flash));

	std::vector<EntityData> entities;

	EntityData door;
	door.name.Assign("Door");
	door.position[0] = 120.0f;
	door.position[1] = 40.0f;
	door.behaviors.emplace();
	BehaviorBinding open_door{ MakeLocalBinding() };
	auto& open{ open_door.local_definition };
	open.name.Assign("Open Door");
	open.triggers.clear();
	open.sequence.clear();
	TriggerDefinition overlap;
	overlap.kind = TriggerKind::OverlapStart;
	overlap.other_tag.Assign("Player");
	open.triggers.push_back(std::move(overlap));

	auto move{ MakeSequenceItem(SequenceItemKind::TimedAction) };
	auto& timed_move{ std::get<TimedActionItem>(move.data) };
	timed_move.action = MakeAction(ActionKind::MoveTo);
	auto& move_params{ std::get<MoveToParams>(timed_move.action.parameters) };
	move_params.destination[1] = 64.0f;
	timed_move.duration_ms = 300.0f;
	timed_move.ease = Ease::OutCubic;
	open.sequence.push_back(std::move(move));

	auto wait{ MakeSequenceItem(SequenceItemKind::Wait) };
	std::get<WaitItem>(wait.data).duration_ms = 100.0f;
	open.sequence.push_back(std::move(wait));

	auto collider{ MakeSequenceItem(SequenceItemKind::Action) };
	auto& collider_action{ std::get<ActionItem>(collider.data).action };
	collider_action = MakeAction(ActionKind::SetColliderMode);
	std::get<SetColliderModeParams>(collider_action.parameters).mode = 0;
	open.sequence.push_back(std::move(collider));

	auto audio{ MakeSequenceItem(SequenceItemKind::Action) };
	auto& door_audio{ std::get<ActionItem>(audio.data).action };
	door_audio = MakeAction(ActionKind::PlayAudio);
	std::get<PlayAudioParams>(door_audio.parameters).asset.Assign("door_open");
	open.sequence.push_back(std::move(audio));

	auto emit{ MakeSequenceItem(SequenceItemKind::EmitSignal) };
	std::get<EmitSignalItem>(emit.data).signal.Assign("door.opened");
	open.sequence.push_back(std::move(emit));
	door.behaviors->bindings.push_back(std::move(open_door));
	entities.push_back(std::move(door));

	EntityData light;
	light.name.Assign("Celebration Light");
	light.position[0] = 260.0f;
	light.position[1] = 40.0f;
	light.behaviors.emplace();
	light.behaviors->bindings.push_back(MakeGlobalBinding(celebration_id));
	entities.push_back(std::move(light));

	EntityData player;
	player.name.Assign("Player");
	player.position[0] = -60.0f;
	player.position[1] = 40.0f;
	player.behaviors.emplace();
	player.behaviors->bindings.push_back(MakeGlobalBinding(damage_id));
	entities.push_back(std::move(player));

	EntityData camera;
	camera.name.Assign("Main Camera");
	entities.push_back(std::move(camera));
	return entities;
}

void DrawHierarchy(std::vector<EntityData>& entities, int& selected_index) {
	ImGui::TextDisabled("Scene Hierarchy");
	ImGui::Separator();
	for (int i{ 0 }; i < static_cast<int>(entities.size()); ++i) {
		if (ImGui::Selectable(entities[static_cast<std::size_t>(i)].name.Data(), selected_index == i, 0, ImVec2{ 0.0f, 25.0f })) {
			selected_index = i;
		}
	}
	if (ImGui::Button("+ Entity", ImVec2{ -FLT_MIN, 0.0f })) {
		EntityData entity;
		entity.name.Assign("New Entity");
		entities.push_back(std::move(entity));
		selected_index = static_cast<int>(entities.size()) - 1;
	}
}

void DrawSceneView(const std::vector<EntityData>& entities, int selected_index) {
	const ImVec2 start{ ImGui::GetCursorScreenPos() };
	const ImVec2 size{ ImGui::GetContentRegionAvail() };
	ImDrawList* draw{ ImGui::GetWindowDrawList() };
	draw->AddRectFilled(start, ImVec2{ start.x + size.x, start.y + size.y }, ImGui::GetColorU32(ImGuiCol_FrameBg));
	const ImVec2 center{ start.x + size.x * 0.5f, start.y + size.y * 0.5f };

	for (int i{ 0 }; i < static_cast<int>(entities.size()); ++i) {
		const auto& entity{ entities[static_cast<std::size_t>(i)] };
		const ImVec2 p{ center.x + entity.position[0], center.y - entity.position[1] };
		const ImU32 color{ ImGui::GetColorU32(i == selected_index ? ImGuiCol_ButtonHovered : ImGuiCol_Button) };
		draw->AddRectFilled(ImVec2{ p.x - 42.0f, p.y - 18.0f }, ImVec2{ p.x + 42.0f, p.y + 18.0f }, color, 3.0f);
		draw->AddText(ImVec2{ p.x - 36.0f, p.y - 6.0f }, ImGui::GetColorU32(ImGuiCol_Text), entity.name.Data());
	}
	draw->AddText(
		ImVec2{ start.x + 10.0f, start.y + 10.0f },
		ImGui::GetColorU32(ImGuiCol_TextDisabled),
		"Select Door, then Play Open Door. Its final Emit Signal starts the global Door Celebration."
	);
	ImGui::InvisibleButton("SceneCanvas", size);
}

void DrawApplication(
	std::vector<EntityData>& entities,
	int& selected_index,
	GlobalBehaviorRegistry& registry,
	DemoRuntimeContext& context
) {
	ImGuiViewport* viewport{ ImGui::GetMainViewport() };
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	constexpr ImGuiWindowFlags flags{
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus
	};
	ImGui::Begin("Behavior Component Inspector Demo", nullptr, flags);

	if (ImGui::BeginTable("Layout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Hierarchy", ImGuiTableColumnFlags_WidthFixed, 200.0f);
		ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 500.0f);

		ImGui::TableNextColumn();
		ImGui::BeginChild("HierarchyChild");
		DrawHierarchy(entities, selected_index);
		ImGui::EndChild();

		ImGui::TableNextColumn();
		ImGui::BeginChild("SceneChild");
		DrawSceneView(entities, selected_index);
		ImGui::EndChild();

		ImGui::TableNextColumn();
		ImGui::BeginChild("InspectorChild");
		ImGui::TextDisabled("Inspector");
		ImGui::Separator();
		if (selected_index >= 0 && selected_index < static_cast<int>(entities.size())) {
			DrawInspector(entities[static_cast<std::size_t>(selected_index)], registry, context);
		}
		ImGui::EndChild();
		ImGui::EndTable();
	}
	ImGui::End();
}

void ConfigureStyle() {
	ImGui::StyleColorsDark();
	ImGuiStyle& style{ ImGui::GetStyle() };
	style.WindowRounding = 0.0f;
	style.ChildRounding = 2.0f;
	style.FrameRounding = 2.0f;
	style.PopupRounding = 3.0f;
	style.ScrollbarRounding = 3.0f;
	style.GrabRounding = 2.0f;
	style.WindowPadding = ImVec2{ 7.0f, 7.0f };
	style.FramePadding = ImVec2{ 5.0f, 3.0f };
	style.ItemSpacing = ImVec2{ 5.0f, 4.0f };
	style.ItemInnerSpacing = ImVec2{ 4.0f, 3.0f };
	style.IndentSpacing = 14.0f;
}

void GlfwErrorCallback(int error, const char* description) {
	std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

} // namespace demo

int main() {
	glfwSetErrorCallback(demo::GlfwErrorCallback);
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

	GLFWwindow* window{ glfwCreateWindow(1500, 900, "Protegon Behaviors Inspector Demo", nullptr, nullptr) };
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
	demo::ConfigureStyle();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	demo::GlobalBehaviorRegistry registry;
	auto entities{ demo::MakeDemoEntities(registry) };
	demo::DemoRuntimeContext runtime;
	int selected_entity{ 0 };

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		const float dt{ ImGui::GetIO().DeltaTime };
		for (auto& entity : entities) {
			if (!entity.behaviors) {
				continue;
			}
			for (auto& binding : entity.behaviors->bindings) {
				demo::UpdateBehaviorRuntime(entity, binding, dt, registry, runtime);
			}
		}
		demo::DispatchSignals(entities, registry, runtime);
		demo::UpdateActivity(runtime, dt);
		demo::DrawApplication(entities, selected_entity, registry, runtime);

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
