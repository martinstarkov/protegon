#include "panels/inspector.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <variant>
#include <string>
#include <string_view>

#include "commands/entity/entity_reference.h"
#include "core/editor.h"
#include "core/editor_context.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/util/hash.h"
#include "panels/inspector_feature_helpers.h"
#include "panels/inspector_fields.h"
#include "panels/scene_hierarchy.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/renderer.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"
#include "runtime/ecs/component_registration.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/gaussian_blur.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/custom_shader.h"
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
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "scripting/script_editor_registry.h"

namespace ptgn::editor {

namespace inspector {

namespace {

void DrawTooltip(const char* text) {
	if (text && *text && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", text);
	}
}

void DrawDisabledWrappedText(std::string_view text) {
	ImGui::PushStyleColor(
		ImGuiCol_Text,
		ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)
	);
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
	ImGui::PopTextWrapPos();
	ImGui::PopStyleColor();
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
	std::optional<ImGuiID> editing_sequence_name;
	std::string editing_sequence_original_name;
	std::unordered_map<ImGuiID, bool> sequence_open_states;
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

bool DrawDurationInput(const char* label, float& milliseconds, float width, const char* tooltip) {
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
			changed		 = updated != milliseconds;
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
	bool& enabled, const char* enabled_tooltip, const char* delete_tooltip, bool& enabled_changed
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
	const ImVec2 text_position{ minimum.x + (maximum.x - minimum.x - text_size.x) * 0.5f,
								minimum.y + (maximum.y - minimum.y - text_size.y) * 0.5f };
	ImGui::GetWindowDrawList()->AddText(text_position, ImGui::GetColorU32(ImGuiCol_Text), text);
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
	return ImGui::CalcTextSize(widest.c_str()).x + button_width * 2.0f + spacing * 2.0f;
}

bool DrawCountControl(
	const char* label, int& value, int minimum, int maximum = 100, bool disabled = false,
	const char* tooltip = nullptr
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
	const char* id, const char* label, bool default_open, bool empty, const char* tooltip,
	bool show_add_button, const char* add_tooltip, bool& add_requested
) {
	static std::unordered_map<ImGuiID, bool> force_open_next_frame;

	ImGui::PushID(id);

	const ImGuiID tree_id{ ImGui::GetID("##Tree") };
	if (force_open_next_frame[tree_id]) {
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		force_open_next_frame[tree_id] = false;
	}

	bool open{ false };
	const float button_size{ ImGui::GetFrameHeight() };
	const int columns{ show_add_button ? 2 : 1 };
	constexpr ImGuiTableFlags table_flags{ ImGuiTableFlags_SizingStretchProp |
										   ImGuiTableFlags_NoSavedSettings |
										   ImGuiTableFlags_NoPadOuterX };

	if (ImGui::BeginTable("##SectionHeaderRow", columns, table_flags)) {
		ImGui::TableSetupColumn("Section", ImGuiTableColumnFlags_WidthStretch, 1.0f);

		if (show_add_button) {
			ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, button_size);
		}

		ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);
		ImGui::TableSetColumnIndex(0);

		ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth |
								  ImGuiTreeNodeFlags_NoTreePushOnOpen |
								  ImGuiTreeNodeFlags_FramePadding |
								  ImGuiTreeNodeFlags_AllowOverlap };

		if (empty) {
			flags |= ImGuiTreeNodeFlags_Leaf;
		} else if (default_open) {
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}

		const bool tree_open{ ImGui::TreeNodeEx("##Tree", flags, "%s", label) };

		open = !empty && tree_open;

		DrawTooltip(empty ? "Add an item to use this section." : tooltip);

