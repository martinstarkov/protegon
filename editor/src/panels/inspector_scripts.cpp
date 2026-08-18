#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "editor/editor.h"
#include "editor/editor_context.h"
#include "core/util/hash.h"
#include "panels/entity_filter_editor.h"
#include "panels/inspector_fields.h"
#include "panels/inspector_feature_helpers.h"
#include "panels/scene_hierarchy.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/animation_event.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/draggable_event.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/dropzone_event.h"
#include "runtime/interaction/interactive.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/collision_event.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/scripting/script.h"
#include "runtime/timer/timer.h"
#include "runtime/timer/timer_event.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/toggle_button.h"
#include "scripting/script_editor_registry.h"

namespace ptgn::editor::inspector {

namespace {
enum class ActionForm {
	Action,
	Tween,
	Delay
};

struct ActionDragPayload {
	int index;
};

struct ScriptInspectorState {
	std::optional<ImGuiID> editing_sequence_name;
	std::string editing_sequence_original_name;
	std::unordered_map<SequenceId, bool> sequence_open_states;
	std::unordered_map<ImGuiID, EntityFilterEditorState> target_filter_states;
};

ScriptInspectorState& GetScriptInspectorState() {
	static ScriptInspectorState state;
	return state;
}

inline constexpr std::array kLifecycleLabels{
	"On Start",			"On Complete", "On Reset",		  "On Stop",
	"On Pause",			"On Resume",   "On Action Start", "On Action Complete",
	"On Action Cancel", "On Repeat",   "On Yoyo"
};
inline constexpr std::array kActionFormLabels{ "Action", "Tween", "Delay" };
inline constexpr std::array kEaseEntries{
	std::pair{ Ease::Linear, "Linear" },	  std::pair{ Ease::InQuad, "In Quad" },
	std::pair{ Ease::OutQuad, "Out Quad" },	  std::pair{ Ease::InOutQuad, "In Out Quad" },
	std::pair{ Ease::OutCubic, "Out Cubic" }, std::pair{ Ease::OutBack, "Out Back" },
};

float CompactControlSpacing() {
	return ImGui::GetStyle().ItemSpacing.x;
}

void SameLineControl() {
	ImGui::SameLine(0.0f, CompactControlSpacing());
}

bool IsItemRightClicked() {
	return ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) &&
		   ImGui::IsMouseClicked(ImGuiMouseButton_Right);
}

bool DrawEnableDisableMenuItem(bool& enabled) {
	if (!ImGui::MenuItem(enabled ? "Disable" : "Enable")) {
		return false;
	}

	enabled = !enabled;
	return true;
}

bool DrawCenteredTextButton(const char* id, const char* text, ImVec2 size) {
	bool pressed{ ImGui::Button(id, size) };
	ImVec2 minimum{ ImGui::GetItemRectMin() };
	ImVec2 maximum{ ImGui::GetItemRectMax() };
	ImVec2 text_size{ ImGui::CalcTextSize(text) };
	ImVec2 text_position{ minimum.x + (maximum.x - minimum.x - text_size.x) * 0.5f,
								minimum.y + (maximum.y - minimum.y - text_size.y) * 0.5f };
	ImGui::GetWindowDrawList()->AddText(text_position, ImGui::GetColorU32(ImGuiCol_Text), text);
	return pressed;
}

bool DrawToggleButton(const char* label, bool& value, ImVec2 size, const char* tooltip) {
	bool dimmed{ !value };
	if (dimmed) {
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
	}
	bool pressed{ ImGui::Button(label, size) };
	if (dimmed) {
		ImGui::PopStyleVar();
	}
	if (pressed) {
		value = !value;
	}
	DrawTooltip(tooltip);
	return pressed;
}

float GetCountControlWidth(const char* label) {
	float button_width{ ImGui::GetFrameHeight() };
	float spacing{ CompactControlSpacing() };
	std::string widest{ std::string{ label } + ": 100" };
	return ImGui::CalcTextSize(widest.c_str()).x + button_width * 2.0f + spacing * 2.0f;
}

bool DrawCountControl(
	const char* label, int& value, int minimum, int maximum = 100, bool disabled = false,
	const char* tooltip = nullptr
) {
	int previous{ value };
	value = std::clamp(value, minimum, maximum);
	float button_width{ ImGui::GetFrameHeight() };
	float spacing{ CompactControlSpacing() };
	std::string widest{ std::string{ label } + ": 100" };
	float text_width{ ImGui::CalcTextSize(widest.c_str()).x };
	float start_x{ ImGui::GetCursorScreenPos().x };

	ImGui::PushID(label);
	ImGui::BeginDisabled(disabled);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s: %d", label, value);
	DrawTooltip(tooltip);
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
	return previous != value;
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

[[nodiscard]] ScriptSequence* ResolveEditorSequence(
	ScriptEditorContext& context, ScriptSequence& binding
) {
	return binding.shared_reference
		? context.shared_sequences.Find(binding.shared_sequence_id)
		: std::addressof(binding);
}

[[nodiscard]] Entity ResolveInspectedEntity(const ScriptEditorContext& context) {
	if (context.owner) {
		return context.owner;
	}

	auto& hierarchy{ context.ctx.editor.GetSceneHierarchyPanel() };
	if (hierarchy.GetActiveTab() == SceneHierarchyTab::Prefabs) {
		return {};
	}

	return hierarchy.GetSelectedEntity();
}

[[nodiscard]] std::vector<TimerKey> GetTimerChoices(
	Entity owner,
	const std::optional<EntityFilter>& target_filter
) {
	if (!owner) {
		return {};
	}

	std::vector<Entity> targets;

	if (target_filter) {
		targets = ResolveEntityFilter(
			*target_filter,
			owner.GetScene(),
			owner
		);
	} else {
		targets.push_back(owner);
	}

	std::vector<TimerKey> choices;

	for (Entity target : targets) {
		const auto* timers{ target.TryGet<::ptgn::impl::Timers>() };
		if (!timers) {
			continue;
		}

		for (const auto& entry : timers->timers) {
			if (entry.config.key.value.empty() ||
				std::ranges::contains(choices, entry.config.key)) {
				continue;
			}

			choices.push_back(entry.config.key);
		}
	}

	return choices;
}

[[nodiscard]] TimerKey GetDefaultTimerKey(
	Entity owner,
	const std::optional<EntityFilter>& target_filter
) {
	const auto choices{ GetTimerChoices(owner, target_filter) };
	return choices.empty() ? TimerKey{} : choices.front();
}

void ApplyContextualTimerElapsedDefault(
	ScriptEditorContext& context,
	EventCondition& event
) {
	if (event.type_hash != Hash<event::TimerElapsed>()) {
		return;
	}

	if (!event.value.is_object()) {
		event.value = json::object();
	}

	event.value["timer"] = GetDefaultTimerKey(
		ResolveInspectedEntity(context),
		std::nullopt
	);
}

[[nodiscard]] bool IsRuntimeActive(const ScriptEditorContext& context) {
	return context.ctx.editor.IsPlaying() ||
		   context.ctx.editor.IsDirectRuntime();
}

[[nodiscard]] Entity ResolveRuntimeOwner(const ScriptEditorContext& context) {
	if (!IsRuntimeActive(context)) {
		return {};
	}

	return ResolveInspectedEntity(context);
}

[[nodiscard]] ScriptSequence* FindRuntimeRootSequence(
	Entity owner, SequenceId id, int resident_index = -1
) {
	if (!owner) {
		return nullptr;
	}

	auto* scripts{ owner.TryGet<::ptgn::impl::Scripts>() };
	if (!scripts) {
		return nullptr;
	}

	scripts->Attach(owner);

	auto find = [&](auto& entries) -> ScriptSequence* {
		for (auto& entry : entries) {
			if (entry.instance && entry.instance->sequence.id == id) {
				return std::addressof(entry.instance->sequence);
			}

			if (entry.sequence.id == id) {
				return entry.instance ? std::addressof(entry.instance->sequence)
								  : std::addressof(entry.sequence);
			}
		}

		return nullptr;
	};

	if (auto* sequence{ find(scripts->scripts) }) {
		return sequence;
	}

	if (auto* sequence{ find(scripts->pending_additions) }) {
		return sequence;
	}

	if (resident_index < 0 || resident_index >= static_cast<int>(scripts->scripts.size())) {
		return nullptr;
	}

	auto& entry{ scripts->scripts[static_cast<std::size_t>(resident_index)] };
	if (entry.type_hash != Hash<Script>()) {
		return nullptr;
	}

	return entry.instance ? std::addressof(entry.instance->sequence)
						 : std::addressof(entry.sequence);
}

[[nodiscard]] std::string TrimMenuText(std::string_view text) {
	const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };

	while (!text.empty() && is_space(static_cast<unsigned char>(text.front()))) {
		text.remove_prefix(1);
	}

	while (!text.empty() && is_space(static_cast<unsigned char>(text.back()))) {
		text.remove_suffix(1);
	}

	return std::string{ text };
}


void AddEditorScriptEntry(
	ScriptEditorContext& context, ::ptgn::impl::Scripts& scripts, ScriptEntry entry
) {
	scripts.scripts.emplace_back(std::move(entry));

	Entity owner{ ::ptgn::impl::ScriptsAccessor::GetOwner(scripts) };
	if (owner && IsRuntimeActive(context)) {
		scripts.Attach(owner);
		script_runtime::AttachEntry(owner, scripts.scripts.back());
	}
}

void EnsureActionValue(ScriptStep& action) {
	const auto* registration{ ScriptRegistry::Find(action.type_hash) };

	if (!registration || !registration->make_default) {
		if (action.value.is_null()) {
			action.value = json::object();
		}
		return;
	}

	json defaults = registration->make_default();

	if (action.value.is_null()) {
		action.value = std::move(defaults);
		return;
	}

	if (defaults.is_object() && action.value.is_object()) {
		defaults.update(action.value, true);

		action.value = std::move(defaults);
	}
}

ActionForm GetActionForm(const ScriptStep& action) {
	if (action.type_hash == Hash<WaitScript>()) {
		return ActionForm::Delay;
	}

	const auto* registration{ ScriptRegistry::Find(action.type_hash) };
	ScriptCompletion completion{
		action.completion.value_or(
			registration
				? registration->completion
				: ScriptCompletion::ScriptControlled
		)
	};

	return completion == ScriptCompletion::Duration
		? ActionForm::Tween
		: ActionForm::Action;
}

void SetActionForm(ScriptStep& action, ActionForm form) {
	bool enabled{ action.enabled };
	auto target{ action.target };
	const auto* registration{ ScriptRegistry::Find(action.type_hash) };

	switch (form) {
		case ActionForm::Action:
			if (!registration || registration->requires_timing ||
				action.type_hash == Hash<WaitScript>()) {
				action = ScriptRegistry::MakeStep<SetVisibleScript>();
			}
			action.completion.reset();
			action.timing.reset();
			break;
		case ActionForm::Tween:
			if (!registration || !registration->supports_timing ||
				action.type_hash == Hash<WaitScript>()) {
				action = ScriptRegistry::MakeStep<MoveToScript>();
			}
			registration	  = ScriptRegistry::Find(action.type_hash);
			action.completion = ScriptCompletion::Duration;
			action.timing	  = registration && registration->default_timing
								  ? registration->default_timing
								  : std::optional<ScriptTiming>{ ScriptTiming{} };
			break;
		case ActionForm::Delay:
			action			  = ScriptRegistry::MakeStep<WaitScript>();
			action.completion = ScriptCompletion::Duration;
			break;
	}

	action.enabled = enabled;
	action.target = std::move(target);
}

std::string ActionSummary(const ScriptStep& action) {
	const auto* editor{ ScriptEditorRegistry::Find(action.type_hash) };
	std::string result{ editor ? editor->options.label : std::string{ "Missing Script" } };
	if (action.timing) {
		result += " (" + std::to_string(static_cast<int>(action.timing->duration_ms)) + "ms)";
	}
	return result;
}

void MoveAction(std::vector<ScriptStep>& actions, int from, int to) {
	if (from < 0 || to < 0 || from >= static_cast<int>(actions.size()) ||
		to >= static_cast<int>(actions.size()) || from == to) {
		return;
	}
	ScriptStep moved{ std::move(actions[static_cast<std::size_t>(from)]) };
	actions.erase(actions.begin() + from);
	actions.insert(actions.begin() + to, std::move(moved));
}

ScriptEntry MakeRootEntry(TypeHashValue type_hash) {
	const auto* registration{ ScriptRegistry::Find(type_hash) };
	if (!registration) {
		return {};
	}
	ScriptEntry entry;
	entry.enabled	= true;
	entry.type_hash = type_hash;
	entry.name		= registration->name;
	entry.value		= registration->make_default ? registration->make_default() : json::object();
	entry.sequence	= registration->make_default_sequence ? registration->make_default_sequence()
														  : ScriptSequence{};
	return entry;
}

template <ScriptClass T>
ScriptEntry MakeRootEntry(T script) {
	const auto* registration{ ScriptRegistry::Find(Hash<T>()) };
	if (!registration) {
		return {};
	}

	json value;
	try {
		value = script;
	} catch (...) {
		value = json::object();
	}

	ScriptEntry entry;
	entry.enabled	= true;
	entry.type_hash = registration->type_hash;
	entry.name		= registration->name;
	entry.value		= std::move(value);
	entry.sequence	= script.sequence;
	return entry;
}

void PromoteToShared(ScriptEditorContext& context, ScriptSequence& binding) {
	if (binding.shared_reference) {
		return;
	}
	ScriptSequence shared{ binding };
	shared.shared_reference	  = false;
	shared.shared_sequence_id = 0;
	shared.runtime			  = ScriptSequenceRuntime{};
	SequenceId shared_id{ shared.id };
	context.shared_sequences.sequences.push_back(std::move(shared));
	binding.shared_reference   = true;
	binding.shared_sequence_id = shared_id;
	binding.runtime			   = ScriptSequenceRuntime{};
}

void DetachToLocal(ScriptEditorContext& context, ScriptSequence& binding) {
	if (!binding.shared_reference) {
		return;
	}
	const auto* shared{ context.shared_sequences.Find(binding.shared_sequence_id) };
	if (!shared) {
		binding.shared_reference   = false;
		binding.shared_sequence_id = 0;
		return;
	}
	ScriptSequence local{ *shared };
	SequenceId binding_id{ binding.id };
	bool enabled{ binding.enabled };
	binding					   = std::move(local);
	binding.id				   = binding_id;
	binding.enabled			   = enabled;
	binding.shared_reference   = false;
	binding.shared_sequence_id = 0;
	binding.runtime			   = ScriptSequenceRuntime{};
}

bool DrawActionTarget(
	ScriptEditorContext& context,
	ScriptStep& action
) {
	Entity owner{ ResolveInspectedEntity(context) };
	Scene* scene{ owner ? std::addressof(owner.GetScene()) : nullptr };

	ImGuiID state_id{ ImGui::GetID("##ActionTargetFilterState") };
	auto& state{
		GetScriptInspectorState()
			.target_filter_states[state_id]
	};

	return DrawEntityFilterButton(
		scene,
		owner,
		action.target,
		state
	);
}

bool DrawActionPicker(
	ScriptEditorContext& context, ScriptStep& action, bool timed_only, float width = -FLT_MIN,
	bool* context_requested = nullptr
) {
	struct Candidate {
		const ScriptRegistration* runtime{ nullptr };
		const ScriptEditorRegistration* editor{ nullptr };
		std::string_view label;
		std::string_view group;
		std::string_view description;
		int menu_order{ 100 };
		bool separator_after{ false };
	};

	const auto resolve_candidate =
		[](const ScriptRegistration& registration) -> std::optional<Candidate> {
		const auto* editor{ ScriptEditorRegistry::Find(registration.type_hash) };
		if (!editor || !HasScriptType(editor->options.type, ScriptType::Sequence)) {
			return std::nullopt;
		}
		return Candidate{
			.runtime		 = &registration,
			.editor			 = editor,
			.label			 = editor->options.label,
			.group			 = editor->options.group,
			.description	 = editor->options.description,
			.menu_order		 = editor->options.menu_order,
			.separator_after = editor->options.separator_after,
		};
	};

	const auto* current_registration{ ScriptRegistry::Find(action.type_hash) };
	std::optional<Candidate> current{ current_registration
												? resolve_candidate(*current_registration)
												: std::nullopt };
	ImGui::SetNextItemWidth(width);

	float popup_min_width{ ImGui::CalcItemWidth() };

	ImGui::SetNextWindowSizeConstraints(
		ImVec2{ popup_min_width, 0.0f }, ImVec2{ FLT_MAX, FLT_MAX }
	);

	bool open{
		ImGui::BeginCombo("##RegisteredAction", current ? current->label.data() : "Missing Action")
	};

	if (context_requested) {
		*context_requested |= IsItemRightClicked();
	}

	DrawTooltip(
		current ? current->description.data()
				: "Choose a registered Script for this sequence entry."
	);
	if (!open) {
		return false;
	}

	bool changed{ false };
	const auto is_available = [&](const Candidate& candidate) {
		const auto& registration{ *candidate.runtime };
		return registration.serializable && registration.type_hash != Hash<Script>() &&
			   registration.type_hash != Hash<WaitScript>() && !candidate.editor->options.hidden &&
			   (!timed_only || registration.supports_timing) &&
			   (timed_only || !registration.requires_timing);
	};
	const auto select_candidate = [&](const Candidate& candidate) {
		const auto& registration{ *candidate.runtime };
		if (ImGui::MenuItem(
				candidate.label.data(), nullptr, registration.type_hash == action.type_hash
			)) {
			bool enabled{ action.enabled };
			auto target{ action.target };
			action		   = ScriptRegistry::MakeStep(registration.type_hash);
			action.enabled = enabled;
			action.target = std::move(target);

			if (action.type_hash == Hash<TimerActionScript>()) {
				if (!action.value.is_object()) {
					action.value = json::object();
				}

				action.value["timer"] = GetDefaultTimerKey(
					ResolveInspectedEntity(context),
					action.target
				);
			}

			if (timed_only) {
				action.completion = ScriptCompletion::Duration;
				action.timing	  = registration.default_timing.value_or(ScriptTiming{});
			} else {
				action.completion.reset();
				action.timing.reset();
			}
			changed = true;
		}
		if (ImGui::IsItemHovered()) {
			std::string type_hash{ std::to_string(registration.type_hash) };
			ImGui::SetTooltip("%s\nType hash: %s", candidate.description.data(), type_hash.c_str());
		}
	};

	std::vector<Candidate> candidates;
	for (const auto& registration : ScriptRegistry::Entries()) {
		if (auto candidate{ resolve_candidate(registration) };
			candidate && is_available(*candidate)) {
			candidates.push_back(*candidate);
		}
	}

	const auto emit_signal_it{ std::ranges::find_if(candidates, [](const Candidate& candidate) {
		return candidate.runtime->type_hash == Hash<EmitSignalScript>();
	}) };
	if (emit_signal_it != candidates.end()) {
		select_candidate(*emit_signal_it);
		ImGui::Separator();
	}

	if (!timed_only && !context.shared_sequences.sequences.empty() &&
		ImGui::BeginMenu("Global")) {
		for (const auto& shared : context.shared_sequences.sequences) {
			bool selected{ false };
			if (action.type_hash == Hash<Script>()) {
				Script current_script;
				if (TryReadScriptJson(action.value, current_script)) {
					selected = current_script.sequence.shared_reference &&
							   current_script.sequence.shared_sequence_id == shared.id;
				}
			}
			if (ImGui::MenuItem(shared.name.c_str(), nullptr, selected)) {
				bool enabled{ action.enabled };
				auto target{ action.target };
				Script script;
				script.sequence.name			   = shared.name;
				script.sequence.shared_reference   = true;
				script.sequence.shared_sequence_id = shared.id;
				action							   = ScriptRegistry::MakeStep(std::move(script));
				action.enabled					   = enabled;
				action.target					   = std::move(target);
				action.completion				   = ScriptCompletion::ScriptControlled;
				action.timing.reset();
				changed = true;
			}
			DrawTooltip("Run this global editor authored Script as the sequence step.");
		}
		ImGui::EndMenu();
	}

	for (const auto& candidate : candidates) {
		if (candidate.group.empty() && candidate.runtime->type_hash != Hash<EmitSignalScript>()) {
			select_candidate(candidate);
		}
	}

	std::vector<std::string_view> groups;
	for (const auto& candidate : candidates) {
		if (!candidate.group.empty() && !std::ranges::contains(groups, candidate.group)) {
			groups.push_back(candidate.group);
		}
	}
	for (const auto group : groups) {
		if (!ImGui::BeginMenu(group.data())) {
			continue;
		}
		std::vector<const Candidate*> grouped;
		for (const auto& candidate : candidates) {
			if (candidate.group == group) {
				grouped.push_back(&candidate);
			}
		}
		std::ranges::sort(grouped, {}, [](const Candidate* candidate) {
			return candidate->menu_order;
		});
		for (std::size_t i{ 0 }; i < grouped.size(); ++i) {
			select_candidate(*grouped[i]);
			if (grouped[i]->separator_after && i + 1 < grouped.size()) {
				ImGui::Separator();
			}
		}
		ImGui::EndMenu();
	}
	ImGui::EndCombo();
	return changed;
}

bool DrawActionPickerWithInline(
	ScriptEditorContext& context, ScriptStep& action, bool timed_only,
	bool* context_requested = nullptr
) {
	EnsureActionValue(action);

	const auto* registered_editor{ ScriptEditorRegistry::Find(action.type_hash) };
	bool allow_inline{
		!timed_only ||
		action.type_hash == Hash<TintToScript>()
	};
	bool has_inline_editor{
		allow_inline &&
		registered_editor &&
		static_cast<bool>(registered_editor->draw_inline)
	};

	if (!has_inline_editor) {
		return DrawActionPicker(context, action, timed_only, -FLT_MIN, context_requested);
	}

	float available{ ImGui::GetContentRegionAvail().x };
	float spacing{ ImGui::GetStyle().ItemSpacing.x };
	bool tint_inline{ action.type_hash == Hash<TintToScript>() };
	float picker_width{
		tint_inline
			? std::max(1.0f, available - spacing - ImGui::GetFrameHeight())
			: std::min(150.0f, std::max(110.0f, available * 0.32f))
	};
	float row_y{ ImGui::GetCursorScreenPos().y };

	bool changed{
		DrawActionPicker(context, action, timed_only, picker_width, context_requested)
	};

	registered_editor = ScriptEditorRegistry::Find(action.type_hash);

	if (registered_editor && registered_editor->draw_inline) {
		ImGui::SameLine(0.0f, spacing);

		ImVec2 inline_position{ ImGui::GetCursorScreenPos() };
		inline_position.y = row_y;
		ImGui::SetCursorScreenPos(inline_position);

		ImGui::SetNextItemWidth(-FLT_MIN);

		auto* previous_target_filter{ context.sequence_target_filter };
		context.sequence_target_filter = std::addressof(action.target);

		bool inline_changed{ registered_editor->draw_inline(context, action.value) };

		context.sequence_target_filter = previous_target_filter;

		if (inline_changed) {
			action.runtime_factory = {};
			changed = true;
		}
	}

	return changed;
}

bool DrawTimingOptions(
	ScriptEditorContext&, ScriptStep& action, ScriptTiming& timing, float left_screen_x
) {
	EnsureActionValue(action);
	bool changed{ false };

	float right_screen_x{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
	float width{ std::max(1.0f, right_screen_x - left_screen_x) };
	ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });

