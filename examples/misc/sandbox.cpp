// behavior_sequence_imgui_demo.cpp
//
// Standalone Dear ImGui mock-up for authoring serialized script/tween behavior sequences.
//
// Expected dependencies:
//   - Dear ImGui
//   - backends/imgui_impl_glfw.cpp
//   - backends/imgui_impl_opengl3.cpp
//   - GLFW
//   - GLAD / OpenGL
//
// Example CMake target shape:
//
//   add_executable(behavior_sequence_demo behavior_sequence_imgui_demo.cpp)
//   target_link_libraries(behavior_sequence_demo PRIVATE imgui glfw glad)
//
// The exact library target names depend on your project.

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glad/gl.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
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

	TextBuffer(std::string_view text) {
		Assign(text);
	}

	void Assign(std::string_view text) {
		const auto count{ std::min(text.size(), N - 1) };
		std::memcpy(data.data(), text.data(), count);
		data[count] = '\0';

		if (count + 1 < N) {
			std::fill(data.begin() + static_cast<std::ptrdiff_t>(count + 1), data.end(), '\0');
		}
	}

	[[nodiscard]] char* Data() {
		return data.data();
	}

	[[nodiscard]] const char* Data() const {
		return data.data();
	}

	[[nodiscard]] constexpr std::size_t Size() const {
		return N;
	}

	[[nodiscard]] bool Empty() const {
		return data[0] == '\0';
	}
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

enum class CompletionPolicy {
	Duration,
	AllScriptsComplete,
	AnyScriptComplete
};

enum class Ease {
	Linear,
	InQuad,
	OutQuad,
	InOutQuad,
	OutCubic,
	OutBack
};

enum class ScriptKind {
	SetVisible,
	MoveTo,
	RotateTo,
	PlayAudio,
	EmitSignal,
	SetColliderMode,
	StartBehavior,
	ApplyDamage
};

enum class ScriptPhase {
	OnStart,
	During,
	OnComplete
};

constexpr std::array kTriggerNames{ "On Create", "Key Pressed", "Overlap Start",
									"Signal",	 "Timer",		"Manual" };

constexpr std::array kReentryNames{ "Ignore While Running", "Restart", "Queue", "Parallel" };

constexpr std::array kCompletionNames{ "Duration", "All Scripts Complete", "Any Script Complete" };

constexpr std::array kEaseNames{ "Linear",		"In Quad",	 "Out Quad",
								 "In-Out Quad", "Out Cubic", "Out Back" };

constexpr std::array kColliderModeNames{ "None", "Overlap", "Discrete", "Continuous" };