		if (show_add_button) {
			ImGui::TableSetColumnIndex(1);

			if (ImGui::Button("+", ImVec2{ button_size, button_size })) {
				add_requested				   = true;
				open						   = true;
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

[[nodiscard]] ScriptSequence* ResolveEditorSequence(
	ScriptEditorContext& context, ScriptSequence& binding
) {
	if (!context.owner) {
		return binding.shared_reference ? nullptr : std::addressof(binding);
	}

	return script_runtime::Resolve(context.owner, binding);
}

void AddEditorScriptEntry(
	ScriptEditorContext& context, ::ptgn::impl::Scripts& scripts, ScriptEntry entry
) {
	if (context.owner) {
		scripts.AddEntryDeferred(std::move(entry));

		return;
	}

	// Prefab JSON components are temporary values and do not receive
	// the normal ECS pending operation flush.
	scripts.scripts.emplace_back(std::move(entry));
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
	const SequenceId shared_id{ shared.id };
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
	const SequenceId binding_id{ binding.id };
	const bool enabled{ binding.enabled };
	binding					   = std::move(local);
	binding.id				   = binding_id;
	binding.enabled			   = enabled;
	binding.shared_reference   = false;
	binding.shared_sequence_id = 0;
	binding.runtime			   = ScriptSequenceRuntime{};
}

bool DrawActionPicker(
	ScriptEditorContext& context, ScriptStep& action, bool timed_only, float width = -FLT_MIN
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
	const std::optional<Candidate> current{ current_registration
												? resolve_candidate(*current_registration)
												: std::nullopt };
	ImGui::SetNextItemWidth(width);

	const float popup_min_width{ ImGui::CalcItemWidth() };

	ImGui::SetNextWindowSizeConstraints(
		ImVec2{ popup_min_width, 0.0f }, ImVec2{ FLT_MAX, FLT_MAX }
	);

	const bool open{
		ImGui::BeginCombo("##RegisteredAction", current ? current->label.data() : "Missing Action")
	};

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
			const bool enabled{ action.enabled };
			action		   = ScriptRegistry::MakeStep(registration.type_hash);
			action.enabled = enabled;
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
			const std::string type_hash{ std::to_string(registration.type_hash) };
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

	if (context.owner && !timed_only && !context.shared_sequences.sequences.empty() &&
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
				script.sequence.name			   = shared.name;
				script.sequence.shared_reference   = true;
				script.sequence.shared_sequence_id = shared.id;
				action							   = ScriptRegistry::MakeStep(std::move(script));
				action.enabled					   = enabled;
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

bool DrawActionPickerWithInline(ScriptEditorContext& context, ScriptStep& action, bool timed_only) {
	EnsureActionValue(action);

	const auto* registered_editor{ ScriptEditorRegistry::Find(action.type_hash) };
	const bool has_inline_editor{ !timed_only && registered_editor &&
								  static_cast<bool>(registered_editor->draw_inline) };

	if (!has_inline_editor) {
		return DrawActionPicker(context, action, timed_only);
	}

	const float available{ ImGui::GetContentRegionAvail().x };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float picker_width{ std::min(150.0f, std::max(110.0f, available * 0.32f)) };
	const float row_y{ ImGui::GetCursorScreenPos().y };

	bool changed{ DrawActionPicker(context, action, timed_only, picker_width) };

	registered_editor = ScriptEditorRegistry::Find(action.type_hash);

	if (registered_editor && registered_editor->draw_inline) {
		ImGui::SameLine(0.0f, spacing);

		ImVec2 inline_position{ ImGui::GetCursorScreenPos() };
		inline_position.y = row_y;
		ImGui::SetCursorScreenPos(inline_position);

		ImGui::SetNextItemWidth(-FLT_MIN);

		if (registered_editor->draw_inline(context, action.value)) {
			action.runtime_factory = {};
			changed				   = true;
		}
	}

	return changed;
}

bool DrawTimingOptions(
	ScriptEditorContext& context, ScriptStep& action, ScriptTiming& timing, float left_screen_x
) {
	(void)context;
	EnsureActionValue(action);
	bool changed{ false };

	const float right_screen_x{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
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
		action.type_hash == Hash<RemoveComponentsScript>()) {
		return false;
	}

	if (action.type_hash == Hash<AddComponentsScript>()) {
		AddComponentsScript add_components;
		if (!TryReadScriptJson(action.value, add_components) || add_components.components.empty()) {
			return false;
		}
	}

	const float right_screen_x{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
	ImGui::SetCursorScreenPos(ImVec2{ left_screen_x, ImGui::GetCursorScreenPos().y });
	bool changed{ false };
	if (ImGui::BeginChild(
			"ActionParameters", ImVec2{ std::max(1.0f, right_screen_x - left_screen_x), 0.0f },
			ImGuiChildFlags_AutoResizeY
		)) {
		changed = editor->draw(context, action.value);
		if (changed) {
			action.runtime_factory = {};
		}
	}
	ImGui::EndChild();
	return changed;
}

bool DrawActions(ScriptEditorContext& context, ScriptSequence& sequence, ScriptSequence& binding) {
	ImGui::PushID("SequenceActions");

	bool changed{ false };
	int remove_index{ -1 };
	int duplicate_index{ -1 };
	int move_from{ -1 };
	int move_to{ -1 };

	for (int index{ 0 }; index < static_cast<int>(sequence.steps.size()); ++index) {
		auto& action{ sequence.steps[static_cast<std::size_t>(index)] };
		bool remove{ false };
		bool duplicate{ false };
		ImGui::PushID(index);

		constexpr float drag_width{ 28.0f };
		const float label_width{ std::max(
			{ ImGui::CalcTextSize("Action").x, ImGui::CalcTextSize("Tween").x,
			  ImGui::CalcTextSize("Delay").x }
		) };
		const float type_width{ label_width + ImGui::GetFrameHeight() +
								ImGui::GetStyle().FramePadding.x * 2.0f };
		const float duration_width{ ImGui::CalcTextSize("5000ms").x +
									ImGui::GetStyle().FramePadding.x * 2.0f };
		const float repeats_width{ GetCountControlWidth("Repeats") };
		const float button_width{ ImGui::GetFrameHeight() };
		ActionForm displayed_form{ GetActionForm(action) };
		const float controls_width{
			EnabledDeleteControlsWidth() +
			(displayed_form == ActionForm::Tween ? CompactControlSpacing() + repeats_width : 0.0f)
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
			ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthFixed, controls_width);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, button_width);

			int column{};
			ImGui::TableSetColumnIndex(column++);
			const bool dimmed{ !action.enabled };
			if (dimmed) {
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
			}
			ImGui::Button("::", ImVec2{ drag_width, button_width });
			if (dimmed) {
				ImGui::PopStyleVar();
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
				duplicate = true;
			}
			DrawTooltip(
				action.enabled ? "Drag to reorder. Right click to duplicate."
							   : "Disabled action. Right click to duplicate."
			);
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				const ActionDragPayload payload{ index };
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
					const auto candidate{ static_cast<ActionForm>(i) };
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
					case ActionForm::Tween: break;
				}
			}

			ImGui::TableSetColumnIndex(column);
			if (displayed_form == ActionForm::Tween) {
				changed |= DrawCountControl(
					"Repeats", action.timing->additional_repeats, 0, 100,
					action.timing->infinite_repeats, "Additional full duration cycles."
				);
				SameLineControl();
			}
			bool enabled_changed{ false };
			remove = DrawEnabledDeleteControls(
				action.enabled, "Enable or disable this action.", "Delete this action.",
				enabled_changed
			);
			changed |= enabled_changed;
			ImGui::EndTable();
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
		ImGui::PushID(i);

		const float lifecycle_width{ 145.0f };
		if (ImGui::BeginTable("LifecycleRow", 3, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Lifecycle", ImGuiTableColumnFlags_WidthFixed, lifecycle_width);
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(
				"Controls", ImGuiTableColumnFlags_WidthFixed, EnabledDeleteControlsWidth()
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
				changed			   = true;
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
					callback.enabled, "Enable or disable this lifecycle callback.",
					"Remove this lifecycle callback.", enabled_changed
				)) {
				remove = i;
			}
			changed |= enabled_changed;
			ImGui::EndTable();
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

bool DrawEvent(
	ScriptEditorContext& context, EventCondition& event, bool stop_event, bool& switch_kind,
	bool& changed
) {
	bool remove{ false };

	const float kind_width{
		std::max(ImGui::CalcTextSize("Start").x, ImGui::CalcTextSize("Stop").x) +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const float consume_width{ ImGui::CalcTextSize("Consume").x +
							   ImGui::GetStyle().FramePadding.x * 2.0f };
	const EventEditorRegistration* selected{ EventEditorRegistry::Find(event.type_hash) };
	bool event_type_changed{ false };

	auto inline_field_count = [](const EventEditorRegistration* registration) {
		return registration ? registration->options.inline_fields : 0;
	};

	if (ImGui::BeginTable(
			"EventRow", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
		)) {
		ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, kind_width);
		ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Consume", ImGuiTableColumnFlags_WidthFixed, consume_width);
		ImGui::TableSetupColumn(
			"Controls", ImGuiTableColumnFlags_WidthFixed, EnabledDeleteControlsWidth()
		);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());

		int column{};
		ImGui::TableSetColumnIndex(column++);
		ImGui::BeginDisabled(!event.enabled);
		if (ImGui::Button(
				stop_event ? "Stop" : "Start", ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() }
			)) {
			switch_kind = true;
		}
		DrawTooltip(
			stop_event ? "Change this to a start trigger." : "Change this to a stop trigger."
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
			if (available_width - event_width -
					spacing * static_cast<float>(initial_inline_fields) <
				minimum_fields_width) {
				event_width = std::max(
					90.0f, available_width - spacing * static_cast<float>(initial_inline_fields) -
							   minimum_fields_width
				);
			}
		}

		ImGui::SetNextItemWidth(std::max(1.0f, event_width));
		if (ImGui::BeginCombo(
				"##Event", selected ? selected->options.label.c_str() : "Missing Event"
			)) {
			auto candidate_available = [&](const EventEditorRegistration& candidate) {
				const auto* registration{ SequenceEventRegistry::Find(candidate.type_hash) };
				return registration && (!registration->available || !context.owner ||
										registration->available(context.owner));
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
					event.name		= candidate.name;
					registration->set_defaults(event);
					selected		   = EventEditorRegistry::Find(event.type_hash);
					event_type_changed = true;
					changed			   = true;
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

		if (selected && !event_type_changed && selected->options.inline_fields > 0 &&
			selected->options.draw) {
			ImGui::SameLine();
			changed |= selected->options.draw(event.value);
		}
		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(column++);
		ImGui::BeginDisabled(!event.enabled);
		changed |= DrawToggleButton(
			"Consume", event.consume, ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() },
			"Stop propagation after this event matches."
		);
		ImGui::EndDisabled();

		ImGui::TableSetColumnIndex(column);
		bool enabled_changed{ false };
		remove = DrawEnabledDeleteControls(
			event.enabled, "Enable or disable this trigger.", "Remove this trigger.",
			enabled_changed
		);
		changed |= enabled_changed;
		ImGui::EndTable();
	}

	return remove;
}

bool DrawEvents(ScriptEditorContext& context, ScriptSequence& sequence) {
	bool changed{ false };
	bool add_requested{ false };
	const bool empty{ sequence.start_events.empty() && sequence.stop_events.empty() &&
					  sequence.lifecycle_actions.empty() };
	const bool open{ DrawUnframedSectionHeader(
		"EventsSection", "Events", true, empty, "Start and stop triggers plus lifecycle callbacks.",
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

				return registration && (!registration->available || !context.owner ||
										registration->available(context.owner));
			};
			auto add_candidate = [&](const EventEditorRegistration& candidate) {
				const auto* registration{ SequenceEventRegistry::Find(candidate.type_hash) };
				if (!candidate_available(candidate)) {
					return;
				}
				if (ImGui::MenuItem(candidate.options.label.c_str())) {
					EventCondition trigger{ .enabled   = true,
											.type_hash = registration->type_hash,
											.name	   = registration->name };
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
	}
	ImGui::PopID();

	if (!open) {
		return changed;
	}

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

	const ImGuiID editor_id{ ImGui::GetID("EditorState") };
	const float button_size{ ImGui::GetFrameHeight() };
	const bool show_runtime_controls{ context.owner && context.ctx.editor.IsPlaying() };
	const int column_count{ show_runtime_controls ? 6 : 3 };
	bool open{ state.sequence_open_states.try_emplace(editor_id, true).first->second };
	ImVec2 name_input_min{};
	ImVec2 name_input_max{};
	bool name_input_drawn{ false };
	bool name_input_hovered{ false };
	bool began_name_edit_this_frame{ false };

	if (ImGui::BeginTable(
			"ScriptSequenceHeader", column_count,
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
		)) {
		ImGui::TableSetupColumn("Sequence", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Options", ImGuiTableColumnFlags_WidthStretch, 1.0f);

		if (show_runtime_controls) {
			ImGui::TableSetupColumn("Play", ImGuiTableColumnFlags_WidthFixed, button_size);
			ImGui::TableSetupColumn("Pause", ImGuiTableColumnFlags_WidthFixed, button_size);
			ImGui::TableSetupColumn("Stop", ImGuiTableColumnFlags_WidthFixed, button_size);
		}

		ImGui::TableSetupColumn(
			"Controls", ImGuiTableColumnFlags_WidthFixed, EnabledDeleteControlsWidth()
		);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);

		ImGui::TableSetColumnIndex(0);
		const ImVec2 header_min{ ImGui::GetCursorScreenPos() };
		const ImVec2 header_max{ header_min.x + std::max(1.0f, ImGui::GetContentRegionAvail().x),
								 header_min.y + button_size };
		const ImVec2 mouse{ ImGui::GetMousePos() };

		auto& stored_open{ state.sequence_open_states[editor_id] };
		ImGui::SetNextItemOpen(stored_open, ImGuiCond_Always);
		const bool editing_before_draw{ state.editing_sequence_name == editor_id };
		const ImVec4 header{ binding.enabled ? ImVec4{ 0.35f, 0.24f, 0.39f, 1.0f }
											 : ImVec4{ 0.25f, 0.25f, 0.25f, 1.0f } };
		const ImVec4 header_hovered{ binding.enabled ? ImVec4{ 0.44f, 0.31f, 0.48f, 1.0f }
													 : ImVec4{ 0.30f, 0.30f, 0.30f, 1.0f } };
		const ImVec4 header_active{ binding.enabled ? ImVec4{ 0.50f, 0.36f, 0.55f, 1.0f }
													: ImVec4{ 0.34f, 0.34f, 0.34f, 1.0f } };

		const bool header_hovered_before_draw{ ImGui::IsMouseHoveringRect(header_min, header_max) };
		const bool header_active_before_draw{ !editing_before_draw && header_hovered_before_draw &&
											  ImGui::IsMouseDown(ImGuiMouseButton_Left) };
		const ImVec4 header_color{
			editing_before_draw ? header
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
			ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_FramePadding |
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_AllowOverlap,
			"%s", editing_before_draw ? "" : sequence->name.c_str()
		);
		ImGui::PopStyleColor(3);
		stored_open = open;

		// Use the measured table cell rectangle for both display and edit modes. Framed TreeNodeEx
		// expands its native background beyond the cursor by a style dependent amount, which causes
		// the one or two pixel width change when the InputText overlay becomes active.
		const ImVec2 tree_min{ header_min };
		const ImVec2 tree_max{ header_max };
		const bool tree_hovered{ ImGui::IsItemHovered() };
		const float text_start_x{ tree_min.x + ImGui::GetFrameHeight() };
		const float minimum_name_width{ 48.0f };
		const float visible_name_width{
			std::max(minimum_name_width, ImGui::CalcTextSize(sequence->name.c_str()).x)
		};
		const float name_hit_end_x{ std::min(tree_max.x, text_start_x + visible_name_width) };
		const bool name_hit_hovered{ tree_hovered && mouse.x >= text_start_x &&
									 mouse.x <= name_hit_end_x };
		const bool tree_clicked_left{ ImGui::IsItemClicked(ImGuiMouseButton_Left) };

		bool begin_edit{ !editing_before_draw && name_hit_hovered &&
						 ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) };
		if (ImGui::BeginPopupContextItem("SequenceNameContext")) {
			if (ImGui::MenuItem("Rename")) {
				begin_edit = true;
			}
			ImGui::EndPopup();
		}
		if (begin_edit) {
			state.editing_sequence_name			 = editor_id;
			state.editing_sequence_original_name = sequence->name;
			began_name_edit_this_frame			 = true;
		}

		if (!editing_before_draw && !begin_edit && tree_clicked_left && mouse.x > name_hit_end_x) {
			open		= !open;
			stored_open = open;
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
			const bool submitted{ ImGui::InputText(
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
			ImGui::SetTooltip("Double click name to rename.");
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
			if (context.owner) {
				const bool global{ binding.shared_reference };

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
			if (sequence && ImGui::MenuItem(
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

		int column{ 2 };

		if (show_runtime_controls) {
			ImGui::TableSetColumnIndex(column++);

			if (DrawCenteredTextButton("##Play", ">", ImVec2{ button_size, button_size })) {
				script_runtime::Start(context.owner, binding.id, true);
			}

			DrawTooltip(
				binding.runtime.running ? "Restart this sequence." : "Start this sequence."
			);

			ImGui::TableSetColumnIndex(column++);

			ImGui::BeginDisabled(!binding.runtime.running);

			if (DrawCenteredTextButton("##Pause", "||", ImVec2{ button_size, button_size })) {
				script_runtime::SetPaused(context.owner, binding.id, !binding.runtime.paused);
			}

			ImGui::EndDisabled();

			DrawTooltip(binding.runtime.paused ? "Resume this sequence." : "Pause this sequence.");

			ImGui::TableSetColumnIndex(column++);

			ImGui::BeginDisabled(!binding.runtime.running);

			if (DrawCenteredTextButton("##Stop", "[]", ImVec2{ button_size, button_size })) {
				script_runtime::Stop(context.owner, binding.id);
			}

			ImGui::EndDisabled();
			DrawTooltip("Stop this sequence.");
		}

		ImGui::TableSetColumnIndex(column);

		bool enabled_changed{ false };

		remove = DrawEnabledDeleteControls(
			binding.enabled, "Enable or disable this script sequence.",
			"Delete this script sequence.", enabled_changed
		);

		changed |= enabled_changed;
		ImGui::EndTable();
	}

	if (state.editing_sequence_name == editor_id && name_input_drawn &&
		!began_name_edit_this_frame) {
		const bool clicked{ ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
							ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
							ImGui::IsMouseClicked(ImGuiMouseButton_Right) };
		const ImVec2 mouse{ ImGui::GetMousePos() };
		const bool inside_input{ mouse.x >= name_input_min.x && mouse.x <= name_input_max.x &&
								 mouse.y >= name_input_min.y && mouse.y <= name_input_max.y };
		if (clicked && !inside_input && !name_input_hovered) {
			state.editing_sequence_name.reset();
			changed = true;
		}
	}

	if (open && !remove) {
		sequence = ResolveEditorSequence(context, binding);
		if (sequence) {
			if (sequence->channel &&
				ImGui::BeginTable("SequenceChannelRow", 2, ImGuiTableFlags_SizingStretchProp)) {
				const float label_width{ ImGui::CalcTextSize("Channel:").x +
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
			changed |= DrawEvents(context, *sequence);

			bool add_action_requested{ false };
			const bool sequence_open{ DrawUnframedSectionHeader(
				"SequenceSection", "Sequence", true, sequence->steps.empty(),
				"Ordered actions executed by this script sequence.", true,
				"Add an action to this sequence.", add_action_requested
			) };
			ImGui::PushID("SequenceSection");
			if (add_action_requested) {
				ImGui::OpenPopup("AddSequenceAction");
			}
			if (ImGui::BeginPopup("AddSequenceAction")) {
				if (ImGui::MenuItem("Action")) {
					sequence->steps.push_back(ScriptRegistry::MakeStep<EmitSignalScript>());
					changed = true;
				}

				if (ImGui::MenuItem("Tween")) {
					sequence->steps.push_back(ScriptRegistry::MakeStep<MoveToScript>());
					changed = true;
				}

				if (ImGui::MenuItem("Delay")) {
					sequence->steps.push_back(ScriptRegistry::MakeStep<WaitScript>());
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

	if (context.owner && !context.shared_sequences.sequences.empty() &&
		ImGui::BeginMenu("Global")) {
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
		const std::string group{ editor->options.group.empty() ? "Other" : editor->options.group };
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
				const SequenceId sequence_id{ editable_sequence->id };

				script.sequence			= *editable_sequence;
				script.sequence.id		= sequence_id;
				script.sequence.runtime = ScriptSequenceRuntime{};
			}

			continue;
		}

		ImGui::PushID(i);
		bool open{ false };
		const float button_size{ ImGui::GetFrameHeight() };

		if (ImGui::BeginTable("ScriptRow", 2, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Script", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(
				"Controls", ImGuiTableColumnFlags_WidthFixed, EnabledDeleteControlsWidth()
			);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);
			ImGui::TableSetColumnIndex(0);

			const ImVec2 header_min{ ImGui::GetCursorScreenPos() };
			const ImVec2 header_max{ header_min.x +
										 std::max(1.0f, ImGui::GetContentRegionAvail().x),
									 header_min.y + button_size };
			const ImVec4 header{ script.enabled ? ImVec4{ 0.20f, 0.34f, 0.33f, 1.0f }
												: ImVec4{ 0.25f, 0.25f, 0.25f, 1.0f } };
			const ImVec4 header_hovered{ script.enabled ? ImVec4{ 0.26f, 0.43f, 0.41f, 1.0f }
														: ImVec4{ 0.30f, 0.30f, 0.30f, 1.0f } };
			const ImVec4 header_active{ script.enabled ? ImVec4{ 0.31f, 0.49f, 0.47f, 1.0f }
													   : ImVec4{ 0.34f, 0.34f, 0.34f, 1.0f } };
			const bool header_hovered_before_draw{
				ImGui::IsMouseHoveringRect(header_min, header_max)
			};
			const bool header_active_before_draw{ header_hovered_before_draw &&
												  ImGui::IsMouseDown(ImGuiMouseButton_Left) };
			const ImVec4 header_color{
				header_active_before_draw ? header_active
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

			ImGui::TableSetColumnIndex(1);
			bool enabled_changed{ false };
			if (DrawEnabledDeleteControls(
					script.enabled, "Enable or disable this script.", "Remove this script.",
					enabled_changed
				)) {
				remove = i;
			}
			changed |= enabled_changed;
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
		if (context.owner) {
			auto& entry{ scripts.scripts[static_cast<std::size_t>(remove)] };

			const SequenceId id{ entry.instance ? entry.instance->sequence.id : entry.sequence.id };

			scripts.RemoveDeferred(id);
		} else {
			scripts.scripts.erase(scripts.scripts.begin() + remove);
		}

		changed = true;
	}
	return changed;
}

bool DrawOptionalViewport(
	EditorContext& ctx, std::string_view label, std::optional<Viewport>& value,
	ViewportSpace viewport_space
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

			changed |= DrawValue(ctx, "Position", value->position, options);
			changed |= DrawValue(ctx, "Size", value->size, options);

			if (value->size.x > 1.0f || value->size.y > 1.0f) {
				value->size.x = std::min(1.0f, value->size.x);
				value->size.y = std::min(1.0f, value->size.y);
				changed		  = true;
			}
		} else {
			changed |= DrawValue(
				ctx, "Position", value->position,
				FieldOptions{
					.speed	= 1.0f,
					.format = "%.0f",
				}
			);

			changed |= DrawValue(
				ctx, "Size", value->size,
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

void MarkTextLayoutDirty(Entity entity) {
	if (entity.Has<TextLayout>()) {
		entity.Get<TextLayout>().dirty = true;
	}
}

void MarkButtonDirty(Entity entity, ::ptgn::impl::ButtonDirty dirty) {
	Entity button{ entity };

	if (!button.Has<::ptgn::impl::ButtonData>()) {
		button = GetParent(entity);
	}

	if (!button || !button.Has<::ptgn::impl::ButtonData>()) {
		return;
	}

	button.Get<::ptgn::impl::ButtonData>().dirty |= dirty;
}

void MarkButtonTextDirty(Entity entity) {
	MarkTextLayoutDirty(entity);
	MarkButtonDirty(entity, ::ptgn::impl::ButtonDirty::Text);
}

void MarkButtonBorderDirty(Entity entity) {
	MarkButtonDirty(entity, ::ptgn::impl::ButtonDirty::Border);
}

void MarkButtonBackgroundDirty(Entity entity) {
	MarkButtonDirty(entity, ::ptgn::impl::ButtonDirty::Background);
}

void MarkButtonSpriteDirty(Entity entity) {
	MarkButtonDirty(entity, ::ptgn::impl::ButtonDirty::Sprite);
}

} // namespace

template <>
struct Contents<::ptgn::impl::Scripts> {
	static bool Draw(EditorContext& ctx, ::ptgn::impl::Scripts& scripts) {
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
};

template <>
struct Contents<::ptgn::impl::RenderTargetSize> {
	static bool Draw(EditorContext&, ::ptgn::impl::RenderTargetSize& target_size) {
		bool changed{ ImGui::Checkbox("Follow Display Size", &target_size.follow_display_size) };

		ImGui::BeginDisabled(target_size.follow_display_size);

		int size[2]{
			target_size.size.x,
			target_size.size.y,
		};

		if (ImGui::DragInt2("Size", size, 1.0f, 1, 16384, "%d", ImGuiSliderFlags_AlwaysClamp)) {
			target_size.size = {
				std::max(size[0], 1),
				std::max(size[1], 1),
			};

			changed = true;
		}

		ImGui::EndDisabled();

		return changed;
	}
};

template <>
struct Contents<::ptgn::impl::CameraData> {
	static bool Draw(EditorContext& ctx, ::ptgn::impl::CameraData& camera) {
		bool changed{ false };

		// Draw this first because it controls the raw viewport's defaults and bounds.
		changed |= DrawValue(ctx, "Viewport Space", camera.viewport_space);

		changed |=
			DrawOptionalViewport(ctx, "Raw Viewport", camera.raw_viewport, camera.viewport_space);

		changed |= DrawValue(ctx, "Pixel Rounding", camera.pixel_rounding);
		changed |= DrawValue(ctx, "Bounding Box", camera.bounding_box);

		if (ctx.local.settings.show_read_only_inspector_data) {
			DrawReadOnlyValue(ctx, "View Projection", camera.view_projection);
		}

		return changed;
	}
};

bool DrawLayerMaskValue(
	std::string_view label,
	LayerMask& value
) {
	ScopedID scope{ label };
	const char* preview{ nullptr };
	char raw_preview[32]{};

	if (value == kLayersAll) {
		preview = "All";
	} else if (value == kLayersNone) {
		preview = "None";
	} else if (value == kLayerDefault) {
		preview = "Default";
	} else {
		std::snprintf(
			raw_preview,
			sizeof(raw_preview),
			"0x%016llX",
			static_cast<unsigned long long>(value)
		);
		preview = raw_preview;
	}

	return DrawPropertyRow(
		label,
		[&]() {
			bool changed{ false };
			ImGui::SetNextItemWidth(-FLT_MIN);

			if (ImGui::BeginCombo("##LayerMask", preview)) {
				if (ImGui::Selectable("All", value == kLayersAll)) {
					value = kLayersAll;
					changed = true;
				}

				if (ImGui::Selectable("None", value == kLayersNone)) {
					value = kLayersNone;
					changed = true;
				}

				if (ImGui::Selectable("Default", value == kLayerDefault)) {
					value = kLayerDefault;
					changed = true;
				}

				ImGui::Separator();

				if (ImGui::BeginChild(
						"##LayerMaskValues",
						ImVec2{ 0.0f, ImGui::GetTextLineHeightWithSpacing() * 10.0f },
						ImGuiChildFlags_Borders
					)) {
					for (int index{ 0 }; index < 64; ++index) {
						const LayerMask layer{ GetLayer(index) };
						bool selected{ (value & layer) != 0 };
						const std::string layer_label{
							index == 0
								? "Layer 0 (Default)"
								: std::string{ "Layer " } + std::to_string(index)
						};

						ImGui::PushID(index);

						if (ImGui::Checkbox(layer_label.c_str(), &selected)) {
							if (selected) {
								value |= layer;
							} else {
								value &= ~layer;
							}

							changed = true;
						}

						ImGui::PopID();
					}
				}

				ImGui::EndChild();
				ImGui::EndCombo();
			}

			return changed;
		}
	);
}

template <>
struct Contents<::ptgn::impl::RenderMask> {
	static bool Draw(EditorContext&, ::ptgn::impl::RenderMask& mask) {
		return DrawLayerMaskValue(
			"Layers",
			mask.layers
		);
	}
};

template <>
struct Contents<::ptgn::impl::CameraMask> {
	static bool Draw(EditorContext&, ::ptgn::impl::CameraMask& mask) {
		bool changed{ false };
		changed |= DrawLayerMaskValue(
			"Include Layer",
			mask.include
		);
		changed |= DrawLayerMaskValue(
			"Exclude Layer",
			mask.exclude
		);
		return changed;
	}
};

template <>
struct Contents<::ptgn::impl::IDrawable> {
	static bool Draw(EditorContext&, ::ptgn::impl::IDrawable& drawable) {
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
	static bool Draw(EditorContext& ctx, Hollow& hollow) {
		return DrawValue(
			ctx, "Line Width", hollow.line_width,
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
	static bool Draw(EditorContext& ctx, Rect& rect) {
		bool changed{ false };

		auto size{ rect.max - rect.min };

		if (DrawValue(
				ctx, "Size", size,
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

		changed |= DrawValue(ctx, "Min", rect.min);
		changed |= DrawValue(ctx, "Max", rect.max);

		return changed;
	}
};

template <>
struct Contents<TextRun> {
	static bool Draw(EditorContext& ctx, TextRun& run) {
		bool changed{ false };
		changed |= DrawValue(ctx, "Text", run.text, FieldOptions{ .multiline = true });
		changed |= DrawValue(ctx, "Font", run.font);
		changed |= DrawValue(ctx, "Color", run.style.color);
		changed |= DrawValue(ctx, "Size", run.style.size);

		const bool style_open{
			ImGui::TreeNodeEx(
				"Style##TextRunStyle",
				ImGuiTreeNodeFlags_SpanAvailWidth
			)
		};

		if (style_open) {
			{
				ScopedUnindent align_with_style;
				changed |= DrawValue(ctx, "Bold Weight", run.style.bold_weight);
				changed |= DrawValue(ctx, "Kerning", run.style.kerning);
				changed |= DrawValue(ctx, "Tracking", run.style.tracking);
				changed |= DrawValue(ctx, "Line Spacing", run.style.line_spacing);
				changed |= DrawValue(ctx, "Flags", run.style.flags);
				changed |= DrawValue(ctx, "Distance Field", run.style.sdf);
				changed |= DrawValue(ctx, "Effect", run.style.effect);
			}

			ImGui::TreePop();
		}

		return changed;
	}
};

template <>
struct Contents<StyledText> {
	static bool Draw(EditorContext& ctx, StyledText& text) {
		bool changed{ false };

		if (text.runs.empty()) {
			text.runs.emplace_back();
			changed = true;
		}

		changed |= DrawVectorEditor(
			ctx, text.runs,
			VectorOptions{
				.item_name	   = "Text Run",
				.default_open  = true,
				.reorderable   = true,
				.minimum_items = 1,
			}
		);

		return changed;
	}
};

template <>
struct Contents<Polygon> {
	static bool Draw(EditorContext& ctx, Polygon& polygon) {
		return DrawVectorEditor(
			ctx, polygon.vertices,
			VectorOptions{
				.item_name	  = "Vertex",
				.default_open = true,
				.reorderable  = true,
			}
		);
	}
};

template <>
struct Contents<ButtonBorderVisuals> {
	static bool Draw(EditorContext& ctx, ButtonBorderVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(ctx, visuals.states);
	}
};

template <>
struct Contents<ButtonBackgroundVisuals> {
	static bool Draw(EditorContext& ctx, ButtonBackgroundVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(ctx, visuals.states);
	}
};

template <>
struct Contents<ButtonTextVisuals> {
	static bool Draw(EditorContext& ctx, ButtonTextVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(ctx, visuals.states);
	}
};

template <>
struct Contents<ButtonSpriteVisuals> {
	static bool Draw(EditorContext& ctx, ButtonSpriteVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(ctx, visuals.states);
	}
};

template <>
struct Contents<ButtonSounds> {
	static bool Draw(EditorContext& ctx, ButtonSounds& sounds) {
		bool changed{ false };
		changed |= DrawEnumArrayEditor<ButtonVisualState>(ctx, "Sounds", sounds.states);
		changed |= DrawValue(ctx, "Exclusive Audio", sounds.exclusive);
		return changed;
	}
};

namespace {

template <typename T>
using ComponentState = std::optional<T>;

enum class InspectorFeature : std::uint8_t {
	Transform,
	Visual,
	Interaction,
	Physics,
	UI,
	Camera,
	Scripts,
	Utilities,
	Count,
};

template <typename... T>
struct FeatureComponents {};

using TransformFeatureComponents = FeatureComponents<
	Transform, Depth, ::ptgn::impl::IgnoreParentTransform, ::ptgn::impl::IgnoreParentPosition,
	::ptgn::impl::IgnoreParentRotation, ::ptgn::impl::IgnoreParentScale,
	::ptgn::impl::IgnoreParentDepth>;

using VisualFeatureComponents = FeatureComponents<
	::ptgn::impl::IDrawable, Visible, ::ptgn::impl::IgnoreParentVisibility, Origin, BlendMode,
	Rect, Circle, RoundedRect, Polygon, Ellipse, Triangle, Line, Capsule, Arc, Color, FillStyle,
	TextureKey, ::ptgn::impl::TextureSize, ::ptgn::impl::TextureCrop,
	::ptgn::impl::AnimationData, ::ptgn::impl::Offsets, Tint,
	::ptgn::impl::IgnoreParentTint, ::ptgn::impl::TextData,
	::ptgn::impl::ParticleEmitterData, LightData, ::ptgn::impl::ShadowCaster,
	::ptgn::impl::GraphicsData, ::ptgn::Material, ShaderKey, ::ptgn::impl::RenderTargetSize,
	::ptgn::impl::RenderMask, ::ptgn::impl::UILayer, ::ptgn::impl::EffectTag,
	::ptgn::impl::HDREffectTag, EffectMargin, Bloom, Blur, GaussianBlur,
	::ptgn::impl::ClearColor, ::ptgn::impl::ClearDepth, ::ptgn::impl::ClearStencil>;

using InteractionFeatureComponents = FeatureComponents<
	::ptgn::impl::Interactive, ::ptgn::impl::Draggable, ::ptgn::impl::Dropzone,
	InteractionLock, ::ptgn::impl::InteractiveTag>;

using PhysicsFeatureComponents = FeatureComponents<
	Collider, RigidBody, ::ptgn::impl::IgnoreParentImmovable, BoundaryBehavior,
	TopDownMovement, PlatformerMovement, PlatformerJump>;

using UIFeatureComponents = FeatureComponents<
	::ptgn::impl::ButtonData, ::ptgn::impl::ButtonAnimationPart, ButtonBackgroundVisuals,
	ButtonBorderVisuals, ButtonSpriteVisuals, ButtonTextVisuals, ButtonSounds,
	::ptgn::impl::ToggleButtonData, ::ptgn::impl::ToggleButtonGroupData,
	::ptgn::impl::ToggleButtonGroupItem, ::ptgn::impl::DropdownData,
	::ptgn::impl::DropdownItem, ::ptgn::impl::TooltipData,
	::ptgn::impl::TooltipHoverData, ::ptgn::impl::TooltipBackgroundPart,
	::ptgn::impl::TooltipTextPart>;

using CameraFeatureComponents =
	FeatureComponents<::ptgn::impl::CameraData, ::ptgn::impl::CameraMask>;

using ScriptsFeatureComponents = FeatureComponents<::ptgn::impl::Scripts>;

using UtilitiesFeatureComponents = FeatureComponents<Lifetime>;

struct FeatureTargetKey {
	const Scene* scene{ nullptr };
	std::optional<UUID> entity;
	std::optional<PrefabKey> prefab;

	bool operator==(const FeatureTargetKey&) const = default;
};

struct ManualFeatureState {
	FeatureTargetKey target;
	std::array<bool, static_cast<std::size_t>(InspectorFeature::Count)> features{};
	bool text_box_state_initialized{ false };
	bool text_box_enabled{ false };
	bool scale_ratio_locked{ true };
	std::optional<ButtonVisualState> button_visual_state;
};

std::vector<ManualFeatureState>& ManualFeatureStates() {
	static std::vector<ManualFeatureState> states;
	return states;
}

ManualFeatureState& GetManualFeatureState(const FeatureTargetKey& target) {
	auto& states{ ManualFeatureStates() };
	const auto it{ std::ranges::find(states, target, &ManualFeatureState::target) };

	if (it != states.end()) {
		return *it;
	}

	states.push_back(ManualFeatureState{ .target = target });
	return states.back();
}

enum class ButtonChildPart : std::uint8_t {
	Background,
	Border,
	Text,
	Sprite,
};

struct ButtonChildInfo {
	Entity child;
	Entity button;
	ButtonChildPart part{ ButtonChildPart::Background };
};

[[nodiscard]] bool IsButtonRoot(Entity entity) {
	return entity &&
		(
			entity.Has<::ptgn::impl::ButtonData>() ||
			entity.Has<::ptgn::impl::ToggleButtonData>() ||
			entity.Has<::ptgn::impl::DropdownData>()
		);
}

template <typename Target>
[[nodiscard]] std::optional<ButtonChildInfo> GetButtonChildInfo(
	const Target& target
) {
	if constexpr (!requires { target.entity; }) {
		return std::nullopt;
	} else {
		Entity child{ target.entity };

		if (!child) {
			return std::nullopt;
		}

		Entity button{ GetParent(child) };

		if (!IsButtonRoot(button)) {
			return std::nullopt;
		}

		if (child.Has<ButtonBackgroundVisuals>()) {
			return ButtonChildInfo{
				.child = child,
				.button = button,
				.part = ButtonChildPart::Background,
			};
		}

		if (child.Has<ButtonBorderVisuals>()) {
			return ButtonChildInfo{
				.child = child,
				.button = button,
				.part = ButtonChildPart::Border,
			};
		}

		if (child.Has<ButtonTextVisuals>()) {
			return ButtonChildInfo{
				.child = child,
				.button = button,
				.part = ButtonChildPart::Text,
			};
		}

		if (child.Has<ButtonSpriteVisuals>()) {
			return ButtonChildInfo{
				.child = child,
				.button = button,
				.part = ButtonChildPart::Sprite,
			};
		}

		return std::nullopt;
	}
}

template <typename Target>
[[nodiscard]] std::optional<ButtonVisualState> GetButtonVisualEditState(
	const Target& target
) {
	if (!GetButtonChildInfo(target)) {
		return std::nullopt;
	}

	return GetManualFeatureState(
		target.GetFeatureTargetKey()
	).button_visual_state;
}

[[nodiscard]] std::span<const ButtonVisualState> GetButtonVisualStateFallbacks(
	ButtonVisualState state
) {
	using enum ButtonVisualState;

	static constexpr std::array idle{ Idle };
	static constexpr std::array hover{ Hover, Idle };
	static constexpr std::array press{ Press, Hover, Idle };
	static constexpr std::array disabled{ Disabled, Idle };
	static constexpr std::array disabled_hover{ DisabledHover, Disabled, Hover, Idle };
	static constexpr std::array disabled_press{
		DisabledPress, DisabledHover, Disabled, Press, Hover, Idle
	};
	static constexpr std::array toggled{ Toggled, Idle };
	static constexpr std::array toggled_hover{ ToggledHover, Toggled, Hover, Idle };
	static constexpr std::array toggled_press{
		ToggledPress, ToggledHover, Toggled, Press, Hover, Idle
	};

	switch (state) {
		case Idle: return idle;
		case Hover: return hover;
		case Press: return press;
		case Disabled: return disabled;
		case DisabledHover: return disabled_hover;
		case DisabledPress: return disabled_press;
		case Toggled: return toggled;
		case ToggledHover: return toggled_hover;
		case ToggledPress: return toggled_press;
	}

	return idle;
}

template <typename Visual, typename T, std::size_t N>
[[nodiscard]] std::optional<T> ResolveButtonVisualProperty(
	const std::array<Visual, N>& states,
	ButtonVisualState state,
	const std::optional<T> Visual::* member
) {
	for (const ButtonVisualState fallback : GetButtonVisualStateFallbacks(state)) {
		const auto& visual{
			states[static_cast<std::size_t>(std::to_underlying(fallback))]
		};

		if (!visual.defined) {
			continue;
		}

		const auto& value{ visual.*member };

		if (value) {
			return value;
		}
	}

	return std::nullopt;
}

template <typename Visual, typename T, std::size_t N>
bool DrawButtonVisualOverrideValue(
	EditorContext& ctx,
	std::string_view label,
	std::array<Visual, N>& states,
	ButtonVisualState state,
	std::optional<T> Visual::* member
) {
	auto& visual{
		states[static_cast<std::size_t>(std::to_underlying(state))]
	};
	auto& value{ visual.*member };
	const bool had_override{ value.has_value() };
	const std::optional<T> inherited{
		ResolveButtonVisualProperty(states, state, member)
	};
	const bool changed{ DrawValue(ctx, label, value) };

	if (changed && !had_override && value && inherited) {
		value = *inherited;
	}

	return changed;
}

[[nodiscard]] bool HasButtonVisualOverrides(const ButtonShapeVisual& visual) {
	return visual.size || visual.origin || visual.anchor || visual.transform ||
		visual.color || visual.fill_style;
}

[[nodiscard]] bool HasButtonVisualOverrides(const ButtonTextVisual& visual) {
	return visual.styled_text || visual.box || visual.origin || visual.anchor ||
		visual.transform || visual.auto_box || visual.padding;
}

[[nodiscard]] bool HasButtonVisualOverrides(const ButtonSpriteVisual& visual) {
	return visual.texture || visual.origin || visual.anchor || visual.transform ||
		visual.size || visual.tint || visual.animation || visual.animation_options;
}

[[nodiscard]] Rect GetButtonInspectorLocalRect(Entity button_entity) {
	Button button{ button_entity };
	V2_float size;

	std::visit(
		[&size]<typename T>(const T& value) {
			if constexpr (std::same_as<T, V2_float>) {
				size = value;
			} else {
				size = V2_float{ value * 2.0f };
			}
		},
		button.GetSize()
	);

	return Rect{ size, button.GetOrDefault<Origin>() };
}

[[nodiscard]] bool IsFeatureManuallyAdded(
	const FeatureTargetKey& target, InspectorFeature feature
) {
	const auto& states{ ManualFeatureStates() };
	const auto it{ std::ranges::find(states, target, &ManualFeatureState::target) };

	if (it == states.end()) {
		return false;
	}

	return it->features[static_cast<std::size_t>(feature)];
}

void SetFeatureManuallyAdded(const FeatureTargetKey& target, InspectorFeature feature, bool added) {
	auto& state{ GetManualFeatureState(target) };
	state.features[static_cast<std::size_t>(feature)] = added;

	if (!std::ranges::any_of(state.features, std::identity{})) {
		std::erase_if(ManualFeatureStates(), [&target](const ManualFeatureState& candidate) {
			return candidate.target == target;
		});
	}
}

template <typename T>
void AssignEntityComponent(Entity entity, ComponentState<T> state) {
	if (!state) {
		if (entity.Has<T>()) {
			entity.Remove<T>();
		}
		return;
	}

	if constexpr (std::is_empty_v<T>) {
		entity.TryAdd<T>();
	} else if (entity.Has<T>()) {
		entity.Get<T>() = *state;
	} else {
		entity.Add<T>(*state);
	}
}

template <typename T>
ComponentState<T> CapturePrefabComponent(const PrefabEntity& prefab) {
	const auto* registration{ ComponentRegistry::Find<T>() };

	if (!registration) {
		return std::nullopt;
	}

	if constexpr (std::is_empty_v<T>) {
		return std::ranges::contains(prefab.tags, std::string{ registration->name })
				 ? ComponentState<T>{ T{} }
				 : std::nullopt;
	} else {
		auto it{ prefab.components.find(std::string{ registration->name }) };

		if (it == prefab.components.end()) {
			return std::nullopt;
		}

		if constexpr (JsonDeserializable<T> && std::default_initializable<T>) {
			T value{};
			try {
				it->second.get_to(value);
				return value;
			} catch (...) {
				return std::nullopt;
			}
		} else if constexpr (::ptgn::impl::JsonGettable<T>) {
			try {
				return it->second.template get<T>();
			} catch (...) {
				return std::nullopt;
			}
		} else {
			return std::nullopt;
		}
	}
}

template <typename T>
void AssignPrefabComponent(PrefabEntity& prefab, ComponentState<T> state) {
	const auto* registration{ ComponentRegistry::Find<T>() };

	if (!registration) {
		return;
	}

	const std::string name{ registration->name };

	if (!state) {
		if constexpr (std::is_empty_v<T>) {
			std::erase(prefab.tags, name);
		} else {
			prefab.components.erase(name);
		}
		return;
	}

	if constexpr (std::is_empty_v<T>) {
		if (!std::ranges::contains(prefab.tags, name)) {
			prefab.tags.push_back(name);
		}
	} else if constexpr (JsonSerializable<T>) {
		json value = *state;
		prefab.components.insert_or_assign(name, std::move(value));
	}
}

template <typename Callback>
void InvokeEntityChanged(Callback callback, Entity entity) {
	if constexpr (!std::same_as<Callback, std::nullptr_t>) {
		if (callback) {
			callback(entity);
		}
	}
}

struct EntityInspectorTarget {
	EditorContext& ctx;
	Entity entity;

	template <typename T>
	[[nodiscard]] static constexpr bool Supports() {
		return true;
	}

	[[nodiscard]] const void* Id() const {
		return std::addressof(entity.Get<UUID>());
	}

	[[nodiscard]] FeatureTargetKey GetFeatureTargetKey() const {
		return FeatureTargetKey{
			.scene	= std::addressof(entity.GetScene()),
			.entity = entity.Get<UUID>(),
		};
	}

	template <typename T>
	[[nodiscard]] ComponentState<T> Capture() const {
		if (!entity.Has<T>()) {
			return std::nullopt;
		}

		if constexpr (std::is_empty_v<T>) {
			return T{};
		} else {
			return entity.Get<T>();
		}
	}

	template <typename T, typename Callback = std::nullptr_t>
	void SetLive(ComponentState<T> state, Callback callback = nullptr) {
		AssignEntityComponent<T>(entity, std::move(state));
		InvokeEntityChanged(callback, entity);
	}

	template <typename T, typename Callback = std::nullptr_t>
	auto MakeApply(Callback callback = nullptr) const {
		Editor* editor{ std::addressof(ctx.editor) };
		const EntityReference reference{ MakeEntityReference(entity) };

		return [editor, reference, callback](ComponentState<T> state) mutable {
			Entity resolved{ reference.Resolve(*editor) };

			if (!resolved) {
				return;
			}

			AssignEntityComponent<T>(resolved, std::move(state));
			InvokeEntityChanged(callback, resolved);
		};
	}

	[[nodiscard]] std::string GetName() const {
		return std::string{ entity.Get<Tag>() };
	}

	[[nodiscard]] std::string GetUUIDText() const {
		const auto& uuid{ entity.Get<UUID>() };

		return [&]<typename T>(const T& value) {
			if constexpr (JsonSerializable<T>) {
				json serialized = value;

				if (serialized.is_string()) {
					return serialized.template get<std::string>();
				}

				return serialized.dump();
			} else if constexpr (requires { std::to_string(value.value); }) {
				return std::to_string(value.value);
			} else if constexpr (requires { std::to_string(value.value()); }) {
				return std::to_string(value.value());
			} else {
				return std::to_string(Hash(value));
			}
		}(uuid);
	}

	void SetName(std::string name) {
		entity.Add<Tag>(std::move(name));
	}

	auto MakeNameApply() const {
		Editor* editor{ std::addressof(ctx.editor) };
		const EntityReference reference{ MakeEntityReference(entity) };

		return [editor, reference](std::string name) {
			Entity resolved{ reference.Resolve(*editor) };

			if (resolved) {
				resolved.Add<Tag>(std::move(name));
			}
		};
	}
};

struct PrefabInspectorTarget {
	EditorContext& ctx;
	PrefabKey key;
	PrefabEntity& prefab;

	template <typename T>
	[[nodiscard]] static constexpr bool Supports() {
		return std::is_empty_v<T> ||
			   (JsonSerializable<T> && JsonDeserializable<T> &&
				(std::default_initializable<T> || ::ptgn::impl::JsonGettable<T>));
	}

	[[nodiscard]] const void* Id() const {
		return std::addressof(prefab);
	}

	[[nodiscard]] FeatureTargetKey GetFeatureTargetKey() const {
		return FeatureTargetKey{
			.prefab = key,
		};
	}

	template <typename T>
	[[nodiscard]] ComponentState<T> Capture() const {
		return CapturePrefabComponent<T>(prefab);
	}

	template <typename T, typename Callback = std::nullptr_t>
	void SetLive(ComponentState<T> state, Callback = nullptr) {
		AssignPrefabComponent<T>(prefab, std::move(state));
	}

	template <typename T, typename Callback = std::nullptr_t>
	auto MakeApply(Callback = nullptr) const {
		EditorContext* context{ std::addressof(ctx) };
		PrefabKey prefab_key{ key };

		return [context, prefab_key](ComponentState<T> state) mutable {
			auto& assets{ context->editor.GetAssetManager() };

			if (!assets.Has(prefab_key)) {
				return;
			}

			auto prefab_asset{ ::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(prefab_key) };

			AssignPrefabComponent<T>(prefab_asset.get().root, std::move(state));

			assets.SavePrefab(prefab_key);
			context->local.state.is_dirty = true;
		};
	}

	[[nodiscard]] std::string GetName() const {
		return prefab.tag;
	}

	void SetName(std::string name) {
		prefab.tag = std::move(name);
	}

	auto MakeNameApply() const {
		EditorContext* context{ std::addressof(ctx) };
		PrefabKey prefab_key{ key };

		return [context, prefab_key](std::string name) {
			auto& assets{ context->editor.GetAssetManager() };

			if (!assets.Has(prefab_key)) {
				return;
			}

			auto prefab_asset{ ::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(prefab_key) };

			prefab_asset.get().root.tag = std::move(name);
			assets.SavePrefab(prefab_key);
			context->local.state.is_dirty = true;
		};
	}
};

template <typename Target, typename T, typename Callback>
void TrackComponentState(
	Target& target, std::string_view label, ComponentState<T> before, ComponentState<T> after,
	bool changed, Callback callback
) {
	if (!changed) {
		return;
	}

	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<T>()) };
	const ImGuiID key{ ImGui::GetID("##ComponentEdit") };

	auto apply{ target.template MakeApply<T>(callback) };

	TrackUndoableInteraction(
		target.ctx, key, std::string{ label }, true, [apply, before]() mutable { apply(before); },
		[apply, after]() mutable { apply(after); }
	);
}

template <typename Target, typename T>
void TrackComponentState(
	Target& target, std::string_view label, ComponentState<T> before, ComponentState<T> after,
	bool changed
) {
	TrackComponentState(target, label, std::move(before), std::move(after), changed, nullptr);
}

template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawRequiredComponent(
	Target& target, std::string_view label, bool tree, Draw&& draw, Callback callback = nullptr
) {
	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<T>()) };

	auto before{ target.template Capture<T>() };
	bool changed{ false };

	if (!before) {
		target.template SetLive<T>(T{}, callback);
		changed = true;
	}

	T value{ target.template Capture<T>().value_or(T{}) };

	if (tree) {
		const std::string node_label{ std::string{ label } + "##Tree" };

		if (ImGui::TreeNodeEx(node_label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) {
			ScopedIndent indent;
			changed |= std::invoke(std::forward<Draw>(draw), value);
			ImGui::TreePop();
		}
	} else {
		changed |= std::invoke(std::forward<Draw>(draw), value);
	}

	if (changed) {
		target.template SetLive<T>(value, callback);
	}

	auto after{ target.template Capture<T>() };

	TrackComponentState(
		target, std::string{ "Edit " } + std::string{ label }, std::move(before), std::move(after),
		changed, callback
	);

	return changed;
}

template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawOptionalComponent(
	Target& target, std::string_view label, bool tree, Draw&& draw, bool contents_read_only = false,
	bool toggle_read_only = false, Callback callback = nullptr
) {
	if (!Target::template Supports<T>()) {
		return false;
	}

	if (
		(contents_read_only || toggle_read_only) &&
		!target.ctx.local.settings.show_read_only_inspector_data
	) {
		return false;
	}

	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<T>()) };

	auto before{ target.template Capture<T>() };
	bool enabled{ before.has_value() };
	bool changed{ false };

	{
		ScopedDisabled disabled{ toggle_read_only };

		if (ImGui::Checkbox("##Enabled", &enabled)) {
			if (enabled) {
				target.template SetLive<T>(T{}, callback);
			} else {
				target.template SetLive<T>(std::nullopt, callback);
			}

			changed = true;
		}
	}

	ImGui::SameLine();

	T value{ target.template Capture<T>().value_or(T{}) };

	if constexpr (std::is_empty_v<T>) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label.data(), label.data() + label.size());
	} else if (tree) {
		const std::string node_label{ std::string{ label } + "##Tree" };

		const bool open{ ImGui::TreeNodeEx(node_label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth) };

		if (open) {
			ScopedIndent indent;
			ScopedDisabled disabled{ !enabled || contents_read_only };

			const bool contents_changed{ std::invoke(std::forward<Draw>(draw), value) };

			if (enabled && !contents_read_only && contents_changed) {
				target.template SetLive<T>(value, callback);
				changed = true;
			}

			ImGui::TreePop();
		}
	} else {
		ScopedDisabled disabled{ !enabled || contents_read_only };

		const bool contents_changed{ std::invoke(std::forward<Draw>(draw), value) };

		if (enabled && !contents_read_only && contents_changed) {
			target.template SetLive<T>(value, callback);
			changed = true;
		}
	}

	auto after{ target.template Capture<T>() };

	TrackComponentState(
		target, std::string{ enabled ? "Edit " : "Disable " } + std::string{ label },
		std::move(before), std::move(after), changed, callback
	);

	return changed;
}

template <typename Target, typename T, typename Draw>
bool DrawReadOnlyExistingComponent(
	Target& target,
	std::string_view label,
	bool tree,
	Draw&& draw
) {
	if (
		!Target::template Supports<T>() ||
		!target.ctx.local.settings.show_read_only_inspector_data
	) {
		return false;
	}

	auto state{ target.template Capture<T>() };

	if (!state) {
		return false;
	}

	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<T>()) };

	if (tree) {
		const std::string node_label{ std::string{ label } + "##ReadOnlyTree" };

		if (ImGui::TreeNodeEx(
				node_label.c_str(),
				ImGuiTreeNodeFlags_SpanAvailWidth
			)) {
			ScopedIndent indent;
			ScopedDisabled disabled{ true };
			(void)std::invoke(
				std::forward<Draw>(draw),
				*state
			);
			ImGui::TreePop();
		}
	} else {
		ScopedDisabled disabled{ true };
		(void)std::invoke(
			std::forward<Draw>(draw),
			*state
		);
	}

	return false;
}

template <typename Target, typename T>
bool DrawReadOnlyExistingReflected(
	Target& target,
	std::string_view label,
	bool tree = true
) {
	return DrawReadOnlyExistingComponent<Target, T>(
		target,
		label,
		tree,
		[&target](T& value) {
			return DrawComponentContents(
				target.ctx,
				value
			);
		}
	);
}

template <typename Target, typename T>
bool DrawOptionalReflected(
	Target& target, std::string_view label, bool tree = true, bool contents_read_only = false,
	bool toggle_read_only = false
) {
	if constexpr (std::is_empty_v<T>) {
		return DrawOptionalComponent<Target, T>(
			target, label, false, [](T&) { return false; }, contents_read_only, toggle_read_only
		);
	} else {
		return DrawOptionalComponent<Target, T>(
			target, label, tree,
			[&target](T& value) { return DrawComponentContents(target.ctx, value); },
			contents_read_only, toggle_read_only
		);
	}
}

template <typename Target, typename T>
bool DrawOptionalValue(Target& target, std::string_view label, FieldOptions options = {}) {
	return DrawOptionalComponent<Target, T>(target, label, false, [&](T& value) {
		return DrawValue(target.ctx, label, value, options);
	});
}

template <typename Target, typename T>
bool AddFeature(Target& target, std::string_view label) {
	auto before{ target.template Capture<T>() };

	if (before) {
		return false;
	}

	target.template SetLive<T>(T{});
	auto after{ target.template Capture<T>() };

	TrackComponentState(
		target, std::string{ "Add " } + std::string{ label }, std::move(before), std::move(after),
		true
	);

	return true;
}

template <typename Target, typename T>
[[nodiscard]] ComponentState<T> CaptureSupportedFeatureComponent(const Target& target) {
	if constexpr (Target::template Supports<T>()) {
		return target.template Capture<T>();
	} else {
		return std::nullopt;
	}
}

template <typename... T>
struct InspectorFeatureState {
	bool manually_added{ false };
	std::tuple<ComponentState<T>...> components;
};

template <typename Target, typename... T>
[[nodiscard]] InspectorFeatureState<T...> CaptureInspectorFeatureState(
	const Target& target, InspectorFeature feature, FeatureComponents<T...>
) {
	return InspectorFeatureState<T...>{
		.manually_added = IsFeatureManuallyAdded(target.GetFeatureTargetKey(), feature),
		.components		= std::tuple{ CaptureSupportedFeatureComponent<Target, T>(target)... },
	};
}

template <typename Target, typename T>
auto MakeSupportedFeatureComponentApply(Target& target) {
	if constexpr (Target::template Supports<T>()) {
		return target.template MakeApply<T>();
	} else {
		return [](ComponentState<T>) {
		};
	}
}

template <typename ApplyTuple, typename StateTuple, std::size_t... I>
void ApplyFeatureComponentStates(ApplyTuple& apply, StateTuple&& state, std::index_sequence<I...>) {
	(std::invoke(std::get<I>(apply), std::get<I>(std::forward<StateTuple>(state))), ...);
}

template <typename Target, typename... T>
auto MakeInspectorFeatureApply(Target& target, InspectorFeature feature, FeatureComponents<T...>) {
	const FeatureTargetKey target_key{ target.GetFeatureTargetKey() };
	auto component_apply{ std::tuple{ MakeSupportedFeatureComponentApply<Target, T>(target)... } };

	return
		[target_key, feature,
		 component_apply = std::move(component_apply)](InspectorFeatureState<T...> state) mutable {
			SetFeatureManuallyAdded(target_key, feature, state.manually_added);

			ApplyFeatureComponentStates(
				component_apply, std::move(state.components), std::index_sequence_for<T...>{}
			);
		};
}

template <typename Target, typename T>
void RemoveSupportedFeatureComponent(Target& target) {
	if constexpr (Target::template Supports<T>()) {
		target.template SetLive<T>(std::nullopt);
	}
}

template <typename Target, typename... T>
void TrackInspectorFeatureState(
	Target& target, InspectorFeature feature, std::string label, InspectorFeatureState<T...> before,
	InspectorFeatureState<T...> after, FeatureComponents<T...> components
) {
	auto apply{ MakeInspectorFeatureApply(target, feature, components) };

	target.ctx.undo.PushApplied(
			std::move(label), [apply, before]() mutable { apply(before); },
			[apply, after]() mutable { apply(after); }
		);
}

template <typename Target, typename... T>
bool AddInspectorFeature(
	Target& target, InspectorFeature feature, std::string_view label,
	FeatureComponents<T...> components
) {
	auto before{ CaptureInspectorFeatureState(target, feature, components) };

	if (before.manually_added) {
		return false;
	}

	SetFeatureManuallyAdded(target.GetFeatureTargetKey(), feature, true);

	auto after{ CaptureInspectorFeatureState(target, feature, components) };

	TrackInspectorFeatureState(
		target, feature, std::string{ "Add " } + std::string{ label } + " Feature",
		std::move(before), std::move(after), components
	);

	return true;
}

template <typename Default, typename Target, typename... T>
bool AddInspectorFeatureWithDefault(
	Target& target, InspectorFeature feature, std::string_view label,
	FeatureComponents<T...> components
) {
	auto before{ CaptureInspectorFeatureState(target, feature, components) };

	SetFeatureManuallyAdded(target.GetFeatureTargetKey(), feature, true);

	if constexpr (Target::template Supports<Default>()) {
		if (!target.template Capture<Default>()) {
			target.template SetLive<Default>(Default{});
		}
	}

	auto after{ CaptureInspectorFeatureState(target, feature, components) };

	TrackInspectorFeatureState(
		target, feature, std::string{ "Add " } + std::string{ label } + " Feature",
		std::move(before), std::move(after), components
	);

	return true;
}

template <typename Target, typename... T>
bool DeleteInspectorFeature(
	Target& target, InspectorFeature feature, std::string_view label,
	FeatureComponents<T...> components
) {
	auto before{ CaptureInspectorFeatureState(target, feature, components) };

	SetFeatureManuallyAdded(target.GetFeatureTargetKey(), feature, false);
	(RemoveSupportedFeatureComponent<Target, T>(target), ...);

	auto after{ CaptureInspectorFeatureState(target, feature, components) };

	TrackInspectorFeatureState(
		target, feature, std::string{ "Delete " } + std::string{ label } + " Feature",
		std::move(before), std::move(after), components
	);

	return true;
}

struct FeatureHeaderResult {
	bool open{ false };
	bool changed{ false };
};

template <typename Target, typename... T>
FeatureHeaderResult DrawFeatureHeader(
	Target& target, InspectorFeature feature, std::string_view label, ImGuiTreeNodeFlags flags,
	FeatureComponents<T...> components
) {
	ScopedID feature_scope{ static_cast<int>(feature) };
	const std::string header_label{ std::string{ label } + "##FeatureHeader" };

	FeatureHeaderResult result{
		.open = ImGui::CollapsingHeader(header_label.c_str(), flags),
	};

	if (ImGui::BeginPopupContextItem("##FeatureContext")) {
		if (ImGui::MenuItem("Delete Feature")) {
			result.changed = DeleteInspectorFeature(target, feature, label, components);
			result.open	   = false;
		}

		ImGui::EndPopup();
	}

	return result;
}

template <typename Target, typename T>
bool DrawIgnoreCheckbox(Target& target, std::string_view tooltip) {
	auto before{ target.template Capture<T>() };
	bool enabled{ before.has_value() };

	if (!ImGui::Checkbox("##IgnoreParent", &enabled)) {
		return false;
	}

	target.template SetLive<T>(enabled ? ComponentState<T>{ T{} } : std::nullopt);

	auto after{ target.template Capture<T>() };

	TrackComponentState(
		target, enabled ? "Ignore Parent" : "Inherit From Parent", std::move(before),
		std::move(after), true
	);

	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
	}

	return true;
}

template <typename Target>
bool DrawName(Target& target) {
	const std::string before{ target.GetName() };
	std::string value{ before };

	const bool changed{
		DrawPropertyRow(
			"Tag",
			[&]() {
				ImGui::SetNextItemWidth(-FLT_MIN);
				return ImGui::InputText("##Tag", &value);
			}
		)
	};

	if (changed) {
		target.SetName(value);

		ScopedID target_scope{ target.Id() };
		const ImGuiID key{ ImGui::GetID("##NameEdit") };
		auto apply{ target.MakeNameApply() };

		TrackUndoableInteraction(
			target.ctx,
			key,
			"Rename Entity",
			true,
			[apply, before]() mutable {
				apply(before);
			},
			[apply, value]() mutable {
				apply(value);
			}
		);
	}

	if constexpr (requires { target.GetUUIDText(); }) {
		const std::string uuid{ target.GetUUIDText() };

		DrawPropertyRow(
			"UUID",
			[&]() {
				ImGui::SetNextItemWidth(-FLT_MIN);
				std::string displayed{ uuid };
				ScopedDisabled read_only{ true };
				(void)ImGui::InputText(
					"##UUID",
					&displayed,
					ImGuiInputTextFlags_ReadOnly
				);
				DrawTooltip("Read only entity identifier.");
				return false;
			}
		);
	}

	return changed;
}

template <typename Target>
struct TransformFeatureState {
	Transform transform;
	Depth depth;
	ComponentState<ButtonBackgroundVisuals> button_backgrounds;
	ComponentState<ButtonBorderVisuals> button_borders;
	ComponentState<ButtonTextVisuals> button_texts;
	ComponentState<ButtonSpriteVisuals> button_sprites;
	bool ignore_position{ false };
	bool ignore_rotation{ false };
	bool ignore_scale{ false };
	bool ignore_depth{ false };
	bool ignore_transform{ false };
};

template <typename Target>
TransformFeatureState<Target> CaptureTransformFeature(const Target& target) {
	const bool ignore_transform{
		target.template Capture<::ptgn::impl::IgnoreParentTransform>().has_value()
	};

	return TransformFeatureState<Target>{
		.transform = target.template Capture<Transform>().value_or(Transform{}),
		.depth	   = target.template Capture<Depth>().value_or(Depth{}),
		.button_backgrounds = target.template Capture<ButtonBackgroundVisuals>(),
		.button_borders = target.template Capture<ButtonBorderVisuals>(),
		.button_texts = target.template Capture<ButtonTextVisuals>(),
		.button_sprites = target.template Capture<ButtonSpriteVisuals>(),
		.ignore_position =
			ignore_transform ||
			target.template Capture<::ptgn::impl::IgnoreParentPosition>().has_value(),
		.ignore_rotation =
			ignore_transform ||
			target.template Capture<::ptgn::impl::IgnoreParentRotation>().has_value(),
		.ignore_scale	  = ignore_transform ||
							target.template Capture<::ptgn::impl::IgnoreParentScale>().has_value(),
		.ignore_depth	  = ignore_transform ||
							target.template Capture<::ptgn::impl::IgnoreParentDepth>().has_value(),
		.ignore_transform = ignore_transform,
	};
}

template <typename Target>
auto MakeTransformFeatureApply(Target& target) {
	auto apply_transform{ target.template MakeApply<Transform>() };
	auto apply_depth{ target.template MakeApply<Depth>() };
	auto apply_position{ target.template MakeApply<::ptgn::impl::IgnoreParentPosition>() };
	auto apply_rotation{ target.template MakeApply<::ptgn::impl::IgnoreParentRotation>() };
	auto apply_scale{ target.template MakeApply<::ptgn::impl::IgnoreParentScale>() };
	auto apply_depth_ignore{ target.template MakeApply<::ptgn::impl::IgnoreParentDepth>() };
	auto apply_transform_ignore{ target.template MakeApply<::ptgn::impl::IgnoreParentTransform>() };
	auto apply_backgrounds{
		target.template MakeApply<ButtonBackgroundVisuals>(&MarkButtonBackgroundDirty)
	};
	auto apply_borders{
		target.template MakeApply<ButtonBorderVisuals>(&MarkButtonBorderDirty)
	};
	auto apply_texts{
		target.template MakeApply<ButtonTextVisuals>(&MarkButtonTextDirty)
	};
	auto apply_sprites{
		target.template MakeApply<ButtonSpriteVisuals>(&MarkButtonSpriteDirty)
	};

	return [apply_transform, apply_depth, apply_position, apply_rotation, apply_scale,
			apply_depth_ignore, apply_transform_ignore, apply_backgrounds, apply_borders,
			apply_texts, apply_sprites](TransformFeatureState<Target> state) mutable {
		apply_transform(state.transform);
		apply_depth(state.depth);
		apply_backgrounds(state.button_backgrounds);
		apply_borders(state.button_borders);
		apply_texts(state.button_texts);
		apply_sprites(state.button_sprites);
		apply_transform_ignore(
			state.ignore_transform
				? ComponentState<
					  ::ptgn::impl::IgnoreParentTransform>{ ::ptgn::impl::IgnoreParentTransform{} }
				: std::nullopt
		);
		apply_position(
			state.ignore_position
				? ComponentState<
					  ::ptgn::impl::IgnoreParentPosition>{ ::ptgn::impl::IgnoreParentPosition{} }
				: std::nullopt
		);
		apply_rotation(
			state.ignore_rotation
				? ComponentState<
					  ::ptgn::impl::IgnoreParentRotation>{ ::ptgn::impl::IgnoreParentRotation{} }
				: std::nullopt
		);
		apply_scale(
			state.ignore_scale
				? ComponentState<
					  ::ptgn::impl::IgnoreParentScale>{ ::ptgn::impl::IgnoreParentScale{} }
				: std::nullopt
		);
		apply_depth_ignore(
			state.ignore_depth
				? ComponentState<
					  ::ptgn::impl::IgnoreParentDepth>{ ::ptgn::impl::IgnoreParentDepth{} }
				: std::nullopt
		);
	};
}

template <typename Target>
void SetTransformFeatureLive(Target& target, const TransformFeatureState<Target>& state) {
	target.template SetLive<Transform>(state.transform);
	target.template SetLive<Depth>(state.depth);
	target.template SetLive<ButtonBackgroundVisuals>(
		state.button_backgrounds,
		&MarkButtonBackgroundDirty
	);
	target.template SetLive<ButtonBorderVisuals>(
		state.button_borders,
		&MarkButtonBorderDirty
	);
	target.template SetLive<ButtonTextVisuals>(
		state.button_texts,
		&MarkButtonTextDirty
	);
	target.template SetLive<ButtonSpriteVisuals>(
		state.button_sprites,
		&MarkButtonSpriteDirty
	);
	target.template SetLive<::ptgn::impl::IgnoreParentTransform>(
		state.ignore_transform
			? ComponentState<
				  ::ptgn::impl::IgnoreParentTransform>{ ::ptgn::impl::IgnoreParentTransform{} }
			: std::nullopt
	);
	target.template SetLive<::ptgn::impl::IgnoreParentPosition>(
		state.ignore_position
			? ComponentState<
				  ::ptgn::impl::IgnoreParentPosition>{ ::ptgn::impl::IgnoreParentPosition{} }
			: std::nullopt
	);
	target.template SetLive<::ptgn::impl::IgnoreParentRotation>(
		state.ignore_rotation
			? ComponentState<
				  ::ptgn::impl::IgnoreParentRotation>{ ::ptgn::impl::IgnoreParentRotation{} }
			: std::nullopt
	);
	target.template SetLive<::ptgn::impl::IgnoreParentScale>(
		state.ignore_scale
			? ComponentState<::ptgn::impl::IgnoreParentScale>{ ::ptgn::impl::IgnoreParentScale{} }
			: std::nullopt
	);
	target.template SetLive<::ptgn::impl::IgnoreParentDepth>(
		state.ignore_depth
			? ComponentState<::ptgn::impl::IgnoreParentDepth>{ ::ptgn::impl::IgnoreParentDepth{} }
			: std::nullopt
	);
}

template <typename Visuals>
void ApplyButtonVisualTransformDelta(
	ComponentState<Visuals>& visuals,
	std::optional<ButtonVisualState> selected_state,
	const Transform& before,
	const Transform& after
) {
	if (!visuals || !selected_state || before == after) {
		return;
	}

	const auto index{
		static_cast<std::size_t>(
			std::to_underlying(*selected_state)
		)
	};
	auto& visual{ visuals->states[index] };

	if (!visual.transform) {
		return;
	}

	auto& transform{ *visual.transform };
	transform.position += after.position - before.position;
	transform.rotation = Radians{
		transform.rotation.value +
		after.rotation.value -
		before.rotation.value
	};

	constexpr float epsilon{ 0.000001f };

	if (std::abs(before.scale.x) > epsilon) {
		transform.scale.x *= after.scale.x / before.scale.x;
	}

	if (std::abs(before.scale.y) > epsilon) {
		transform.scale.y *= after.scale.y / before.scale.y;
	}

	transform.ClampScale();
}

template <typename Target>
void ApplyButtonVisualTransformDelta(
	TransformFeatureState<Target>& state,
	std::optional<ButtonVisualState> selected_state,
	const Transform& before
) {
	ApplyButtonVisualTransformDelta(
		state.button_backgrounds,
		selected_state,
		before,
		state.transform
	);
	ApplyButtonVisualTransformDelta(
		state.button_borders,
		selected_state,
		before,
		state.transform
	);
	ApplyButtonVisualTransformDelta(
		state.button_texts,
		selected_state,
		before,
		state.transform
	);
	ApplyButtonVisualTransformDelta(
		state.button_sprites,
		selected_state,
		before,
		state.transform
	);
}

template <typename Target>
[[nodiscard]] std::optional<V2_float> GetTargetWorldReferencePosition(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity && entity.Has<Transform>()) {
			return GetDrawTransform(entity).position;
		}
	}

	return std::nullopt;
}

template <typename Target>
[[nodiscard]] std::optional<V2_float> GetTargetWorldReferencePosition(
	const Target& target, V2_float local_position
) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity && entity.Has<Transform>()) {
			return GetDrawTransform(entity).Apply(local_position);
		}
	}

	return std::nullopt;
}

template <typename Target>
[[nodiscard]] PositionPicker::Convert MakeTransformPositionConverter(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		return [entity](V2_float world_position) mutable -> std::optional<V2_float> {
			if (!entity || !entity.Has<Transform>()) {
				return std::nullopt;
			}

			const bool ignores_parent_position{
				entity.Has<::ptgn::impl::IgnoreParentTransform>() ||
				entity.Has<::ptgn::impl::IgnoreParentPosition>()
			};

			Entity parent{ GetParent(entity) };

			if (!parent || ignores_parent_position) {
				return world_position;
			}

			return GetDrawTransform(parent).ApplyInverse(world_position);
		};
	} else {
		return [](V2_float) -> std::optional<V2_float> {
			return std::nullopt;
		};
	}
}


template <typename Target>
[[nodiscard]] bool ShouldShowTransformRelativePosition(const Target& target) {
	if constexpr (!requires { target.entity; }) {
		return false;
	} else {
		Entity entity{ target.entity };

		if (!entity) {
			return false;
		}

		const bool ignores_parent_position{
			entity.Has<::ptgn::impl::IgnoreParentTransform>() ||
			entity.Has<::ptgn::impl::IgnoreParentPosition>()
		};

		return !ignores_parent_position && static_cast<bool>(GetParent(entity));
	}
}

template <typename Target, typename Apply>
bool DrawTransformFeatureFields(
	Target& target, TransformFeatureState<Target>& state, Apply apply_state
) {
	constexpr ImGuiTableFlags flags{
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings |
		ImGuiTableFlags_NoPadOuterX
	};

	if (!ImGui::BeginTable("##TransformFields", 5, flags)) {
		return false;
	}

	const float compact_width{ ImGui::GetFrameHeight() };
	const float pick_width{
		ImGui::CalcTextSize("Pick").x +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};

	ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 76.0f);
	ImGui::TableSetupColumn("Lock", ImGuiTableColumnFlags_WidthFixed, compact_width);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, pick_width);
	ImGui::TableSetupColumn("Inheritance", ImGuiTableColumnFlags_WidthFixed, compact_width);

	bool changed{ false };
	auto& editor_state{ GetManualFeatureState(target.GetFeatureTargetKey()) };

	auto begin_row = [](const char* id, const char* label) {
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
		ImGui::PushID(id);
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
	};

	auto draw_ignore = [](bool& value, const char* tooltip) {
		ImGui::TableSetColumnIndex(4);
		const bool local_changed{ ImGui::Checkbox("##IgnoreParent", &value) };
		DrawTooltip(tooltip);
		return local_changed;
	};

	begin_row("Position", "Position");
	ImGui::TableSetColumnIndex(2);
	{
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float available{ ImGui::GetContentRegionAvail().x };
		const float field_width{ std::max(36.0f, (available - spacing) * 0.5f) };

		ImGui::SetNextItemWidth(field_width);
		changed |= ImGui::DragFloat(
			"##X", &state.transform.position.x, 1.0f, 0.0f, 0.0f, "X %.0f"
		);
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		changed |= ImGui::DragFloat(
			"##Y", &state.transform.position.y, 1.0f, 0.0f, 0.0f, "Y %.0f"
		);
	}
	ImGui::TableSetColumnIndex(3);
	const auto selected_button_state{
		GetButtonVisualEditState(target)
	};
	(void)DrawPositionPickButton(
		target.ctx,
		"Position",
		state.transform.position,
		MakeTransformPositionConverter(target),
		PositionPicker::Apply{
			[apply_state, state, selected_button_state](V2_float picked) mutable {
				const Transform before_transform{ state.transform };
				state.transform.position = picked;
				ApplyButtonVisualTransformDelta(
					state,
					selected_button_state,
					before_transform
				);
				apply_state(state);
			}
		},
		GetTargetWorldReferencePosition(target),
		ShouldShowTransformRelativePosition(target)
	);
	changed |= draw_ignore(state.ignore_position, "Ignore parent position.");
	ImGui::PopID();

	begin_row("Depth", "Depth");
	ImGui::TableSetColumnIndex(2);
	ImGui::SetNextItemWidth(-FLT_MIN);
	changed |= ImGui::DragFloat(
		"##Value", &state.depth.value, 0.05f, -1000.0f, 1000.0f, "%.2f",
		ImGuiSliderFlags_AlwaysClamp
	);
	changed |= draw_ignore(state.ignore_depth, "Ignore parent depth.");
	ImGui::PopID();

	begin_row("Rotation", "Rotation");
	ImGui::TableSetColumnIndex(2);
	ImGui::SetNextItemWidth(-FLT_MIN);
	changed |= ImGui::DragFloat(
		"##Value", &state.transform.rotation.value, 1.0f, 0.0f, 360.0f, "%.1f deg",
		ImGuiSliderFlags_AlwaysClamp
	);
	changed |= draw_ignore(state.ignore_rotation, "Ignore parent rotation.");
	ImGui::PopID();

	begin_row("Scale", "Scale");
	ImGui::TableSetColumnIndex(1);
	(void)ImGui::Checkbox("##LockRatio", &editor_state.scale_ratio_locked);
	DrawTooltip(
		"Lock the scale ratio. Editing either axis changes the other by the same proportional factor."
	);

	ImGui::TableSetColumnIndex(2);
	{
		const V2_float before_scale{ state.transform.scale };
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float available{ ImGui::GetContentRegionAvail().x };
		const float field_width{ std::max(36.0f, (available - spacing) * 0.5f) };

		ImGui::SetNextItemWidth(field_width);
		const bool x_changed{ ImGui::DragFloat(
			"##X", &state.transform.scale.x, 0.01f, -1000.0f, 1000.0f, "X %.2f",
			ImGuiSliderFlags_AlwaysClamp
		) };
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		const bool y_changed{ ImGui::DragFloat(
			"##Y", &state.transform.scale.y, 0.01f, -1000.0f, 1000.0f, "Y %.2f",
			ImGuiSliderFlags_AlwaysClamp
		) };

		if (editor_state.scale_ratio_locked) {
			constexpr float kScaleRatioEpsilon{ 0.000001f };

			if (x_changed && !y_changed) {
				if (std::abs(before_scale.x) > kScaleRatioEpsilon) {
					state.transform.scale.y =
						before_scale.y * (state.transform.scale.x / before_scale.x);
				} else if (std::abs(before_scale.y) <= kScaleRatioEpsilon) {
					state.transform.scale.y = state.transform.scale.x;
				}
			} else if (y_changed && !x_changed) {
				if (std::abs(before_scale.y) > kScaleRatioEpsilon) {
					state.transform.scale.x =
						before_scale.x * (state.transform.scale.y / before_scale.y);
				} else if (std::abs(before_scale.x) <= kScaleRatioEpsilon) {
					state.transform.scale.x = state.transform.scale.y;
				}
			}
		}

		changed |= x_changed || y_changed;
	}
	changed |= draw_ignore(state.ignore_scale, "Ignore parent scale.");
	ImGui::PopID();

	ImGui::EndTable();
	return changed;
}

template <typename Target, typename T>
[[nodiscard]] bool HasFeatureComponent(const Target& target) {
	if constexpr (!Target::template Supports<T>()) {
		return false;
	} else {
		return target.template Capture<T>().has_value();
	}
}

template <typename Target, typename... T>
[[nodiscard]] bool HasAnyFeatureComponent(const Target& target, FeatureComponents<T...>) {
	return (HasFeatureComponent<Target, T>(target) || ...);
}

template <typename Target, typename... T>
[[nodiscard]] bool HasInspectorFeature(
	const Target& target, InspectorFeature feature, FeatureComponents<T...> components
) {
	return IsFeatureManuallyAdded(target.GetFeatureTargetKey(), feature) ||
		   HasAnyFeatureComponent(target, components);
}

template <typename Target>
[[nodiscard]] bool HasTransformFeature(const Target& target) {
	return HasInspectorFeature(target, InspectorFeature::Transform, TransformFeatureComponents{});
}

template <typename Target>
[[nodiscard]] bool IsPrimarySceneRenderTarget(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };
		return entity && entity == entity.GetScene().GetRenderTarget();
	} else {
		return false;
	}
}

template <typename Target>
[[nodiscard]] bool IsReservedFixedCamera(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };
		return entity && entity == entity.GetScene().GetFixedCamera();
	} else {
		return false;
	}
}

template <typename Target>
[[nodiscard]] bool HasVisualFeature(const Target& target) {
	if (IsPrimarySceneRenderTarget(target) || IsReservedFixedCamera(target)) {
		return false;
	}

	if (IsFeatureManuallyAdded(
			target.GetFeatureTargetKey(),
			InspectorFeature::Visual
		)) {
		return true;
	}

	return HasFeatureComponent<
		Target,
		::ptgn::impl::IDrawable
	>(target);
}

template <typename Target>
[[nodiscard]] bool HasInteractionFeature(const Target& target) {
	if (IsFeatureManuallyAdded(
			target.GetFeatureTargetKey(),
			InspectorFeature::Interaction
		)) {
		return true;
	}

	const bool has_editable_component{
		HasFeatureComponent<Target, ::ptgn::impl::Interactive>(target) ||
		HasFeatureComponent<Target, ::ptgn::impl::Draggable>(target) ||
		HasFeatureComponent<Target, ::ptgn::impl::Dropzone>(target) ||
		HasFeatureComponent<Target, ::ptgn::impl::InteractiveTag>(target)
	};

	const bool has_visible_read_only_component{
		target.ctx.local.settings.show_read_only_inspector_data &&
		[&]() {
			if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
				return target.entity.template Has<InteractionLock>();
			} else if constexpr (Target::template Supports<InteractionLock>()) {
				return target.template Capture<InteractionLock>().has_value();
			} else {
				return false;
			}
		}()
	};

	return has_editable_component ||
		   has_visible_read_only_component;
}

template <typename Target>
[[nodiscard]] bool HasPhysicsFeature(const Target& target) {
	return HasInspectorFeature(target, InspectorFeature::Physics, PhysicsFeatureComponents{});
}

template <typename Target>
[[nodiscard]] bool HasUIFeature(const Target& target) {
	return HasInspectorFeature(target, InspectorFeature::UI, UIFeatureComponents{});
}

template <typename Target>
[[nodiscard]] bool HasCameraFeature(const Target& target) {
	if (IsPrimarySceneRenderTarget(target)) {
		return false;
	}

	return HasInspectorFeature(target, InspectorFeature::Camera, CameraFeatureComponents{});
}

template <typename Target>
[[nodiscard]] bool HasScriptsFeature(const Target& target) {
	return HasInspectorFeature(target, InspectorFeature::Scripts, ScriptsFeatureComponents{});
}

template <typename Target>
[[nodiscard]] bool HasUtilitiesFeature(const Target& target) {
	return HasInspectorFeature(target, InspectorFeature::Utilities, UtilitiesFeatureComponents{});
}

template <typename Visuals, typename Visual>
[[nodiscard]] Origin ResolveButtonChildAnchor(
	const Visuals& visuals,
	ButtonVisualState state,
	Origin fallback
) {
	return ResolveButtonVisualProperty(
		visuals.states,
		state,
		&Visual::anchor
	).value_or(fallback);
}

template <typename Visuals, typename Visual>
[[nodiscard]] V2_float GetButtonChildStateWorldPosition(
	Entity child,
	Entity button,
	ButtonVisualState state,
	Origin fallback_anchor,
	V2_float relative_position
) {
	if (!child || !button || !child.Has<Visuals>()) {
		return {};
	}

	const auto& visuals{ child.Get<Visuals>() };
	const Origin anchor{
		ResolveButtonChildAnchor<Visuals, Visual>(
			visuals,
			state,
			fallback_anchor
		)
	};
	const V2_float anchor_position{
		GetButtonInspectorLocalRect(button).GetOriginPoint(anchor)
	};

	return GetDrawTransform(button).Apply(
		anchor_position + relative_position
	);
}

template <typename Visuals, typename Visual>
[[nodiscard]] PositionPicker::Convert MakeButtonChildStatePositionConverter(
	Entity child,
	Entity button,
	ButtonVisualState state,
	Origin fallback_anchor
) {
	return [child, button, state, fallback_anchor](
		V2_float world_position
	) mutable -> std::optional<V2_float> {
		if (!child || !button || !child.Has<Visuals>()) {
			return std::nullopt;
		}

		const auto& visuals{ child.Get<Visuals>() };
		const Origin anchor{
			ResolveButtonChildAnchor<Visuals, Visual>(
				visuals,
				state,
				fallback_anchor
			)
		};
		const V2_float anchor_position{
			GetButtonInspectorLocalRect(button).GetOriginPoint(anchor)
		};
		const V2_float button_local{
			GetDrawTransform(button).ApplyInverse(world_position)
		};

		return button_local - anchor_position;
	};
}

template <typename Visuals, typename Visual>
[[nodiscard]] Transform ResolveButtonChildStateLocalTransform(
	const Visuals& visuals,
	Entity button,
	ButtonVisualState state,
	Origin fallback_anchor
) {
	Transform transform{
		ResolveButtonVisualProperty(
			visuals.states,
			state,
			&Visual::transform
		).value_or(Transform{})
	};
	const Origin anchor{
		ResolveButtonChildAnchor<Visuals, Visual>(
			visuals,
			state,
			fallback_anchor
		)
	};
	transform.position +=
		GetButtonInspectorLocalRect(button).GetOriginPoint(anchor);
	return transform;
}

[[nodiscard]] PositionPicker::Convert MakeButtonTextBoxPositionConverter(
	Entity child,
	Entity button,
	ButtonVisualState state
) {
	return [child, button, state](
		V2_float world_position
	) mutable -> std::optional<V2_float> {
		if (!child || !button || !child.Has<ButtonTextVisuals>()) {
			return std::nullopt;
		}

		const auto& visuals{ child.Get<ButtonTextVisuals>() };
		const Transform local_transform{
			ResolveButtonChildStateLocalTransform<
				ButtonTextVisuals,
				ButtonTextVisual
			>(
				visuals,
				button,
				state,
				Origin::Center
			)
		};
		const V2_float button_local{
			GetDrawTransform(button).ApplyInverse(world_position)
		};
		return local_transform.ApplyInverse(button_local);
	};
}

[[nodiscard]] std::optional<V2_float> GetButtonTextBoxWorldPosition(
	Entity child,
	Entity button,
	ButtonVisualState state,
	V2_float local_position
) {
	if (!child || !button || !child.Has<ButtonTextVisuals>()) {
		return std::nullopt;
	}

	const auto& visuals{ child.Get<ButtonTextVisuals>() };
	const Transform local_transform{
		ResolveButtonChildStateLocalTransform<
			ButtonTextVisuals,
			ButtonTextVisual
		>(
			visuals,
			button,
			state,
			Origin::Center
		)
	};
	return GetDrawTransform(button).Apply(
		local_transform.Apply(local_position)
	);
}

template <
	typename Target,
	typename Visuals,
	typename Visual,
	typename Callback
>
bool DrawButtonChildStateTransformComponent(
	Target& target,
	const ButtonChildInfo& child_info,
	ButtonVisualState state,
	Origin fallback_anchor,
	std::string_view part_label,
	Callback callback
) {
	if constexpr (!Target::template Supports<Visuals>()) {
		return false;
	} else {
		const auto header_open{
			ImGui::CollapsingHeader(
				"Transform##ButtonVisualStateTransform",
				ImGuiTreeNodeFlags_DefaultOpen
			)
		};

		if (!header_open) {
			return false;
		}

		ScopedIndent feature_indent;
		AutoLabelWidthScope label_width{ "ButtonVisualStateTransformFields" };

		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<Visuals>()) };

		auto before{ target.template Capture<Visuals>() };
		Visuals visuals{ before.value_or(Visuals{}) };
		const auto index{
			static_cast<std::size_t>(std::to_underlying(state))
		};
		auto& visual{ visuals.states[index] };
		Transform transform{
			ResolveButtonVisualProperty(
				visuals.states,
				state,
				&Visual::transform
			).value_or(Transform{})
		};
		const bool had_override{ visual.transform.has_value() };
		bool changed{ false };

		DrawDisabledWrappedText(
			PrettyName(magic_enum::enum_name(state)) + " " +
			std::string{ part_label } +
			" transform. Unset values inherit from fallback states."
		);

		changed |= DrawPropertyRow(
			"Position",
			[&]() {
				const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
				const float pick_width{
					ImGui::CalcTextSize("Pick").x +
					ImGui::GetStyle().FramePadding.x * 2.0f
				};
				const float available{ ImGui::GetContentRegionAvail().x };
				const float field_width{
					std::max(36.0f, (available - pick_width - spacing * 2.0f) * 0.5f)
				};
				bool local_changed{ false };

				ImGui::SetNextItemWidth(field_width);
				local_changed |= ImGui::DragFloat(
					"##X",
					&transform.position.x,
					1.0f,
					0.0f,
					0.0f,
					"X %.0f"
				);
				ImGui::SameLine(0.0f, spacing);
				ImGui::SetNextItemWidth(field_width);
				local_changed |= ImGui::DragFloat(
					"##Y",
					&transform.position.y,
					1.0f,
					0.0f,
					0.0f,
					"Y %.0f"
				);
				ImGui::SameLine(0.0f, spacing);

				auto apply{ target.template MakeApply<Visuals>(callback) };
				Visuals snapshot{ visuals };

				(void)DrawPositionPickButton(
					target.ctx,
					"ButtonVisualStatePosition",
					transform.position,
					MakeButtonChildStatePositionConverter<Visuals, Visual>(
						child_info.child,
						child_info.button,
						state,
						fallback_anchor
					),
					[apply, snapshot = std::move(snapshot), index, state](
						V2_float picked
					) mutable {
						auto& selected{ snapshot.states[index] };
						Transform updated{
							ResolveButtonVisualProperty(
								snapshot.states,
								state,
								&Visual::transform
							).value_or(Transform{})
						};
						updated.position = picked;
						selected.defined = true;
						selected.transform = updated;
						apply(ComponentState<Visuals>{ snapshot });
					},
					GetButtonChildStateWorldPosition<Visuals, Visual>(
						child_info.child,
						child_info.button,
						state,
						fallback_anchor,
						transform.position
					),
					true
				);

				return local_changed;
			}
		);
		changed |= DrawValue(
			target.ctx,
			"Rotation",
			transform.rotation,
			FieldOptions{
				.speed = 1.0f,
				.min = 0.0,
				.max = 360.0,
				.format = "%.1f deg",
				.flags = ImGuiSliderFlags_AlwaysClamp,
			}
		);
		changed |= DrawValue(
			target.ctx,
			"Scale",
			transform.scale,
			FieldOptions{
				.speed = 0.01f,
				.format = "%.3f",
			}
		);

		if (had_override) {
			if (ImGui::Button("Use Inherited Transform", ImVec2{ -FLT_MIN, 0.0f })) {
				visual.transform.reset();
				visual.defined = HasButtonVisualOverrides(visual);
				changed = true;
			}
		}

		if (changed && (!had_override || visual.transform.has_value())) {
			transform.ClampScale();
			visual.defined = true;
			visual.transform = transform;
		}

		if (changed) {
			target.template SetLive<Visuals>(
				ComponentState<Visuals>{ visuals },
				callback
			);
		}

		auto after{ target.template Capture<Visuals>() };
		TrackComponentState(
			target,
			std::string{ "Edit " } + std::string{ part_label } + " State Transform",
			std::move(before),
			std::move(after),
			changed,
			callback
		);

		return changed;
	}
}

template <typename Target>
bool DrawButtonChildStateTransformFeature(
	Target& target,
	const ButtonChildInfo& child_info,
	ButtonVisualState state
) {
	switch (child_info.part) {
		case ButtonChildPart::Background:
			return DrawButtonChildStateTransformComponent<
				Target,
				ButtonBackgroundVisuals,
				ButtonShapeVisual
			>(
				target,
				child_info,
				state,
				child_info.button.GetOrDefault<Origin>(),
				"Button Background",
				&MarkButtonBackgroundDirty
			);
		case ButtonChildPart::Border:
			return DrawButtonChildStateTransformComponent<
				Target,
				ButtonBorderVisuals,
				ButtonShapeVisual
			>(
				target,
				child_info,
				state,
				child_info.button.GetOrDefault<Origin>(),
				"Button Border",
				&MarkButtonBorderDirty
			);
		case ButtonChildPart::Text:
			return DrawButtonChildStateTransformComponent<
				Target,
				ButtonTextVisuals,
				ButtonTextVisual
			>(
				target,
				child_info,
				state,
				Origin::Center,
				"Button Text",
				&MarkButtonTextDirty
			);
		case ButtonChildPart::Sprite:
			return DrawButtonChildStateTransformComponent<
				Target,
				ButtonSpriteVisuals,
				ButtonSpriteVisual
			>(
				target,
				child_info,
				state,
				child_info.button.GetOrDefault<Origin>(),
				"Button Sprite",
				&MarkButtonSpriteDirty
			);
	}

	return false;
}

template <typename Target>
bool DrawTransformFeature(Target& target) {
	if (const auto child_info{ GetButtonChildInfo(target) }) {
		if (const auto state{ GetButtonVisualEditState(target) }) {
			return DrawButtonChildStateTransformFeature(
				target,
				*child_info,
				*state
			);
		}
	}

	if (!HasTransformFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target,
		InspectorFeature::Transform,
		"Transform",
		ImGuiTreeNodeFlags_DefaultOpen,
		TransformFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	const auto before{ CaptureTransformFeature(target) };
	auto state{ before };
	auto apply{ MakeTransformFeatureApply(target) };
	bool changed{ header.changed };

	changed |= DrawTransformFeatureFields(
		target,
		state,
		apply
	);

	if (changed) {
		state.ignore_transform = false;
		state.transform.ClampScale();
		ApplyButtonVisualTransformDelta(
			state,
			GetButtonVisualEditState(target),
			before.transform
		);
		SetTransformFeatureLive(target, state);
	}

	if (changed) {
		ScopedID target_scope{ target.Id() };
		const ImGuiID key{ ImGui::GetID("##TransformFeature") };

		TrackUndoableInteraction(
			target.ctx,
			key,
			"Edit Transform",
			true,
			[apply, before]() mutable {
				apply(before);
			},
			[apply, state]() mutable {
				apply(state);
			}
		);
	}

	return changed;
}

std::string NormalizeFeatureName(std::string_view input) {
	std::string result;
	result.reserve(input.size());

	for (const char c : input) {
		if (std::isalnum(static_cast<unsigned char>(c))) {
			result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
		}
	}

	return result;
}

template <typename Target>
[[nodiscard]] PositionPicker::Convert MakeLocalPositionConverter(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		return [entity](V2_float world_position) mutable -> std::optional<V2_float> {
			if (!entity || !entity.Has<Transform>()) {
				return std::nullopt;
			}

			return GetDrawTransform(entity).ApplyInverse(world_position);
		};
	} else {
		return [](V2_float) -> std::optional<V2_float> {
			return std::nullopt;
		};
	}
}

template <typename Target>
[[nodiscard]] constexpr bool CanPickLocalPosition() {
	return requires(Target target) {
		target.entity;
	};
}

template <
	typename Target,
	typename Component,
	typename Locator,
	typename Callback
>
bool DrawPickableLocalPosition(
	Target& target,
	Component& component,
	std::string_view label,
	Locator locator,
	Callback callback,
	bool* remove_requested = nullptr
) {
	V2_float& position{ locator(component) };

	const bool changed{ DrawPropertyRow(label, [&]() {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float pick_width{
			ImGui::CalcTextSize("Pick").x +
			ImGui::GetStyle().FramePadding.x * 2.0f
		};
		const float remove_width{ remove_requested ? ImGui::GetFrameHeight() : 0.0f };
		const float remove_spacing{ remove_requested ? spacing : 0.0f };
		const float available{ ImGui::GetContentRegionAvail().x };
		const float field_width{
			std::max(
				36.0f,
				(available - pick_width - remove_width - remove_spacing - spacing * 2.0f) * 0.5f
			)
		};

		bool local_changed{ false };

		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat(
			"##X",
			&position.x,
			0.1f,
			0.0f,
			0.0f,
			"X %.2f"
		);

		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat(
			"##Y",
			&position.y,
			0.1f,
			0.0f,
			0.0f,
			"Y %.2f"
		);

		ImGui::SameLine(0.0f, spacing);

		{
			ScopedDisabled disabled{ !CanPickLocalPosition<Target>() };

			auto apply{ target.template MakeApply<Component>(callback) };
			Component snapshot{ component };

			(void)DrawPositionPickButton(
				target.ctx,
				label,
				position,
				MakeLocalPositionConverter(target),
				[
					apply,
					snapshot = std::move(snapshot),
					locator
				](V2_float picked) mutable {
					locator(snapshot) = picked;
					apply(ComponentState<Component>{ snapshot });
				},
				GetTargetWorldReferencePosition(target, position),
				true
			);

			if constexpr (!CanPickLocalPosition<Target>()) {
				DrawTooltip("Position picking is available for scene entities.");
			}
		}

		if (remove_requested) {
			ImGui::SameLine(0.0f, spacing);
			if (ImGui::Button("-", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() })) {
				*remove_requested = true;
			}
			DrawTooltip("Delete this vertex.");
		}

		return local_changed;
	}) };