	std::optional<MoveToScript> move;
	std::optional<RotateToScript> rotate;
	std::optional<ScaleToScript> scale;

	if (action.type_hash == Hash<MoveToScript>()) {
		MoveToScript value{};
		if (TryReadScriptJson(action.value, value)) {
			move = std::move(value);
		}
	} else if (action.type_hash == Hash<RotateToScript>()) {
		RotateToScript value{};
		if (TryReadScriptJson(action.value, value)) {
			rotate = std::move(value);
		}
	} else if (action.type_hash == Hash<ScaleToScript>()) {
		ScaleToScript value{};
		if (TryReadScriptJson(action.value, value)) {
			scale = std::move(value);
		}
	}

	int columns{ move || scale ? 5 : (rotate ? 4 : 2) };
	if (!ImGui::BeginTable(
			"TweenOptions", columns, ImGuiTableFlags_SizingStretchProp, ImVec2{ width, 0.0f }
		)) {
		return false;
	}

	if (move || scale) {
		ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed, 82.0f);
	} else if (rotate) {
		ImGui::TableSetupColumn("Degrees", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Shortest", ImGuiTableColumnFlags_WidthFixed, 82.0f);
	}
	ImGui::TableSetupColumn("Ease", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableSetupColumn("Options", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

	int column{};
	if (move) {
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= ImGui::DragFloat(
			"##TweenX", &move->destination.x, 1.0f, -100000.0f, 100000.0f, "X: %.0f"
		);
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= ImGui::DragFloat(
			"##TweenY", &move->destination.y, 1.0f, -100000.0f, 100000.0f, "Y: %.0f"
		);
		ImGui::TableSetColumnIndex(column++);
		if (ImGui::Button(
				move->relative ? "Relative" : "Absolute",
				ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			move->relative = !move->relative;
			changed		   = true;
		}
		DrawTooltip(
			move->relative ? "Offset from the entity's current position."
						   : "Use an absolute world position."
		);
	} else if (scale) {
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |=
			ImGui::DragFloat("##TweenScaleX", &scale->scale.x, 0.01f, -100.0f, 100.0f, "X: %.2f");
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |=
			ImGui::DragFloat("##TweenScaleY", &scale->scale.y, 0.01f, -100.0f, 100.0f, "Y: %.2f");
		ImGui::TableSetColumnIndex(column++);
		if (ImGui::Button(
				scale->relative ? "Relative" : "Absolute",
				ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			scale->relative = !scale->relative;
			changed			= true;
		}
		DrawTooltip(
			scale->relative ? "Multiply the entity's current scale." : "Use an absolute scale."
		);
	} else if (rotate) {
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= ImGui::DragFloat(
			"##TweenDegrees", &rotate->degrees, 1.0f, -3600.0f, 3600.0f, "%.1f deg"
		);
		ImGui::TableSetColumnIndex(column++);
		changed |= DrawToggleButton(
			"Shortest", rotate->shortest_path, ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() },
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
				changed		= true;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::TableSetColumnIndex(column);
	std::vector<std::string> selected_options;
	if (timing.infinite_repeats) {
		selected_options.emplace_back("Infinite");
	}
	if (timing.reversed) {
		selected_options.emplace_back("Reversed");
	}
	if (timing.yoyo) {
		selected_options.emplace_back("Yoyo");
	}
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
		changed |= ImGui::Checkbox("Infinite", &timing.infinite_repeats);
		DrawTooltip("Repeat this Tween indefinitely. Duration still controls every cycle.");
		changed |= ImGui::Checkbox("Reversed", &timing.reversed);
		changed |= ImGui::Checkbox("Yoyo", &timing.yoyo);
		ImGui::EndCombo();
	}
	DrawSelectedItemsTooltip(selected_options);
	ImGui::EndTable();

	if (move) {
		action.value		   = *move;
		action.runtime_factory = {};
	} else if (rotate) {
		action.value		   = *rotate;
		action.runtime_factory = {};
	} else if (scale) {
		action.value		   = *scale;
		action.runtime_factory = {};
	}
	return changed;
}

bool DrawActionParameters(ScriptEditorContext& context, ScriptStep& action, float left_screen_x) {
	EnsureActionValue(action);
	const auto* editor{ ScriptEditorRegistry::Find(action.type_hash) };
	if (!editor || !HasScriptType(editor->options.type, ScriptType::Sequence) || !editor->draw ||
		action.type_hash == Hash<Script>() || action.type_hash == Hash<WaitScript>() ||
		action.type_hash == Hash<EmitSignalScript>() ||
		action.type_hash == Hash<SetVisibleScript>() || action.type_hash == Hash<MoveToScript>() ||
		action.type_hash == Hash<RotateToScript>() || action.type_hash == Hash<ScaleToScript>() ||
		action.type_hash == Hash<TintToScript>() ||
		action.type_hash == Hash<RemoveComponentsScript>()) {
		return false;
	}

	if (action.type_hash == Hash<AddComponentsScript>()) {
		AddComponentsScript add_components;
		if (!TryReadScriptJson(action.value, add_components) || add_components.components.empty()) {
			return false;
		}
	}

	float right_screen_x{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
	ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });
	bool changed{ false };
	if (ImGui::BeginChild(
			"ActionParameters", ImVec2{ std::max(1.0f, right_screen_x - left_screen_x), 0.0f },
			ImGuiChildFlags_AutoResizeY
		)) {
		auto* previous_target_filter{ context.sequence_target_filter };
		context.sequence_target_filter = std::addressof(action.target);

		changed = editor->draw(context, action.value);

		context.sequence_target_filter = previous_target_filter;

		if (changed) {
			action.runtime_factory = {};
		}
	}
	ImGui::EndChild();
	return changed;
}


[[nodiscard]] const ScriptSequenceRuntime& GetDisplayedSequenceRuntime(
	const ScriptSequence& sequence, const ScriptSequence& binding
) {
	if (binding.runtime.running || binding.runtime.paused) {
		return binding.runtime;
	}

	return sequence.runtime;
}

void DrawActiveActionProgress(
	const ScriptSequenceRuntime& runtime, std::size_t action_index, const ScriptStep& action
) {
	if (!runtime.running || runtime.step_index != action_index) {
		return;
	}

	float duration_ms{ action.timing ? action.timing->duration_ms : 0.0f };
	float progress{
		duration_ms > 0.0f ? std::clamp(runtime.elapsed_ms / duration_ms, 0.0f, 1.0f) : 0.0f
	};

	ImGui::ProgressBar(progress, ImVec2{ -FLT_MIN, 2.0f }, "");
}

bool DrawActions(
	ScriptEditorContext& context, ScriptSequence& sequence, ScriptSequence& binding,
	int resident_index
) {
	ImGui::PushID("SequenceActions");

	Entity runtime_owner{ ResolveRuntimeOwner(context) };
	const ScriptSequence* runtime_binding{
		runtime_owner ? FindRuntimeRootSequence(runtime_owner, binding.id, resident_index) : nullptr
	};
	const ScriptSequenceRuntime& displayed_runtime{
		runtime_binding ? runtime_binding->runtime : GetDisplayedSequenceRuntime(sequence, binding)
	};

	bool changed{ false };
	int remove_index{ -1 };
	int duplicate_index{ -1 };
	int move_from{ -1 };
	int move_to{ -1 };

	for (int index{ 0 }; index < static_cast<int>(sequence.steps.size()); ++index) {
		auto& action{ sequence.steps[static_cast<std::size_t>(index)] };
		bool remove{ false };
		bool duplicate{ false };
		bool action_context_requested{ false };
		ImGui::PushID(index);

		constexpr float drag_width{ 28.0f };
		float label_width{ std::max(
			{ ImGui::CalcTextSize("Action").x, ImGui::CalcTextSize("Tween").x,
			  ImGui::CalcTextSize("Delay").x }
		) };
		float type_width{ label_width + ImGui::GetFrameHeight() +
								ImGui::GetStyle().FramePadding.x * 2.0f };
		float target_width{ 77.0f };
		float duration_width{ ImGui::CalcTextSize("5000ms").x +
									ImGui::GetStyle().FramePadding.x * 2.0f };
		float repeats_width{ GetCountControlWidth("Repeats") };
		float button_width{ ImGui::GetFrameHeight() };
		ActionForm displayed_form{ GetActionForm(action) };
		ActionForm requested_form{ displayed_form };
		bool form_changed{ false };
		float parameter_left_screen_x{ ImGui::GetCursorScreenPos().x };
		int column_count{
			displayed_form == ActionForm::Tween
				? 6
				: displayed_form == ActionForm::Action
					? 4
					: 3
		};

		if (ImGui::BeginTable(
				"ActionRow", column_count,
				ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
			)) {
			ImGui::TableSetupColumn("Drag", ImGuiTableColumnFlags_WidthFixed, drag_width);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, type_width);

			if (displayed_form == ActionForm::Tween) {
				ImGui::TableSetupColumn(
					"Target", ImGuiTableColumnFlags_WidthFixed, target_width
				);
				ImGui::TableSetupColumn(
					"Duration", ImGuiTableColumnFlags_WidthFixed, duration_width
				);
				ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn(
					"Repeats", ImGuiTableColumnFlags_WidthFixed, repeats_width
				);
			} else if (displayed_form == ActionForm::Action) {
				ImGui::TableSetupColumn(
					"Target", ImGuiTableColumnFlags_WidthFixed, target_width
				);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			} else {
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			}

			ImGui::TableNextRow(ImGuiTableRowFlags_None, button_width);

			int column{};
			ImGui::TableSetColumnIndex(column++);
			bool dimmed{ !action.enabled };

			if (dimmed) {
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
			}

			ImGui::Button("::", ImVec2{ drag_width, button_width });

			if (dimmed) {
				ImGui::PopStyleVar();
			}

			action_context_requested = IsItemRightClicked();

			DrawTooltip(
				action.enabled ? "Drag to reorder. Right click for action options."
							   : "Disabled action. Right click for action options."
			);

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				ActionDragPayload payload{ index };
				ImGui::SetDragDropPayload("PTGN_SCRIPT_ACTION", &payload, sizeof(payload));
				ImGui::Text("%d. %s", index + 1, ActionSummary(action).c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload{
						ImGui::AcceptDragDropPayload("PTGN_SCRIPT_ACTION") }) {
					const auto* drag{ static_cast<const ActionDragPayload*>(payload->Data) };

					if (drag) {
						move_from = drag->index;
						move_to	  = index;
					}
				}

				ImGui::EndDragDropTarget();
			}

			ImGui::TableSetColumnIndex(column++);
			parameter_left_screen_x = ImGui::GetCursorScreenPos().x;
			ImGui::SetNextItemWidth(-FLT_MIN);

			if (ImGui::BeginCombo(
					"##Form", kActionFormLabels[static_cast<std::size_t>(displayed_form)]
				)) {
				for (int i{ 0 }; i < static_cast<int>(kActionFormLabels.size()); ++i) {
					auto candidate{ static_cast<ActionForm>(i) };

					if (ImGui::Selectable(
							kActionFormLabels[static_cast<std::size_t>(i)],
							candidate == displayed_form
						)) {
						requested_form = candidate;
						form_changed   = true;
					}
				}

				ImGui::EndCombo();
			}

			action_context_requested |= IsItemRightClicked();
			DrawTooltip("Choose an Action, Tween, or Delay. Right click for action options.");

			if (displayed_form == ActionForm::Tween) {
				ImGui::TableSetColumnIndex(column++);
				changed |= DrawActionTarget(context, action);

				ImGui::TableSetColumnIndex(column++);
				changed |= DrawDurationInput(
					"##Duration", action.timing->duration_ms, -FLT_MIN,
					"Duration of each Tween cycle."
				);

				ImGui::TableSetColumnIndex(column++);
				changed |= DrawActionPickerWithInline(
					context, action, true, &action_context_requested
				);

				ImGui::TableSetColumnIndex(column);
				changed |= DrawCountControl(
					"Repeats", action.timing->additional_repeats, 0, 100,
					action.timing->infinite_repeats, "Additional full duration cycles."
				);
			} else {
				if (displayed_form == ActionForm::Action) {
					ImGui::TableSetColumnIndex(column++);
					changed |= DrawActionTarget(context, action);
				}

				ImGui::TableSetColumnIndex(column);

				switch (displayed_form) {
					case ActionForm::Action:
						changed |= DrawActionPickerWithInline(
							context, action, false, &action_context_requested
						);
						break;
					case ActionForm::Delay:
						changed |= DrawDurationInput(
							"##Duration", action.timing->duration_ms, -FLT_MIN,
							"Delay before continuing."
						);
						break;
					case ActionForm::Tween: break;
				}
			}

			ImGui::EndTable();
		}

		if (action_context_requested) {
			ImGui::OpenPopup("ActionContext");
		}

		if (ImGui::BeginPopup("ActionContext")) {
			changed |= DrawEnableDisableMenuItem(action.enabled);

			if (ImGui::MenuItem("Duplicate")) {
				duplicate = true;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Delete")) {
				remove = true;
			}

			ImGui::EndPopup();
		}

		if (form_changed) {
			SetActionForm(action, requested_form);
			displayed_form = requested_form;
			changed		   = true;
		}

		if (displayed_form == ActionForm::Tween && action.timing) {
			changed |= DrawTimingOptions(context, action, *action.timing, parameter_left_screen_x);
		}

		if (displayed_form == ActionForm::Action || displayed_form == ActionForm::Tween) {
			changed |= DrawActionParameters(context, action, parameter_left_screen_x);
		}

		DrawActiveActionProgress(
			displayed_runtime, static_cast<std::size_t>(index), action
		);

		ImGui::PopID();

		if (remove) {
			remove_index = index;
		}

		if (duplicate) {
			duplicate_index = index;
		}
	}

	if (move_from >= 0 && move_to >= 0) {
		MoveAction(sequence.steps, move_from, move_to);
		binding.runtime = ScriptSequenceRuntime{};
		changed			= true;
	}

	if (duplicate_index >= 0) {
		ScriptStep copy{ sequence.steps[static_cast<std::size_t>(duplicate_index)] };
		sequence.steps.insert(sequence.steps.begin() + duplicate_index + 1, std::move(copy));
		binding.runtime = ScriptSequenceRuntime{};
		changed			= true;
	}

	if (remove_index >= 0) {
		sequence.steps.erase(sequence.steps.begin() + remove_index);
		binding.runtime = ScriptSequenceRuntime{};
		changed			= true;
	}

	ImGui::PopID();
	return changed;
}


bool DrawLifecycleRows(ScriptEditorContext& context, ScriptSequence& sequence) {
	ImGui::PushID("LifecycleRows");

	bool changed{ false };
	int remove{ -1 };

	for (int i{ 0 }; i < static_cast<int>(sequence.lifecycle_actions.size()); ++i) {
		auto& callback{ sequence.lifecycle_actions[static_cast<std::size_t>(i)] };
		bool context_requested{ false };
		ImGui::PushID(i);

		float lifecycle_width{ 145.0f };

		if (ImGui::BeginTable(
				"LifecycleRow", 2,
				ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
			)) {
			ImGui::TableSetupColumn("Lifecycle", ImGuiTableColumnFlags_WidthFixed, lifecycle_width);
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

			ImGui::TableSetColumnIndex(0);
			ImGui::BeginDisabled(!callback.enabled);
			int lifecycle{ static_cast<int>(callback.lifecycle) };
			ImGui::SetNextItemWidth(-FLT_MIN);

			if (ImGui::Combo(
					"##Lifecycle", &lifecycle, kLifecycleLabels.data(),
					static_cast<int>(kLifecycleLabels.size())
				)) {
				callback.lifecycle = static_cast<SequenceLifecycle>(lifecycle);
				changed			   = true;
			}

			context_requested |= IsItemRightClicked();
			DrawTooltip("Choose when this callback runs.");
			ImGui::EndDisabled();

			ImGui::TableSetColumnIndex(1);
			ImGui::BeginDisabled(!callback.enabled);
			changed |= DrawActionPickerWithInline(context, callback.action, false);
			context_requested |= IsItemRightClicked();
			ImGui::EndDisabled();
			ImGui::EndTable();
		}

		if (context_requested) {
			ImGui::OpenPopup("LifecycleContext");
		}

		if (ImGui::BeginPopup("LifecycleContext")) {
			changed |= DrawEnableDisableMenuItem(callback.enabled);
			ImGui::Separator();

			if (ImGui::MenuItem("Delete")) {
				remove = i;
			}

			ImGui::EndPopup();
		}

		ImGui::BeginDisabled(!callback.enabled);
		changed |= DrawActionParameters(context, callback.action, ImGui::GetCursorScreenPos().x);
		ImGui::EndDisabled();
		ImGui::PopID();
	}

	if (remove >= 0) {
		sequence.lifecycle_actions.erase(sequence.lifecycle_actions.begin() + remove);
		changed = true;
	}

	ImGui::PopID();
	return changed;
}


template <typename... TEvents>
[[nodiscard]] bool IsTriggerType(TypeHashValue type_hash) {
	return ((type_hash == Hash<TEvents>()) || ...);
}

[[nodiscard]] bool HasRequiredTriggerComponent(Entity owner, TypeHashValue type_hash) {
	if (IsTriggerType<
			event::MouseMoveOver, event::MouseMoveOut, event::MousePressedOver,
			event::MouseHeldOver, event::MouseReleasedOver
		>(type_hash)) {
		return owner && owner.Has<::ptgn::impl::Interactive>();
	}

	if (IsTriggerType<
			event::ButtonPress, event::ButtonHoverStart, event::ButtonHover,
			event::ButtonHoverStop
		>(type_hash)) {
		return owner && owner.Has<::ptgn::impl::ButtonData>();
	}

	if (IsTriggerType<event::ToggleButtonToggle>(type_hash)) {
		return owner && owner.Has<::ptgn::impl::ToggleButtonData>();
	}

	if (IsTriggerType<
			event::DropdownOpen, event::DropdownClose, event::DropdownToggle,
			event::DropdownItemPress
		>(type_hash)) {
		return owner && owner.Has<::ptgn::impl::DropdownData>();
	}

	if (IsTriggerType<
			event::DragStart, event::Drag, event::DragStop, event::PickupDraggable,
			event::DropDraggable, event::DragEnter, event::DragLeave, event::DragOver,
			event::DragOut
		>(type_hash)) {
		return owner && owner.Has<::ptgn::impl::Draggable>();
	}

	if (IsTriggerType<
			event::PickupFromDropzone, event::DropIntoDropzone, event::EnterDropzone,
			event::LeaveDropzone, event::MoveOverDropzone, event::MoveOutsideDropzone
		>(type_hash)) {
		return owner && owner.Has<::ptgn::impl::Dropzone>();
	}

	if (IsTriggerType<
			event::OverlapStart, event::Overlap, event::OverlapStop, event::Collision
		>(type_hash)) {
		return owner && owner.Has<Collider>();
	}

	if (IsTriggerType<
			event::AnimationStart, event::AnimationStop, event::AnimationPause,
			event::AnimationResume, event::AnimationFrameChange, event::AnimationUpdate,
			event::AnimationComplete, event::AnimationLoopComplete
		>(type_hash)) {
		return owner && owner.Has<::ptgn::impl::AnimationData>();
	}

	return true;
}

[[nodiscard]] bool IsTriggerCandidateAvailable(
	const ScriptEditorContext& context, const EventEditorRegistration& candidate
) {
	const auto* registration{ SequenceEventRegistry::Find(candidate.type_hash) };
	Entity owner{ ResolveInspectedEntity(context) };

	if (!registration || !HasRequiredTriggerComponent(owner, candidate.type_hash)) {
		return false;
	}

	if (owner && registration->available && !registration->available(owner)) {
		return false;
	}

	return true;
}


bool DrawTimerElapsedTrigger(
	ScriptEditorContext& context,
	json& value
) {
	TimerKey timer;
	std::optional<millisecondsf> duration_override;

	if (value.is_object()) {
		const auto timer_it{ value.find("timer") };
		if (timer_it != value.end()) {
			try {
				timer_it->get_to(timer);
			} catch (...) {
				timer = TimerKey{};
			}
		}

		const auto duration_it{ value.find("duration") };
		if (duration_it != value.end() && !duration_it->is_null()) {
			try {
				millisecondsf duration;
				duration_it->get_to(duration);

				if (duration > millisecondsf{ 0.0f }) {
					duration_override = duration;
				}
			} catch (...) {
				duration_override.reset();
			}
		}
	}

	Entity owner{ ResolveInspectedEntity(context) };
	const auto choices{ GetTimerChoices(owner, std::nullopt) };

	float available{ ImGui::GetContentRegionAvail().x };
	float spacing{ ImGui::GetStyle().ItemSpacing.x };
	float checkbox_width{ ImGui::GetFrameHeight() };
	float duration_width{
		std::min(
			110.0f,
			std::max(72.0f, available * 0.32f)
		)
	};
	float timer_width{
		std::max(
			1.0f,
			available -
				checkbox_width -
				duration_width -
				spacing * 2.0f
		)
	};

	bool changed{ false };
	ImGui::SetNextItemWidth(timer_width);

	const char* preview{
		timer.value.empty()
			? "No Timer"
			: timer.value.c_str()
	};

	if (ImGui::BeginCombo("##TimerTrigger", preview)) {
		std::string custom_name{ timer.value };

		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputTextWithHint(
				"##CustomTimerName",
				"Custom timer name...",
				&custom_name
			)) {
			timer.value = std::move(custom_name);
			changed = true;
		}

		ImGui::Separator();

		bool none_selected{ timer.value.empty() };
		if (ImGui::Selectable("None", none_selected)) {
			if (!none_selected) {
				timer.value.clear();
				changed = true;
			}
		}

		for (const auto& choice : choices) {
			bool selected{ choice == timer };

			if (ImGui::Selectable(choice.value.c_str(), selected)) {
				if (!selected) {
					timer = choice;
					changed = true;
				}
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}

		if (choices.empty()) {
			ImGui::TextDisabled("No timers on this entity.");
		}

		ImGui::EndCombo();
	}

	DrawTooltip(
		"Named timer whose elapsed threshold starts or stops this sequence. "
		"Open the combo to choose an existing timer or enter a custom name."
	);

	std::optional<millisecondsf> timer_duration;

	if (owner) {
		if (const auto* timers{ owner.TryGet<::ptgn::impl::Timers>() }) {
			const auto it{ std::ranges::find_if(
				timers->timers,
				[&timer](const TimerEntry& entry) {
					return entry.config.key == timer;
				}
			) };

			if (it != timers->timers.end()) {
				timer_duration = it->config.duration;
			}
		}
	}

	millisecondsf displayed_duration{
		duration_override.value_or(
			timer_duration.value_or(millisecondsf{ 1000.0f })
		)
	};
	displayed_duration = millisecondsf{
		std::max(0.001f, displayed_duration.count())
	};

	if (timer_duration && *timer_duration > millisecondsf{ 0.0f }) {
		displayed_duration = std::min(
			displayed_duration,
			*timer_duration
		);
	}

	bool use_duration_override{ duration_override.has_value() };

	ImGui::SameLine(0.0f, spacing);
	if (ImGui::Checkbox(
			"##TimerElapsedDurationOverride",
			&use_duration_override
		)) {
		if (use_duration_override) {
			duration_override = displayed_duration;
		} else {
			duration_override.reset();
		}

		changed = true;
	}
	DrawTooltip(
		"Use a custom elapsed duration. Unchecked uses the timer's configured duration."
	);

	ImGui::SameLine(0.0f, spacing);
	ImGui::BeginDisabled(!use_duration_override);

	if (DrawDurationTextInput(
			"##TimerElapsedDuration",
			displayed_duration,
			duration_width,
			false,
			"Elapsed duration required before this trigger matches."
		)) {
		displayed_duration = millisecondsf{
			std::max(0.001f, displayed_duration.count())
		};

		if (timer_duration && *timer_duration > millisecondsf{ 0.0f }) {
			displayed_duration = std::min(
				displayed_duration,
				*timer_duration
			);
		}

		duration_override = displayed_duration;
		changed = true;
	}

	ImGui::EndDisabled();

	if (changed) {
		if (!value.is_object()) {
			value = json::object();
		}

		value["timer"] = timer;

		if (duration_override) {
			value["duration"] = *duration_override;
		} else {
			value["duration"] = nullptr;
		}
	}

	return changed;
}

bool DrawEvent(
	ScriptEditorContext& context, EventCondition& event, bool stop_event, bool& switch_kind,
	bool& changed
) {
	bool remove{ false };
	bool context_requested{ false };

	float kind_width{
		std::max(ImGui::CalcTextSize("Start").x, ImGui::CalcTextSize("Stop").x) +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};
	float consume_width{ ImGui::CalcTextSize("Consume").x +
							   ImGui::GetStyle().FramePadding.x * 2.0f };
	const EventEditorRegistration* selected{ EventEditorRegistry::Find(event.type_hash) };
	bool event_type_changed{ false };

	auto inline_field_count = [](const EventEditorRegistration* registration) {
		return registration ? registration->options.inline_fields : 0;
	};

	if (ImGui::BeginTable(
			"EventRow", 3,
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
		)) {
		ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, kind_width);
		ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Consume", ImGuiTableColumnFlags_WidthFixed, consume_width);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

		int column{};
		ImGui::TableSetColumnIndex(column++);
		ImGui::BeginDisabled(!event.enabled);

		if (ImGui::Button(
				stop_event ? "Stop" : "Start", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			switch_kind = true;
		}

		context_requested |= IsItemRightClicked();

		DrawTooltip(
			stop_event ? "Change this to a start trigger. Right click for trigger options."
					   : "Change this to a stop trigger. Right click for trigger options."
		);
		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(column++);
		ImGui::BeginDisabled(!event.enabled);

		int initial_inline_fields{ inline_field_count(selected) };
		float available_width{ ImGui::GetContentRegionAvail().x };
		float spacing{ ImGui::GetStyle().ItemSpacing.x };
		float event_width{ available_width };

		if (initial_inline_fields > 0) {
			event_width = std::clamp(available_width * 0.4f, 110.0f, 190.0f);
			float minimum_fields_width{ 80.0f * initial_inline_fields };

			if (available_width - event_width -
					spacing * static_cast<float>(initial_inline_fields) <
				minimum_fields_width) {
				event_width = std::max(
					90.0f, available_width - spacing * static_cast<float>(initial_inline_fields) -
							   minimum_fields_width
				);
			}
		}

		std::string selected_label{
			selected ? TrimMenuText(selected->options.label) : std::string{ "Missing Event" }
		};
		ImGui::SetNextItemWidth(std::max(1.0f, event_width));
		bool combo_open{ ImGui::BeginCombo("##Event", selected_label.c_str()) };
		context_requested |= IsItemRightClicked();

		if (combo_open) {
			auto select_candidate = [&](const EventEditorRegistration& candidate) {
				const auto* registration{ SequenceEventRegistry::Find(candidate.type_hash) };

				if (!IsTriggerCandidateAvailable(context, candidate)) {
					return;
				}

				std::string label{ TrimMenuText(candidate.options.label) };

				if (ImGui::MenuItem(
						label.c_str(), nullptr, candidate.type_hash == event.type_hash
					)) {
					event.type_hash = candidate.type_hash;
					event.name		= registration->name;
					registration->set_defaults(event);
					ApplyContextualTimerElapsedDefault(context, event);
					selected		   = EventEditorRegistry::Find(event.type_hash);
					event_type_changed = true;
					changed			   = true;
				}

				DrawTooltip(candidate.options.description.c_str());
			};

			for (const auto& candidate : EventEditorRegistry::Entries()) {
				if (TrimMenuText(candidate.options.group).empty()) {
					select_candidate(candidate);
				}
			}

			std::vector<std::string> groups;

			for (const auto& candidate : EventEditorRegistry::Entries()) {
				std::string group{ TrimMenuText(candidate.options.group) };

				if (!group.empty() && IsTriggerCandidateAvailable(context, candidate) &&
					!std::ranges::contains(groups, group)) {
					groups.push_back(group);
				}
			}

			for (const auto& group : groups) {
				if (!ImGui::BeginMenu(group.c_str())) {
					continue;
				}

				for (const auto& candidate : EventEditorRegistry::Entries()) {
					if (TrimMenuText(candidate.options.group) == group) {
						select_candidate(candidate);
					}
				}

				ImGui::EndMenu();
			}

			ImGui::EndCombo();
		}

		if (selected && !event_type_changed && selected->options.inline_fields > 0) {
			ImGui::SameLine();

			if (event.type_hash == Hash<event::TimerElapsed>()) {
				changed |= DrawTimerElapsedTrigger(context, event.value);
			} else if (selected->options.draw) {
				changed |= selected->options.draw(event.value);
			}
		}

		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(column);
		ImGui::BeginDisabled(!event.enabled);
		changed |= DrawToggleButton(
			"Consume", event.consume, ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() },
			"Stop propagation after this event matches."
		);
		ImGui::EndDisabled();
		ImGui::EndTable();
	}

	if (context_requested) {
		ImGui::OpenPopup("TriggerContext");
	}

	if (ImGui::BeginPopup("TriggerContext")) {
		changed |= DrawEnableDisableMenuItem(event.enabled);
		ImGui::Separator();

		if (ImGui::MenuItem("Delete")) {
			remove = true;
		}

		ImGui::EndPopup();
	}

	return remove;
}


