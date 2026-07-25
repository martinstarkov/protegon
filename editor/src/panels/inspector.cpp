#include "panels/inspector.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <span>
#include <unordered_map>
#include <cfloat>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>
#ifndef MAGIC_ENUM_RANGE_MAX
#define MAGIC_ENUM_RANGE_MAX 512
#endif
#include <magic_enum/magic_enum.hpp>
#include <string>
#include <string_view>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "runtime/graphics/render_target.h"
#include "core/math/transform.h"
#include "panels/component_editor_registry.h"
#include "panels/inspector_fields.h"
#include "scripting/script_editor_registry.h"
#include "panels/scene_hierarchy.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/animation/offsets.h"
#include "runtime/ecs/component_registration.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/gaussian_blur.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/graphics.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "core/util/hash.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/script.h"

namespace ptgn::editor::inspector {

namespace {

void DrawTooltip(const char* text) {
	if (text && *text && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", text);
	}
}

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

struct ScriptInspectorState {
	std::optional<SequenceId> editing_sequence_name;
	std::string editing_sequence_original_name;
	std::unordered_map<SequenceId, bool> sequence_open_states;
};

ScriptInspectorState& GetScriptInspectorState() {
	static ScriptInspectorState state;
	return state;
}

inline constexpr std::array kLifecycleLabels{
	"On Start", "On Complete", "On Reset", "On Stop", "On Pause",
	"On Resume", "On Action Start", "On Action Complete", "On Action Cancel",
	"On Repeat", "On Yoyo"
};
inline constexpr std::array kActionFormLabels{ "Action", "Tween", "Delay" };
inline constexpr std::array kEaseEntries{
	std::pair{ Ease::Linear, "Linear" },
	std::pair{ Ease::InQuad, "In Quad" },
	std::pair{ Ease::OutQuad, "Out Quad" },
	std::pair{ Ease::InOutQuad, "In-Out Quad" },
	std::pair{ Ease::OutCubic, "Out Cubic" },
	std::pair{ Ease::OutBack, "Out Back" },
};

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
	bool changed{ false };

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
			const float updated{ static_cast<float>(value * multiplier) };
			changed = updated != milliseconds;
			milliseconds = updated;
		}
		format(milliseconds, state.buffer.data(), state.buffer.size());
	}

	state.was_active = active;
	DrawTooltip(tooltip);
	return changed;
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
	bool& enabled, const char* enabled_tooltip, const char* delete_tooltip,
	bool& enabled_changed
) {
	const float size{ ImGui::GetFrameHeight() };
	enabled_changed |= ImGui::Checkbox("##Enabled", &enabled);
	DrawTooltip(enabled_tooltip);
	SameLineControl();
	const bool remove{ ImGui::Button("x", ImVec2{ size, size }) };
	DrawTooltip(delete_tooltip);
	return remove;
}

bool DrawCenteredTextButton(const char* id, const char* text, ImVec2 size) {
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

bool DrawToggleButton(const char* label, bool& value, ImVec2 size, const char* tooltip) {
	const bool dimmed{ !value };
	if (dimmed) {
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
	}
	const bool pressed{ ImGui::Button(label, size) };
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
	const float button_width{ ImGui::GetFrameHeight() };
	const float spacing{ CompactControlSpacing() };
	const std::string widest{ std::string{ label } + ": 100" };
	return ImGui::CalcTextSize(widest.c_str()).x +
		button_width * 2.0f + spacing * 2.0f;
}

bool DrawCountControl(
	const char* label, int& value, int minimum, int maximum = 100,
	bool disabled = false, const char* tooltip = nullptr
) {
	const int previous{ value };
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
			ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, button_size);
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
		DrawTooltip(empty ? "Add an item to use this section." : tooltip);

		if (show_add_button) {
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Button("+", ImVec2{ button_size, button_size })) {
				add_requested = true;
				open = true;
				force_open_next_frame[tree_id] = true;
			}
			DrawTooltip(add_tooltip);
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

void EnsureActionValue(ScriptStep& action) {
	if (!action.value.is_null()) {
		return;
	}
	if (const auto* registration{ ScriptRegistry::Find(action.type_hash) };
		registration && registration->make_default) {
		action.value = registration->make_default();
	}
	if (action.value.is_null()) {
		action.value = json::object();
	}
}

ActionForm GetActionForm(const ScriptStep& action) {
	if (action.type_hash == Hash<WaitScript>()) {
		return ActionForm::Delay;
	}
	return action.timing ? ActionForm::Tween : ActionForm::Action;
}

void SetActionForm(ScriptStep& action, ActionForm form) {
	const bool enabled{ action.enabled };
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
			registration = ScriptRegistry::Find(action.type_hash);
			action.completion = ScriptCompletion::Duration;
			action.timing = registration && registration->default_timing
				? registration->default_timing
				: std::optional<ScriptTiming>{ ScriptTiming{} };
			break;
		case ActionForm::Delay:
			action = ScriptRegistry::MakeStep<WaitScript>();
			action.completion = ScriptCompletion::Duration;
			break;
	}

	action.enabled = enabled;
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
	entry.enabled = true;
	entry.type_hash = type_hash;
	entry.value = registration->make_default ? registration->make_default() : json::object();
	entry.sequence = registration->make_default_sequence
		? registration->make_default_sequence()
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
	entry.enabled = true;
	entry.type_hash = registration->type_hash;
	entry.value = std::move(value);
	entry.sequence = script.sequence;
	return entry;
}

void PromoteToShared(ScriptEditorContext& context, ScriptSequence& binding) {
	if (binding.shared_reference) {
		return;
	}
	ScriptSequence shared{ binding };
	shared.shared_reference = false;
	shared.shared_sequence_id = 0;
	shared.runtime = ScriptSequenceRuntime{};
	const SequenceId shared_id{ shared.id };
	context.shared_sequences.sequences.push_back(std::move(shared));
	binding.shared_reference = true;
	binding.shared_sequence_id = shared_id;
	binding.runtime = ScriptSequenceRuntime{};
}