	return changed;
}

template <
	typename Target,
	typename Component,
	typename Value,
	typename Locator,
	typename Callback
>
bool DrawGeometryValue(
	Target& target,
	Component& component,
	Value& value,
	std::string_view label,
	Locator locator,
	Callback callback
);

template <
	std::size_t I,
	typename Target,
	typename Component,
	typename Parent,
	typename Locator,
	typename Callback
>
bool DrawReflectedGeometryMember(
	Target& target,
	Component& component,
	Parent& parent,
	Locator locator,
	Callback callback
) {
	auto members{ ReflectMembers(parent) };
	auto& member{ std::get<I>(members) };

	auto member_locator = [locator](Component& root) -> decltype(auto) {
		auto reflected{ ReflectMembers(locator(root)) };
		return std::get<I>(reflected).value;
	};

	return DrawGeometryValue(
		target,
		component,
		member.value,
		PrettyName(member.name),
		member_locator,
		callback
	);
}

template <
	typename Target,
	typename Component,
	typename Parent,
	typename Locator,
	typename Callback,
	std::size_t... I
>
bool DrawReflectedGeometryMembers(
	Target& target,
	Component& component,
	Parent& parent,
	Locator locator,
	Callback callback,
	std::index_sequence<I...>
) {
	bool changed{ false };
	((changed |= DrawReflectedGeometryMember<I>(
		target,
		component,
		parent,
		locator,
		callback
	)), ...);
	return changed;
}