bool DrawAddTriggerPopup(ScriptEditorContext& context, ScriptSequence& sequence) {
	if (!ImGui::BeginPopup("AddTrigger")) {
		return false;
	}

	bool changed{ false };
	auto add_candidate = [&](const EventEditorRegistration& candidate) {
		const auto* registration{ SequenceEventRegistry::Find(candidate.type_hash) };

		if (!registration || !IsTriggerCandidateAvailable(context, candidate)) {
			return;
		}

		std::string label{ TrimMenuText(candidate.options.label) };

		if (ImGui::MenuItem(label.c_str())) {
			EventCondition trigger{
				.enabled   = true,
				.type_hash = registration->type_hash,
				.name      = registration->name,
			};
			registration->set_defaults(trigger);
			ApplyContextualTimerElapsedDefault(context, trigger);
			sequence.start_events.push_back(std::move(trigger));
			changed = true;
		}

		DrawTooltip(candidate.options.description.c_str());
	};

	for (const auto& candidate : EventEditorRegistry::Entries()) {
		if (TrimMenuText(candidate.options.group).empty()) {
			add_candidate(candidate);
		}
	}

	std::vector<std::string> groups;

	for (const auto& candidate : EventEditorRegistry::Entries()) {
		std::string group{ TrimMenuText(candidate.options.group) };

		if (!group.empty() && IsTriggerCandidateAvailable(context, candidate) &&
			!std::ranges::contains(groups, group)) {
			groups.push_back(group);
		}
	}

	for (const auto& group : groups) {
		if (!ImGui::BeginMenu(group.c_str())) {
			continue;
		}

		for (const auto& candidate : EventEditorRegistry::Entries()) {
			if (TrimMenuText(candidate.options.group) == group) {
				add_candidate(candidate);
			}
		}

		ImGui::EndMenu();
	}

	ImGui::EndPopup();
	return changed;
}