void DetachToLocal(ScriptEditorContext& context, ScriptSequence& binding) {
	if (!binding.shared_reference) {
		return;
	}
	const auto* shared{ context.shared_sequences.Find(binding.shared_sequence_id) };
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

bool DrawActionPicker(
	ScriptEditorContext& context, ScriptStep& action, bool timed_only,
	float width = -FLT_MIN
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

	const auto resolve_candidate = [](const ScriptRegistration& registration)
		-> std::optional<Candidate> {
		const auto* editor{ ScriptEditorRegistry::Find(registration.type_hash) };
		if (!editor || !HasScriptType(editor->options.type, ScriptType::Sequence)) {
			return std::nullopt;
		}
		return Candidate{
			.runtime = &registration,
			.editor = editor,
			.label = editor->options.label,
			.group = editor->options.group,
			.description = editor->options.description,
			.menu_order = editor->options.menu_order,
			.separator_after = editor->options.separator_after,
		};
	};

	const auto* current_registration{ ScriptRegistry::Find(action.type_hash) };
	const std::optional<Candidate> current{
		current_registration ? resolve_candidate(*current_registration) : std::nullopt
	};
	ImGui::SetNextItemWidth(width);
	const bool open{ ImGui::BeginCombo(
		"##RegisteredAction", current ? current->label.data() : "Missing Action"
	) };
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
		return registration.serializable &&
			registration.type_hash != Hash<Script>() &&
			registration.type_hash != Hash<WaitScript>() &&
			!candidate.editor->options.hidden &&
			(!timed_only || registration.supports_timing) &&
			(timed_only || !registration.requires_timing);
	};
	const auto select_candidate = [&](const Candidate& candidate) {
		const auto& registration{ *candidate.runtime };
		if (ImGui::MenuItem(
				candidate.label.data(), nullptr, registration.type_hash == action.type_hash
			)) {
			const bool enabled{ action.enabled };
			action = ScriptRegistry::MakeStep(registration.type_hash);
			action.enabled = enabled;
			if (timed_only) {
				action.completion = ScriptCompletion::Duration;
				action.timing = registration.default_timing.value_or(ScriptTiming{});
			} else {
				action.completion.reset();
				action.timing.reset();
			}
			changed = true;
		}
		if (ImGui::IsItemHovered()) {
			const std::string type_hash{ std::to_string(registration.type_hash) };
			ImGui::SetTooltip(
				"%s\nType hash: %s", candidate.description.data(), type_hash.c_str()
			);
		}
	};

	std::vector<Candidate> candidates;
	for (const auto& registration : ScriptRegistry::Entries()) {
		if (auto candidate{ resolve_candidate(registration) };
			candidate && is_available(*candidate)) {
			candidates.push_back(*candidate);
		}
	}

	const auto emit_signal_it{ std::ranges::find_if(
		candidates, [](const Candidate& candidate) {
			return candidate.runtime->type_hash == Hash<EmitSignalScript>();
		}
	) };
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
				const bool enabled{ action.enabled };
				Script script;
				script.sequence.name = shared.name;
				script.sequence.shared_reference = true;
				script.sequence.shared_sequence_id = shared.id;
				action = ScriptRegistry::MakeStep(std::move(script));
				action.enabled = enabled;
				action.completion = ScriptCompletion::ScriptControlled;
				action.timing.reset();
				changed = true;
			}
			DrawTooltip("Run this global editor-authored Script as the sequence step.");
		}
		ImGui::EndMenu();
	}

	for (const auto& candidate : candidates) {
		if (candidate.group.empty() &&
			candidate.runtime->type_hash != Hash<EmitSignalScript>()) {
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
	ScriptEditorContext& context, ScriptStep& action, bool timed_only
) {
	EnsureActionValue(action);
	const auto* editor{ ScriptEditorRegistry::Find(action.type_hash) };
	const bool has_inline_editor{
		!timed_only && editor &&
		HasScriptType(editor->options.type, ScriptType::Sequence) &&
		static_cast<bool>(editor->draw_inline)
	};
	if (!has_inline_editor) {
		return DrawActionPicker(context, action, timed_only);
	}

	bool changed{ false };
	const float available{ ImGui::GetContentRegionAvail().x };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float picker_width{ std::min(150.0f, std::max(110.0f, available * 0.32f)) };
	changed |= DrawActionPicker(context, action, timed_only, picker_width);

	editor = ScriptEditorRegistry::Find(action.type_hash);
	if (editor && editor->draw_inline) {
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (editor->draw_inline(action.value, context)) {
			action.runtime_factory = {};
			changed = true;
		}
	}
	return changed;
}
bool DrawTimingOptions(
	ScriptEditorContext& context, ScriptStep& action, ScriptTiming& timing,
	float left_screen_x
) {
	(void)context;
	EnsureActionValue(action);
	bool changed{ false };

	const float right_screen_x{
		ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x
	};
	const float width{ std::max(1.0f, right_screen_x - left_screen_x) };
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

	const int columns{ move || scale ? 5 : (rotate ? 4 : 2) };
	if (!ImGui::BeginTable(
			"TweenOptions", columns, ImGuiTableFlags_SizingStretchProp,
			ImVec2{ width, 0.0f }
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
			"##TweenX", &move->destination.x, 1.0f,
			-100000.0f, 100000.0f, "X: %.0f"
		);
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= ImGui::DragFloat(
			"##TweenY", &move->destination.y, 1.0f,
			-100000.0f, 100000.0f, "Y: %.0f"
		);
		ImGui::TableSetColumnIndex(column++);
		if (ImGui::Button(
				move->relative ? "Relative" : "Absolute",
				ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			move->relative = !move->relative;
			changed = true;
		}
		DrawTooltip(
			move->relative ? "Offset from the entity's current position."
							 : "Use an absolute world position."
		);
	} else if (scale) {
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= ImGui::DragFloat(
			"##TweenScaleX", &scale->scale.x, 0.01f,
			-100.0f, 100.0f, "X: %.2f"
		);
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= ImGui::DragFloat(
			"##TweenScaleY", &scale->scale.y, 0.01f,
			-100.0f, 100.0f, "Y: %.2f"
		);
		ImGui::TableSetColumnIndex(column++);
		if (ImGui::Button(
				scale->relative ? "Relative" : "Absolute",
				ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			scale->relative = !scale->relative;
			changed = true;
		}
		DrawTooltip(
			scale->relative ? "Multiply the entity's current scale."
							  : "Use an absolute scale."
		);
	} else if (rotate) {
		ImGui::TableSetColumnIndex(column++);
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= ImGui::DragFloat(
			"##TweenDegrees", &rotate->degrees, 1.0f,
			-3600.0f, 3600.0f, "%.1f deg"
		);
		ImGui::TableSetColumnIndex(column++);
		changed |= DrawToggleButton(
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
				changed = true;
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
		changed |= ImGui::Checkbox("Infinite", &timing.infinite_repeats);
		DrawTooltip(
			"Repeat this Tween indefinitely. Duration still controls every cycle."
		);
		changed |= ImGui::Checkbox("Reversed", &timing.reversed);
		changed |= ImGui::Checkbox("Yoyo", &timing.yoyo);
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
	return changed;
}

bool DrawActionParameters(
	ScriptEditorContext& context, ScriptStep& action, float left_screen_x
) {
	EnsureActionValue(action);
	const auto* editor{ ScriptEditorRegistry::Find(action.type_hash) };
	if (!editor ||
		!HasScriptType(editor->options.type, ScriptType::Sequence) ||
		!editor->draw ||
		action.type_hash == Hash<Script>() ||
		action.type_hash == Hash<WaitScript>() ||
		action.type_hash == Hash<EmitSignalScript>() ||
		action.type_hash == Hash<SetVisibleScript>() ||
		action.type_hash == Hash<MoveToScript>() ||
		action.type_hash == Hash<RotateToScript>() ||
		action.type_hash == Hash<ScaleToScript>() ||
		action.type_hash == Hash<RemoveComponentsScript>()) {
		return false;
	}

	if (action.type_hash == Hash<AddComponentsScript>()) {
		AddComponentsScript add_components;
		if (!TryReadScriptJson(action.value, add_components) ||
			add_components.components.empty()) {
			return false;
		}
	}

	const float right_screen_x{
		ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x
	};
	ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });
	bool changed{ false };
	if (ImGui::BeginChild(
			"ActionParameters",
			ImVec2{ std::max(1.0f, right_screen_x - left_screen_x), 0.0f },
			ImGuiChildFlags_AutoResizeY
		)) {
		changed = editor->draw(action.value, context);
		if (changed) {
			action.runtime_factory = {};
		}
	}
	ImGui::EndChild();
	return changed;
}

bool DrawActions(
	ScriptEditorContext& context, ScriptSequence& sequence, ScriptSequence& binding
) {
	bool changed{ false };
	int remove_index{ -1 };
	int duplicate_index{ -1 };
	int move_from{ -1 };
	int move_to{ -1 };

	for (int index{ 0 }; index < static_cast<int>(sequence.steps.size()); ++index) {
		auto& action{ sequence.steps[static_cast<std::size_t>(index)] };
		bool remove{ false };
		bool duplicate{ false };
		ImGui::PushID(&action);

		constexpr float drag_width{ 28.0f };
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
			ImGui::CalcTextSize("5000ms").x + ImGui::GetStyle().FramePadding.x * 2.0f
		};
		const float repeats_width{ GetCountControlWidth("Repeats") };
		const float button_width{ ImGui::GetFrameHeight() };
		ActionForm displayed_form{ GetActionForm(action) };
		const float controls_width{
			EnabledDeleteControlsWidth() +
			(displayed_form == ActionForm::Tween
				? CompactControlSpacing() + repeats_width
				: 0.0f)
		};
		ActionForm requested_form{ displayed_form };
		bool form_changed{ false };
		float parameter_left_screen_x{ ImGui::GetCursorScreenPos().x };
		const int column_count{ displayed_form == ActionForm::Tween ? 5 : 4 };

		if (ImGui::BeginTable("ActionRow", column_count, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Drag", ImGuiTableColumnFlags_WidthFixed, drag_width);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, type_width);
			if (displayed_form == ActionForm::Tween) {
				ImGui::TableSetupColumn(
					"Duration", ImGuiTableColumnFlags_WidthFixed, duration_width
				);
				ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
			} else {
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			}
			ImGui::TableSetupColumn(
				"Controls", ImGuiTableColumnFlags_WidthFixed, controls_width
			);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, button_width);

			int column{};
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
			DrawTooltip(
				action.enabled
					? "Drag to reorder. Right-click to duplicate."
					: "Disabled action. Right-click to duplicate."
			);
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				const ActionDragPayload payload{ index };
				ImGui::SetDragDropPayload("PTGN_SCRIPT_ACTION", &payload, sizeof(payload));
				ImGui::Text("%d. %s", index + 1, ActionSummary(action).c_str());
				ImGui::EndDragDropSource();
			}
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload{
						ImGui::AcceptDragDropPayload("PTGN_SCRIPT_ACTION")
					}) {
					const auto* drag{ static_cast<const ActionDragPayload*>(payload->Data) };
					if (drag) {
						move_from = drag->index;
						move_to = index;
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
					const auto candidate{ static_cast<ActionForm>(i) };
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
			DrawTooltip("Choose an Action, Tween, or Delay.");

			if (displayed_form == ActionForm::Tween) {
				ImGui::TableSetColumnIndex(column++);
				changed |= DrawDurationInput(
					"##Duration", action.timing->duration_ms, -FLT_MIN,
					"Duration of each Tween cycle."
				);
				ImGui::TableSetColumnIndex(column++);
				changed |= DrawActionPicker(context, action, true);
			} else {
				ImGui::TableSetColumnIndex(column++);
				switch (displayed_form) {
					case ActionForm::Action:
						changed |= DrawActionPickerWithInline(context, action, false);
						break;
					case ActionForm::Delay:
						changed |= DrawDurationInput(
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
				changed |= DrawCountControl(
					"Repeats", action.timing->additional_repeats, 0, 100,
					action.timing->infinite_repeats,
					"Additional full-duration cycles."
				);
				SameLineControl();
			}
			bool enabled_changed{ false };
			remove = DrawEnabledDeleteControls(
				action.enabled,
				"Enable or disable this action.",
				"Delete this action.",
				enabled_changed
			);
			changed |= enabled_changed;
			ImGui::EndTable();
		}

		if (form_changed) {
			SetActionForm(action, requested_form);
			displayed_form = requested_form;
			changed = true;
		}
		if (displayed_form == ActionForm::Tween && action.timing) {
			changed |= DrawTimingOptions(
				context, action, *action.timing, parameter_left_screen_x
			);
		}
		if (displayed_form == ActionForm::Action || displayed_form == ActionForm::Tween) {
			changed |= DrawActionParameters(context, action, parameter_left_screen_x);
		}
		if (binding.runtime.running &&
			binding.runtime.step_index == static_cast<std::size_t>(index)) {
			const float duration_ms{ action.timing ? action.timing->duration_ms : 0.0f };
			const float progress{
				duration_ms > 0.0f
					? std::clamp(binding.runtime.elapsed_ms / duration_ms, 0.0f, 1.0f)
					: 0.0f
			};
			ImGui::ProgressBar(progress, ImVec2{ -FLT_MIN, 2.0f }, "");
		}
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
		changed = true;
	}
	if (duplicate_index >= 0) {
		ScriptStep copy{ sequence.steps[static_cast<std::size_t>(duplicate_index)] };
		sequence.steps.insert(
			sequence.steps.begin() + duplicate_index + 1, std::move(copy)
		);
		binding.runtime = ScriptSequenceRuntime{};
		changed = true;
	}
	if (remove_index >= 0) {
		sequence.steps.erase(sequence.steps.begin() + remove_index);
		binding.runtime = ScriptSequenceRuntime{};
		changed = true;
	}
	return changed;
}
bool DrawLifecycleRows(ScriptEditorContext& context, ScriptSequence& sequence) {
	bool changed{ false };
	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(sequence.lifecycle_actions.size()); ++i) {
		auto& callback{ sequence.lifecycle_actions[static_cast<std::size_t>(i)] };
		ImGui::PushID(&callback);

		const float lifecycle_width{ 145.0f };
		if (ImGui::BeginTable("LifecycleRow", 3, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn(
				"Lifecycle", ImGuiTableColumnFlags_WidthFixed, lifecycle_width
			);
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(
				"Controls", ImGuiTableColumnFlags_WidthFixed,
				EnabledDeleteControlsWidth()
			);
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
				changed = true;
			}
			DrawTooltip("Choose when this callback runs.");
			ImGui::EndDisabled();

			ImGui::TableSetColumnIndex(1);
			ImGui::BeginDisabled(!callback.enabled);
			changed |= DrawActionPickerWithInline(context, callback.action, false);
			ImGui::EndDisabled();

			ImGui::TableSetColumnIndex(2);
			bool enabled_changed{ false };
			if (DrawEnabledDeleteControls(
					callback.enabled,
					"Enable or disable this lifecycle callback.",
					"Remove this lifecycle callback.",
					enabled_changed
				)) {
				remove = i;
			}
			changed |= enabled_changed;
			ImGui::EndTable();
		}

		ImGui::BeginDisabled(!callback.enabled);
		changed |= DrawActionParameters(
			context, callback.action, ImGui::GetCursorScreenPos().x
		);
		ImGui::EndDisabled();
		ImGui::PopID();
	}
	if (remove >= 0) {
		sequence.lifecycle_actions.erase(sequence.lifecycle_actions.begin() + remove);
		changed = true;
	}
	return changed;
}

bool DrawEvent(
	ScriptEditorContext& context, EventCondition& event,
	bool stop_event, bool& switch_kind, bool& changed
) {
	bool remove{ false };
	ImGui::PushID(&event);

	const float kind_width{
		std::max(ImGui::CalcTextSize("Start").x, ImGui::CalcTextSize("Stop").x) +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const float consume_width{
		ImGui::CalcTextSize("Consume").x + ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const EventEditorRegistration* selected{ EventEditorRegistry::Find(event.type_hash) };
	bool event_type_changed{ false };

	auto inline_field_count = [](const EventEditorRegistration* registration) {
		return registration ? registration->options.inline_fields : 0;
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
		DrawTooltip(
			stop_event ? "Change this to a start trigger."
						 : "Change this to a stop trigger."
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
			if (available_width - event_width - spacing * static_cast<float>(initial_inline_fields) <
				minimum_fields_width) {
				event_width = std::max(
					90.0f,
					available_width - spacing * static_cast<float>(initial_inline_fields) - minimum_fields_width
				);
			}
		}

		ImGui::SetNextItemWidth(std::max(1.0f, event_width));
		if (ImGui::BeginCombo(
				"##Event", selected ? selected->options.label.c_str() : "Missing Event"
			)) {
			auto candidate_available = [&](const EventEditorRegistration& candidate) {
				const auto* registration{ SequenceEventRegistry::Find(candidate.type_hash) };
				return registration &&
					(!registration->available || registration->available(context.owner));
			};
			auto select_candidate = [&](const EventEditorRegistration& candidate) {
				const auto* registration{ SequenceEventRegistry::Find(candidate.type_hash) };
				if (!candidate_available(candidate)) {
					return;
				}
				if (ImGui::MenuItem(
						candidate.options.label.c_str(), nullptr,
						candidate.type_hash == event.type_hash
					)) {
					event.type_hash = candidate.type_hash;
					registration->set_defaults(event);
					selected = EventEditorRegistry::Find(event.type_hash);
					event_type_changed = true;
					changed = true;
				}
				DrawTooltip(candidate.options.description.c_str());
			};

			for (const auto& candidate : EventEditorRegistry::Entries()) {
				if (candidate.options.group.empty()) {
					select_candidate(candidate);
				}
			}
			std::vector<std::string> groups;
			for (const auto& candidate : EventEditorRegistry::Entries()) {
				if (!candidate.options.group.empty() && candidate_available(candidate) &&
					!std::ranges::contains(groups, candidate.options.group)) {
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

		if (selected && !event_type_changed && selected->options.inline_fields > 0 && selected->options.draw) {
			ImGui::SameLine();
			changed |= selected->options.draw(event.value);
		}
		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(column++);
		ImGui::BeginDisabled(!event.enabled);
		changed |= DrawToggleButton(
			"Consume", event.consume,
			ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() },
			"Stop propagation after this event matches."
		);
		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(column);
		bool enabled_changed{ false };
		remove = DrawEnabledDeleteControls(
			event.enabled,
			"Enable or disable this trigger.",
			"Remove this trigger.",
			enabled_changed
		);
		changed |= enabled_changed;
		ImGui::EndTable();
	}

	ImGui::PopID();
	return remove;
}

bool DrawEvents(ScriptEditorContext& context, ScriptSequence& sequence) {
	bool changed{ false };
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
				const auto* registration{ SequenceEventRegistry::Find(candidate.type_hash) };
				return registration &&
					(!registration->available || registration->available(context.owner));
			};
			auto add_candidate = [&](const EventEditorRegistration& candidate) {
				const auto* registration{ SequenceEventRegistry::Find(candidate.type_hash) };
				if (!candidate_available(candidate)) {
					return;
				}
				if (ImGui::MenuItem(candidate.options.label.c_str())) {
					EventCondition trigger{
						.enabled = true,
						.type_hash = registration->type_hash,
					};
					registration->set_defaults(trigger);
					sequence.start_events.push_back(std::move(trigger));
					changed = true;
				}
				DrawTooltip(candidate.options.description.c_str());
			};

			for (const auto& candidate : EventEditorRegistry::Entries()) {
				if (candidate.options.group.empty()) {
					add_candidate(candidate);
				}
			}
			std::vector<std::string> groups;
			for (const auto& candidate : EventEditorRegistry::Entries()) {
				if (!candidate.options.group.empty() && candidate_available(candidate) &&
					!std::ranges::contains(groups, candidate.options.group)) {
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
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Lifecycle callback")) {
			for (int i{ 0 }; i < static_cast<int>(kLifecycleLabels.size()); ++i) {
				if (ImGui::MenuItem(kLifecycleLabels[static_cast<std::size_t>(i)])) {
					sequence.lifecycle_actions.push_back(LifecycleScript{
						.enabled = true,
						.lifecycle = static_cast<SequenceLifecycle>(i),
						.action = ScriptRegistry::MakeStep<EmitSignalScript>(),
					});
					changed = true;
				}
			}
			ImGui::EndMenu();
		}
		DrawTooltip("Add a lifecycle callback.");
		ImGui::EndPopup();
	}
	ImGui::PopID();

	if (!open) {
		return changed;
	}

	int remove_start{ -1 };
	int move_start_to_stop{ -1 };
	for (int i{ 0 }; i < static_cast<int>(sequence.start_events.size()); ++i) {
		bool switch_kind{ false };
		if (DrawEvent(
				context, sequence.start_events[static_cast<std::size_t>(i)],
				false, switch_kind, changed
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
				context, sequence.stop_events[static_cast<std::size_t>(i)],
				true, switch_kind, changed
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
	ScriptEditorContext& context, ScriptSequence& binding, bool& changed
) {
	ScriptSequence* sequence{ script_runtime::Resolve(context.owner, binding) };
	if (!sequence) {
		ImGui::TextDisabled("Missing shared Script Sequence");
		return false;
	}

	bool remove{ false };
	auto& state{ GetScriptInspectorState() };
	ImGui::PushID(static_cast<int>(binding.id));
	const float button_size{ ImGui::GetFrameHeight() };
	const int column_count{ 6 };
	const float available_width{ ImGui::GetContentRegionAvail().x };
	const float sequence_width{
		std::max(1.0f, (available_width - CompactControlSpacing()) * 0.5f)
	};
	bool open{ state.sequence_open_states.try_emplace(binding.id, true).first->second };
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
		ImGui::TableSetupColumn("Options", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Play", ImGuiTableColumnFlags_WidthFixed, button_size);
		ImGui::TableSetupColumn("Pause", ImGuiTableColumnFlags_WidthFixed, button_size);
		ImGui::TableSetupColumn("Stop", ImGuiTableColumnFlags_WidthFixed, button_size);
		ImGui::TableSetupColumn(
			"Controls", ImGuiTableColumnFlags_WidthFixed,
			EnabledDeleteControlsWidth()
		);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);

		ImGui::TableSetColumnIndex(0);
		const ImVec2 header_min{ ImGui::GetCursorScreenPos() };
		const ImVec2 header_max{
			header_min.x + std::max(1.0f, ImGui::GetContentRegionAvail().x),
			header_min.y + button_size
		};
		const ImVec2 mouse{ ImGui::GetMousePos() };

		auto& stored_open{ state.sequence_open_states[binding.id] };
		ImGui::SetNextItemOpen(stored_open, ImGuiCond_Always);
		const bool editing_before_draw{ state.editing_sequence_name == binding.id };
		const ImVec4 header{
			binding.enabled ? ImVec4{ 0.35f, 0.24f, 0.39f, 1.0f }
							: ImVec4{ 0.25f, 0.25f, 0.25f, 1.0f }
		};
		const ImVec4 header_hovered{
			binding.enabled ? ImVec4{ 0.44f, 0.31f, 0.48f, 1.0f }
							: ImVec4{ 0.30f, 0.30f, 0.30f, 1.0f }
		};
		const ImVec4 header_active{
			binding.enabled ? ImVec4{ 0.50f, 0.36f, 0.55f, 1.0f }
							: ImVec4{ 0.34f, 0.34f, 0.34f, 1.0f }
		};

		const bool header_hovered_before_draw{
			ImGui::IsMouseHoveringRect(header_min, header_max)
		};
		const bool header_active_before_draw{
			!editing_before_draw && header_hovered_before_draw &&
			ImGui::IsMouseDown(ImGuiMouseButton_Left)
		};
		const ImVec4 header_color{
			editing_before_draw
				? header
				: (header_active_before_draw
					? header_active
					: (header_hovered_before_draw ? header_hovered : header))
		};
		ImGui::GetWindowDrawList()->AddRectFilled(
			header_min, header_max, ImGui::GetColorU32(header_color),
			ImGui::GetStyle().FrameRounding
		);

		const ImVec4 transparent{};
		ImGui::PushStyleColor(ImGuiCol_Header, transparent);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, transparent);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, transparent);
		open = ImGui::TreeNodeEx(
			"##ScriptSequence",
			ImGuiTreeNodeFlags_OpenOnArrow |
				ImGuiTreeNodeFlags_FramePadding |
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_AllowOverlap,
			"%s", editing_before_draw ? "" : sequence->name.c_str()
		);
		ImGui::PopStyleColor(3);
		stored_open = open;

		// Use the measured table-cell rectangle for both display and edit modes. Framed TreeNodeEx
		// expands its native background beyond the cursor by a style-dependent amount, which causes
		// the one- or two-pixel width change when the InputText overlay becomes active.
		const ImVec2 tree_min{ header_min };
		const ImVec2 tree_max{ header_max };
		const bool tree_hovered{ ImGui::IsItemHovered() };
		const float text_start_x{ tree_min.x + ImGui::GetFrameHeight() };
		const float minimum_name_width{ 48.0f };
		const float visible_name_width{
			std::max(minimum_name_width, ImGui::CalcTextSize(sequence->name.c_str()).x)
		};
		const float name_hit_end_x{
			std::min(tree_max.x, text_start_x + visible_name_width)
		};
		const bool name_hit_hovered{
			tree_hovered && mouse.x >= text_start_x && mouse.x <= name_hit_end_x
		};
		const bool tree_clicked_left{ ImGui::IsItemClicked(ImGuiMouseButton_Left) };

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
			state.editing_sequence_name = binding.id;
			state.editing_sequence_original_name = sequence->name;
			began_name_edit_this_frame = true;
		}

		if (!editing_before_draw && !begin_edit &&
			tree_clicked_left && mouse.x > name_hit_end_x) {
			open = !open;
			stored_open = open;
		}

		if (state.editing_sequence_name == binding.id) {
			ImGui::SetCursorScreenPos(ImVec2{ text_start_x, tree_min.y });
			ImGui::SetNextItemWidth(std::max(
				minimum_name_width,
				tree_max.x - text_start_x - ImGui::GetStyle().FramePadding.x
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
			name_input_hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
			if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				sequence->name = state.editing_sequence_original_name;
				state.editing_sequence_name.reset();
			} else if (submitted) {
				state.editing_sequence_name.reset();
				changed = true;
			}
		}
		if (tree_hovered && state.editing_sequence_name != binding.id) {
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
			if (ImGui::MenuItem("Global sequence", nullptr, global)) {
				if (global) {
					DetachToLocal(context, binding);
				} else {
					PromoteToShared(context, binding);
				}
				sequence = script_runtime::Resolve(context.owner, binding);
				changed = true;
			}
			if (sequence && ImGui::MenuItem(
					"Remove binding on complete", nullptr,
					sequence->remove_binding_on_complete
				)) {
				sequence->remove_binding_on_complete =
					!sequence->remove_binding_on_complete;
				changed = true;
			}
			if (sequence && ImGui::MenuItem(
					"Destroy owner on complete", nullptr,
					sequence->destroy_owner_on_complete
				)) {
				sequence->destroy_owner_on_complete =
					!sequence->destroy_owner_on_complete;
				changed = true;
			}
			if (sequence) {
				const bool has_channel{ sequence->channel.has_value() };
				if (ImGui::MenuItem("Use sequence channel", nullptr, has_channel)) {
					if (has_channel) {
						sequence->channel.reset();
					} else {
						sequence->channel = "default";
					}
					changed = true;
				}
			}
			ImGui::Separator();
			if (sequence && ImGui::MenuItem(
					"Ignore on retrigger", nullptr,
					sequence->reentry == ReentryMode::IgnoreWhileRunning
				)) {
				sequence->reentry = ReentryMode::IgnoreWhileRunning;
				changed = true;
			}
			if (sequence && ImGui::MenuItem(
					"Restart on retrigger", nullptr,
					sequence->reentry == ReentryMode::Restart
				)) {
				sequence->reentry = ReentryMode::Restart;
				changed = true;
			}
			if (sequence && ImGui::MenuItem(
					"Queue on retrigger", nullptr,
					sequence->reentry == ReentryMode::Queue
				)) {
				sequence->reentry = ReentryMode::Queue;
				changed = true;
			}
			ImGui::EndCombo();
		}
		DrawSelectedItemsTooltip(selected_sequence_options);

		ImGui::TableSetColumnIndex(2);
		if (DrawCenteredTextButton(
				"##Play", ">", ImVec2{ button_size, button_size }
			)) {
			script_runtime::Start(context.owner, binding.id, true);
		}
		DrawTooltip(
			binding.runtime.running ? "Restart this sequence." : "Start this sequence."
		);

		ImGui::TableSetColumnIndex(3);
		ImGui::BeginDisabled(!binding.runtime.running);
		if (DrawCenteredTextButton(
				"##Pause", "||", ImVec2{ button_size, button_size }
			)) {
			script_runtime::SetPaused(
				context.owner, binding.id, !binding.runtime.paused
			);
		}
		ImGui::EndDisabled();
		DrawTooltip(
			binding.runtime.paused ? "Resume this sequence." : "Pause this sequence."
		);

		ImGui::TableSetColumnIndex(4);
		ImGui::BeginDisabled(!binding.runtime.running);
		if (DrawCenteredTextButton(
				"##Stop", "[]", ImVec2{ button_size, button_size }
			)) {
			script_runtime::Stop(context.owner, binding.id);
		}
		ImGui::EndDisabled();
		DrawTooltip("Stop this sequence.");

		ImGui::TableSetColumnIndex(5);
		bool enabled_changed{ false };
		remove = DrawEnabledDeleteControls(
			binding.enabled,
			"Enable or disable this script sequence.",
			"Delete this script sequence.",
			enabled_changed
		);
		changed |= enabled_changed;
		ImGui::EndTable();
	}

	if (state.editing_sequence_name == binding.id && name_input_drawn &&
		!began_name_edit_this_frame) {
		const bool clicked{
			ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
			ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
			ImGui::IsMouseClicked(ImGuiMouseButton_Right)
		};
		const ImVec2 mouse{ ImGui::GetMousePos() };
		const bool inside_input{
			mouse.x >= name_input_min.x && mouse.x <= name_input_max.x &&
			mouse.y >= name_input_min.y && mouse.y <= name_input_max.y
		};
		if (clicked && !inside_input && !name_input_hovered) {
			state.editing_sequence_name.reset();
			changed = true;
		}
	}

	if (open && !remove) {
		sequence = script_runtime::Resolve(context.owner, binding);
		if (sequence) {
			if (sequence->channel && ImGui::BeginTable(
					"SequenceChannelRow", 2, ImGuiTableFlags_SizingStretchProp
				)) {
				const float label_width{
					ImGui::CalcTextSize("Channel:").x +
					ImGui::GetStyle().ItemInnerSpacing.x
				};
				ImGui::TableSetupColumn(
					"Label", ImGuiTableColumnFlags_WidthFixed, label_width
				);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("Channel:");
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::InputText(
					"##SequenceChannel", &sequence->channel->value
				);
				DrawTooltip(
					"Only one binding owns a channel at a time. Restart replaces it; Queue waits."
				);
				ImGui::EndTable();
			}
			changed |= DrawEvents(context, *sequence);

			bool add_action_requested{ false };
			const bool sequence_open{ DrawUnframedSectionHeader(
				"SequenceSection", "Sequence", true, sequence->steps.empty(),
				"Ordered actions executed by this script sequence.",
				true, "Add an action to this sequence.", add_action_requested
			) };
			ImGui::PushID("SequenceSection");
			if (add_action_requested) {
				ImGui::OpenPopup("AddSequenceAction");
			}
			if (ImGui::BeginPopup("AddSequenceAction")) {
				if (ImGui::MenuItem("Emit Signal")) {
					sequence->steps.push_back(
						ScriptRegistry::MakeStep<EmitSignalScript>()
					);
					changed = true;
				}
				if (ImGui::MenuItem("Action")) {
					sequence->steps.push_back(
						ScriptRegistry::MakeStep<SetVisibleScript>()
					);
					changed = true;
				}
				if (ImGui::MenuItem("Tween")) {
					sequence->steps.push_back(
						ScriptRegistry::MakeStep<MoveToScript>()
					);
					changed = true;
				}
				if (ImGui::MenuItem("Delay")) {
					sequence->steps.push_back(
						ScriptRegistry::MakeStep<WaitScript>()
					);
					changed = true;
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();
			if (sequence_open) {
				changed |= DrawActions(context, *sequence, binding);
			}
		}
	}

	ImGui::PopID();
	return remove;
}

bool DrawAddRootScriptPopup(
	ScriptEditorContext& context, ::ptgn::impl::Scripts& scripts
) {
	if (!ImGui::BeginPopup("AddScript")) {
		return false;
	}

	bool changed{ false };
	const auto add_registered = [&](const ScriptRegistration& registration) {
		const auto* editor{ ScriptEditorRegistry::Find(registration.type_hash) };
		if (!editor ||
			!HasScriptType(editor->options.type, ScriptType::Resident) ||
			editor->options.hidden) {
			return;
		}
		if (ImGui::MenuItem(editor->options.label.c_str())) {
			scripts.AddEntryDeferred(MakeRootEntry(registration.type_hash));
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
				script.sequence.name = shared.name;
				script.sequence.shared_reference = true;
				script.sequence.shared_sequence_id = shared.id;
				scripts.AddEntryDeferred(MakeRootEntry(std::move(script)));
				changed = true;
			}
			DrawTooltip("Add a reference to this global editor-authored script.");
		}
		ImGui::EndMenu();
	}

	std::vector<std::string> groups;
	for (const auto& registration : ScriptRegistry::Entries()) {
		if (registration.type_hash == Hash<Script>()) {
			continue;
		}
		const auto* editor{ ScriptEditorRegistry::Find(registration.type_hash) };
		if (!editor ||
			!HasScriptType(editor->options.type, ScriptType::Resident) ||
			editor->options.hidden) {
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
			if (registration.type_hash == Hash<Script>()) {
				continue;
			}
			const auto* editor{ ScriptEditorRegistry::Find(registration.type_hash) };
			if (!editor ||
				!HasScriptType(editor->options.type, ScriptType::Resident) ||
				editor->options.hidden) {
				continue;
			}
			const std::string_view candidate_group{
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

bool DrawResidentScripts(
	ScriptEditorContext& context, ::ptgn::impl::Scripts& scripts
) {
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

	for (const int i : display_order) {
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
			if (!script.instance && registration) {
				script_runtime::AttachEntry(context.owner, script);
			}
			auto* sequence_script{ script.instance.get() };
			if (!sequence_script) {
				continue;
			}

			sequence_script->sequence.enabled = script.enabled;
			if (DrawSequence(context, sequence_script->sequence, changed)) {
				remove = i;
			}
			script.enabled = sequence_script->sequence.enabled;
			const SequenceId sequence_id{ sequence_script->sequence.id };
			script.sequence = sequence_script->sequence;
			script.sequence.id = sequence_id;
			script.sequence.runtime = ScriptSequenceRuntime{};
			continue;
		}

		ImGui::PushID(&script);
		bool open{ false };
		const float button_size{ ImGui::GetFrameHeight() };

		if (ImGui::BeginTable("ScriptRow", 2, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Script", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(
				"Controls", ImGuiTableColumnFlags_WidthFixed,
				EnabledDeleteControlsWidth()
			);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);
			ImGui::TableSetColumnIndex(0);

			const ImVec2 header_min{ ImGui::GetCursorScreenPos() };
			const ImVec2 header_max{
				header_min.x + std::max(1.0f, ImGui::GetContentRegionAvail().x),
				header_min.y + button_size
			};
			const ImVec4 header{
				script.enabled ? ImVec4{ 0.20f, 0.34f, 0.33f, 1.0f }
							   : ImVec4{ 0.25f, 0.25f, 0.25f, 1.0f }
			};
			const ImVec4 header_hovered{
				script.enabled ? ImVec4{ 0.26f, 0.43f, 0.41f, 1.0f }
							   : ImVec4{ 0.30f, 0.30f, 0.30f, 1.0f }
			};
			const ImVec4 header_active{
				script.enabled ? ImVec4{ 0.31f, 0.49f, 0.47f, 1.0f }
							   : ImVec4{ 0.34f, 0.34f, 0.34f, 1.0f }
			};
			const bool header_hovered_before_draw{
				ImGui::IsMouseHoveringRect(header_min, header_max)
			};
			const bool header_active_before_draw{
				header_hovered_before_draw &&
				ImGui::IsMouseDown(ImGuiMouseButton_Left)
			};
			const ImVec4 header_color{
				header_active_before_draw
					? header_active
					: (header_hovered_before_draw ? header_hovered : header)
			};
			ImGui::GetWindowDrawList()->AddRectFilled(
				header_min, header_max, ImGui::GetColorU32(header_color),
				ImGui::GetStyle().FrameRounding
			);

			const ImVec4 transparent{};
			ImGui::PushStyleColor(ImGuiCol_Header, transparent);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, transparent);
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, transparent);

			const bool has_contents{ editor && editor->has_contents };
			ImGuiTreeNodeFlags flags{
				ImGuiTreeNodeFlags_FramePadding |
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_NoTreePushOnOpen
			};
			if (has_contents) {
				flags |= ImGuiTreeNodeFlags_DefaultOpen;
			} else {
				flags |= ImGuiTreeNodeFlags_Leaf;
			}
			open = ImGui::TreeNodeEx(
				"##Script", flags, "%s",
				editor ? editor->options.label.c_str() : "Missing Script"
			);
			ImGui::PopStyleColor(3);
			if (editor) {
				DrawTooltip(editor->options.description.c_str());
			}

			ImGui::TableSetColumnIndex(1);
			bool enabled_changed{ false };
			if (DrawEnabledDeleteControls(
					script.enabled,
					"Enable or disable this script.",
					"Remove this script.",
					enabled_changed
				)) {
				remove = i;
			}
			changed |= enabled_changed;
			ImGui::EndTable();
		}

		if (open && editor && editor->has_contents && editor->draw) {
			if (editor->draw(script.value, context)) {
				changed = true;
				script.runtime_factory = {};
				if (!script.instance) {
					script_runtime::AttachEntry(context.owner, script);
				} else if (registration && registration->apply) {
					registration->apply(*script.instance, script.value);
				}
			}
		}
		ImGui::PopID();
	}

	if (remove >= 0) {
		auto& entry{ scripts.scripts[static_cast<std::size_t>(remove)] };
		const SequenceId id{ entry.instance ? entry.instance->sequence.id : entry.sequence.id };
		scripts.RemoveDeferred(id);
		changed = true;
	}
	return changed;
}

bool DrawScriptsComponent(Entity entity) {
	if (!entity) {
		return false;
	}

	auto* scripts{ entity.TryGet<::ptgn::impl::Scripts>() };
	if (!scripts) {
		return false;
	}
	scripts->Attach(entity);
	ScriptEditorContext context{ entity, entity.GetScene().ctx().shared_script_sequences };

	bool changed{ false };
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.20f, 0.34f, 0.33f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.26f, 0.43f, 0.41f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.31f, 0.49f, 0.47f, 1.0f });
	if (ImGui::Button("+ Script", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddScript");
	}
	ImGui::PopStyleColor(3);
	DrawTooltip("Add a custom script or an editor-authored sequence script.");

	changed |= DrawAddRootScriptPopup(context, *scripts);
	changed |= DrawResidentScripts(context, *scripts);
	return changed;
}


template <std::size_t N>
struct FixedString {
	char value[N];

	constexpr FixedString(const char (&text)[N]) {
		std::copy_n(text, N, value);
	}

	[[nodiscard]] constexpr std::string_view View() const {
		return { value, N - 1 };
	}
};

template <FixedString Reason, typename... T>
std::optional<std::string_view> HasAnyComponent(Entity entity) {
	if (entity.HasAny<T...>()) {
		return Reason.View();
	}

	return std::nullopt;
}

bool DrawOptionalViewport(
	std::string_view label, std::optional<Viewport>& value, ViewportSpace viewport_space
) {
	ImGui::PushID(&value);

	bool enabled{ value.has_value() };
	bool changed{ DrawPropertyRow(label, [&]() {
		return DrawDisabledIf(IsReadOnly(), [&]() {
			return ImGui::Checkbox("##enabled", &enabled);
		});
	}) };

	if (enabled != value.has_value()) {
		if (enabled) {
			auto& viewport{ value.emplace() };

			if (viewport_space == ViewportSpace::Normalized) {
				viewport.position = V2_float{ 0.0f, 0.0f };
				viewport.size	  = V2_float{ 1.0f, 1.0f };
			}
		} else {
			value.reset();
		}

		changed = true;
	}

	if (value.has_value()) {
		ImGui::Indent();

		if (viewport_space == ViewportSpace::Normalized) {
			FieldOptions options{
				.speed	= 0.01f,
				.min	= 0.0,
				.max	= 1.0,
				.format = "%.3f",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			};

			changed |= DrawValue("Position", value->position, options);
			changed |= DrawValue("Size", value->size, options);

			if (value->size.x > 1.0f || value->size.y > 1.0f) {
				value->size.x = std::min(1.0f, value->size.x);
				value->size.y = std::min(1.0f, value->size.y);
				changed		  = true;
			}
		} else {
			changed |= DrawValue(
				"Position", value->position,
				FieldOptions{
					.speed	= 1.0f,
					.format = "%.0f",
				}
			);

			changed |= DrawValue(
				"Size", value->size,
				FieldOptions{
					.speed	= 1.0f,
					.min	= 1.0f,
					.max	= 4096.0f,
					.format = "%.0f",
				}
			);

			if (value->size.x < 1.0f || value->size.y < 1.0f) {
				value->size.x = std::max(1.0f, value->size.x);
				value->size.y = std::max(1.0f, value->size.y);
				changed		  = true;
			}
		}

		ImGui::Unindent();
	}

	ImGui::PopID();

	return changed;
}

} // namespace

// TODO: Add Material.

template <>
struct Contents<::ptgn::impl::CameraData> {
	static bool Draw(::ptgn::impl::CameraData& camera) {
		bool changed{ false };

		// Draw this first because it controls the raw viewport's defaults and bounds.
		changed |= DrawValue("Viewport Space", camera.viewport_space);

		changed |= DrawOptionalViewport("Raw Viewport", camera.raw_viewport, camera.viewport_space);

		changed |= DrawValue("Pixel Rounding", camera.pixel_rounding);
		changed |= DrawValue("Bounding Box", camera.bounding_box);

		DrawReadOnlyValue("View Projection", camera.view_projection);

		return changed;
	}
};

template <>
struct Contents<::ptgn::impl::IDrawable> {
	static bool Draw(::ptgn::impl::IDrawable& drawable) {
		auto* current_info{ ::ptgn::impl::IDrawable::FindInfo(drawable.hash) };

		std::string preview{ current_info ? std::string{ current_info->GetDisplayName() }
										  : "<Unregistered Drawable>" };

		bool changed{ DrawPropertyRow("Drawable", [&]() {
			bool local_changed{ false };

			if (ImGui::BeginCombo("##value", preview.c_str())) {
				for (const auto& info : ::ptgn::impl::IDrawable::data()) {
					bool selected{ drawable.hash == info.hash };
					std::string display_name{ info.GetDisplayName() };

					if (ImGui::Selectable(display_name.c_str(), selected)) {
						drawable.hash = info.hash;
						local_changed = true;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			return local_changed;
		}) };

		if (!current_info && drawable.hash != 0) {
			ImGui::TextDisabled("Stored hash: %zu", drawable.hash);
		}

		return changed;
	}
};

// Only types whose default reflected layout is not ideal need a specialization.
template <>
struct Contents<Hollow> {
	static bool Draw(Hollow& hollow) {
		return DrawValue(
			"Line Width", hollow.line_width,
			FieldOptions{
				.speed	= 0.1f,
				.min	= kMinLineWidth,
				.max	= 1000.0f,
				.format = "%.2f",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			}
		);
	}
};

template <>
struct Contents<Rect> {
	static bool Draw(Rect& rect) {
		bool changed{ false };

		auto size{ rect.max - rect.min };

		if (DrawValue(
				"Size", size,
				FieldOptions{
					.speed	= 0.1f,
					.min	= 0.0,
					.max	= 0.0,
					.format = "%.3f",
				}
			)) {
			size.x = std::max(size.x, 0.0f);
			size.y = std::max(size.y, 0.0f);

			auto center{ rect.GetCenter() };
			auto half_size{ size * 0.5f };

			rect.min = center - half_size;
			rect.max = center + half_size;

			changed = true;
		}

		changed |= DrawValue("Min", rect.min);
		changed |= DrawValue("Max", rect.max);

		return changed;
	}
};

template <>
struct Contents<TextRun> {
	static bool Draw(TextRun& run) {
		bool changed{ false };
		changed |= DrawValue("Text", run.text, FieldOptions{ .multiline = true });
		changed |= DrawValue("Font", run.font);
		changed |= DrawValue("Style", run.style);
		return changed;
	}
};

template <>
struct Contents<StyledText> {
	static bool Draw(StyledText& text) {
		return DrawVectorEditor(
			text.runs, VectorOptions{
						   .item_name	 = "Text Run",
						   .default_open = true,
						   .reorderable	 = true,
					   }
		);
	}
};

template <>
struct Contents<Polygon> {
	static bool Draw(Polygon& polygon) {
		return DrawVectorEditor(
			polygon.vertices, VectorOptions{
								  .item_name	= "Vertex",
								  .default_open = true,
								  .reorderable	= true,
							  }
		);
	}
};

template <>
struct Contents<ButtonBorderVisuals> {
	static bool Draw(ButtonBorderVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(visuals.states);
	}
};

template <>
struct Contents<ButtonBackgroundVisuals> {
	static bool Draw(ButtonBackgroundVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(visuals.states);
	}
};

template <>
struct Contents<ButtonTextVisuals> {
	static bool Draw(ButtonTextVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(visuals.states);
	}
};

template <>
struct Contents<ButtonSpriteVisuals> {
	static bool Draw(ButtonSpriteVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(visuals.states);
	}
};

template <>
struct Contents<ButtonSounds> {
	static bool Draw(ButtonSounds& sounds) {
		bool changed{ false };
		changed |= DrawEnumArrayEditor<ButtonVisualState>("Sounds", sounds.states);
		changed |= DrawValue("Exclusive Audio", sounds.exclusive);
		return changed;
	}
};

} // namespace ptgn::editor::inspector

namespace ptgn::editor {

namespace {

using namespace inspector;

template <typename T>
bool DrawRegisteredContents(Entity entity) {
	return DrawComponentContents(entity.Get<T>());
}

void MarkTextLayoutDirty(Entity entity) {
	if (entity.Has<TextLayout>()) {
		entity.Get<TextLayout>().dirty = true;
	}
}

void MarkParentButtonDirty(Entity entity, ::ptgn::impl::ButtonDirty dirty) {
	Entity parent{ GetParent(entity) };

	if (!parent || !parent.Has<::ptgn::impl::ButtonData>()) {
		return;
	}

	parent.Get<::ptgn::impl::ButtonData>().dirty |= dirty;
}

void MarkButtonTextDirty(Entity entity) {
	MarkTextLayoutDirty(entity);
	MarkParentButtonDirty(entity, ::ptgn::impl::ButtonDirty::Text);
}

void MarkButtonBorderDirty(Entity entity) {
	MarkParentButtonDirty(entity, ::ptgn::impl::ButtonDirty::Border);
}

void MarkButtonBackgroundDirty(Entity entity) {
	MarkParentButtonDirty(entity, ::ptgn::impl::ButtonDirty::Background);
}

void MarkButtonSpriteDirty(Entity entity) {
	MarkParentButtonDirty(entity, ::ptgn::impl::ButtonDirty::Sprite);
}

void DrawAddDrawableMenuItem(Entity entity, const auto& info) {
	auto name{ std::string{ info.GetDisplayName() } };

	if (ImGui::MenuItem(name.c_str())) {
		entity.Add<::ptgn::impl::IDrawable>(info.hash);
	}
}

void DrawAddDrawableMenu(Entity entity, std::string_view label) {
	auto menu_label{ std::string{ label } };

	if (!ImGui::BeginMenu(menu_label.c_str())) {
		return;
	}

	const auto& drawables{ ::ptgn::impl::IDrawable::data() };

	auto draw_menu_item = [entity](const ::ptgn::impl::IDrawable::Info& info) mutable {
		auto display_name{ std::string{ info.GetDisplayName() } };

		if (ImGui::MenuItem(display_name.c_str())) {
			entity.Add<::ptgn::impl::IDrawable>(info.hash);
		}
	};

	std::vector<std::string_view> groups;

	for (const auto& info : drawables) {
		if (!info.group.has_value() || info.group->empty()) {
			continue;
		}

		if (std::ranges::find(groups, info.group.value()) == groups.end()) {
			groups.push_back(info.group.value());
		}
	}

	std::ranges::sort(groups);

	for (auto group : groups) {
		auto group_label{ std::string{ group } };

		if (!ImGui::BeginMenu(group_label.c_str())) {
			continue;
		}

		for (const auto& info : drawables) {
			if (info.group.has_value() && info.group.value() == group) {
				draw_menu_item(info);
			}
		}

		ImGui::EndMenu();
	}

	bool has_grouped_drawables{ !groups.empty() };
	bool has_ungrouped_drawables{ std::ranges::any_of(drawables, [](const auto& info) {
		return !info.group.has_value() || info.group->empty();
	}) };

	if (has_grouped_drawables && has_ungrouped_drawables) {
		ImGui::Separator();
	}

	for (const auto& info : drawables) {
		if (!info.group.has_value() || info.group->empty()) {
			draw_menu_item(info);
		}
	}

	ImGui::EndMenu();
}

bool DrawScaleValue(Entity entity, V2_float& scale, const FieldOptions& options) {
	ImGui::PushID(entity.Get<UUID>());
	ImGui::PushID("Scale");

	ImGuiStorage* storage{ ImGui::GetStateStorage() };
	ImGuiID scale_lock_id{ ImGui::GetID("ScaleLocked") };
	bool scale_locked{ storage->GetBool(scale_lock_id, true) };

	bool changed{ DrawPropertyRow("Scale", [&]() {
		if (ImGui::Checkbox("##ScaleLocked", &scale_locked)) {
			storage->SetBool(scale_lock_id, scale_locked);
		}

		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(scale_locked ? "Unlock ratio" : "Lock ratio");
		}

		ImGui::SameLine();

		constexpr float kFieldSpacing{ 4.0f };

		float available_width{ ImGui::GetContentRegionAvail().x };
		float field_width{ (available_width - kFieldSpacing) * 0.5f };

		V2_float previous_scale{ scale };

		ImGui::SetNextItemWidth(field_width);
		bool x_changed{ ImGui::DragFloat(
			"##X", &scale.x, options.speed, static_cast<float>(options.min),
			static_cast<float>(options.max), options.format, options.flags
		) };

		ImGui::SameLine(0.0f, kFieldSpacing);

		ImGui::SetNextItemWidth(field_width);
		bool y_changed{ ImGui::DragFloat(
			"##Y", &scale.y, options.speed, static_cast<float>(options.min),
			static_cast<float>(options.max), options.format, options.flags
		) };

		bool value_changed{ x_changed || y_changed };

		if (!value_changed || !scale_locked) {
			return value_changed;
		}

		constexpr float kScaleEpsilon{ 0.000001f };

		if (x_changed && !y_changed) {
			if (std::abs(previous_scale.x) > kScaleEpsilon) {
				float factor{ scale.x / previous_scale.x };
				scale.y = previous_scale.y * factor;
			} else if (std::abs(previous_scale.y) <= kScaleEpsilon) {
				scale.y = scale.x;
			}
		} else if (y_changed && !x_changed) {
			if (std::abs(previous_scale.y) > kScaleEpsilon) {
				float factor{ scale.y / previous_scale.y };
				scale.x = previous_scale.x * factor;
			} else if (std::abs(previous_scale.x) <= kScaleEpsilon) {
				scale.x = scale.y;
			}
		}

		return true;
	}) };

	ImGui::PopID();
	ImGui::PopID();

	return changed;
}

void DrawTransformComponent(Entity entity) {
	auto& transform{ entity.TryAdd<Transform>() };
	auto& depth{ entity.TryAdd<Depth>() };

	ImGui::PushID(static_cast<int>(Hash<Transform>()));
	bool open{ ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen) };
	ImGui::PopID();

	if (!open) {
		ImGui::Spacing();
		return;
	}

	auto read_only_reason{ HasAnyComponent<
		"Controlled by Button Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals,
		ButtonSpriteVisuals, ButtonTextVisuals>(entity) };

	if (read_only_reason.has_value() && !read_only_reason->empty()) {
		ImGui::TextDisabled(
			"%.*s", static_cast<int>(read_only_reason->size()), read_only_reason->data()
		);
	}

	ImGui::Indent();

	ReadOnlyScope read_only_scope{ read_only_reason.has_value() };

	DrawValue(
		"Position", transform.position,
		FieldOptions{
			.speed	= 1.0f,
			.format = "%.0f",
		}
	);

	DrawValue(
		"Depth", depth.value,
		FieldOptions{
			.speed	= 0.05f,
			.min	= -1000.0,
			.max	= 1000.0,
			.format = "%.2f",
			.flags	= ImGuiSliderFlags_AlwaysClamp,
		}
	);

	DrawValue(
		"Rotation", transform.rotation,
		FieldOptions{
			.speed	= 1.0f,
			.min	= 0.0,
			.max	= 360.0,
			.format = "%.1f deg",
			.flags	= ImGuiSliderFlags_AlwaysClamp,
		}
	);

	if (DrawScaleValue(
			entity, transform.scale,
			FieldOptions{
				.speed	= 0.01f,
				.min	= -1000.0,
				.max	= 1000.0,
				.format = "%.2f",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			}
		)) {
		transform.ClampScale();
	}

	ImGui::Unindent();
}


bool DrawRenderTargetSizeContents(Entity entity) {
	auto& target_size{ entity.Get<::ptgn::impl::RenderTargetSize>() };

	bool changed{
		ImGui::Checkbox(
			"Follow Display Size",
			&target_size.follow_display_size
		)
	};

	ImGui::BeginDisabled(target_size.follow_display_size);

	int size[2]{
		target_size.size.x,
		target_size.size.y,
	};

	if (ImGui::DragInt2(
			"Size",
			size,
			1.0f,
			1,
			16384,
			"%d",
			ImGuiSliderFlags_AlwaysClamp
		)) {
		target_size.size = {
			std::max(size[0], 1),
			std::max(size[1], 1),
		};

		changed = true;
	}

	ImGui::EndDisabled();

	return changed;
}

} // namespace

PTGN_REGISTER_COMPONENT(::ptgn::impl::Scripts,
	{
		.draw_contents = &DrawScriptsComponent,
	}
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::RenderTargetSize,
	{
		.label = "Render Target Size",
		.group = "Graphics",
		.removable = false,
		.addable = false,
		.draw_contents = &DrawRenderTargetSizeContents,
	}
);


PTGN_REGISTER_COMPONENT(
	::ptgn::impl::CameraData,
	{
		.draw_contents = &DrawRegisteredContents<::ptgn::impl::CameraData>,
	}
);

PTGN_REGISTER_COMPONENT(
	Rect,
	{
		.get_read_only_reason = &HasAnyComponent<
			"Controlled by Button Shape Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals>,
		.draw_contents = &DrawRegisteredContents<Rect>,
	}
);

PTGN_REGISTER_COMPONENT(
	Polygon, {
				 .draw_contents = &DrawRegisteredContents<Polygon>,
			 }
);

PTGN_REGISTER_COMPONENT(
	Circle,
	{ .get_read_only_reason = &HasAnyComponent<
		  "Controlled by Button Shape Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals> }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::IDrawable, { .label		  = "Drawable",
							   .draw_contents = &DrawRegisteredContents<::ptgn::impl::IDrawable>,
							   .draw_add_menu = &DrawAddDrawableMenu }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::TextData, {
					.on_changed = &MarkTextLayoutDirty,
					.get_read_only_reason =
						&HasAnyComponent<"Controlled by Button Text Visuals", ButtonTextVisuals>,
	}
);

PTGN_REGISTER_COMPONENT(
	Color,
	{ .get_read_only_reason = &HasAnyComponent<
		  "Controlled by Button Shape Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals> }
);

PTGN_REGISTER_COMPONENT(
	FillStyle,
	{ .get_read_only_reason = &HasAnyComponent<
		  "Controlled by Button Shape Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals>,
	  .draw_contents = &DrawRegisteredContents<FillStyle> }
);

PTGN_REGISTER_COMPONENT(
	TextureKey, { .get_read_only_reason =
					  &HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	Tint, { .get_read_only_reason =
				&HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::AnimationData,
	{ .get_read_only_reason =
		  &HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::TextureSize,
	{ .get_read_only_reason =
		  &HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::ButtonAnimationPart,
	{ .get_read_only_reason =
		  &HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	Visible, { .get_read_only_reason = &HasAnyComponent<
				   "Controlled by Button Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals,
				   ButtonSpriteVisuals, ButtonTextVisuals> }
);

PTGN_REGISTER_COMPONENT(
	Origin, { .get_read_only_reason = &HasAnyComponent<
				  "Controlled by Button Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals,
				  ButtonSpriteVisuals, ButtonTextVisuals> }
);

PTGN_REGISTER_COMPONENT(
	ButtonBackgroundVisuals, {
								 .removable		  = false,
								 .addable		  = false,
								 .draw_after_tags = true,
								 .on_changed	  = &MarkButtonBackgroundDirty,
								 .draw_contents = &DrawRegisteredContents<ButtonBackgroundVisuals>,
							 }
);

PTGN_REGISTER_COMPONENT(
	ButtonBorderVisuals, {
							 .removable		  = false,
							 .addable		  = false,
							 .draw_after_tags = true,
							 .on_changed	  = &MarkButtonBorderDirty,
							 .draw_contents	  = &DrawRegisteredContents<ButtonBorderVisuals>,
						 }
);

PTGN_REGISTER_COMPONENT(
	ButtonSpriteVisuals, {
							 .removable		  = false,
							 .addable		  = false,
							 .draw_after_tags = true,
							 .on_changed	  = &MarkButtonSpriteDirty,
							 .draw_contents	  = &DrawRegisteredContents<ButtonSpriteVisuals>,
						 }
);

PTGN_REGISTER_COMPONENT(
	ButtonTextVisuals, {
						   .removable		= false,
						   .addable			= false,
						   .draw_after_tags = true,
						   .on_changed		= &MarkButtonTextDirty,
						   .draw_contents	= &DrawRegisteredContents<ButtonTextVisuals>,
					   }
);

PTGN_REGISTER_COMPONENT(
	ButtonSounds, {
					  .removable	   = false,
					  .addable		   = false,
					  .draw_after_tags = true,
					  .draw_contents   = &DrawRegisteredContents<ButtonSounds>,
				  }
);

void InspectorPanel::OnRender(EditorContext& ctx) {
	inspector::InspectorAssetManagerScope asset_manager_scope{ ctx.editor.GetAssetManager() };

	ImGui::Begin("Inspector");

	auto& scene_hierarchy{ ctx.editor.GetSceneHierarchyPanel() };
	auto selected_entity{ scene_hierarchy.GetSelectedEntity() };

	if (!selected_entity) {
		ImGui::End();
		return;
	}

	auto name{ std::string{ selected_entity.Get<Tag>() } };
	if (ImGui::InputText("Name", &name)) {
		selected_entity.Add<Tag>(name);
	}

	ImGui::Separator();

	DrawTransformComponent(selected_entity);
	ComponentEditorRegistry::DrawComponents(selected_entity);
	ComponentEditorRegistry::DrawTagComponents(selected_entity);
	ComponentEditorRegistry::DrawComponents(selected_entity, true);

	ImGui::Separator();

	if (ImGui::Button("Add Component", ImVec2{ -1.0f, 0.0f })) {
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup")) {
		ComponentEditorRegistry::DrawAddComponentMenu(selected_entity);
		ImGui::EndPopup();
	}

	ImGui::End();
}

} // namespace ptgn::editor