template <
	typename Target,
	typename Component,
	typename Variant,
	typename Locator,
	typename Callback,
	std::size_t... I
>
bool DrawGeometryVariant(
	Target& target,
	Component& component,
	Variant& value,
	std::string_view label,
	Locator locator,
	Callback callback,
	std::index_sequence<I...>
) {
	bool changed{ false };
	std::string preview{ "None" };

	auto update_preview = [&]<std::size_t Index>() {
		if (value.index() != Index) {
			return;
		}

		using Alternative = std::variant_alternative_t<Index, Variant>;

		if constexpr (!std::same_as<Alternative, std::monostate>) {
			preview = TypeLabel<Alternative>();
		}
	};

	(update_preview.template operator()<I>(), ...);

	changed |= DrawPropertyRow(label, [&]() {
		bool local_changed{ false };

		if (ImGui::BeginCombo("##value", preview.c_str())) {
			auto draw_option = [&]<std::size_t Index>() {
				using Alternative = std::variant_alternative_t<Index, Variant>;

				const std::string option{
					std::same_as<Alternative, std::monostate>
						? "None"
						: TypeLabel<Alternative>()
				};

				if constexpr (std::default_initializable<Alternative>) {
					if (ImGui::Selectable(option.c_str(), value.index() == Index)) {
						value.template emplace<Index>();
						local_changed = true;
					}
				}
			};

			(draw_option.template operator()<I>(), ...);
			ImGui::EndCombo();
		}

		return local_changed;
	});

	auto draw_selected = [&]<std::size_t Index>() {
		if (value.index() != Index) {
			return;
		}

		using Alternative = std::variant_alternative_t<Index, Variant>;

		if constexpr (!std::same_as<Alternative, std::monostate>) {
			auto alternative_locator = [locator](Component& root) -> Alternative& {
				return std::get<Index>(locator(root));
			};

			changed |= DrawGeometryValue(
				target,
				component,
				std::get<Index>(value),
				TypeLabel<Alternative>(),
				alternative_locator,
				callback
			);
		}
	};

	(draw_selected.template operator()<I>(), ...);
	return changed;
}