bool DrawAddActionPopup(ScriptSequence& sequence) {
	if (!ImGui::BeginPopup("AddAction")) {
		return false;
	}

	bool changed{ false };

	if (ImGui::MenuItem("Action")) {
		sequence.steps.push_back(ScriptRegistry::MakeStep<EmitSignalScript>());
		changed = true;
	}

	if (ImGui::MenuItem("Tween")) {
		auto action{ ScriptRegistry::MakeStep<MoveToScript>() };
		SetActionForm(action, ActionForm::Tween);
		sequence.steps.push_back(std::move(action));
		changed = true;
	}

	if (ImGui::MenuItem("Delay")) {
		sequence.steps.push_back(ScriptRegistry::MakeStep<WaitScript>());
		changed = true;
	}

	ImGui::Separator();

	if (ImGui::BeginMenu("Lifecycle callback")) {
		for (int i{ 0 }; i < static_cast<int>(kLifecycleLabels.size()); ++i) {
			if (ImGui::MenuItem(kLifecycleLabels[static_cast<std::size_t>(i)])) {
				sequence.lifecycle_actions.push_back(
					LifecycleScript{
						.enabled   = true,
						.lifecycle = static_cast<SequenceLifecycle>(i),
						.action	   = ScriptRegistry::MakeStep<EmitSignalScript>(),
					}
				);
				changed = true;
			}
		}

		ImGui::EndMenu();
	}

	DrawTooltip("Add a lifecycle callback.");
	ImGui::EndPopup();
	return changed;
}