template <typename TEnum, std::size_t N>
bool DrawEnumCombo(const char* label, TEnum& value, const std::array<const char*, N>& names) {
	int current{ static_cast<int>(value) };
	bool changed{ false };

	if (ImGui::BeginCombo(label, names[static_cast<std::size_t>(current)])) {
		for (int i{ 0 }; i < static_cast<int>(N); ++i) {
			const bool selected{ current == i };

			if (ImGui::Selectable(names[static_cast<std::size_t>(i)], selected)) {
				current = i;
				changed = true;
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	if (changed) {
		value = static_cast<TEnum>(current);
	}

	return changed;
}

struct TriggerDefinition {
	Id id{ NextId() };
	TriggerKind kind{ TriggerKind::OnCreate };
	bool enabled{ true };

	TextBuffer<64> key{ "Space" };
	TextBuffer<64> other_tag{ "Player" };
	TextBuffer<96> signal{ "door.opened" };
	float timer_seconds{ 1.0f };
};

struct SetVisibleParams {
	bool visible{ true };
};

struct MoveToParams {
	float destination[2]{ 0.0f, 64.0f };
	bool relative{ true };
};

struct RotateToParams {
	float degrees{ 90.0f };
	bool shortest_path{ true };
};

struct PlayAudioParams {
	TextBuffer<96> asset{ "door_open" };
	float volume{ 1.0f };
	bool loop{ false };
};

struct EmitSignalParams {
	TextBuffer<96> signal{ "door.opened" };
};

struct SetColliderModeParams {
	int mode{ 0 };
};

struct StartBehaviorParams {
	TextBuffer<96> behavior{ "Damage Flash" };
	bool restart{ false };
};

struct ApplyDamageParams {
	float amount{ 10.0f };
	bool critical{ false };
	TextBuffer<64> damage_type{ "Physical" };
};

using ScriptParameters = std::variant<
	SetVisibleParams, MoveToParams, RotateToParams, PlayAudioParams, EmitSignalParams,
	SetColliderModeParams, StartBehaviorParams, ApplyDamageParams>;

struct ScriptDefinition {
	Id id{ NextId() };
	ScriptKind kind{ ScriptKind::SetVisible };
	bool enabled{ true };
	ScriptParameters parameters{ SetVisibleParams{} };
};

struct ScriptDescriptor {
	ScriptKind kind;
	const char* key;
	const char* label;
	const char* group;
	const char* description;
	const char* source;
};

constexpr std::array kScriptRegistry{
	ScriptDescriptor{ ScriptKind::SetVisible, "engine.set_visible", "Set Visible", "Entity",
					  "Changes whether the selected entity is visible.", "Engine" },
	ScriptDescriptor{ ScriptKind::MoveTo, "engine.move_to", "Move To", "Transform",
					  "Interpolates an entity to a target position.", "Engine" },
	ScriptDescriptor{ ScriptKind::RotateTo, "engine.rotate_to", "Rotate To", "Transform",
					  "Interpolates an entity to a target rotation.", "Engine" },
	ScriptDescriptor{ ScriptKind::PlayAudio, "engine.play_audio", "Play Audio", "Audio",
					  "Plays an audio asset with configurable volume and looping.", "Engine" },
	ScriptDescriptor{ ScriptKind::EmitSignal, "engine.emit_signal", "Emit Signal", "Flow",
					  "Emits a named signal that can trigger another behavior.", "Engine" },
	ScriptDescriptor{ ScriptKind::SetColliderMode, "engine.set_collider_mode", "Set Collider Mode",
					  "Physics", "Changes the collision mode of the selected entity.", "Engine" },
	ScriptDescriptor{ ScriptKind::StartBehavior, "engine.start_behavior", "Start Behavior", "Flow",
					  "Starts another behavior by its stable authored identifier.", "Engine" },
	ScriptDescriptor{ ScriptKind::ApplyDamage, "game.apply_damage", "Apply Damage", "Game",
					  "Example user-registered script with reflected custom parameters.", "User" },
};

const ScriptDescriptor& GetScriptDescriptor(ScriptKind kind) {
	const auto it{ std::find_if(
		kScriptRegistry.begin(), kScriptRegistry.end(),
		[kind](const ScriptDescriptor& descriptor) { return descriptor.kind == kind; }
	) };

	return it != kScriptRegistry.end() ? *it : kScriptRegistry.front();
}

ScriptDefinition MakeScript(ScriptKind kind) {
	ScriptDefinition script;
	script.kind = kind;

	switch (kind) {
		case ScriptKind::SetVisible:	  script.parameters = SetVisibleParams{}; break;
		case ScriptKind::MoveTo:		  script.parameters = MoveToParams{}; break;
		case ScriptKind::RotateTo:		  script.parameters = RotateToParams{}; break;
		case ScriptKind::PlayAudio:		  script.parameters = PlayAudioParams{}; break;
		case ScriptKind::EmitSignal:	  script.parameters = EmitSignalParams{}; break;
		case ScriptKind::SetColliderMode: script.parameters = SetColliderModeParams{}; break;
		case ScriptKind::StartBehavior:	  script.parameters = StartBehaviorParams{}; break;
		case ScriptKind::ApplyDamage:	  script.parameters = ApplyDamageParams{}; break;
	}

	return script;
}

struct SequenceStepDefinition {
	Id id{ NextId() };
	TextBuffer<96> name{ "New Step" };

	float delay_ms{ 0.0f };
	float duration_ms{ 300.0f };
	Ease ease{ Ease::Linear };

	int repeats{ 0 };
	bool infinite_repeats{ false };
	bool reversed{ false };
	bool yoyo{ false };

	CompletionPolicy completion{ CompletionPolicy::Duration };

	std::vector<ScriptDefinition> on_start;
	std::vector<ScriptDefinition> during;
	std::vector<ScriptDefinition> on_complete;
};

struct RuntimeState {
	bool running{ false };
	bool paused{ false };
	bool completed{ false };
	bool queued{ false };

	std::size_t current_step{ 0 };
	float step_elapsed_ms{ 0.0f };
	int completed_runs{ 0 };
};

struct BehaviorDefinition {
	Id id{ NextId() };
	TextBuffer<96> name{ "New Behavior" };
	bool enabled{ true };

	ReentryMode reentry{ ReentryMode::IgnoreWhileRunning };
	bool destroy_owner_on_complete{ false };

	std::vector<TriggerDefinition> triggers;
	std::vector<SequenceStepDefinition> steps;

	RuntimeState runtime;
};

struct SignalEvent {
	std::string name;
	Id source_behavior{ 0 };
};

struct SignalLogEntry {
	std::string text;
	float remaining_seconds{ 5.0f };
};

struct SequenceRuntimeContext {
	std::deque<SignalEvent> pending_signals;
	std::vector<SignalLogEntry> signal_log;
};

ScriptDefinition CloneScript(const ScriptDefinition& source) {
	ScriptDefinition copy{ source };
	copy.id = NextId();
	return copy;
}

SequenceStepDefinition CloneStep(const SequenceStepDefinition& source) {
	SequenceStepDefinition copy{ source };
	copy.id = NextId();

	for (auto& script : copy.on_start) {
		script.id = NextId();
	}

	for (auto& script : copy.during) {
		script.id = NextId();
	}

	for (auto& script : copy.on_complete) {
		script.id = NextId();
	}

	return copy;
}

BehaviorDefinition CloneBehavior(const BehaviorDefinition& source) {
	BehaviorDefinition copy{ source };
	copy.id		 = NextId();
	copy.runtime = {};

	for (auto& trigger : copy.triggers) {
		trigger.id = NextId();
	}

	for (auto& step : copy.steps) {
		step = CloneStep(step);
	}

	return copy;
}

void PushSignalLog(SequenceRuntimeContext& context, std::string text) {
	context.signal_log.push_back(
		SignalLogEntry{
			.text			   = std::move(text),
			.remaining_seconds = 5.0f,
		}
	);

	constexpr std::size_t kMaxSignalLogEntries{ 12 };

	if (context.signal_log.size() > kMaxSignalLogEntries) {
		context.signal_log.erase(
			context.signal_log.begin(),
			context.signal_log.begin() +
				static_cast<std::ptrdiff_t>(context.signal_log.size() - kMaxSignalLogEntries)
		);
	}
}

void ExecuteScripts(
	const std::vector<ScriptDefinition>& scripts, const BehaviorDefinition& behavior,
	SequenceRuntimeContext& context
) {
	for (const auto& script : scripts) {
		if (!script.enabled) {
			continue;
		}

		switch (script.kind) {
			case ScriptKind::EmitSignal: {
				const auto& params{ std::get<EmitSignalParams>(script.parameters) };

				if (params.signal.Empty()) {
					break;
				}

				context.pending_signals.push_back(
					SignalEvent{
						.name			 = params.signal.Data(),
						.source_behavior = behavior.id,
					}
				);

				PushSignalLog(
					context, std::string{ behavior.name.Data() } + " emitted \"" +
								 params.signal.Data() + "\""
				);
				break;
			}

			default:
				// Other script kinds are only visualized by this standalone UI demo.
				break;
		}
	}
}

void EnterCurrentStep(BehaviorDefinition& behavior, SequenceRuntimeContext& context) {
	auto& runtime{ behavior.runtime };

	if (!runtime.running || runtime.current_step >= behavior.steps.size()) {
		return;
	}

	const auto& step{ behavior.steps[runtime.current_step] };
	ExecuteScripts(step.on_start, behavior, context);
}

void StartRuntime(
	BehaviorDefinition& behavior, SequenceRuntimeContext& context, bool restart = true
) {
	if (!behavior.enabled) {
		return;
	}

	auto& runtime{ behavior.runtime };

	if (runtime.running && !restart) {
		switch (behavior.reentry) {
			case ReentryMode::IgnoreWhileRunning: return;

			case ReentryMode::Restart:			  break;

			case ReentryMode::Queue:			  runtime.queued = true; return;

			case ReentryMode::Parallel:
				// The demo visualizes one runtime. A real implementation would create another
				// independent runtime instance here.
				break;
		}
	}

	runtime.running			= !behavior.steps.empty();
	runtime.paused			= false;
	runtime.completed		= behavior.steps.empty();
	runtime.current_step	= 0;
	runtime.step_elapsed_ms = 0.0f;

	if (runtime.running) {
		PushSignalLog(context, std::string{ behavior.name.Data() } + " started");
		EnterCurrentStep(behavior, context);
	}
}

void StopRuntime(BehaviorDefinition& behavior) {
	behavior.runtime = {};
}

void CompleteCurrentStep(BehaviorDefinition& behavior, SequenceRuntimeContext& context) {
	auto& runtime{ behavior.runtime };

	if (!runtime.running || behavior.steps.empty() ||
		runtime.current_step >= behavior.steps.size()) {
		return;
	}

	const auto& completed_step{ behavior.steps[runtime.current_step] };
	ExecuteScripts(completed_step.on_complete, behavior, context);

	if (runtime.current_step + 1 < behavior.steps.size()) {
		++runtime.current_step;
		runtime.step_elapsed_ms = 0.0f;
		EnterCurrentStep(behavior, context);
		return;
	}

	runtime.running	  = false;
	runtime.paused	  = false;
	runtime.completed = true;
	++runtime.completed_runs;

	PushSignalLog(context, std::string{ behavior.name.Data() } + " completed");

	if (runtime.queued) {
		runtime.queued = false;
		StartRuntime(behavior, context);
	}
}

float GetActiveStepProgress(const BehaviorDefinition& behavior) {
	const auto& runtime{ behavior.runtime };

	if (!runtime.running || runtime.current_step >= behavior.steps.size()) {
		return runtime.completed ? 1.0f : 0.0f;
	}

	const auto& step{ behavior.steps[runtime.current_step] };
	const float active_elapsed{ runtime.step_elapsed_ms - step.delay_ms };

	if (active_elapsed <= 0.0f) {
		return 0.0f;
	}

	if (step.duration_ms <= 0.0f) {
		return 1.0f;
	}

	return std::clamp(active_elapsed / step.duration_ms, 0.0f, 1.0f);
}

void UpdateRuntime(
	BehaviorDefinition& behavior, float delta_seconds, SequenceRuntimeContext& context
) {
	auto& runtime{ behavior.runtime };

	if (!runtime.running || runtime.paused || runtime.current_step >= behavior.steps.size()) {
		return;
	}

	const auto& step{ behavior.steps[runtime.current_step] };
	runtime.step_elapsed_ms += delta_seconds * 1000.0f;

	// During scripts would receive progress here in the real engine.
	// This demo keeps their parameter UI visible while simulating timing only.
	const float total_duration{ std::max(0.0f, step.delay_ms) + std::max(0.0f, step.duration_ms) };

	if (runtime.step_elapsed_ms >= total_duration) {
		CompleteCurrentStep(behavior, context);
	}
}

bool HasMatchingSignalTrigger(const BehaviorDefinition& behavior, std::string_view signal) {
	return behavior.enabled &&
		   std::ranges::any_of(behavior.triggers, [signal](const TriggerDefinition& trigger) {
			   return trigger.enabled && trigger.kind == TriggerKind::Signal &&
					  std::string_view{ trigger.signal.Data() } == signal;
		   });
}

void DispatchPendingSignals(
	std::vector<BehaviorDefinition>& behaviors, SequenceRuntimeContext& context
) {
	while (!context.pending_signals.empty()) {
		SignalEvent event{ std::move(context.pending_signals.front()) };
		context.pending_signals.pop_front();

		for (auto& behavior : behaviors) {
			if (!HasMatchingSignalTrigger(behavior, event.name)) {
				continue;
			}

			PushSignalLog(
				context, std::string{ behavior.name.Data() } + " received \"" + event.name + "\""
			);

			StartRuntime(behavior, context, false);
		}
	}
}

void UpdateSignalLog(SequenceRuntimeContext& context, float delta_seconds) {
	for (auto& entry : context.signal_log) {
		entry.remaining_seconds -= delta_seconds;
	}

	std::erase_if(context.signal_log, [](const SignalLogEntry& entry) {
		return entry.remaining_seconds <= 0.0f;
	});
}

template <typename T>
void MoveItem(std::vector<T>& items, int from, int to) {
	if (from < 0 || to < 0 || from >= static_cast<int>(items.size()) ||
		to >= static_cast<int>(items.size()) || from == to) {
		return;
	}

	T moved{ std::move(items[static_cast<std::size_t>(from)]) };
	items.erase(items.begin() + from);

	// "to" refers to the original hovered item. After erasing, inserting at the same
	// numeric index places a downward drag after that item and an upward drag before it.
	items.insert(items.begin() + to, std::move(moved));
}

void DrawHelpMarker(const char* text) {
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
		ImGui::TextUnformatted(text);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void DrawSectionTitle(const char* title) {
	ImGui::Spacing();
	ImGui::TextUnformatted(title);
	ImGui::Separator();
}

bool DrawTriggerEditor(TriggerDefinition& trigger) {
	bool remove{ false };

	ImGui::PushID(static_cast<int>(trigger.id));

	char header[160]{};
	std::snprintf(
		header, sizeof(header), "%s%s", trigger.enabled ? "" : "[Disabled] ",
		kTriggerNames[static_cast<std::size_t>(trigger.kind)]
	);

	const bool open{ ImGui::TreeNodeEx(
		"##trigger", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s",
		header
	) };

	if (ImGui::BeginPopupContextItem("TriggerContext")) {
		if (ImGui::MenuItem(trigger.enabled ? "Disable" : "Enable")) {
			trigger.enabled = !trigger.enabled;
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Remove")) {
			remove = true;
		}

		ImGui::EndPopup();
	}

	if (open) {
		ImGui::Checkbox("Enabled", &trigger.enabled);
		DrawEnumCombo("Type", trigger.kind, kTriggerNames);

		switch (trigger.kind) {
			case TriggerKind::OnCreate:
				ImGui::TextDisabled("Runs once when the behavior owner is created.");
				break;

			case TriggerKind::KeyPressed:
				ImGui::InputText("Key", trigger.key.Data(), trigger.key.Size());
				ImGui::TextDisabled("Demo field. Replace with your Key enum drawer.");
				break;

			case TriggerKind::OverlapStart:
				ImGui::InputText(
					"Other Entity Tag", trigger.other_tag.Data(), trigger.other_tag.Size()
				);
				break;

			case TriggerKind::Signal:
				ImGui::InputText("Signal", trigger.signal.Data(), trigger.signal.Size());
				break;

			case TriggerKind::Timer:
				ImGui::DragFloat(
					"After", &trigger.timer_seconds, 0.05f, 0.0f, 3600.0f, "%.2f s",
					ImGuiSliderFlags_AlwaysClamp
				);
				break;

			case TriggerKind::Manual:
				ImGui::TextDisabled("Triggered by code or the editor preview button.");
				break;
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
	return remove;
}

void DrawScriptParameters(ScriptDefinition& script) {
	switch (script.kind) {
		case ScriptKind::SetVisible: {
			auto& params{ std::get<SetVisibleParams>(script.parameters) };
			ImGui::Checkbox("Visible", &params.visible);
			break;
		}

		case ScriptKind::MoveTo: {
			auto& params{ std::get<MoveToParams>(script.parameters) };
			ImGui::DragFloat2("Destination", params.destination, 1.0f, -100000.0f, 100000.0f);
			ImGui::Checkbox("Relative", &params.relative);
			break;
		}

		case ScriptKind::RotateTo: {
			auto& params{ std::get<RotateToParams>(script.parameters) };
			ImGui::DragFloat("Degrees", &params.degrees, 1.0f, -3600.0f, 3600.0f, "%.1f deg");
			ImGui::Checkbox("Shortest Path", &params.shortest_path);
			break;
		}

		case ScriptKind::PlayAudio: {
			auto& params{ std::get<PlayAudioParams>(script.parameters) };
			ImGui::InputText("Audio Asset", params.asset.Data(), params.asset.Size());
			ImGui::SliderFloat("Volume", &params.volume, 0.0f, 1.0f, "%.2f");
			ImGui::Checkbox("Loop", &params.loop);
			break;
		}

		case ScriptKind::EmitSignal: {
			auto& params{ std::get<EmitSignalParams>(script.parameters) };
			ImGui::InputText("Signal", params.signal.Data(), params.signal.Size());
			break;
		}

		case ScriptKind::SetColliderMode: {
			auto& params{ std::get<SetColliderModeParams>(script.parameters) };

			if (ImGui::BeginCombo(
					"Mode", kColliderModeNames[static_cast<std::size_t>(params.mode)]
				)) {
				for (int i{ 0 }; i < static_cast<int>(kColliderModeNames.size()); ++i) {
					const bool selected{ params.mode == i };

					if (ImGui::Selectable(
							kColliderModeNames[static_cast<std::size_t>(i)], selected
						)) {
						params.mode = i;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			break;
		}

		case ScriptKind::StartBehavior: {
			auto& params{ std::get<StartBehaviorParams>(script.parameters) };
			ImGui::InputText("Behavior", params.behavior.Data(), params.behavior.Size());
			ImGui::Checkbox("Restart If Running", &params.restart);
			break;
		}

		case ScriptKind::ApplyDamage: {
			auto& params{ std::get<ApplyDamageParams>(script.parameters) };
			ImGui::DragFloat("Amount", &params.amount, 0.25f, 0.0f, 100000.0f, "%.2f");
			ImGui::InputText("Damage Type", params.damage_type.Data(), params.damage_type.Size());
			ImGui::Checkbox("Critical", &params.critical);
			break;
		}
	}
}

bool MatchesSearch(std::string_view text, std::string_view search) {
	if (search.empty()) {
		return true;
	}

	std::string lower_text{ text };
	std::string lower_search{ search };

	std::ranges::transform(lower_text, lower_text.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});

	std::ranges::transform(lower_search, lower_search.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});

	return lower_text.find(lower_search) != std::string::npos;
}

std::optional<ScriptDefinition> DrawAddScriptPopup(const char* popup_name) {
	std::optional<ScriptDefinition> result;

	if (!ImGui::BeginPopup(popup_name)) {
		return result;
	}

	static TextBuffer<96> search;
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint(
		"##ScriptSearch", "Search registered scripts...", search.Data(), search.Size()
	);

	ImGui::Separator();

	const std::string_view search_view{ search.Data() };
	const char* previous_group{ nullptr };

	for (const auto& descriptor : kScriptRegistry) {
		const bool matches{ MatchesSearch(descriptor.label, search_view) ||
							MatchesSearch(descriptor.key, search_view) ||
							MatchesSearch(descriptor.group, search_view) };

		if (!matches) {
			continue;
		}

		if (!previous_group || std::strcmp(previous_group, descriptor.group) != 0) {
			if (previous_group) {
				ImGui::Spacing();
			}

			ImGui::TextDisabled("%s", descriptor.group);
			previous_group = descriptor.group;
		}

		ImGui::PushID(static_cast<int>(descriptor.kind));

		if (ImGui::Selectable(descriptor.label)) {
			result = MakeScript(descriptor.kind);
			search.Assign("");
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(descriptor.description);
			ImGui::Separator();
			ImGui::TextDisabled("%s", descriptor.key);
			ImGui::TextDisabled("Source: %s", descriptor.source);
			ImGui::EndTooltip();
		}

		ImGui::PopID();
	}

	ImGui::EndPopup();
	return result;
}

struct ScriptDragPayload {
	Id list_id;
	int index;
};

void DrawScriptList(const char* label, std::vector<ScriptDefinition>& scripts, Id list_id) {
	ImGui::PushID(label);

	if (scripts.empty()) {
		ImGui::TextDisabled("No scripts.");
	}

	int remove_index{ -1 };
	int duplicate_index{ -1 };
	int move_from{ -1 };
	int move_to{ -1 };

	for (int i{ 0 }; i < static_cast<int>(scripts.size()); ++i) {
		auto& script{ scripts[static_cast<std::size_t>(i)] };
		const auto& descriptor{ GetScriptDescriptor(script.kind) };

		ImGui::PushID(static_cast<int>(script.id));

		char header[192]{};
		std::snprintf(
			header, sizeof(header), "%s%s", script.enabled ? "" : "[Disabled] ", descriptor.label
		);

		ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_DefaultOpen |
								  ImGuiTreeNodeFlags_SpanAvailWidth |
								  ImGuiTreeNodeFlags_AllowOverlap };

		const bool open{ ImGui::TreeNodeEx("##script", flags, "%s", header) };

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			const ScriptDragPayload payload{ list_id, i };
			ImGui::SetDragDropPayload("PTGN_SEQUENCE_SCRIPT", &payload, sizeof(payload));
			ImGui::Text("Move %s", descriptor.label);
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload{
					ImGui::AcceptDragDropPayload("PTGN_SEQUENCE_SCRIPT") }) {
				const auto* drag{ static_cast<const ScriptDragPayload*>(payload->Data) };

				if (drag && drag->list_id == list_id) {
					move_from = drag->index;
					move_to	  = i;
				}
			}

			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginPopupContextItem("ScriptContext")) {
			if (ImGui::MenuItem(script.enabled ? "Disable" : "Enable")) {
				script.enabled = !script.enabled;
			}

			if (ImGui::MenuItem("Duplicate")) {
				duplicate_index = i;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Remove")) {
				remove_index = i;
			}

			ImGui::EndPopup();
		}

		if (open) {
			ImGui::Checkbox("Enabled", &script.enabled);
			ImGui::TextDisabled("%s", descriptor.key);
			ImGui::TextWrapped("%s", descriptor.description);
			ImGui::Spacing();

			DrawScriptParameters(script);

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	if (move_from >= 0 && move_to >= 0) {
		MoveItem(scripts, move_from, move_to);
	}

	if (duplicate_index >= 0) {
		auto copy{ CloneScript(scripts[static_cast<std::size_t>(duplicate_index)]) };
		scripts.insert(scripts.begin() + duplicate_index + 1, std::move(copy));
	}

	if (remove_index >= 0) {
		scripts.erase(scripts.begin() + remove_index);
	}

	ImGui::Spacing();

	char add_label[128]{};
	std::snprintf(add_label, sizeof(add_label), "+ Add %s Script", label);

	if (ImGui::Button(add_label, ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddScriptPopup");
	}

	if (auto added{ DrawAddScriptPopup("AddScriptPopup") }) {
		scripts.emplace_back(std::move(*added));
	}

	ImGui::PopID();
}

void DrawStepEditor(
	SequenceStepDefinition& step, bool active, float active_progress, bool& request_remove,
	bool& request_duplicate
) {
	ImGui::PushID(static_cast<int>(step.id));

	char label[192]{};
	std::snprintf(
		label, sizeof(label), "%s%s", active ? "[Running] " : "",
		step.name.Empty() ? "Unnamed Step" : step.name.Data()
	);

	ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
							  ImGuiTreeNodeFlags_AllowOverlap };

	const bool open{ ImGui::TreeNodeEx("##step", flags, "%s", label) };

	if (ImGui::BeginPopupContextItem("StepContext")) {
		if (ImGui::MenuItem("Duplicate")) {
			request_duplicate = true;
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Remove")) {
			request_remove = true;
		}

		ImGui::EndPopup();
	}

	if (active) {
		ImGui::ProgressBar(active_progress, ImVec2{ -FLT_MIN, 3.0f }, "");
	}

	if (open) {
		ImGui::InputText("Name", step.name.Data(), step.name.Size());

		ImGui::DragFloat(
			"Delay Before", &step.delay_ms, 10.0f, 0.0f, 3600000.0f, "%.0f ms",
			ImGuiSliderFlags_AlwaysClamp
		);

		ImGui::DragFloat(
			"Duration", &step.duration_ms, 10.0f, 0.0f, 3600000.0f, "%.0f ms",
			ImGuiSliderFlags_AlwaysClamp
		);

		DrawEnumCombo("Ease", step.ease, kEaseNames);
		DrawEnumCombo("Completion", step.completion, kCompletionNames);

		ImGui::Checkbox("Infinite Repeats", &step.infinite_repeats);

		ImGui::BeginDisabled(step.infinite_repeats);
		ImGui::InputInt("Additional Repeats", &step.repeats);
		step.repeats = std::max(0, step.repeats);
		ImGui::EndDisabled();

		ImGui::Checkbox("Reversed", &step.reversed);
		ImGui::SameLine();
		ImGui::Checkbox("Yoyo", &step.yoyo);

		ImGui::Spacing();

		if (ImGui::BeginTabBar("ScriptPhases")) {
			if (ImGui::BeginTabItem("On Start")) {
				DrawScriptList(
					"On Start", step.on_start, step.id * 10 + static_cast<Id>(ScriptPhase::OnStart)
				);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("During")) {
				DrawScriptList(
					"During", step.during, step.id * 10 + static_cast<Id>(ScriptPhase::During)
				);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("On Complete")) {
				DrawScriptList(
					"On Complete", step.on_complete,
					step.id * 10 + static_cast<Id>(ScriptPhase::OnComplete)
				);
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

struct StepDragPayload {
	int index;
};

void DrawBehaviorInspector(BehaviorDefinition& behavior) {
	ImGui::PushID(static_cast<int>(behavior.id));

	ImGui::InputText("Name", behavior.name.Data(), behavior.name.Size());
	ImGui::Checkbox("Enabled", &behavior.enabled);

	DrawEnumCombo("Reentry", behavior.reentry, kReentryNames);
	DrawHelpMarker(
		"Controls what happens when the behavior is triggered again while already running."
	);

	ImGui::Checkbox("Destroy Owner On Complete", &behavior.destroy_owner_on_complete);

	DrawSectionTitle("Triggers");

	int remove_trigger{ -1 };

	for (int i{ 0 }; i < static_cast<int>(behavior.triggers.size()); ++i) {
		if (DrawTriggerEditor(behavior.triggers[static_cast<std::size_t>(i)])) {
			remove_trigger = i;
		}
	}

	if (remove_trigger >= 0) {
		behavior.triggers.erase(behavior.triggers.begin() + remove_trigger);
	}

	if (ImGui::Button("+ Add Trigger")) {
		behavior.triggers.emplace_back();
	}

	DrawSectionTitle("Sequence");

	if (behavior.steps.empty()) {
		ImGui::TextDisabled("No sequence steps. Add a step to begin authoring.");
	}

	int remove_step{ -1 };
	int duplicate_step{ -1 };
	int move_from{ -1 };
	int move_to{ -1 };

	const float active_progress{ GetActiveStepProgress(behavior) };

	for (int i{ 0 }; i < static_cast<int>(behavior.steps.size()); ++i) {
		auto& step{ behavior.steps[static_cast<std::size_t>(i)] };

		bool request_remove{ false };
		bool request_duplicate{ false };

		const bool active{ behavior.runtime.running &&
						   behavior.runtime.current_step == static_cast<std::size_t>(i) };

		ImGui::PushID(static_cast<int>(step.id));
		ImGui::SmallButton("::");

		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Drag to reorder this step");
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			const StepDragPayload payload{ i };
			ImGui::SetDragDropPayload("PTGN_SEQUENCE_STEP", &payload, sizeof(payload));
			ImGui::Text("Move %s", step.name.Data());
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("PTGN_SEQUENCE_STEP") }) {
				const auto* drag{ static_cast<const StepDragPayload*>(payload->Data) };

				if (drag) {
					move_from = drag->index;
					move_to	  = i;
				}
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();
		ImGui::PopID();

		DrawStepEditor(
			step, active, active ? active_progress : 0.0f, request_remove, request_duplicate
		);

		if (request_remove) {
			remove_step = i;
		}

		if (request_duplicate) {
			duplicate_step = i;
		}

		ImGui::Spacing();
	}

	if (move_from >= 0 && move_to >= 0) {
		MoveItem(behavior.steps, move_from, move_to);
	}

	if (duplicate_step >= 0) {
		auto copy{ CloneStep(behavior.steps[static_cast<std::size_t>(duplicate_step)]) };
		behavior.steps.insert(behavior.steps.begin() + duplicate_step + 1, std::move(copy));
	}

	if (remove_step >= 0) {
		behavior.steps.erase(behavior.steps.begin() + remove_step);

		if (behavior.runtime.current_step >= behavior.steps.size()) {
			StopRuntime(behavior);
		}
	}

	if (ImGui::Button("+ Add Step", ImVec2{ -FLT_MIN, 0.0f })) {
		behavior.steps.emplace_back();
	}

	ImGui::PopID();
}

void DrawRuntimeControls(BehaviorDefinition& behavior, SequenceRuntimeContext& context) {
	auto& runtime{ behavior.runtime };

	if (ImGui::Button(runtime.running && !runtime.paused ? "Restart" : "Play")) {
		StartRuntime(behavior, context);
	}

	ImGui::SameLine();

	ImGui::BeginDisabled(!runtime.running);

	if (ImGui::Button(runtime.paused ? "Resume" : "Pause")) {
		runtime.paused = !runtime.paused;
	}

	ImGui::SameLine();

	if (ImGui::Button("Advance Step")) {
		CompleteCurrentStep(behavior, context);
	}

	ImGui::EndDisabled();

	ImGui::SameLine();

	if (ImGui::Button("Stop")) {
		StopRuntime(behavior);
	}
}

void DrawRuntimePanel(BehaviorDefinition& behavior, SequenceRuntimeContext& context) {
	DrawSectionTitle("Runtime Preview");
	DrawRuntimeControls(behavior, context);

	const auto& runtime{ behavior.runtime };

	ImGui::Spacing();

	if (runtime.running) {
		ImGui::Text("State: %s", runtime.paused ? "Paused" : "Running");

		if (runtime.current_step < behavior.steps.size()) {
			const auto& step{ behavior.steps[runtime.current_step] };
			ImGui::Text("Step: %zu / %zu", runtime.current_step + 1, behavior.steps.size());
			ImGui::TextWrapped("Current: %s", step.name.Data());

			const float progress{ GetActiveStepProgress(behavior) };
			ImGui::ProgressBar(progress, ImVec2{ -FLT_MIN, 0.0f });

			if (runtime.step_elapsed_ms < step.delay_ms) {
				ImGui::Text("Waiting: %.0f / %.0f ms", runtime.step_elapsed_ms, step.delay_ms);
			} else {
				ImGui::Text(
					"Active: %.0f / %.0f ms",
					std::max(0.0f, runtime.step_elapsed_ms - step.delay_ms), step.duration_ms
				);
			}
		}
	} else if (runtime.completed) {
		ImGui::Text("State: Completed");
		ImGui::ProgressBar(1.0f, ImVec2{ -FLT_MIN, 0.0f });
	} else {
		ImGui::TextDisabled("State: Stopped");
	}

	ImGui::Text("Completed Runs: %d", runtime.completed_runs);

	if (runtime.queued) {
		ImGui::TextDisabled("One additional run is queued.");
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextWrapped(
		"This is a visual simulation only. In the engine, each play would instantiate "
		"registered script definitions into independent runtime script objects."
	);
}

void DrawSignalActivityPanel(const SequenceRuntimeContext& context) {
	DrawSectionTitle("Signal Activity");

	if (context.signal_log.empty()) {
		ImGui::TextDisabled("Run Open Door to see door.opened start Door Celebration.");
		return;
	}

	for (auto it{ context.signal_log.rbegin() }; it != context.signal_log.rend(); ++it) {
		ImGui::BulletText("%s", it->text.c_str());
	}
}

void DrawRegistryPanel() {
	DrawSectionTitle("Registered Scripts");

	static TextBuffer<96> search;
	ImGui::InputTextWithHint(
		"##RegistrySearch", "Filter registry...", search.Data(), search.Size()
	);

	ImGui::Spacing();

	for (const auto& descriptor : kScriptRegistry) {
		const std::string_view search_view{ search.Data() };

		if (!MatchesSearch(descriptor.label, search_view) &&
			!MatchesSearch(descriptor.key, search_view) &&
			!MatchesSearch(descriptor.group, search_view) &&
			!MatchesSearch(descriptor.source, search_view)) {
			continue;
		}

		ImGui::PushID(static_cast<int>(descriptor.kind));

		const bool open{ ImGui::TreeNodeEx(
			"##registry", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", descriptor.label
		) };

		if (open) {
			ImGui::TextDisabled("%s", descriptor.key);
			ImGui::Text("Group: %s", descriptor.group);
			ImGui::Text("Source: %s", descriptor.source);
			ImGui::TextWrapped("%s", descriptor.description);
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextWrapped(
		"game.apply_damage demonstrates a user-provided registration. "
		"The editor treats it the same way as engine presets."
	);
}

std::vector<BehaviorDefinition> MakeDemoBehaviors() {
	std::vector<BehaviorDefinition> behaviors;

	BehaviorDefinition open_door;
	open_door.name.Assign("Open Door");
	open_door.reentry = ReentryMode::IgnoreWhileRunning;

	TriggerDefinition overlap;
	overlap.kind = TriggerKind::OverlapStart;
	overlap.other_tag.Assign("Player");
	open_door.triggers.push_back(overlap);

	SequenceStepDefinition open;
	open.name.Assign("Open");
	open.duration_ms = 300.0f;
	open.ease		 = Ease::OutCubic;

	auto move{ MakeScript(ScriptKind::MoveTo) };
	auto& move_params{ std::get<MoveToParams>(move.parameters) };
	move_params.destination[0] = 0.0f;
	move_params.destination[1] = 64.0f;
	move_params.relative	   = true;
	open.during.push_back(std::move(move));

	auto play_audio{ MakeScript(ScriptKind::PlayAudio) };
	auto& audio_params{ std::get<PlayAudioParams>(play_audio.parameters) };
	audio_params.asset.Assign("door_open");
	open.on_complete.push_back(std::move(play_audio));

	open_door.steps.push_back(std::move(open));

	SequenceStepDefinition disable_collider;
	disable_collider.name.Assign("Disable Collider");
	disable_collider.delay_ms	 = 100.0f;
	disable_collider.duration_ms = 0.0f;

	auto collider{ MakeScript(ScriptKind::SetColliderMode) };
	std::get<SetColliderModeParams>(collider.parameters).mode = 0;
	disable_collider.on_start.push_back(std::move(collider));

	open_door.steps.push_back(std::move(disable_collider));

	SequenceStepDefinition announce_opened;
	announce_opened.name.Assign("Announce Door Opened");
	announce_opened.duration_ms = 0.0f;

	auto emit_door_opened{ MakeScript(ScriptKind::EmitSignal) };
	std::get<EmitSignalParams>(emit_door_opened.parameters).signal.Assign("door.opened");
	announce_opened.on_complete.push_back(std::move(emit_door_opened));

	open_door.steps.push_back(std::move(announce_opened));
	behaviors.push_back(std::move(open_door));

	BehaviorDefinition door_celebration;
	door_celebration.name.Assign("Door Celebration");
	door_celebration.reentry = ReentryMode::Restart;

	TriggerDefinition door_opened_signal;
	door_opened_signal.kind = TriggerKind::Signal;
	door_opened_signal.signal.Assign("door.opened");
	door_celebration.triggers.push_back(std::move(door_opened_signal));

	SequenceStepDefinition celebrate;
	celebrate.name.Assign("Celebrate Door Opened");
	celebrate.duration_ms = 750.0f;
	celebrate.ease		  = Ease::OutBack;

	auto celebration_rotation{ MakeScript(ScriptKind::RotateTo) };
	std::get<RotateToParams>(celebration_rotation.parameters).degrees = 360.0f;
	celebrate.during.push_back(std::move(celebration_rotation));

	auto celebration_audio{ MakeScript(ScriptKind::PlayAudio) };
	std::get<PlayAudioParams>(celebration_audio.parameters).asset.Assign("success_chime");
	celebrate.on_start.push_back(std::move(celebration_audio));

	door_celebration.steps.push_back(std::move(celebrate));
	behaviors.push_back(std::move(door_celebration));

	BehaviorDefinition damage_flash;
	damage_flash.name.Assign("Damage Flash");
	damage_flash.reentry = ReentryMode::Restart;

	TriggerDefinition damage_signal;
	damage_signal.kind = TriggerKind::Signal;
	damage_signal.signal.Assign("player.damaged");
	damage_flash.triggers.push_back(damage_signal);

	SequenceStepDefinition apply_damage;
	apply_damage.name.Assign("Apply Damage");
	apply_damage.duration_ms = 0.0f;

	auto damage{ MakeScript(ScriptKind::ApplyDamage) };
	auto& damage_params{ std::get<ApplyDamageParams>(damage.parameters) };
	damage_params.amount = 25.0f;
	damage_params.damage_type.Assign("Storm");
	apply_damage.on_start.push_back(std::move(damage));

	damage_flash.steps.push_back(std::move(apply_damage));

	SequenceStepDefinition flash;
	flash.name.Assign("Flash");
	flash.duration_ms = 120.0f;
	flash.repeats	  = 1;
	flash.yoyo		  = true;

	auto hide{ MakeScript(ScriptKind::SetVisible) };
	std::get<SetVisibleParams>(hide.parameters).visible = false;
	flash.during.push_back(std::move(hide));

	auto show{ MakeScript(ScriptKind::SetVisible) };
	std::get<SetVisibleParams>(show.parameters).visible = true;
	flash.on_complete.push_back(std::move(show));

	damage_flash.steps.push_back(std::move(flash));
	behaviors.push_back(std::move(damage_flash));

	BehaviorDefinition intro;
	intro.name.Assign("Intro Sequence");
	intro.reentry = ReentryMode::IgnoreWhileRunning;

	TriggerDefinition on_create;
	on_create.kind = TriggerKind::OnCreate;
	intro.triggers.push_back(on_create);

	SequenceStepDefinition wait;
	wait.name.Assign("Initial Delay");
	wait.delay_ms	 = 500.0f;
	wait.duration_ms = 0.0f;
	intro.steps.push_back(std::move(wait));

	SequenceStepDefinition rotate;
	rotate.name.Assign("Rotate Logo");
	rotate.duration_ms = 900.0f;
	rotate.ease		   = Ease::OutBack;

	auto rotate_script{ MakeScript(ScriptKind::RotateTo) };
	std::get<RotateToParams>(rotate_script.parameters).degrees = 360.0f;
	rotate.during.push_back(std::move(rotate_script));

	intro.steps.push_back(std::move(rotate));
	behaviors.push_back(std::move(intro));

	return behaviors;
}

struct BehaviorDragPayload {
	int index;
};

void DrawBehaviorList(
	std::vector<BehaviorDefinition>& behaviors, int& selected_index, SequenceRuntimeContext& context
) {
	ImGui::TextUnformatted("Behaviors");
	ImGui::Separator();

	int remove_index{ -1 };
	int duplicate_index{ -1 };
	int move_from{ -1 };
	int move_to{ -1 };

	for (int i{ 0 }; i < static_cast<int>(behaviors.size()); ++i) {
		auto& behavior{ behaviors[static_cast<std::size_t>(i)] };

		ImGui::PushID(static_cast<int>(behavior.id));

		const bool selected{ selected_index == i };

		char label[160]{};
		std::snprintf(
			label, sizeof(label), "%s%s%s", behavior.runtime.running ? "> " : "",
			behavior.enabled ? "" : "[Disabled] ", behavior.name.Data()
		);

		if (ImGui::Selectable(label, selected, 0, ImVec2{ 0.0f, 34.0f })) {
			selected_index = i;
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			const BehaviorDragPayload payload{ i };
			ImGui::SetDragDropPayload("PTGN_BEHAVIOR", &payload, sizeof(payload));
			ImGui::Text("Move %s", behavior.name.Data());
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("PTGN_BEHAVIOR") }) {
				const auto* drag{ static_cast<const BehaviorDragPayload*>(payload->Data) };

				if (drag) {
					move_from = drag->index;
					move_to	  = i;
				}
			}

			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginPopupContextItem("BehaviorContext")) {
			if (ImGui::MenuItem("Preview Trigger")) {
				StartRuntime(behavior, context, false);
			}

			if (ImGui::MenuItem("Duplicate")) {
				duplicate_index = i;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Remove")) {
				remove_index = i;
			}

			ImGui::EndPopup();
		}

		ImGui::PopID();
	}

	if (move_from >= 0 && move_to >= 0) {
		MoveItem(behaviors, move_from, move_to);

		if (selected_index == move_from) {
			selected_index = move_to;
		}
	}

	if (duplicate_index >= 0) {
		auto copy{ CloneBehavior(behaviors[static_cast<std::size_t>(duplicate_index)]) };
		copy.name.Assign(
			std::string{ behaviors[static_cast<std::size_t>(duplicate_index)].name.Data() } +
			" Copy"
		);

		behaviors.insert(behaviors.begin() + duplicate_index + 1, std::move(copy));
		selected_index = duplicate_index + 1;
	}

	if (remove_index >= 0) {
		behaviors.erase(behaviors.begin() + remove_index);

		if (behaviors.empty()) {
			selected_index = -1;
		} else {
			selected_index = std::clamp(selected_index, 0, static_cast<int>(behaviors.size()) - 1);
		}
	}

	ImGui::Spacing();

	if (ImGui::Button("+ Add Behavior", ImVec2{ -FLT_MIN, 0.0f })) {
		behaviors.emplace_back();
		selected_index = static_cast<int>(behaviors.size()) - 1;
	}

	ImGui::Spacing();
	ImGui::TextWrapped(
		"Drag behaviors to reorder. Right-click for preview, duplicate, and remove."
	);
}

void DrawToolbar(BehaviorDefinition* selected, SequenceRuntimeContext& context) {
	if (!selected) {
		ImGui::TextDisabled("No behavior selected.");
		return;
	}

	DrawRuntimeControls(*selected, context);

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	if (ImGui::Button("Fire Trigger")) {
		StartRuntime(*selected, context, false);
	}

	ImGui::SameLine();
	ImGui::TextDisabled("%s", selected->name.Data());

	if (std::string_view{ selected->name.Data() } == "Open Door") {
		ImGui::SameLine();
		ImGui::TextDisabled("-> emits door.opened -> starts Door Celebration");
	}
}

void DrawMainEditor(
	std::vector<BehaviorDefinition>& behaviors, int& selected_index, SequenceRuntimeContext& context
) {
	ImGuiViewport* viewport{ ImGui::GetMainViewport() };

	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	constexpr ImGuiWindowFlags window_flags{ ImGuiWindowFlags_NoDecoration |
											 ImGuiWindowFlags_NoMove |
											 ImGuiWindowFlags_NoSavedSettings |
											 ImGuiWindowFlags_NoBringToFrontOnFocus };

	ImGui::Begin("Behavior Sequence Authoring Demo", nullptr, window_flags);

	BehaviorDefinition* selected{ selected_index >= 0 &&
										  selected_index < static_cast<int>(behaviors.size())
									  ? &behaviors[static_cast<std::size_t>(selected_index)]
									  : nullptr };

	ImGui::BeginChild("Toolbar", ImVec2{ 0.0f, 42.0f }, true);
	DrawToolbar(selected, context);
	ImGui::EndChild();

	if (ImGui::BeginTable(
			"MainLayout", 3,
			ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
				ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn("Behavior List", ImGuiTableColumnFlags_WidthFixed, 245.0f);

		ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch, 1.0f);

		ImGui::TableSetupColumn("Registry", ImGuiTableColumnFlags_WidthFixed, 330.0f);

		ImGui::TableNextColumn();
		ImGui::BeginChild("BehaviorListChild", ImVec2{ 0.0f, 0.0f }, false);
		DrawBehaviorList(behaviors, selected_index, context);
		ImGui::EndChild();

		ImGui::TableNextColumn();
		ImGui::BeginChild("InspectorChild", ImVec2{ 0.0f, 0.0f }, false);

		selected = selected_index >= 0 && selected_index < static_cast<int>(behaviors.size())
					 ? &behaviors[static_cast<std::size_t>(selected_index)]
					 : nullptr;

		if (selected) {
			DrawBehaviorInspector(*selected);
		} else {
			ImGui::TextDisabled("Select or create a behavior.");
		}

		ImGui::EndChild();

		ImGui::TableNextColumn();
		ImGui::BeginChild("RightPanelChild", ImVec2{ 0.0f, 0.0f }, false);

		if (selected) {
			DrawRuntimePanel(*selected, context);
		}

		DrawSignalActivityPanel(context);
		DrawRegistryPanel();
		ImGui::EndChild();

		ImGui::EndTable();
	}

	ImGui::End();
}

void ConfigureStyle() {
	ImGui::StyleColorsDark();

	ImGuiStyle& style{ ImGui::GetStyle() };
	style.WindowRounding	= 0.0f;
	style.ChildRounding		= 4.0f;
	style.FrameRounding		= 3.0f;
	style.PopupRounding		= 4.0f;
	style.ScrollbarRounding = 4.0f;
	style.GrabRounding		= 3.0f;
	style.TabRounding		= 3.0f;
	style.WindowPadding		= ImVec2{ 10.0f, 10.0f };
	style.FramePadding		= ImVec2{ 7.0f, 5.0f };
	style.ItemSpacing		= ImVec2{ 8.0f, 7.0f };
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

	GLFWwindow* window{
		glfwCreateWindow(1600, 950, "Protegon Behavior Sequence UI Demo", nullptr, nullptr)
	};

	if (!window) {
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	int status{ gladLoadGL(glfwGetProcAddress) };
	if (!status) {
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io{ ImGui::GetIO() };
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	demo::ConfigureStyle();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	auto behaviors{ demo::MakeDemoBehaviors() };
	demo::SequenceRuntimeContext runtime_context;
	int selected_behavior{ 0 };

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		for (auto& behavior : behaviors) {
			demo::UpdateRuntime(behavior, io.DeltaTime, runtime_context);
		}

		demo::DispatchPendingSignals(behaviors, runtime_context);
		demo::UpdateSignalLog(runtime_context, io.DeltaTime);

		demo::DrawMainEditor(behaviors, selected_behavior, runtime_context);

		ImGui::Render();

		int display_width{ 0 };
		int display_height{ 0 };
		glfwGetFramebufferSize(window, &display_width, &display_height);

		glViewport(0, 0, display_width, display_height);
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