template <
	typename Target,
	typename Component,
	typename Value,
	typename Locator,
	typename Callback
>
bool DrawGeometryValue(
	Target& target,
	Component& component,
	Value& value,
	std::string_view label,
	Locator locator,
	Callback callback
) {
	using Type = std::remove_cvref_t<Value>;

	if constexpr (std::same_as<Type, V2_float>) {
		if constexpr (std::same_as<std::remove_cvref_t<Component>, Ellipse>) {
			return DrawValue(
				target.ctx,
				label,
				value,
				FieldOptions{
					.speed = 0.1f,
					.format = "%.3f",
				}
			);
		} else {
			return DrawPickableLocalPosition(
				target,
				component,
				label,
				locator,
				callback
			);
		}
	} else if constexpr (std::same_as<Type, Rect>) {
		bool changed{ false };
		V2_float size{ value.GetSize() };

		if (DrawValue(
				target.ctx,
				"Size",
				size,
				FieldOptions{
					.speed = 0.1f,
					.format = "%.3f",
				}
			)) {
			size.x = std::max(size.x, 0.0f);
			size.y = std::max(size.y, 0.0f);

			const V2_float center{ value.GetCenter() };
			const V2_float half_size{ size * 0.5f };
			value.min = center - half_size;
			value.max = center + half_size;
			changed = true;
		}

		auto min_locator = [locator](Component& root) -> V2_float& {
			return locator(root).min;
		};
		auto max_locator = [locator](Component& root) -> V2_float& {
			return locator(root).max;
		};

		changed |= DrawPickableLocalPosition(
			target,
			component,
			"Min",
			min_locator,
			callback
		);
		changed |= DrawPickableLocalPosition(
			target,
			component,
			"Max",
			max_locator,
			callback
		);

		return changed;
	} else if constexpr (kIsVector<Type>) {
		if constexpr (std::same_as<typename Type::value_type, V2_float>) {
			bool changed{ false };
			std::optional<std::size_t> remove;

			for (std::size_t index{ 0 }; index < value.size(); ++index) {
				ScopedID vertex_scope{ static_cast<int>(index) };
				auto vertex_locator = [locator, index](Component& root) -> V2_float& {
					return locator(root)[index];
				};
				bool remove_vertex{ false };

				changed |= DrawPickableLocalPosition(
					target,
					component,
					std::string{ "Vertex " } + std::to_string(index + 1),
					vertex_locator,
					callback,
					&remove_vertex
				);

				if (remove_vertex) {
					remove = index;
				}
			}

			if (remove) {
				value.erase(value.begin() + static_cast<std::ptrdiff_t>(*remove));
				changed = true;
			}

			if (ImGui::Button("+ Vertex", ImVec2{ -FLT_MIN, 0.0f })) {
				value.emplace_back();
				changed = true;
			}

			return changed;
		} else {
			return DrawValue(target.ctx, label, value);
		}
	} else if constexpr (kIsArray<Type>) {
		if constexpr (std::same_as<typename Type::value_type, V2_float>) {
			bool changed{ false };

			for (std::size_t index{ 0 }; index < value.size(); ++index) {
				ScopedID vertex_scope{ static_cast<int>(index) };
				auto vertex_locator = [locator, index](Component& root) -> V2_float& {
					return locator(root)[index];
				};

				changed |= DrawPickableLocalPosition(
					target,
					component,
					std::string{ "Vertex " } + std::to_string(index + 1),
					vertex_locator,
					callback
				);
			}

			return changed;
		} else {
			return DrawValue(target.ctx, label, value);
		}
	} else if constexpr (kIsVariant<Type>) {
		return DrawGeometryVariant(
			target,
			component,
			value,
			label,
			locator,
			callback,
			std::make_index_sequence<std::variant_size_v<Type>>{}
		);
	} else if constexpr (ReflectedValue<Type>) {
		auto reflected{ ReflectValue(value) };
		auto value_locator = [locator](Component& root) -> decltype(auto) {
			return ReflectValue(locator(root)).value;
		};

		return DrawGeometryValue(
			target,
			component,
			reflected.value,
			label,
			value_locator,
			callback
		);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		return DrawReflectedGeometryMembers(
			target,
			component,
			value,
			locator,
			callback,
			std::make_index_sequence<std::tuple_size_v<decltype(members)>>{}
		);
	} else {
		return DrawValue(target.ctx, label, value);
	}
}

template <
	typename Target,
	typename Component,
	typename Callback = std::nullptr_t
>
bool DrawGeometryComponent(
	Target& target,
	Component& component,
	Callback callback = nullptr
) {
	auto root_locator = [](Component& value) -> Component& {
		return value;
	};

	return DrawGeometryValue(
		target,
		component,
		component,
		TypeLabel<Component>(),
		root_locator,
		callback
	);
}

template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawRequiredInlineVisualComponent(
	Target& target,
	std::string_view label,
	Draw&& draw,
	Callback callback = nullptr
) {
	return DrawRequiredComponent<Target, T>(
		target,
		label,
		false,
		std::forward<Draw>(draw),
		callback
	);
}


template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawRequiredInlineVisualComponentWithDefault(
	Target& target,
	std::string_view label,
	T default_value,
	Draw&& draw,
	Callback callback = nullptr
) {
	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<T>()) };

	auto before{ target.template Capture<T>() };
	bool changed{ false };

	if (!before) {
		target.template SetLive<T>(std::move(default_value), callback);
		changed = true;
	}

	T value{ target.template Capture<T>().value_or(T{}) };
	changed |= std::invoke(std::forward<Draw>(draw), value);

	if (changed) {
		target.template SetLive<T>(value, callback);
	}

	auto after{ target.template Capture<T>() };
	TrackComponentState(
		target,
		std::string{ "Edit " } + std::string{ label },
		std::move(before),
		std::move(after),
		changed,
		callback
	);

	return changed;
}

template <typename Target, typename T>
bool DrawOptionalVisualComponent(
	Target& target,
	std::string_view label,
	bool tree = false,
	bool contents_read_only = false,
	bool toggle_read_only = false
) {
	if constexpr (std::is_empty_v<T>) {
		return DrawOptionalComponent<Target, T>(
			target,
			label,
			false,
			[](T&) {
				return false;
			},
			contents_read_only,
			toggle_read_only
		);
	} else {
		return DrawOptionalComponent<Target, T>(
			target,
			label,
			tree,
			[&target](T& value) {
				return DrawContents(target.ctx, value);
			},
			contents_read_only,
			toggle_read_only
		);
	}
}

[[nodiscard]] bool IsShapeRenderer(std::string_view visual) {
	return visual == "rect" ||
		   visual == "circle" ||
		   visual == "roundedrect" ||
		   visual == "polygon" ||
		   visual == "ellipse" ||
		   visual == "triangle" ||
		   visual == "line" ||
		   visual == "capsule" ||
		   visual == "arc";
}

[[nodiscard]] bool IsEffectRenderer(std::string_view visual);

inline constexpr V2_float kInspectorDefaultShapeSize{ 100.0f, 100.0f };
inline constexpr float kInspectorDefaultShapeRadius{ 50.0f };

// Keep renderer selected geometry consistent with the Scene Hierarchy create menu.
template <typename T>
[[nodiscard]] T MakeDefaultShapeGeometry() {
	if constexpr (std::same_as<T, Rect>) {
		if constexpr (std::constructible_from<T, V2_float>) {
			return T{ kInspectorDefaultShapeSize };
		}
	} else if constexpr (std::same_as<T, Circle>) {
		if constexpr (std::constructible_from<T, float>) {
			return T{ kInspectorDefaultShapeRadius };
		}
	} else if constexpr (std::same_as<T, Line>) {
		if constexpr (std::constructible_from<T, V2_float, V2_float>) {
			return T{ V2_float{ -100.0f, -100.0f }, V2_float{ 100.0f, 100.0f } };
		}
	} else if constexpr (std::same_as<T, Polygon>) {
		std::vector<V2_float> vertices{
			{ 0.0f, -50.0f },
			{ 47.0f, -15.0f },
			{ 29.0f, 40.0f },
			{ -29.0f, 40.0f },
			{ -47.0f, -15.0f },
		};

		if constexpr (std::constructible_from<T, std::vector<V2_float>>) {
			return T{ std::move(vertices) };
		}
	} else if constexpr (std::same_as<T, Ellipse>) {
		if constexpr (std::constructible_from<T, V2_float>) {
			return T{ V2_float{ 100.0f, 50.0f } };
		}
	} else if constexpr (std::same_as<T, Arc>) {
		if constexpr (std::constructible_from<T, float, float, float, bool>) {
			return T{ kInspectorDefaultShapeRadius, 0.0f, 90.0f, true };
		} else if constexpr (std::constructible_from<T, float, Degrees, Degrees, bool>) {
			return T{
				kInspectorDefaultShapeRadius,
				Degrees{ 0.0f },
				Degrees{ 90.0f },
				true,
			};
		}
	} else if constexpr (std::same_as<T, RoundedRect>) {
		if constexpr (std::constructible_from<T, V2_float, float>) {
			return T{ kInspectorDefaultShapeSize, 10.0f };
		} else if constexpr (std::constructible_from<T, Rect, float>) {
			return T{ MakeDefaultShapeGeometry<Rect>(), 10.0f };
		}
	} else if constexpr (std::same_as<T, Triangle>) {
		if constexpr (std::constructible_from<T, V2_float, V2_float, V2_float>) {
			return T{
				V2_float{ -100.0f, 50.0f },
				V2_float{ 0.0f, -50.0f },
				V2_float{ 100.0f, 50.0f },
			};
		}
	} else if constexpr (std::same_as<T, Capsule>) {
		if constexpr (std::constructible_from<T, V2_float, V2_float, float>) {
			return T{
				V2_float{ -100.0f, -100.0f },
				V2_float{ 100.0f, 100.0f },
				kInspectorDefaultShapeRadius,
			};
		} else if constexpr (std::constructible_from<T, Line, float>) {
			return T{
				MakeDefaultShapeGeometry<Line>(),
				kInspectorDefaultShapeRadius,
			};
		}
	}

	return T{};
}