bool DrawSequenceOptionsCombo(
	ScriptEditorContext& context, ScriptSequence& binding, ScriptSequence*& sequence,
	float width = -FLT_MIN
) {
	bool changed{ false };
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

	ImGui::SetNextItemWidth(width);

	if (ImGui::BeginCombo("##SequenceOptions", "Options")) {
		if (ResolveInspectedEntity(context)) {
			bool global{ binding.shared_reference };

			if (ImGui::MenuItem("Global sequence", nullptr, global)) {
				if (global) {
					DetachToLocal(context, binding);
				} else {
					PromoteToShared(context, binding);
				}

				sequence = ResolveEditorSequence(context, binding);
				changed	 = true;
			}

			ImGui::Separator();
		}

		if (sequence &&
			ImGui::MenuItem(
				"Remove binding on complete", nullptr, sequence->remove_binding_on_complete
			)) {
			sequence->remove_binding_on_complete = !sequence->remove_binding_on_complete;
			changed								 = true;
		}

		if (sequence &&
			ImGui::MenuItem(
				"Destroy owner on complete", nullptr, sequence->destroy_owner_on_complete
			)) {
			sequence->destroy_owner_on_complete = !sequence->destroy_owner_on_complete;
			changed								= true;
		}

		if (sequence) {
			bool has_channel{ sequence->channel.has_value() };

			if (ImGui::MenuItem("Use sequence channel", nullptr, has_channel)) {
				if (has_channel) {
					sequence->channel.reset();
				} else {
					sequence->channel = "default";
				}

				changed = true;
			}
		}

		if (sequence &&
			ImGui::MenuItem(
				"Ignore on retrigger", nullptr,
				sequence->reentry == ReentryMode::IgnoreWhileRunning
			)) {
			sequence->reentry = ReentryMode::IgnoreWhileRunning;
			changed			  = true;
		}

		if (sequence &&
			ImGui::MenuItem(
				"Restart on retrigger", nullptr, sequence->reentry == ReentryMode::Restart
			)) {
			sequence->reentry = ReentryMode::Restart;
			changed			  = true;
		}

		if (sequence &&
			ImGui::MenuItem(
				"Queue on retrigger", nullptr, sequence->reentry == ReentryMode::Queue
			)) {
			sequence->reentry = ReentryMode::Queue;
			changed			  = true;
		}

		ImGui::EndCombo();
	}

	DrawSelectedItemsTooltip(selected_sequence_options);
	return changed;
}