using RendererOwnedComponents = FeatureComponents<
	Rect, Circle, RoundedRect, Polygon, Ellipse, Triangle, Line, Capsule, Arc, Color, FillStyle,
	TextureKey, ::ptgn::impl::TextureSize, ::ptgn::impl::TextureCrop,
	::ptgn::impl::AnimationData, ::ptgn::impl::Offsets, ::ptgn::impl::TextData,
	::ptgn::impl::ParticleEmitterData, LightData, ::ptgn::impl::ShadowCaster,
	::ptgn::impl::GraphicsData, ::ptgn::Material, ShaderKey,
	::ptgn::impl::RenderTargetSize, ::ptgn::impl::EffectTag,
	::ptgn::impl::HDREffectTag, EffectMargin, Bloom, Blur, GaussianBlur,
	::ptgn::impl::ClearColor, ::ptgn::impl::ClearDepth, ::ptgn::impl::ClearStencil>;

template <typename Target, typename T>
void ClearRendererOwnedComponent(Target& target) {
	if constexpr (Target::template Supports<T>()) {
		target.template SetLive<T>(std::nullopt);
	}
}

template <typename Target, typename... T>
void ClearRendererOwnedComponents(Target& target, FeatureComponents<T...>) {
	(ClearRendererOwnedComponent<Target, T>(target), ...);
}

template <typename T, typename Target>
void SetRendererOwnedComponent(Target& target, T value = T{}) {
	if constexpr (Target::template Supports<T>()) {
		target.template SetLive<T>(std::move(value));
	}
}

[[nodiscard]] ::ptgn::impl::TextData MakeDefaultTextRendererData() {
	::ptgn::impl::TextData data;
	auto members{ ReflectMembers(data) };

	std::apply(
		[](auto&&... member) {
			(
				[&] {
					using Member = std::remove_cvref_t<decltype(member.value)>;

					if constexpr (std::same_as<Member, StyledText>) {
						if (member.value.runs.empty()) {
							member.value.runs.emplace_back();
						}
					}
				}(),
				...
			);
		},
		members
	);

	return data;
}

template <typename Target>
void InitializeRendererOwnedComponents(Target& target, std::string_view visual) {
	auto add_shape = [&]<typename T>() {
		SetRendererOwnedComponent<T>(target, MakeDefaultShapeGeometry<T>());
		SetRendererOwnedComponent<Color>(target, Color{ color::White });
		SetRendererOwnedComponent<FillStyle>(target, FillStyle{ Solid{} });
	};

	if (visual == "rect") {
		add_shape.template operator()<Rect>();
	} else if (visual == "circle") {
		add_shape.template operator()<Circle>();
	} else if (visual == "roundedrect") {
		add_shape.template operator()<RoundedRect>();
	} else if (visual == "polygon") {
		add_shape.template operator()<Polygon>();
	} else if (visual == "ellipse") {
		add_shape.template operator()<Ellipse>();
	} else if (visual == "triangle") {
		add_shape.template operator()<Triangle>();
	} else if (visual == "line") {
		SetRendererOwnedComponent<Line>(target, MakeDefaultShapeGeometry<Line>());
		SetRendererOwnedComponent<Color>(target, Color{ color::White });
		SetRendererOwnedComponent<FillStyle>(target, FillStyle{ kMinLineWidth });
	} else if (visual == "capsule") {
		add_shape.template operator()<Capsule>();
	} else if (visual == "arc") {
		add_shape.template operator()<Arc>();
	} else if (visual.contains("sprite")) {
		SetRendererOwnedComponent<TextureKey>(target);
	} else if (visual.contains("text")) {
		SetRendererOwnedComponent<::ptgn::impl::TextData>(
			target,
			MakeDefaultTextRendererData()
		);
	} else if (visual.contains("particle")) {
		SetRendererOwnedComponent<::ptgn::impl::ParticleEmitterData>(target);
	} else if (visual.contains("light")) {
		SetRendererOwnedComponent<LightData>(target);
	} else if (visual.contains("customshader")) {
		SetRendererOwnedComponent<::ptgn::Material>(target);
	} else if (visual.contains("graphics")) {
		SetRendererOwnedComponent<::ptgn::impl::GraphicsData>(target);
	} else if (visual.contains("rendertarget")) {
		SetRendererOwnedComponent<::ptgn::impl::RenderTargetSize>(target);
	}

	if (visual.contains("gaussianblur")) {
		SetRendererOwnedComponent<GaussianBlur>(target);
	} else if (visual.contains("blur")) {
		SetRendererOwnedComponent<Blur>(target);
	} else if (visual.contains("bloom")) {
		SetRendererOwnedComponent<Bloom>(target);
	}
}

template <typename Target>
void ApplyRendererSelection(
	Target& target,
	ComponentState<::ptgn::impl::IDrawable> drawable
) {
	ClearRendererOwnedComponents(target, RendererOwnedComponents{});
	target.template SetLive<::ptgn::impl::IDrawable>(drawable);

	if (!drawable) {
		return;
	}

	const auto* info{
		::ptgn::impl::IDrawable::FindInfo(drawable->hash)
	};

	if (!info) {
		return;
	}

	InitializeRendererOwnedComponents(
		target,
		NormalizeFeatureName(info->GetDisplayName())
	);
}

struct RendererRowResult {
	std::string visual;
	bool changed{ false };
};

template <typename Target>
RendererRowResult DrawRendererRow(Target& target) {
	using Drawable = ::ptgn::impl::IDrawable;

	auto drawable{ target.template Capture<Drawable>() };
	auto before_visible{ target.template Capture<Visible>() };
	bool visible{
		before_visible
			? before_visible->visible
			: true
	};

	const auto* info{
		drawable
			? Drawable::FindInfo(drawable->hash)
			: nullptr
	};
	const std::string preview{
		info
			? std::string{ info->GetDisplayName() }
			: "None"
	};

	const float start_x{ ImGui::GetCursorPosX() };
	const float checkbox_slot{
		ImGui::GetFrameHeight() +
		ImGui::GetStyle().ItemSpacing.x
	};

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Renderer");
	MeasurePropertyLabel("Renderer", start_x + checkbox_slot);
	ImGui::SameLine();
	ImGui::SetCursorPosX(GetPropertyValueX(start_x));

	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float visible_width{
		ImGui::GetFrameHeight() +
		ImGui::GetStyle().ItemInnerSpacing.x +
		ImGui::CalcTextSize("Visible").x
	};
	const float combo_width{
		std::max(
			80.0f,
			ImGui::GetContentRegionAvail().x -
				visible_width -
				spacing
		)
	};

	bool renderer_changed{ false };
	auto before_renderer{
		CaptureInspectorFeatureState(
			target,
			InspectorFeature::Visual,
			VisualFeatureComponents{}
		)
	};

	auto choose_renderer = [&](ComponentState<Drawable> selected) {
		const bool same_renderer{
			selected.has_value() == drawable.has_value() &&
			(!selected || selected->hash == drawable->hash)
		};

		if (same_renderer) {
			return;
		}

		ApplyRendererSelection(target, selected);
		drawable = target.template Capture<Drawable>();
		renderer_changed = true;
	};

	ImGui::SetNextItemWidth(combo_width);

	if (ImGui::BeginCombo("##RendererSelector", preview.c_str())) {
		if (ImGui::Selectable("None", !drawable.has_value())) {
			choose_renderer(std::nullopt);
		}

		auto draw_candidate = [&](const auto& candidate) {
			const std::string label{ candidate.GetDisplayName() };
			const bool selected{
				drawable &&
				drawable->hash == candidate.hash
			};

			if (ImGui::Selectable(label.c_str(), selected)) {
				choose_renderer(Drawable{ candidate.hash });
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		};

		for (const auto& candidate : Drawable::data()) {
			const std::string visual{
				NormalizeFeatureName(candidate.GetDisplayName())
			};

			if (!IsShapeRenderer(visual) &&
				!IsEffectRenderer(visual)) {
				draw_candidate(candidate);
			}
		}

		if (ImGui::BeginMenu("Shapes")) {
			for (const auto& candidate : Drawable::data()) {
				const std::string visual{
					NormalizeFeatureName(candidate.GetDisplayName())
				};

				if (IsShapeRenderer(visual)) {
					draw_candidate(candidate);
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Effects")) {
			for (const auto& candidate : Drawable::data()) {
				const std::string visual{
					NormalizeFeatureName(candidate.GetDisplayName())
				};

				if (IsEffectRenderer(visual)) {
					draw_candidate(candidate);
				}
			}

			ImGui::EndMenu();
		}

		ImGui::EndCombo();
	}

	if (renderer_changed) {
		auto after_renderer{
			CaptureInspectorFeatureState(
				target,
				InspectorFeature::Visual,
				VisualFeatureComponents{}
			)
		};

		TrackInspectorFeatureState(
			target,
			InspectorFeature::Visual,
			"Change Renderer",
			std::move(before_renderer),
			std::move(after_renderer),
			VisualFeatureComponents{}
		);
	}

	bool visible_changed{ false };

	if (drawable) {
		ImGui::SameLine(0.0f, spacing);
		visible_changed = ImGui::Checkbox(
			"Visible##RendererVisible",
			&visible
		);
	}

	if (visible_changed) {
		Visible updated{
			before_visible.value_or(Visible{})
		};
		updated.visible = visible;
		target.template SetLive<Visible>(
			ComponentState<Visible>{ updated }
		);
	}

	auto after_visible{ target.template Capture<Visible>() };

	TrackComponentState(
		target,
		"Toggle Visibility",
		std::move(before_visible),
		std::move(after_visible),
		visible_changed
	);

	const auto* selected_info{
		drawable
			? Drawable::FindInfo(drawable->hash)
			: nullptr
	};

	return RendererRowResult{
		.visual = selected_info
			? NormalizeFeatureName(selected_info->GetDisplayName())
			: std::string{},
		.changed = renderer_changed,
	};
}

template <typename Target>
bool DrawEffectMargin(Target& target) {
	return DrawOptionalComponent<Target, EffectMargin>(
		target,
		"Effect Margin",
		false,
		[&target](EffectMargin& margin) {
			const FieldOptions options{
				.speed = 1.0f,
				.min = 0.0,
				.max = 4096.0,
				.flags = ImGuiSliderFlags_AlwaysClamp,
			};

			return [&target, &options]<typename Margin>(Margin& value) {
				if constexpr (ReflectedValue<Margin>) {
					auto member{ ReflectValue(value) };
					return DrawValue(
						target.ctx,
						"Effect Margin",
						member.value,
						options
					);
				} else if constexpr (ReflectedMembers<Margin>) {
					bool changed{ false };
					auto members{ ReflectMembers(value) };

					std::apply(
						[&](auto&&... member) {
							(
								(
									changed |= DrawValue(
										target.ctx,
										PrettyName(member.name),
										member.value,
										options
									)
								),
								...
							);
						},
						members
					);

					return changed;
				} else {
					return DrawContents(target.ctx, value);
				}
			}(margin);
		}
	);
}

[[nodiscard]] bool IsEffectRenderer(
	std::string_view visual
) {
	if (visual.empty()) {
		return false;
	}

	const bool standard_renderer{
		visual == "rect" ||
		visual == "circle" ||
		visual == "roundedrect" ||
		visual == "polygon" ||
		visual == "ellipse" ||
		visual == "triangle" ||
		visual == "line" ||
		visual == "capsule" ||
		visual == "arc" ||
		visual.contains("sprite") ||
		visual.contains("text") ||
		visual.contains("particle") ||
		visual.contains("light") ||
		visual.contains("graphics") ||
		visual.contains("customshader") ||
		visual.contains("rendertarget")
	};

	return !standard_renderer;
}

template <typename Target, typename Effect>
bool DrawSelectedEffectComponent(
	Target& target,
	std::string_view label
) {
	if constexpr (!Target::template Supports<Effect>()) {
		return false;
	} else {
		return DrawRequiredComponent<Target, Effect>(
			target,
			label,
			true,
			[&target](Effect& value) {
				return DrawComponentContents(
					target.ctx,
					value
				);
			}
		);
	}
}

template <typename Target>
bool DrawVisualEffects(
	Target& target,
	std::string_view visual
) {
	bool changed{ false };

	if (visual.contains("gaussianblur")) {
		changed |= DrawSelectedEffectComponent<Target, GaussianBlur>(
			target,
			"Gaussian Blur"
		);
	} else if (visual.contains("blur")) {
		changed |= DrawSelectedEffectComponent<Target, Blur>(
			target,
			"Blur"
		);
	} else if (visual.contains("bloom")) {
		changed |= DrawSelectedEffectComponent<Target, Bloom>(
			target,
			"Bloom"
		);
	}

	if (IsEffectRenderer(visual)) {
		changed |= DrawEffectMargin(target);
	}

	return changed;
}

bool DrawTextRunsFlat(
	EditorContext& ctx,
	StyledText& text
) {
	bool changed{ false };

	if (text.runs.empty()) {
		text.runs.emplace_back();
		changed = true;
	}

	ImGui::SeparatorText("Content");

	changed |= DrawVectorEditor(
		ctx,
		text.runs,
		VectorOptions{
			.item_name = "Text Run",
			.default_open = true,
			.reorderable = true,
			.minimum_items = 1,
		}
	);

	return changed;
}

template <
	std::size_t I,
	typename Target,
	typename TextData
>
bool DrawTextBoxMember(
	Target& target,
	TextData& text_data,
	auto& box
) {
	using Box = std::remove_cvref_t<decltype(box)>;
	bool changed{ false };
	auto& editor_state{
		GetManualFeatureState(
			target.GetFeatureTargetKey()
		)
	};

	bool box_has_non_default_data{ false };

	if constexpr (
		JsonSerializable<Box> &&
		std::default_initializable<Box>
	) {
		json current = box;
		json defaults = Box{};
		box_has_non_default_data = current != defaults;
	}

	if (!editor_state.text_box_state_initialized) {
		editor_state.text_box_enabled =
			box_has_non_default_data;
		editor_state.text_box_state_initialized = true;
	} else if (box_has_non_default_data) {
		editor_state.text_box_enabled = true;
	}

	bool enabled{
		editor_state.text_box_enabled
	};

	ScopedID box_scope{ "TextBox" };

	if (ImGui::Checkbox(
			"##Enabled",
			&enabled
		)) {
		editor_state.text_box_enabled = enabled;

		if (!enabled) {
			box = Box{};
		}

		changed = true;
	}

	ImGui::SameLine();

	const bool open{
		ImGui::TreeNodeEx(
			"Text Box##Tree",
			ImGuiTreeNodeFlags_SpanAvailWidth
		)
	};

	if (!open) {
		return changed;
	}

	ScopedIndent indent;
	ScopedDisabled disabled{ !enabled };
	auto box_members{ ReflectMembers(box) };

	auto draw_box_member = [&](auto&& box_member) {
		const std::string box_name{
			NormalizeFeatureName(box_member.name)
		};

		if (box_name == "rect") {
			using BoxMember =
				std::remove_cvref_t<
					decltype(box_member.value)
				>;

			if constexpr (std::same_as<BoxMember, Rect>) {
				auto box_locator =
					[](TextData& value) -> Box& {
						auto reflected{
							ReflectMembers(value)
						};
						return std::get<I>(
							reflected
						).value;
					};

				auto rect_locator =
					[box_locator](TextData& value) -> Rect& {
						auto reflected{
							ReflectMembers(
								box_locator(value)
							)
						};
						constexpr std::size_t count{
							std::tuple_size_v<
								decltype(reflected)
							>
						};
						Rect* result{ nullptr };

						[&]<std::size_t... Index>(
							std::index_sequence<Index...>
						) {
							(
								[&] {
									auto& candidate{
										std::get<Index>(
											reflected
										)
									};

									if constexpr (
										std::same_as<
											std::remove_cvref_t<
												decltype(
													candidate.value
												)
											>,
											Rect
										>
									) {
										if (
											NormalizeFeatureName(
												candidate.name
											) == "rect"
										) {
											result =
												std::addressof(
													candidate.value
												);
										}
									}
								}(),
								...
							);
						}(
							std::make_index_sequence<
								count
							>{}
						);

						return *result;
					};

				changed |= DrawGeometryValue(
					target,
					text_data,
					box_member.value,
					"Rect",
					rect_locator,
					&MarkTextLayoutDirty
				);
			} else {
				changed |= DrawValue(
					target.ctx,
					"Rect",
					box_member.value
				);
			}

			return;
		}

		if (box_name == "style") {
			const bool style_open{
				ImGui::TreeNodeEx(
					"Additional Options##TextBoxAdditionalOptions",
					ImGuiTreeNodeFlags_SpanAvailWidth
				)
			};

			if (style_open) {
				{
					ScopedUnindent align_with_additional_options;
					changed |= DrawComponentContents(
						target.ctx,
						box_member.value
					);
				}

				ImGui::TreePop();
			}

			return;
		}

		changed |= DrawValue(
			target.ctx,
			PrettyName(box_member.name),
			box_member.value
		);
	};

	std::apply(
		[&](auto&&... box_member) {
			(draw_box_member(box_member), ...);
		},
		box_members
	);

	ImGui::TreePop();
	return changed;
}

template <
	std::size_t I,
	typename Target,
	typename TextData
>
bool DrawTextPrimaryMember(
	Target& target,
	TextData& text_data
) {
	auto members{ ReflectMembers(text_data) };
	auto& member{ std::get<I>(members) };
	const std::string normalized{
		NormalizeFeatureName(member.name)
	};

	if (
		normalized == "text" ||
		normalized == "content"
	) {
		using Member =
			std::remove_cvref_t<
				decltype(member.value)
			>;

		if constexpr (std::same_as<Member, StyledText>) {
			return DrawTextRunsFlat(
				target.ctx,
				member.value
			);
		} else {
			return DrawValue(
				target.ctx,
				"Content",
				member.value
			);
		}
	}

	if (
		normalized.contains("glyph") ||
		normalized.contains("clip") ||
		normalized.contains("currentrun")
	) {
		return false;
	}

	if (normalized == "box") {
		using Box =
			std::remove_cvref_t<
				decltype(member.value)
			>;

		if constexpr (ReflectedMembers<Box>) {
			return DrawTextBoxMember<I>(
				target,
				text_data,
				member.value
			);
		}
	}

	return DrawValue(
		target.ctx,
		PrettyName(member.name),
		member.value
	);
}

template <typename Target, typename TextData, std::size_t... I>
bool DrawTextPrimaryMembers(
	Target& target,
	TextData& text_data,
	std::index_sequence<I...>
) {
	bool changed{ false };
	((changed |= DrawTextPrimaryMember<I>(
		target,
		text_data
	)), ...);
	return changed;
}

template <typename Target>
bool DrawTextPrimary(
	Target& target,
	::ptgn::impl::TextData& text_data
) {
	auto members{ ReflectMembers(text_data) };
	return DrawTextPrimaryMembers(
		target,
		text_data,
		std::make_index_sequence<
			std::tuple_size_v<
				decltype(members)
			>
		>{}
	);
}

template <typename Target>
bool DrawTextAdditional(
	Target& target,
	::ptgn::impl::TextData& text_data
) {
	bool changed{ false };
	auto members{ ReflectMembers(text_data) };

	auto draw_member = [&](auto&& member) {
		const std::string normalized{
			NormalizeFeatureName(member.name)
		};

		if (
			normalized.contains("glyph") ||
			normalized.contains("clip")
		) {
			changed |= DrawValue(
				target.ctx,
				PrettyName(member.name),
				member.value
			);
		}
	};

	std::apply(
		[&](auto&&... member) {
			(draw_member(member), ...);
		},
		members
	);

	return changed;
}

template <typename T>
bool DrawFlattenedConfig(
	EditorContext& ctx,
	T& value
) {
	if constexpr (!ReflectedMembers<T>) {
		return DrawContents(ctx, value);
	} else {
		bool changed{ false };
		auto members{ ReflectMembers(value) };

		auto draw_member = [&](auto&& member) {
			using Member =
				std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{
				NormalizeFeatureName(member.name)
			};

			if (normalized == "config") {
				if constexpr (ReflectedMembers<Member>) {
					changed |= DrawMembers(
						ctx,
						member.value
					);
					return;
				}
			}

			changed |= DrawValue(
				ctx,
				PrettyName(member.name),
				member.value
			);
		};

		std::apply(
			[&](auto&&... member) {
				(draw_member(member), ...);
			},
			members
		);

		return changed;
	}
}

template <typename Target>
bool DrawLineWidthVisual(Target& target) {
	if constexpr (!Target::template Supports<FillStyle>()) {
		return false;
	} else {
		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<FillStyle>()) };

		auto before{ target.template Capture<FillStyle>() };
		bool enabled{ before.has_value() };
		bool changed{ false };

		if (ImGui::Checkbox("##Enabled", &enabled)) {
			target.template SetLive<FillStyle>(
				enabled
					? ComponentState<FillStyle>{ FillStyle{ kMinLineWidth } }
					: std::nullopt
			);
			changed = true;
		}

		ImGui::SameLine();

		float line_width{ kMinLineWidth };
		if (const auto current{ target.template Capture<FillStyle>() }) {
			line_width = current->GetLineWidth().value_or(kMinLineWidth);
		}

		{
			ScopedDisabled disabled{ !enabled };
			if (DrawValue(
				target.ctx,
				"Line Width",
				line_width,
				FieldOptions{
					.speed = 0.1f,
					.min = kMinLineWidth,
					.max = 1000.0f,
					.format = "%.2f",
					.flags = ImGuiSliderFlags_AlwaysClamp,
				}
			)) {
				target.template SetLive<FillStyle>(FillStyle{ line_width });
				changed = true;
			}
		}

		auto after{ target.template Capture<FillStyle>() };
		TrackComponentState(
			target,
			"Edit Line Width",
			std::move(before),
			std::move(after),
			changed
		);

		return changed;
	}
}

template <typename Target>
bool DrawShapeVisual(
	Target& target,
	std::string_view visual
) {
	bool changed{ false };

	auto draw_shape = [&]<typename T>() {
		changed |= DrawRequiredInlineVisualComponentWithDefault<Target, T>(
			target,
			TypeLabel<T>(),
			MakeDefaultShapeGeometry<T>(),
			[&target](T& value) {
				return DrawGeometryComponent(target, value);
			}
		);
	};

	if (visual == "rect") {
		draw_shape.template operator()<Rect>();
	} else if (visual == "circle") {
		draw_shape.template operator()<Circle>();
	} else if (visual == "roundedrect") {
		draw_shape.template operator()<RoundedRect>();
	} else if (visual == "polygon") {
		draw_shape.template operator()<Polygon>();
	} else if (visual == "ellipse") {
		draw_shape.template operator()<Ellipse>();
	} else if (visual == "triangle") {
		draw_shape.template operator()<Triangle>();
	} else if (visual == "line") {
		draw_shape.template operator()<Line>();
	} else if (visual == "capsule") {
		draw_shape.template operator()<Capsule>();
	} else if (visual == "arc") {
		draw_shape.template operator()<Arc>();
	}

	changed |= DrawOptionalVisualComponent<Target, Color>(
		target,
		"Color"
	);
	if (visual == "line") {
		changed |= DrawLineWidthVisual(target);
	} else {
		changed |= DrawOptionalVisualComponent<Target, FillStyle>(
			target,
			"Fill Style"
		);
	}

	return changed;
}

bool DrawAnimationDataFlattened(
	EditorContext& ctx,
	::ptgn::impl::AnimationData& animation
) {
	bool changed{ false };
	auto members{ ReflectMembers(animation) };

	auto draw_member = [&](auto&& member) {
		const std::string normalized{
			NormalizeFeatureName(member.name)
		};

		using Member = std::remove_cvref_t<
			decltype(member.value)
		>;

		if (normalized == "config") {
			if constexpr (ReflectedMembers<Member>) {
				auto config_members{
					ReflectMembers(member.value)
				};

				std::apply(
					[&](auto&&... config_member) {
						(
							(
								changed |= DrawValue(
									ctx,
									PrettyName(config_member.name),
									config_member.value
								)
							),
							...
						);
					},
					config_members
				);

				return;
			}
		}

		changed |= DrawValue(
			ctx,
			PrettyName(member.name),
			member.value
		);
	};

	std::apply(
		[&](auto&&... member) {
			(draw_member(member), ...);
		},
		members
	);

	if (ctx.local.settings.show_read_only_inspector_data) {
		[&]<typename T>(T& value) {
			if constexpr (ReflectedReadOnlyMembers<T>) {
				auto read_only_members{
					ReflectReadOnlyMembers(value)
				};

				std::apply(
					[&](auto&&... member) {
						(
							DrawReadOnlyValue(
								ctx,
								PrettyName(member.name),
								member.value
							),
							...
						);
					},
					read_only_members
				);
			}
		}(animation);
	}

	return changed;
}

template <typename Value>
[[nodiscard]] std::optional<V2_int> ExtractTexturePixelSize(const Value& value);

template <typename Value>
[[nodiscard]] std::optional<V2_int> ExtractTexturePixelSize(const Value& value) {
	using Type = std::remove_cvref_t<Value>;

	if constexpr (std::same_as<Type, V2_int>) {
		return value;
	} else if constexpr (std::same_as<Type, V2_float>) {
		return V2_int{
			static_cast<int>(std::round(value.x)),
			static_cast<int>(std::round(value.y))
		};
	} else if constexpr (ReflectedValue<Type>) {
		return ExtractTexturePixelSize(ReflectValue(value).value);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		if constexpr (std::tuple_size_v<decltype(members)> == 1) {
			return ExtractTexturePixelSize(std::get<0>(members).value);
		}
	}

	return std::nullopt;
}

template <typename Value>
bool DrawTextureSizeAsIntegers(EditorContext& ctx, Value& value) {
	using Type = std::remove_cvref_t<Value>;

	if constexpr (std::same_as<Type, V2_int>) {
		return DrawValue(ctx, "Texture Size", value);
	} else if constexpr (std::same_as<Type, V2_float>) {
		V2_int displayed{
			static_cast<int>(std::round(value.x)),
			static_cast<int>(std::round(value.y))
		};

		if (!DrawValue(ctx, "Texture Size", displayed)) {
			return false;
		}

		value = V2_float{
			static_cast<float>(displayed.x),
			static_cast<float>(displayed.y)
		};
		return true;
	} else if constexpr (ReflectedValue<Type>) {
		auto member{ ReflectValue(value) };
		return DrawTextureSizeAsIntegers(ctx, member.value);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		if constexpr (std::tuple_size_v<decltype(members)> == 1) {
			return DrawTextureSizeAsIntegers(ctx, std::get<0>(members).value);
		} else {
			return DrawComponentContents(ctx, value);
		}
	} else {
		return DrawComponentContents(ctx, value);
	}
}

template <typename Target>
[[nodiscard]] std::optional<V2_int> ResolveAnimationTextureSize(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity) {
			const auto texture_size{ GetTextureSize(entity) };
			return texture_size && texture_size->IsPositive()
				? texture_size
				: std::nullopt;
		}
	}

	if constexpr (Target::template Supports<::ptgn::impl::TextureSize>()) {
		if (const auto texture_size{ target.template Capture<::ptgn::impl::TextureSize>() }) {
			const auto pixels{ ExtractTexturePixelSize(*texture_size) };
			return pixels && pixels->IsPositive() ? pixels : std::nullopt;
		}
	}

	return std::nullopt;
}

template <typename Target>
bool SynchronizeAnimationFrameData(Target& target, std::string_view reason) {
	using AnimationData = ::ptgn::impl::AnimationData;
	using TextureCrop = ::ptgn::impl::TextureCrop;

	if constexpr (!Target::template Supports<AnimationData>()) {
		return false;
	} else {
		auto before_animation{ target.template Capture<AnimationData>() };

		if (!before_animation) {
			return false;
		}

		AnimationData animation{ *before_animation };
		const auto texture_size{ ResolveAnimationTextureSize(target) };
		animation.config.frame_size =
			::ptgn::impl::GetFrameSize(texture_size, animation.config.frame_count);

		if (animation.config.frame_count == 0) {
			animation.current_frame = 0;
		} else {
			animation.current_frame %= animation.config.frame_count;
		}

		animation.frame_dirty = true;
		target.template SetLive<AnimationData>(animation);
		auto after_animation{ target.template Capture<AnimationData>() };
		TrackComponentState(
			target,
			reason,
			std::move(before_animation),
			after_animation,
			true
		);

		if constexpr (Target::template Supports<TextureCrop>()) {
			auto before_crop{ target.template Capture<TextureCrop>() };
			TextureCrop crop{ before_crop.value_or(TextureCrop{}) };
			crop.Update(animation, texture_size);
			target.template SetLive<TextureCrop>(crop);
			auto after_crop{ target.template Capture<TextureCrop>() };
			TrackComponentState(
				target,
				"Update Animation Texture Crop",
				std::move(before_crop),
				std::move(after_crop),
				true
			);
		}

		return true;
	}
}

template <typename Target>
bool SynchronizeAnimationTextureCrop(Target& target) {
	using AnimationData = ::ptgn::impl::AnimationData;
	using TextureCrop = ::ptgn::impl::TextureCrop;

	if constexpr (
		!Target::template Supports<AnimationData>() ||
		!Target::template Supports<TextureCrop>()
	) {
		return false;
	} else {
		const auto animation{ target.template Capture<AnimationData>() };

		if (!animation) {
			return false;
		}

		auto before_crop{ target.template Capture<TextureCrop>() };
		TextureCrop crop{ before_crop.value_or(TextureCrop{}) };
		crop.Update(*animation, ResolveAnimationTextureSize(target));
		target.template SetLive<TextureCrop>(crop);
		auto after_crop{ target.template Capture<TextureCrop>() };
		TrackComponentState(
			target,
			"Update Animation Texture Crop",
			std::move(before_crop),
			std::move(after_crop),
			true
		);
		return true;
	}
}

template <typename Target>
bool DisableAnimationTextureCrop(Target& target) {
	using TextureCrop = ::ptgn::impl::TextureCrop;

	if constexpr (!Target::template Supports<TextureCrop>()) {
		return false;
	} else {
		auto before_crop{ target.template Capture<TextureCrop>() };

		if (!before_crop) {
			return false;
		}

		target.template SetLive<TextureCrop>(std::nullopt);
		auto after_crop{ target.template Capture<TextureCrop>() };
		TrackComponentState(
			target,
			"Disable Texture Crop",
			std::move(before_crop),
			std::move(after_crop),
			true
		);
		return true;
	}
}

template <typename Target>
bool DrawSpritePrimary(Target& target) {
	using AnimationData = ::ptgn::impl::AnimationData;

	bool changed{ false };
	const auto before_texture{ target.template Capture<TextureKey>() };
	const auto before_animation{ target.template Capture<AnimationData>() };

	changed |= DrawRequiredInlineVisualComponent<Target, TextureKey>(
		target,
		"Texture Key",
		[&target](TextureKey& value) {
			return DrawValue(target.ctx, "Texture Key", value);
		}
	);

	changed |= DrawOptionalComponent<Target, ::ptgn::impl::TextureSize>(
		target,
		"Texture Size",
		false,
		[&target](::ptgn::impl::TextureSize& value) {
			return DrawTextureSizeAsIntegers(target.ctx, value);
		}
	);

	changed |= DrawOptionalComponent<Target, AnimationData>(
		target,
		"Animation",
		true,
		[&target](AnimationData& value) {
			const std::size_t before_frame_count{ value.config.frame_count };
			const bool local_changed{ DrawAnimationDataFlattened(target.ctx, value) };

			if (local_changed && before_frame_count != value.config.frame_count) {
				value.config.frame_size = ::ptgn::impl::GetFrameSize(
					ResolveAnimationTextureSize(target),
					value.config.frame_count
				);
				value.current_frame = value.config.frame_count == 0
					? 0
					: value.current_frame % value.config.frame_count;
				value.frame_dirty = true;
			}

			return local_changed;
		}
	);

	const auto after_texture{ target.template Capture<TextureKey>() };
	const auto after_animation{ target.template Capture<AnimationData>() };
	const bool texture_changed{ before_texture != after_texture };
	const bool animation_enabled{ after_animation.has_value() };
	const bool animation_was_enabled{ before_animation.has_value() };
	const bool frame_count_changed{
		before_animation && after_animation &&
		before_animation->config.frame_count != after_animation->config.frame_count
	};

	if (!animation_enabled && animation_was_enabled) {
		changed |= DisableAnimationTextureCrop(target);
	} else if (animation_enabled && texture_changed) {
		changed |= SynchronizeAnimationFrameData(
			target,
			"Recalculate Animation Frame Size From Texture"
		);
	} else if (animation_enabled && (!animation_was_enabled || frame_count_changed)) {
		changed |= SynchronizeAnimationTextureCrop(target);
	}

	return changed;
}

template <typename Target>
bool DrawMaterialDetails(Target& target, ::ptgn::Material& material) {
	bool changed{ false };

	ImGui::SeparatorText("Uniforms");
	changed |= DrawVectorEditor(
		target.ctx,
		material.uniforms,
		VectorOptions{
			.item_name = "Uniform",
		}
	);

	const std::size_t max_texture_slots{
		std::max(
			std::size_t{ 1 },
			static_cast<std::size_t>(
				target.ctx.editor.GetMaxTextureSlots()
			)
		)
	};

	changed |= DrawValue(
		target.ctx,
		"Texture Slot Capacity",
		material.texture_slot_capacity,
		FieldOptions{
			.speed = 1.0f,
			.min = 1.0f,
			.max = static_cast<float>(max_texture_slots),
			.format = "%llu",
			.flags = ImGuiSliderFlags_AlwaysClamp,
		}
	);

	if (material.texture_slot_capacity.has_value()) {
		const std::size_t clamped{
			std::clamp(
				*material.texture_slot_capacity,
				std::size_t{ 1 },
				max_texture_slots
			)
		};

		if (*material.texture_slot_capacity != clamped) {
			material.texture_slot_capacity = clamped;
			changed = true;
		}
	}

	return changed;
}

template <typename Target>
bool DrawCustomShaderPrimary(Target& target) {
	bool changed{ false };

	changed |= DrawRequiredComponent<Target, ::ptgn::Material>(
		target,
		"Material Shader",
		false,
		[&target](::ptgn::Material& material) {
			return DrawValue(
				target.ctx,
				"Shader Key",
				material.shader
			);
		}
	);

	changed |= DrawOptionalComponent<Target, TextureKey>(
		target,
		"Texture Key",
		false,
		[&target](TextureKey& value) {
			return DrawValue(target.ctx, "Texture Key", value);
		}
	);

	changed |= DrawRequiredComponent<Target, ::ptgn::Material>(
		target,
		"Material",
		false,
		[&target](::ptgn::Material& material) {
			return DrawMaterialDetails(target, material);
		}
	);

	return changed;
}

template <typename Target>
bool DrawSpriteAdditional(Target& target) {
	bool changed{ false };
	const bool animated{
		target.template Capture<
			::ptgn::impl::AnimationData
		>().has_value()
	};

	changed |= DrawOptionalVisualComponent<
		Target,
		::ptgn::impl::Offsets
	>(
		target,
		"Offsets",
		true
	);

	if (animated) {
		changed |= DrawReadOnlyExistingReflected<
			Target,
			::ptgn::impl::TextureCrop
		>(
			target,
			"Texture Crop",
			true
		);
	} else {
		changed |= DrawOptionalVisualComponent<
			Target,
			::ptgn::impl::TextureCrop
		>(
			target,
			"Texture Crop",
			true
		);
	}

	return changed;
}

template <typename Target, typename T>
bool DrawOptionalNamedValue(
	Target& target,
	std::string_view label
) {
	return DrawOptionalComponent<Target, T>(
		target,
		label,
		false,
		[&target, label](T& value) {
			return [&target, label]<typename Value>(Value& reflected_value) {
				if constexpr (ReflectedValue<Value>) {
					auto member{
						ReflectValue(reflected_value)
					};
					return DrawValue(
						target.ctx,
						label,
						member.value
					);
				} else if constexpr (ReflectedMembers<Value>) {
					auto members{
						ReflectMembers(reflected_value)
					};

					if constexpr (
						std::tuple_size_v<
							decltype(members)
						> == 1
					) {
						auto& member{
							std::get<0>(members)
						};
						return DrawValue(
							target.ctx,
							label,
							member.value
						);
					} else {
						return DrawMembers(
							target.ctx,
							reflected_value
						);
					}
				} else {
					return DrawComponentContents(
						target.ctx,
						reflected_value
					);
				}
			}(value);
		}
	);
}

template <typename Target>
bool DrawRenderTargetPrimary(Target& target) {
	bool changed{ false };

	changed |= DrawOptionalVisualComponent<
		Target,
		::ptgn::impl::RenderTargetSize
	>(
		target,
		"Render Target Size"
	);
	changed |= DrawOptionalNamedValue<
		Target,
		::ptgn::impl::ClearColor
	>(
		target,
		"Clear Color"
	);
	changed |= DrawOptionalNamedValue<
		Target,
		::ptgn::impl::ClearDepth
	>(
		target,
		"Clear Depth"
	);
	changed |= DrawOptionalNamedValue<
		Target,
		::ptgn::impl::ClearStencil
	>(
		target,
		"Clear Stencil"
	);

	return changed;
}

template <typename Target>
bool DrawVisualAdditionalOptions(
	Target& target,
	std::string_view visual,
	bool draw_tint
) {
	const bool open{
		ImGui::TreeNodeEx(
			"Additional Options##Visual",
			ImGuiTreeNodeFlags_SpanAvailWidth
		)
	};

	if (!open) {
		return false;
	}

	bool changed{ false };

	{
		ScopedUnindent align_with_additional_options;

		if (visual.contains("sprite")) {
			changed |= DrawSpriteAdditional(target);
		}

		if (visual == "rect" || visual == "circle") {
			changed |= DrawOptionalVisualComponent<
				Target,
				::ptgn::impl::ShadowCaster
			>(
				target,
				"Shadow Caster",
				true
			);
		}

		if (visual.contains("text")) {
			ScopedID text_additional_scope{ "TextAdditional" };
			changed |= DrawRequiredComponent<
				Target,
				::ptgn::impl::TextData
			>(
				target,
				"Text Additional Options",
				false,
				[&target](::ptgn::impl::TextData& value) {
					return DrawTextAdditional(target, value);
				},
				&MarkTextLayoutDirty
			);
		}

		changed |= DrawOptionalVisualComponent<Target, BlendMode>(
			target,
			"Blend Mode"
		);

		if (draw_tint) {
			changed |= DrawOptionalVisualComponent<Target, Tint>(
				target,
				"Tint"
			);
			changed |= DrawOptionalVisualComponent<
				Target,
				::ptgn::impl::IgnoreParentTint
			>(
				target,
				"Ignore Parent Tint"
			);
		}

		changed |= DrawOptionalVisualComponent<
			Target,
			::ptgn::impl::IgnoreParentVisibility
		>(
			target,
			"Ignore Parent Visibility"
		);
		changed |= DrawOptionalVisualComponent<
			Target,
			::ptgn::impl::RenderMask
		>(
			target,
			"Render Layer"
		);

		{
			ScopedID ui_layer_scope{ "VisualUILayer" };
			changed |= DrawOptionalVisualComponent<
				Target,
				::ptgn::impl::UILayer
			>(
				target,
				"UI Layer"
			);
		}
	}

	ImGui::TreePop();
	return changed;
}

template <
	typename Target,
	typename Visuals,
	typename Draw,
	typename Callback
>
bool DrawButtonChildStateVisualComponent(
	Target& target,
	ButtonVisualState state,
	std::string_view part_label,
	Draw&& draw,
	Callback callback
) {
	if constexpr (!Target::template Supports<Visuals>()) {
		return false;
	} else {
		const bool open{
			ImGui::CollapsingHeader(
				"Visual##ButtonVisualStateVisual",
				ImGuiTreeNodeFlags_DefaultOpen
			)
		};

		if (!open) {
			return false;
		}

		ScopedIndent feature_indent;
		AutoLabelWidthScope label_width{ "ButtonVisualStateVisualFields" };
		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<Visuals>()) };

		auto before{ target.template Capture<Visuals>() };
		Visuals visuals{ before.value_or(Visuals{}) };
		const auto index{
			static_cast<std::size_t>(std::to_underlying(state))
		};
		auto& visual{ visuals.states[index] };

		DrawDisabledWrappedText(
			PrettyName(magic_enum::enum_name(state)) + " " +
			std::string{ part_label } +
			" overrides. Unset values inherit from fallback states."
		);

		const bool changed{
			std::invoke(
				std::forward<Draw>(draw),
				visuals,
				visual
			)
		};

		if (changed) {
			visual.defined = HasButtonVisualOverrides(visual);
			target.template SetLive<Visuals>(
				ComponentState<Visuals>{ visuals },
				callback
			);
		}

		auto after{ target.template Capture<Visuals>() };
		TrackComponentState(
			target,
			std::string{ "Edit " } + std::string{ part_label } + " State Visual",
			std::move(before),
			std::move(after),
			changed,
			callback
		);

		return changed;
	}
}

template <typename Target, typename Locator>
bool DrawPickableButtonTextBoxPosition(
	Target& target,
	ButtonTextVisuals& visuals,
	const ButtonChildInfo& child_info,
	ButtonVisualState state,
	std::string_view label,
	Locator locator
) {
	V2_float& position{ locator(visuals) };

	return DrawPropertyRow(label, [&]() {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float pick_width{
			ImGui::CalcTextSize("Pick").x +
			ImGui::GetStyle().FramePadding.x * 2.0f
		};
		const float available{ ImGui::GetContentRegionAvail().x };
		const float field_width{
			std::max(36.0f, (available - pick_width - spacing * 2.0f) * 0.5f)
		};
		bool changed{ false };

		ImGui::SetNextItemWidth(field_width);
		changed |= ImGui::DragFloat(
			"##X", &position.x, 0.1f, 0.0f, 0.0f, "X %.2f"
		);
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		changed |= ImGui::DragFloat(
			"##Y", &position.y, 0.1f, 0.0f, 0.0f, "Y %.2f"
		);
		ImGui::SameLine(0.0f, spacing);

		auto apply{
			target.template MakeApply<ButtonTextVisuals>(
				&MarkButtonTextDirty
			)
		};
		ButtonTextVisuals snapshot{ visuals };

		(void)DrawPositionPickButton(
			target.ctx,
			label,
			position,
			MakeButtonTextBoxPositionConverter(
				child_info.child,
				child_info.button,
				state
			),
			[apply, snapshot = std::move(snapshot), locator](
				V2_float picked
			) mutable {
				locator(snapshot) = picked;
				apply(ComponentState<ButtonTextVisuals>{ snapshot });
			},
			GetButtonTextBoxWorldPosition(
				child_info.child,
				child_info.button,
				state,
				position
			),
			true
		);

		return changed;
	});
}

template <typename Target>
bool DrawButtonTextBoxOverride(
	Target& target,
	ButtonTextVisuals& visuals,
	ButtonTextVisual& visual,
	const ButtonChildInfo& child_info,
	ButtonVisualState state
) {
	const auto index{
		static_cast<std::size_t>(std::to_underlying(state))
	};
	const std::optional<TextBox> inherited{
		ResolveButtonVisualProperty(
			visuals.states,
			state,
			&ButtonTextVisual::box
		)
	};
	bool enabled{ visual.box.has_value() };
	bool changed{ false };

	ScopedID box_scope{ "ButtonTextBoxOverride" };

	if (ImGui::Checkbox("##Enabled", &enabled)) {
		if (enabled) {
			visual.box = inherited.value_or(TextBox{});
			visual.defined = true;
		} else {
			visual.box.reset();
		}
		changed = true;
	}

	ImGui::SameLine();
	const bool open{
		ImGui::TreeNodeEx(
			"Text Box##ButtonTextBoxTree",
			ImGuiTreeNodeFlags_SpanAvailWidth
		)
	};

	if (!open) {
		return changed;
	}

	ScopedIndent indent;
	TextBox displayed{
		visual.box.value_or(inherited.value_or(TextBox{}))
	};
	TextBox& box{ enabled ? visual.box.value() : displayed };
	ScopedDisabled disabled{ !enabled };

	auto members{ ReflectMembers(box) };
	auto draw_member = [&](auto&& member) {
		const std::string normalized{ NormalizeFeatureName(member.name) };

		if (normalized == "rect") {
			using Member = std::remove_cvref_t<decltype(member.value)>;
			if constexpr (std::same_as<Member, Rect>) {
				V2_float size{ member.value.GetSize() };
				if (DrawValue(
					target.ctx,
					"Size",
					size,
					FieldOptions{
						.speed = 0.1f,
						.format = "%.3f",
					}
				)) {
					size.x = std::max(size.x, 0.0f);
					size.y = std::max(size.y, 0.0f);
					const V2_float center{ member.value.GetCenter() };
					const V2_float half_size{ size * 0.5f };
					member.value.min = center - half_size;
					member.value.max = center + half_size;
					changed = true;
				}

				if (enabled) {
					auto min_locator = [index](ButtonTextVisuals& root) -> V2_float& {
						return root.states[index].box.value().rect.min;
					};
					auto max_locator = [index](ButtonTextVisuals& root) -> V2_float& {
						return root.states[index].box.value().rect.max;
					};
					changed |= DrawPickableButtonTextBoxPosition(
						target,
						visuals,
						child_info,
						state,
						"Min",
						min_locator
					);
					changed |= DrawPickableButtonTextBoxPosition(
						target,
						visuals,
						child_info,
						state,
						"Max",
						max_locator
					);
				} else {
					changed |= DrawValue(target.ctx, "Min", member.value.min);
					changed |= DrawValue(target.ctx, "Max", member.value.max);
				}
			}
			return;
		}

		if (normalized == "style") {
			if (ImGui::TreeNodeEx(
					"Additional Options##ButtonTextBoxAdditionalOptions",
					ImGuiTreeNodeFlags_SpanAvailWidth
				)) {
				{
					ScopedUnindent align_with_additional_options;
					changed |= DrawComponentContents(
						target.ctx,
						member.value
					);
				}
				ImGui::TreePop();
			}
			return;
		}

		changed |= DrawValue(
			target.ctx,
			PrettyName(member.name),
			member.value
		);
	};

	std::apply(
		[&](auto&&... member) {
			(draw_member(member), ...);
		},
		members
	);

	ImGui::TreePop();
	return changed;
}