bool DrawSequenceToolbar(
	ScriptEditorContext& context, ScriptSequence& binding, ScriptSequence*& sequence,
	int resident_index
) {
	bool changed{ false };
	bool show_runtime_controls{ IsRuntimeActive(context) };
	Entity runtime_owner{ ResolveRuntimeOwner(context) };
	ScriptSequence* runtime_binding{
		show_runtime_controls
			? FindRuntimeRootSequence(runtime_owner, binding.id, resident_index)
			: nullptr
	};
	bool can_control_runtime{ runtime_owner && runtime_binding };
	float button_size{ ImGui::GetFrameHeight() };
	float spacing{ ImGui::GetStyle().ItemSpacing.x };

	if (!ImGui::BeginTable(
			"SequenceToolbar", 3,
			ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings
		)) {
		return false;
	}

	ImGui::TableSetupColumn("Trigger", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableSetupColumn("Options", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);

	ImGui::TableSetColumnIndex(0);

	if (ImGui::Button("+ Trigger", ImVec2{ -FLT_MIN, button_size })) {
		ImGui::OpenPopup("AddTrigger");
	}

	DrawTooltip("Add a start trigger.");

	if (sequence) {
		changed |= DrawAddTriggerPopup(context, *sequence);
	}

	ImGui::TableSetColumnIndex(1);

	if (ImGui::Button("+ Action", ImVec2{ -FLT_MIN, button_size })) {
		ImGui::OpenPopup("AddAction");
	}

	DrawTooltip("Add an Action, Tween, Delay, or lifecycle callback.");

	if (sequence) {
		changed |= DrawAddActionPopup(*sequence);
	}

	ImGui::TableSetColumnIndex(2);

	float available_width{ ImGui::GetContentRegionAvail().x };
	float runtime_width{
		show_runtime_controls ? button_size * 3.0f + spacing * 3.0f : 0.0f
	};
	float options_width{ std::max(1.0f, available_width - runtime_width) };

	changed |= DrawSequenceOptionsCombo(
		context, binding, sequence, show_runtime_controls ? options_width : -FLT_MIN
	);

	if (show_runtime_controls) {
		const ScriptSequenceRuntime& runtime{
			runtime_binding ? runtime_binding->runtime
						: (sequence ? GetDisplayedSequenceRuntime(*sequence, binding)
									: binding.runtime)
		};

		ImGui::SameLine(0.0f, spacing);
		ImGui::BeginDisabled(!can_control_runtime);

		if (DrawCenteredTextButton("##Play", ">", ImVec2{ button_size, button_size })) {
			(void)script_runtime::Start(runtime_owner, runtime_binding->id, true);
		}

		ImGui::EndDisabled();
		DrawTooltip(runtime.running ? "Restart this sequence." : "Start this sequence.");

		ImGui::SameLine(0.0f, spacing);
		ImGui::BeginDisabled(!can_control_runtime || !runtime.running);

		if (DrawCenteredTextButton("##Pause", "||", ImVec2{ button_size, button_size })) {
			(void)script_runtime::SetPaused(runtime_owner, runtime_binding->id, !runtime.paused);
		}

		ImGui::EndDisabled();
		DrawTooltip(runtime.paused ? "Resume this sequence." : "Pause this sequence.");

		ImGui::SameLine(0.0f, spacing);
		ImGui::BeginDisabled(!can_control_runtime || !runtime.running);

		if (DrawCenteredTextButton("##Stop", "[]", ImVec2{ button_size, button_size })) {
			(void)script_runtime::Stop(runtime_owner, runtime_binding->id);
		}

		ImGui::EndDisabled();
		DrawTooltip("Stop this sequence.");
	}

	ImGui::EndTable();
	return changed;
}

bool DrawEvents(ScriptEditorContext& context, ScriptSequence& sequence) {
	bool changed{ false };
	int remove_start{ -1 };
	int move_start_to_stop{ -1 };

	ImGui::PushID("StartEvents");

	for (int i{ 0 }; i < static_cast<int>(sequence.start_events.size()); ++i) {
		ImGui::PushID(i);

		bool switch_kind{ false };

		if (DrawEvent(
				context, sequence.start_events[static_cast<std::size_t>(i)], false, switch_kind,
				changed
			)) {
			remove_start = i;
		}

		if (switch_kind) {
			move_start_to_stop = i;
		}

		ImGui::PopID();
	}

	ImGui::PopID();

	int remove_stop{ -1 };
	int move_stop_to_start{ -1 };

	ImGui::PushID("StopEvents");

	for (int i{ 0 }; i < static_cast<int>(sequence.stop_events.size()); ++i) {
		ImGui::PushID(i);

		bool switch_kind{ false };

		if (DrawEvent(
				context, sequence.stop_events[static_cast<std::size_t>(i)], true, switch_kind,
				changed
			)) {
			remove_stop = i;
		}

		if (switch_kind) {
			move_stop_to_start = i;
		}

		ImGui::PopID();
	}

	ImGui::PopID();

	std::optional<EventCondition> moved_to_stop;
	std::optional<EventCondition> moved_to_start;

	if (remove_start < 0 && move_start_to_stop >= 0) {
		moved_to_stop.emplace(
			std::move(sequence.start_events[static_cast<std::size_t>(move_start_to_stop)])
		);
	}

	if (remove_stop < 0 && move_stop_to_start >= 0) {
		moved_to_start.emplace(
			std::move(sequence.stop_events[static_cast<std::size_t>(move_stop_to_start)])
		);
	}

	if (remove_start >= 0) {
		sequence.start_events.erase(sequence.start_events.begin() + remove_start);
		changed = true;
	} else if (move_start_to_stop >= 0) {
		sequence.start_events.erase(sequence.start_events.begin() + move_start_to_stop);
		changed = true;
	}

	if (remove_stop >= 0) {
		sequence.stop_events.erase(sequence.stop_events.begin() + remove_stop);
		changed = true;
	} else if (move_stop_to_start >= 0) {
		sequence.stop_events.erase(sequence.stop_events.begin() + move_stop_to_start);
		changed = true;
	}

	if (moved_to_stop) {
		sequence.stop_events.push_back(std::move(*moved_to_stop));
	}

	if (moved_to_start) {
		sequence.start_events.push_back(std::move(*moved_to_start));
	}

	changed |= DrawLifecycleRows(context, sequence);
	return changed;
}


bool DrawSequence(
	ScriptEditorContext& context, ScriptSequence& binding, bool& changed, int resident_index
) {
	ScriptSequence* sequence{ ResolveEditorSequence(context, binding) };

	if (!sequence) {
		ImGui::TextDisabled("Missing shared Script Sequence");
		return false;
	}

	bool remove{ false };
	auto& state{ GetScriptInspectorState() };

	ImGui::PushID("ScriptSequence");
	ImGui::PushID(resident_index);

	ImGuiID editor_id{ ImGui::GetID("EditorState") };
	float button_size{ ImGui::GetFrameHeight() };
	auto& stored_open{ state.sequence_open_states.try_emplace(binding.id, true).first->second };
	bool open{ stored_open };
	ImVec2 name_input_min{};
	ImVec2 name_input_max{};
	bool name_input_drawn{ false };
	bool name_input_hovered{ false };
	bool began_name_edit_this_frame{ false };
	bool begin_edit{ false };

	if (ImGui::BeginTable(
			"ScriptSequenceHeader", 1,
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
		)) {
		ImGui::TableSetupColumn("Sequence", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);
		ImGui::TableSetColumnIndex(0);

		ImVec2 header_min{ ImGui::GetCursorScreenPos() };
		ImVec2 header_max{
			header_min.x + std::max(1.0f, ImGui::GetContentRegionAvail().x),
			header_min.y + button_size,
		};
		ImVec2 mouse{ ImGui::GetMousePos() };

		ImGui::SetNextItemOpen(stored_open, ImGuiCond_Always);
		bool editing_before_draw{ state.editing_sequence_name == editor_id };
		ImVec4 header{ binding.enabled ? ImVec4{ 0.35f, 0.24f, 0.39f, 1.0f }
											 : ImVec4{ 0.25f, 0.25f, 0.25f, 1.0f } };
		ImVec4 header_hovered{ binding.enabled ? ImVec4{ 0.44f, 0.31f, 0.48f, 1.0f }
													 : ImVec4{ 0.30f, 0.30f, 0.30f, 1.0f } };
		ImVec4 header_active{ binding.enabled ? ImVec4{ 0.50f, 0.36f, 0.55f, 1.0f }
													: ImVec4{ 0.34f, 0.34f, 0.34f, 1.0f } };
		bool header_hovered_before_draw{
			ImGui::IsWindowHovered() &&
			ImGui::IsMouseHoveringRect(header_min, header_max)
		};
		bool header_active_before_draw{ !editing_before_draw && header_hovered_before_draw &&
											  ImGui::IsMouseDown(ImGuiMouseButton_Left) };
		ImVec4 header_color{
			editing_before_draw ? header
								: (header_active_before_draw
									   ? header_active
									   : (header_hovered_before_draw ? header_hovered : header))
		};
		ImGui::GetWindowDrawList()->AddRectFilled(
			header_min, header_max, ImGui::GetColorU32(header_color),
			ImGui::GetStyle().FrameRounding
		);

		ImVec4 transparent{};
		ImGui::PushStyleColor(ImGuiCol_Header, transparent);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, transparent);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, transparent);
		open = ImGui::TreeNodeEx(
			"##ScriptSequence",
			ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_AllowOverlap,
			"%s", editing_before_draw ? "" : sequence->name.c_str()
		);
		ImGui::PopStyleColor(3);
		stored_open = open;

		ImVec2 tree_min{ header_min };
		ImVec2 tree_max{ header_max };
		bool tree_hovered{ ImGui::IsItemHovered() };
		float text_start_x{ tree_min.x + ImGui::GetFrameHeight() };
		float minimum_name_width{ 48.0f };
		float visible_name_width{
			std::max(minimum_name_width, ImGui::CalcTextSize(sequence->name.c_str()).x)
		};
		float name_hit_end_x{ std::min(tree_max.x, text_start_x + visible_name_width) };
		bool name_hit_hovered{ tree_hovered && mouse.x >= text_start_x &&
									 mouse.x <= name_hit_end_x };
		begin_edit = !editing_before_draw && name_hit_hovered &&
					 ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

		if (ImGui::BeginPopupContextItem("SequenceContext")) {
			if (ImGui::MenuItem("Rename")) {
				begin_edit = true;
			}

			if (DrawEnableDisableMenuItem(binding.enabled)) {
				changed = true;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Delete")) {
				remove = true;
			}

			ImGui::EndPopup();
		}

		if (begin_edit) {
			state.editing_sequence_name			 = editor_id;
			state.editing_sequence_original_name = sequence->name;
			began_name_edit_this_frame			 = true;
		}

		if (state.editing_sequence_name == editor_id) {
			ImGui::SetCursorScreenPos(ImVec2{ text_start_x, tree_min.y });
			ImGui::SetNextItemWidth(
				std::max(
					minimum_name_width, tree_max.x - text_start_x - ImGui::GetStyle().FramePadding.x
				)
			);

			if (begin_edit) {
				ImGui::SetKeyboardFocusHere();
			}

			bool submitted{ ImGui::InputText(
				"##SequenceName", &sequence->name, ImGuiInputTextFlags_EnterReturnsTrue
			) };
			changed			   |= ImGui::IsItemEdited();
			name_input_min		= ImGui::GetItemRectMin();
			name_input_max		= ImGui::GetItemRectMax();
			name_input_drawn	= true;
			name_input_hovered	= ImGui::IsItemHovered() || ImGui::IsItemActive();

			if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				sequence->name = state.editing_sequence_original_name;
				state.editing_sequence_name.reset();
			} else if (submitted) {
				state.editing_sequence_name.reset();
				changed = true;
			}
		}

		if (tree_hovered && state.editing_sequence_name != editor_id) {
			ImGui::SetTooltip("Double click to rename. Right click for sequence options.");
		}

		ImGui::EndTable();
	}

	if (state.editing_sequence_name == editor_id && name_input_drawn &&
		!began_name_edit_this_frame) {
		bool clicked{ ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
							ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
							ImGui::IsMouseClicked(ImGuiMouseButton_Right) };
		ImVec2 mouse{ ImGui::GetMousePos() };
		bool inside_input{ mouse.x >= name_input_min.x && mouse.x <= name_input_max.x &&
								 mouse.y >= name_input_min.y && mouse.y <= name_input_max.y };

		if (clicked && !inside_input && !name_input_hovered) {
			state.editing_sequence_name.reset();
			changed = true;
		}
	}

	if (remove) {
		state.editing_sequence_name.reset();
	}

	if (open && !remove) {
		sequence = ResolveEditorSequence(context, binding);

		if (sequence) {
			changed |= DrawSequenceToolbar(context, binding, sequence, resident_index);

			if (sequence && sequence->channel &&
				ImGui::BeginTable(
					"SequenceChannelRow", 2,
					ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
				)) {
				float label_width{ ImGui::CalcTextSize("Channel:").x +
										 ImGui::GetStyle().ItemInnerSpacing.x };
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, label_width);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("Channel:");
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::InputText("##SequenceChannel", &sequence->channel->value);
				DrawTooltip(
					"Only one binding owns a channel at a time. Restart replaces it; Queue waits."
				);
				ImGui::EndTable();
			}

			if (sequence) {
				changed |= DrawEvents(context, *sequence);
				changed |= DrawActions(context, *sequence, binding, resident_index);
			}
		}
	}

	ImGui::PopID();
	ImGui::PopID();
	return remove;
}