template <typename Target>
bool DrawButtonChildStateVisualFeature(
	Target& target,
	const ButtonChildInfo& child_info,
	ButtonVisualState state
) {
	switch (child_info.part) {
		case ButtonChildPart::Background:
			return DrawButtonChildStateVisualComponent<
				Target,
				ButtonBackgroundVisuals
			>(
				target,
				state,
				"Button Background",
				[&target, state](auto& visuals, ButtonShapeVisual&) {
					bool changed{ false };
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Size", visuals.states, state, &ButtonShapeVisual::size
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonShapeVisual::origin
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonShapeVisual::anchor
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Color", visuals.states, state, &ButtonShapeVisual::color
					);
					return changed;
				},
				&MarkButtonBackgroundDirty
			);
		case ButtonChildPart::Border:
			return DrawButtonChildStateVisualComponent<
				Target,
				ButtonBorderVisuals
			>(
				target,
				state,
				"Button Border",
				[&target, state](auto& visuals, ButtonShapeVisual&) {
					bool changed{ false };
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Size", visuals.states, state, &ButtonShapeVisual::size
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonShapeVisual::origin
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonShapeVisual::anchor
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Color", visuals.states, state, &ButtonShapeVisual::color
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Fill Style", visuals.states, state, &ButtonShapeVisual::fill_style
					);
					return changed;
				},
				&MarkButtonBorderDirty
			);
		case ButtonChildPart::Text:
			return DrawButtonChildStateVisualComponent<
				Target,
				ButtonTextVisuals
			>(
				target,
				state,
				"Button Text",
				[&target, &child_info, state](ButtonTextVisuals& visuals, ButtonTextVisual& visual) {
					bool changed{ false };
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Content", visuals.states, state, &ButtonTextVisual::styled_text
					);
					changed |= DrawButtonTextBoxOverride(
						target,
						visuals,
						visual,
						child_info,
						state
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonTextVisual::origin
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonTextVisual::anchor
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Auto Box", visuals.states, state, &ButtonTextVisual::auto_box
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Padding", visuals.states, state, &ButtonTextVisual::padding
					);
					return changed;
				},
				&MarkButtonTextDirty
			);
		case ButtonChildPart::Sprite:
			return DrawButtonChildStateVisualComponent<
				Target,
				ButtonSpriteVisuals
			>(
				target,
				state,
				"Button Sprite",
				[&target, state](auto& visuals, ButtonSpriteVisual&) {
					bool changed{ false };
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Texture Key", visuals.states, state, &ButtonSpriteVisual::texture
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonSpriteVisual::origin
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonSpriteVisual::anchor
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Size", visuals.states, state, &ButtonSpriteVisual::size
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Tint", visuals.states, state, &ButtonSpriteVisual::tint
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Animation", visuals.states, state, &ButtonSpriteVisual::animation
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Animation Options", visuals.states, state,
						&ButtonSpriteVisual::animation_options
					);
					return changed;
				},
				&MarkButtonSpriteDirty
			);
	}

	return false;
}

template <typename Target>
bool DrawVisualFeature(Target& target) {
	if (const auto child_info{ GetButtonChildInfo(target) }) {
		if (const auto state{ GetButtonVisualEditState(target) }) {
			return DrawButtonChildStateVisualFeature(
				target,
				*child_info,
				*state
			);
		}
	}

	if (!HasVisualFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target,
		InspectorFeature::Visual,
		"Visual",
		ImGuiTreeNodeFlags_DefaultOpen,
		VisualFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;
	AutoLabelWidthScope visual_label_width{ "VisualFeatureFields" };

	bool changed{ header.changed };
	const RendererRowResult renderer{ DrawRendererRow(target) };
	const std::string& visual{ renderer.visual };
	changed |= renderer.changed;

	if (renderer.changed) {
		return true;
	}

	if (visual.empty()) {
		ImGui::TextDisabled(
			"Choose a renderer to expose its relevant components."
		);
		return changed;
	}

	changed |= DrawVisualEffects(target, visual);

	bool draw_tint{ false };
	const bool shape{
		visual == "rect" ||
		visual == "circle" ||
		visual == "roundedrect" ||
		visual == "polygon" ||
		visual == "ellipse" ||
		visual == "triangle" ||
		visual == "line" ||
		visual == "capsule" ||
		visual == "arc"
	};

	if (shape) {
		changed |= DrawShapeVisual(target, visual);
	} else if (visual.contains("sprite")) {
		changed |= DrawSpritePrimary(target);
		draw_tint = true;
	} else if (visual.contains("text")) {
		ScopedID text_primary_scope{ "TextPrimary" };
		changed |= DrawRequiredInlineVisualComponent<
			Target,
			::ptgn::impl::TextData
		>(
			target,
			"Text",
			[&target](::ptgn::impl::TextData& value) {
				return DrawTextPrimary(target, value);
			},
			&MarkTextLayoutDirty
		);
		draw_tint = true;
	} else if (visual.contains("particle")) {
		changed |= DrawRequiredInlineVisualComponent<
			Target,
			::ptgn::impl::ParticleEmitterData
		>(
			target,
			"Particle Emitter",
			[&target](::ptgn::impl::ParticleEmitterData& value) {
				return DrawFlattenedConfig(target.ctx, value);
			}
		);
	} else if (visual.contains("light")) {
		changed |= DrawRequiredInlineVisualComponent<
			Target,
			LightData
		>(
			target,
			"Light",
			[&target](LightData& value) {
				return DrawContents(target.ctx, value);
			}
		);
	} else if (visual.contains("customshader")) {
		changed |= DrawCustomShaderPrimary(target);
	} else if (visual.contains("graphics")) {
		changed |= DrawRequiredInlineVisualComponent<
			Target,
			::ptgn::impl::GraphicsData
		>(
			target,
			"Graphics",
			[&target](::ptgn::impl::GraphicsData& value) {
				return DrawContents(target.ctx, value);
			}
		);
		changed |= DrawOptionalValue<Target, ShaderKey>(
			target,
			"Shader"
		);
	} else if (visual.contains("rendertarget")) {
		changed |= DrawRenderTargetPrimary(target);
	}

	changed |= DrawOptionalVisualComponent<Target, Origin>(
		target,
		"Origin"
	);
	changed |= DrawVisualAdditionalOptions(
		target,
		visual,
		draw_tint
	);

	return changed;
}

template <typename Target>
bool DrawReadOnlyInteractionLock(Target& target) {
	if (!target.ctx.local.settings.show_read_only_inspector_data) {
		return false;
	}

	if constexpr (!Target::template Supports<InteractionLock>()) {
		return false;
	} else {
		auto state{ target.template Capture<InteractionLock>() };
		bool enabled{ state.has_value() };

		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<InteractionLock>()) };

		{
			ScopedDisabled disabled{ true };
			ImGui::Checkbox("##Enabled", &enabled);
		}

		DrawTooltip("Interaction Lock is managed by the runtime.");
		ImGui::SameLine();

		if (!state) {
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Interaction Lock");
			return false;
		}

		if (ImGui::TreeNodeEx(
				"Interaction Lock##ReadOnlyInteractionLock",
				ImGuiTreeNodeFlags_SpanAvailWidth
			)) {
			ScopedIndent indent;
			AutoLabelWidthScope label_width{ "InteractionLockReadOnlyFields" };
			ReadOnlyScope read_only{ true };

			[&]<typename T>(const T& value) {
				if constexpr (ReflectedMembers<T>) {
					auto members{ ReflectMembers(value) };
					std::apply(
						[&](auto&&... member) {
							(DrawReadOnlyValue(
								target.ctx,
								PrettyName(member.name),
								member.value
							), ...);
						},
						members
					);
				}

				if constexpr (ReflectedReadOnlyMembers<T>) {
					auto members{ ReflectReadOnlyMembers(value) };
					std::apply(
						[&](auto&&... member) {
							(DrawReadOnlyValue(
								target.ctx,
								PrettyName(member.name),
								member.value
							), ...);
						},
						members
					);
				}
			}(*state);

			ImGui::TreePop();
		}

		return false;
	}
}

template <typename Target>
bool DrawInteractionFeature(Target& target) {
	if (!HasInteractionFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::Interaction, "Interaction", ImGuiTreeNodeFlags_None,
		InteractionFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	bool changed{ header.changed };

	changed |=
		DrawOptionalReflected<Target, ::ptgn::impl::Interactive>(target, "Interactive", true);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::Draggable>(target, "Draggable", true);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::Dropzone>(target, "Dropzone", true);
	changed |= DrawReadOnlyInteractionLock(target);

	return changed;
}

template <typename Target>
bool DrawRigidBodyWithInheritance(Target& target) {
	bool inheritance_changed{ false };

	const bool changed{
		DrawOptionalComponent<Target, RigidBody>(
			target,
			"Rigid Body",
			true,
			[&](RigidBody& value) {
				const bool rigid_body_changed{
					DrawComponentContents(target.ctx, value)
				};

				inheritance_changed |= DrawOptionalReflected<
					Target,
					::ptgn::impl::IgnoreParentImmovable
				>(
					target,
					"Ignore Parent Immovable",
					false
				);

				return rigid_body_changed;
			}
		)
	};

	if (
		!target.template Capture<RigidBody>() &&
		target.template Capture<
			::ptgn::impl::IgnoreParentImmovable
		>()
	) {
		auto before{
			target.template Capture<
				::ptgn::impl::IgnoreParentImmovable
			>()
		};
		target.template SetLive<
			::ptgn::impl::IgnoreParentImmovable
		>(std::nullopt);
		auto after{
			target.template Capture<
				::ptgn::impl::IgnoreParentImmovable
			>()
		};

		TrackComponentState(
			target,
			"Remove Ignore Parent Immovable",
			std::move(before),
			std::move(after),
			true
		);

		inheritance_changed = true;
	}

	return changed || inheritance_changed;
}

template <typename Target>
bool DrawPhysicsFeature(Target& target) {
	if (!HasPhysicsFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::Physics, "Physics & Movement", ImGuiTreeNodeFlags_None,
		PhysicsFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	bool changed{ header.changed };

	changed |= DrawOptionalComponent<Target, Collider>(
		target,
		"Collider",
		true,
		[&target](Collider& value) {
			return DrawGeometryComponent(target, value);
		}
	);
	changed |= DrawRigidBodyWithInheritance(target);
	changed |= DrawOptionalReflected<Target, BoundaryBehavior>(
		target,
		"Boundary Behavior",
		false
	);

	enum class MovementKind {
		None,
		TopDown,
		Platformer,
	};

	MovementKind movement{ target.template Capture<TopDownMovement>() ? MovementKind::TopDown
						   : target.template Capture<PlatformerMovement>()
							   ? MovementKind::Platformer
							   : MovementKind::None };

	const char* preview{ movement == MovementKind::TopDown		? "Top Down"
						 : movement == MovementKind::Platformer ? "Platformer"
																: "None" };

	if (ImGui::BeginCombo("Movement", preview)) {
		auto choose = [&](MovementKind candidate, const char* label) {
			if (!ImGui::Selectable(label, movement == candidate)) {
				return;
			}

			if (candidate == MovementKind::TopDown) {
				AddFeature<Target, TopDownMovement>(target, "Top Down Movement");
				target.template SetLive<PlatformerMovement>(std::nullopt);
			} else if (candidate == MovementKind::Platformer) {
				AddFeature<Target, PlatformerMovement>(target, "Platformer Movement");
				target.template SetLive<TopDownMovement>(std::nullopt);
			} else {
				target.template SetLive<TopDownMovement>(std::nullopt);
				target.template SetLive<PlatformerMovement>(std::nullopt);
			}

			changed = true;
		};

		choose(MovementKind::None, "None");
		choose(MovementKind::TopDown, "Top Down");
		choose(MovementKind::Platformer, "Platformer");
		ImGui::EndCombo();
	}

	if (target.template Capture<TopDownMovement>()) {
		changed |= DrawRequiredComponent<Target, TopDownMovement>(
			target, "Top Down Movement", true,
			[&target](TopDownMovement& value) { return DrawComponentContents(target.ctx, value); }
		);
	}

	if (target.template Capture<PlatformerMovement>()) {
		changed |= DrawRequiredComponent<Target, PlatformerMovement>(
			target, "Platformer Movement", true, [&target](PlatformerMovement& value) {
				return DrawComponentContents(target.ctx, value);
			}
		);
		changed |= DrawOptionalReflected<Target, PlatformerJump>(target, "Platformer Jump", true);
	}

	return changed;
}

bool DrawButtonVisualStateSelector(std::optional<ButtonVisualState>& state) {
	return DrawPropertyRow(
		"Visual State",
		[&]() {
			const std::string preview{
				state
					? PrettyName(magic_enum::enum_name(*state))
					: "Base Entity"
			};

			ImGui::SetNextItemWidth(-FLT_MIN);

			if (!ImGui::BeginCombo("##ButtonVisualState", preview.c_str())) {
				return false;
			}

			bool changed{ false };
			const bool base_selected{ !state };

			if (ImGui::Selectable("Base Entity", base_selected)) {
				state.reset();
				changed = !base_selected;
			}

			ImGui::Separator();

			for (const auto candidate : magic_enum::enum_values<ButtonVisualState>()) {
				const bool selected{ state && *state == candidate };
				const std::string label{
					PrettyName(magic_enum::enum_name(candidate))
				};

				if (ImGui::Selectable(label.c_str(), selected)) {
					state = candidate;
					changed = !selected;
				}
			}

			ImGui::EndCombo();
			return changed;
		}
	);
}

template <typename Target>
bool DrawUIFeature(Target& target) {
	if (!HasUIFeature(target)) {
		return false;
	}

	if (const auto child_info{ GetButtonChildInfo(target) }) {
		const bool open{
			ImGui::CollapsingHeader(
				"UI##ButtonChildUI",
				ImGuiTreeNodeFlags_None
			)
		};

		if (!open) {
			return false;
		}

		ScopedIndent feature_indent;
		auto& editor_state{
			GetManualFeatureState(target.GetFeatureTargetKey())
		};
		(void)DrawButtonVisualStateSelector(
			editor_state.button_visual_state
		);

		if (editor_state.button_visual_state) {
			DrawDisabledWrappedText(
				"Transform and Visual edit the selected state. Unset values inherit from fallback states."
			);
		} else {
			DrawDisabledWrappedText(
				"Transform and Visual edit the base child entity."
			);
		}

		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::UI, "UI", ImGuiTreeNodeFlags_None, UIFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	bool changed{ header.changed };

	changed |= DrawOptionalReflected<Target, ::ptgn::impl::ButtonData>(target, "Button", true);


	changed |= DrawOptionalReflected<Target, ::ptgn::impl::ToggleButtonData>(
		target, "Toggle Button", true
	);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::ToggleButtonGroupData>(
		target, "Toggle Group", true
	);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::ToggleButtonGroupItem>(
		target, "Toggle Group Item", true
	);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::DropdownData>(target, "Dropdown", true);
	changed |=
		DrawOptionalReflected<Target, ::ptgn::impl::DropdownItem>(target, "Dropdown Item", true);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::TooltipData>(target, "Tooltip", true);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::TooltipHoverData>(
		target, "Tooltip Hover", true
	);

	return changed;
}

template <typename Target>
bool DrawCameraFeature(Target& target) {
	if (!HasCameraFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target,
		InspectorFeature::Camera,
		"Camera",
		ImGuiTreeNodeFlags_None,
		CameraFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;
	AutoLabelWidthScope camera_label_width{ "CameraFeatureFields" };

	bool changed{ header.changed };
	changed |= DrawOptionalComponent<
		Target,
		::ptgn::impl::CameraData
	>(
		target,
		"Camera",
		false,
		[&target](::ptgn::impl::CameraData& value) {
			return DrawContents(target.ctx, value);
		}
	);
	changed |= DrawRequiredComponent<
		Target,
		::ptgn::impl::CameraMask
	>(
		target,
		"Layers",
		false,
		[](::ptgn::impl::CameraMask& value) {
			bool masks_changed{ DrawLayerMaskValue("Include Layer", value.include) };
			masks_changed |= DrawLayerMaskValue("Exclude Layer", value.exclude);
			return masks_changed;
		}
	);

	return changed;
}

template <typename Target>
bool DrawScriptsFeature(Target& target) {
	if (!HasScriptsFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::Scripts, "Scripts", ImGuiTreeNodeFlags_None,
		ScriptsFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	bool changed{ header.changed };
	changed |= DrawRequiredComponent<Target, ::ptgn::impl::Scripts>(
		target, "Scripts", false,
		[&target](::ptgn::impl::Scripts& value) { return DrawComponentContents(target.ctx, value); }
	);
	return changed;
}

template <typename Target>
bool DrawUtilitiesFeature(Target& target) {
	if (!HasUtilitiesFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::Utilities, "Utilities", ImGuiTreeNodeFlags_None,
		UtilitiesFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	bool changed{ header.changed };

	changed |= DrawOptionalReflected<Target, Lifetime>(target, "Lifetime", true);

	return changed;
}

template <typename Target, typename... T>
[[nodiscard]] consteval bool SupportsAnyFeatureComponent(FeatureComponents<T...>) {
	return (Target::template Supports<T>() || ...);
}

template <typename Target>
bool DrawAddFeatureMenu(Target& target) {
	bool changed{ false };

	if (ImGui::Button("Add Feature", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddFeaturePopup");
	}

	if (!ImGui::BeginPopup("AddFeaturePopup")) {
		return false;
	}

	auto item_with_default = [&]<typename Default, typename... T>(
								 InspectorFeature feature, const char* label, bool feature_exists,
								 FeatureComponents<T...> components
							 ) {
		if constexpr (!Target::template Supports<Default>()) {
			return;
		}

		ScopedDisabled disabled{ feature_exists };

		if (ImGui::MenuItem(label)) {
			changed |= AddInspectorFeatureWithDefault<Default>(target, feature, label, components);
		}
	};

	auto item_without_default = [&]<typename... T>(
									InspectorFeature feature, const char* label,
									bool feature_exists, FeatureComponents<T...> components
								) {
		if constexpr (!SupportsAnyFeatureComponent<Target>(components)) {
			return;
		}

		ScopedDisabled disabled{ feature_exists };

		if (ImGui::MenuItem(label)) {
			changed |= AddInspectorFeature(target, feature, label, components);
		}
	};

	item_with_default.template operator()<Transform>(
		InspectorFeature::Transform, "Transform", HasTransformFeature(target),
		TransformFeatureComponents{}
	);
	const bool primary_render_target{ IsPrimarySceneRenderTarget(target) };
	const bool fixed_camera{ IsReservedFixedCamera(target) };
	const bool visual_exists{ HasVisualFeature(target) };
	const bool camera_exists{ HasCameraFeature(target) };

	if (!primary_render_target && !fixed_camera && !camera_exists) {
		item_with_default.template operator()<::ptgn::impl::IDrawable>(
			InspectorFeature::Visual, "Visual", visual_exists, VisualFeatureComponents{}
		);
	}
	item_with_default.template operator()<::ptgn::impl::Interactive>(
		InspectorFeature::Interaction, "Interaction", HasInteractionFeature(target),
		InteractionFeatureComponents{}
	);
	item_with_default.template operator()<Collider>(
		InspectorFeature::Physics, "Physics & Movement", HasPhysicsFeature(target),
		PhysicsFeatureComponents{}
	);
	item_with_default.template operator()<::ptgn::impl::ButtonData>(
		InspectorFeature::UI, "UI", HasUIFeature(target), UIFeatureComponents{}
	);
	if (!primary_render_target && !visual_exists) {
		item_with_default.template operator()<::ptgn::impl::CameraData>(
			InspectorFeature::Camera, "Camera", camera_exists, CameraFeatureComponents{}
		);
	}
	item_with_default.template operator()<::ptgn::impl::Scripts>(
		InspectorFeature::Scripts, "Scripts", HasScriptsFeature(target), ScriptsFeatureComponents{}
	);
	item_without_default(
		InspectorFeature::Utilities, "Utilities", HasUtilitiesFeature(target),
		UtilitiesFeatureComponents{}
	);

	ImGui::EndPopup();
	return changed;
}

template <typename Target>
bool DrawInspectorContents(EditorContext& ctx, Target& target) {
	CommitInactiveInspectorEdit(ctx);

	bool changed{ DrawName(target) };

	ImGui::Separator();

	changed |= DrawTransformFeature(target);
	changed |= DrawVisualFeature(target);
	changed |= DrawInteractionFeature(target);
	changed |= DrawPhysicsFeature(target);
	changed |= DrawUIFeature(target);
	changed |= DrawCameraFeature(target);
	changed |= DrawScriptsFeature(target);
	changed |= DrawUtilitiesFeature(target);

	ImGui::Separator();
	changed |= DrawAddFeatureMenu(target);

	return changed;
}

} // namespace

void DrawEntityInspector(EditorContext& ctx, Entity entity) {
	EntityInspectorTarget target{
		.ctx	= ctx,
		.entity = entity,
	};

	(void)DrawInspectorContents(ctx, target);
}

void DrawPrefabInspector(EditorContext& ctx, const PrefabKey& key) {
	auto& assets{ ctx.editor.GetAssetManager() };

	if (!assets.Has(key)) {
		ImGui::TextDisabled("Prefab is not currently loaded.");
		return;
	}

	auto prefab_asset{ ::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(key) };

	PrefabInspectorTarget target{
		.ctx	= ctx,
		.key	= key,
		.prefab = prefab_asset.get().root,
	};

	if (!DrawInspectorContents(ctx, target)) {
		return;
	}

	assets.SavePrefab(key);
	ctx.local.state.is_dirty = true;
}

} // namespace inspector

void InspectorPanel::OnRender(EditorContext& ctx) {
	auto& hierarchy{ ctx.editor.GetSceneHierarchyPanel() };

	const bool prefab_tab_active{ hierarchy.GetActiveTab() == SceneHierarchyTab::Prefabs };

	ImGui::Begin(
		prefab_tab_active ? "Prefab Inspector###Inspector" : "Entity Inspector###Inspector"
	);

	if (prefab_tab_active) {
		if (const auto& selected_prefab{ hierarchy.GetSelectedPrefab() };
			selected_prefab.has_value()) {
			inspector::DrawPrefabInspector(ctx, selected_prefab.value());
		}
	} else if (auto entity{ hierarchy.GetSelectedEntity() }) {
		inspector::DrawEntityInspector(ctx, entity);
	}

	ImGui::End();
}

} // namespace ptgn::editor