bool DrawAddRootScriptPopup(ScriptEditorContext& context, ::ptgn::impl::Scripts& scripts) {
	if (!ImGui::BeginPopup("AddScript")) {
		return false;
	}

	bool changed{ false };
	const auto add_registered = [&](const ScriptRegistration& registration) {
		const auto* editor{ ScriptEditorRegistry::Find(registration.type_hash) };
		if (!editor || !HasScriptType(editor->options.type, ScriptType::Resident) ||
			editor->options.hidden) {
			return;
		}
		if (ImGui::MenuItem(editor->options.label.c_str())) {
			AddEditorScriptEntry(context, scripts, MakeRootEntry(registration.type_hash));
			changed = true;
		}
		DrawTooltip(editor->options.description.c_str());
	};

	const auto* sequence_registration{ ScriptRegistry::Find(Hash<Script>()) };
	if (sequence_registration) {
		add_registered(*sequence_registration);
		ImGui::Separator();
	}

	if (!context.shared_sequences.sequences.empty() && ImGui::BeginMenu("Global")) {
		for (const auto& shared : context.shared_sequences.sequences) {
			if (ImGui::MenuItem(shared.name.c_str())) {
				Script script;
				script.sequence.name			   = shared.name;
				script.sequence.shared_reference   = true;
				script.sequence.shared_sequence_id = shared.id;
				AddEditorScriptEntry(context, scripts, MakeRootEntry(std::move(script)));
				changed = true;
			}
			DrawTooltip("Add a reference to this global editor authored script.");
		}
		ImGui::EndMenu();
	}

	std::vector<std::string> groups;
	for (const auto& registration : ScriptRegistry::Entries()) {
		if (registration.type_hash == Hash<Script>()) {
			continue;
		}
		const auto* editor{ ScriptEditorRegistry::Find(registration.type_hash) };
		if (!editor || !HasScriptType(editor->options.type, ScriptType::Resident) ||
			editor->options.hidden) {
			continue;
		}
		std::string group{ editor->options.group.empty() ? "Other" : editor->options.group };
		if (!std::ranges::contains(groups, group)) {
			groups.push_back(group);
		}
	}

	for (const auto& group : groups) {
		if (!ImGui::BeginMenu(group.c_str())) {
			continue;
		}
		for (const auto& registration : ScriptRegistry::Entries()) {
			if (registration.type_hash == Hash<Script>()) {
				continue;
			}
			const auto* editor{ ScriptEditorRegistry::Find(registration.type_hash) };
			if (!editor || !HasScriptType(editor->options.type, ScriptType::Resident) ||
				editor->options.hidden) {
				continue;
			}
			std::string_view candidate_group{
				editor->options.group.empty() ? std::string_view{ "Other" }
											  : std::string_view{ editor->options.group }
			};
			if (candidate_group == group) {
				add_registered(registration);
			}
		}
		ImGui::EndMenu();
	}
	ImGui::EndPopup();
	return changed;
}


bool DrawResidentScripts(ScriptEditorContext& context, ::ptgn::impl::Scripts& scripts) {
	bool changed{ false };
	int remove{ -1 };

	std::vector<int> display_order;
	display_order.reserve(scripts.scripts.size());

	for (int i{ 0 }; i < static_cast<int>(scripts.scripts.size()); ++i) {
		if (scripts.scripts[static_cast<std::size_t>(i)].type_hash != Hash<Script>()) {
			display_order.push_back(i);
		}
	}

	for (int i{ 0 }; i < static_cast<int>(scripts.scripts.size()); ++i) {
		if (scripts.scripts[static_cast<std::size_t>(i)].type_hash == Hash<Script>()) {
			display_order.push_back(i);
		}
	}

	for (int i : display_order) {
		auto& script{ scripts.scripts[static_cast<std::size_t>(i)] };
		const auto* registration{ ScriptRegistry::Find(script.type_hash) };
		const auto* registered_editor{ ScriptEditorRegistry::Find(script.type_hash) };
		const auto* editor{
			registered_editor &&
					HasScriptType(registered_editor->options.type, ScriptType::Resident)
				? registered_editor
				: nullptr
		};

		if (script.type_hash == Hash<Script>()) {
			ScriptSequence* editable_sequence{ std::addressof(script.sequence) };

			if (context.owner) {
				if (!script.instance && registration) {
					script_runtime::AttachEntry(context.owner, script);
				}

				auto* sequence_script{ script.instance.get() };

				if (!sequence_script) {
					continue;
				}

				editable_sequence = std::addressof(sequence_script->sequence);
			}

			editable_sequence->enabled = script.enabled;

			if (DrawSequence(context, *editable_sequence, changed, i)) {
				remove = i;
			}

			script.enabled = editable_sequence->enabled;

			if (context.owner) {
				SequenceId sequence_id{ editable_sequence->id };

				script.sequence			= *editable_sequence;
				script.sequence.id		= sequence_id;
				script.sequence.runtime = ScriptSequenceRuntime{};
			}

			continue;
		}

		ImGui::PushID(i);
		bool open{ false };
		float button_size{ ImGui::GetFrameHeight() };

		if (ImGui::BeginTable(
				"ScriptRow", 1,
				ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
			)) {
			ImGui::TableSetupColumn("Script", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);
			ImGui::TableSetColumnIndex(0);

			ImVec2 header_min{ ImGui::GetCursorScreenPos() };
			ImVec2 header_max{
				header_min.x + std::max(1.0f, ImGui::GetContentRegionAvail().x),
				header_min.y + button_size,
			};
			ImVec4 header{ script.enabled ? ImVec4{ 0.20f, 0.34f, 0.33f, 1.0f }
												: ImVec4{ 0.25f, 0.25f, 0.25f, 1.0f } };
			ImVec4 header_hovered{ script.enabled ? ImVec4{ 0.26f, 0.43f, 0.41f, 1.0f }
														: ImVec4{ 0.30f, 0.30f, 0.30f, 1.0f } };
			ImVec4 header_active{ script.enabled ? ImVec4{ 0.31f, 0.49f, 0.47f, 1.0f }
													   : ImVec4{ 0.34f, 0.34f, 0.34f, 1.0f } };
			bool header_hovered_before_draw{
				ImGui::IsWindowHovered() &&
				ImGui::IsMouseHoveringRect(header_min, header_max)
			};
			bool header_active_before_draw{ header_hovered_before_draw &&
												  ImGui::IsMouseDown(ImGuiMouseButton_Left) };
			ImVec4 header_color{
				header_active_before_draw ? header_active
										  : (header_hovered_before_draw ? header_hovered : header)
			};
			ImGui::GetWindowDrawList()->AddRectFilled(
				header_min, header_max, ImGui::GetColorU32(header_color),
				ImGui::GetStyle().FrameRounding
			);

			ImVec4 transparent{};
			ImGui::PushStyleColor(ImGuiCol_Header, transparent);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, transparent);
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, transparent);

			bool has_contents{ editor && editor->has_contents };
			ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_FramePadding |
									  ImGuiTreeNodeFlags_SpanAvailWidth |
									  ImGuiTreeNodeFlags_NoTreePushOnOpen };

			if (has_contents) {
				flags |= ImGuiTreeNodeFlags_DefaultOpen;
			} else {
				flags |= ImGuiTreeNodeFlags_Leaf;
			}

			open = ImGui::TreeNodeEx(
				"##Script", flags, "%s", editor ? editor->options.label.c_str() : "Missing Script"
			);
			ImGui::PopStyleColor(3);

			if (editor) {
				DrawTooltip(editor->options.description.c_str());
			}

			if (ImGui::BeginPopupContextItem("ScriptContext")) {
				if (DrawEnableDisableMenuItem(script.enabled)) {
					changed = true;
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Delete")) {
					remove = i;
				}

				ImGui::EndPopup();
			}

			ImGui::EndTable();
		}

		if (open && editor && editor->has_contents && editor->draw) {
			if (editor->draw(context, script.value)) {
				changed				   = true;
				script.runtime_factory = {};

				if (context.owner) {
					if (!script.instance) {
						script_runtime::AttachEntry(context.owner, script);
					} else if (registration && registration->apply) {
						registration->apply(*script.instance, script.value);
					}
				}
			}
		}

		ImGui::PopID();
	}

	if (remove >= 0) {
		if (context.owner && IsRuntimeActive(context)) {
			auto& entry{ scripts.scripts[static_cast<std::size_t>(remove)] };
			SequenceId id{ entry.instance ? entry.instance->sequence.id : entry.sequence.id };
			scripts.RemoveDeferred(id);
		} else {
			scripts.scripts.erase(scripts.scripts.begin() + remove);
		}

		changed = true;
	}

	return changed;
}

} // namespace

bool DrawScriptsComponent(EditorContext& ctx, ::ptgn::impl::Scripts& scripts) {
	auto entity{ ::ptgn::impl::ScriptsAccessor::GetOwner(scripts) };

	// Prefab component editing has no live Scene or Entity. Supply an
	// empty registry so the ordinary data editors can still be reused.
	SharedScriptSequenceRegistry prefab_shared_sequences;

	auto& shared_sequences{ entity ? entity.GetScene().ctx().shared_script_sequences
								   : prefab_shared_sequences };

	ScriptEditorContext context{ ctx, entity, shared_sequences };

	bool changed{ false };

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.20f, 0.34f, 0.33f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.26f, 0.43f, 0.41f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.31f, 0.49f, 0.47f, 1.0f });

	if (ImGui::Button("+ Script", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddScript");
	}

	ImGui::PopStyleColor(3);

	DrawTooltip("Add a custom script or an editor authored sequence script.");

	changed |= DrawAddRootScriptPopup(context, scripts);
	changed |= DrawResidentScripts(context, scripts);

	return changed;
}

} // namespace ptgn::editor::inspector
