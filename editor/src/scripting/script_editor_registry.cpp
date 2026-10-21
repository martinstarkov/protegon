// script_editor_registry.cpp
#include "scripting/script_editor_registry.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#ifndef MAGIC_ENUM_RANGE_MAX
#define MAGIC_ENUM_RANGE_MAX 512
#endif
#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <concepts>
#include <cstdint>
#include <limits>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "app/project.h"
#include "core/event/key_event.h"
#include "core/event/mouse_event.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "editor/color_picker.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "panels/content_browser.h"
#include "panels/entity_filter_editor.h"
#include "panels/inspector_fields.h"
#include "panels/inspector_helpers.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/animation_event.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity_filter.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/interaction/draggable_event.h"
#include "runtime/interaction/dropzone_event.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/physics/collision_event.h"
#include "runtime/scene/scene_event.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scripting/script_event.h"
#include "runtime/scripting/scripts.h"
#include "runtime/scripting/variable_event.h"
#include "runtime/timer/timer.h"
#include "runtime/timer/timer_event.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "scripting/script_registration_editor.h"

namespace ptgn::editor {

namespace {

constexpr int kMaxAudioPlayLoops{ 100 };

void DrawItemTooltip(const char* text) {
	if (text && *text && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", text);
	}
}

void SameLineControl() {
	ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
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
	DrawItemTooltip(tooltip);
	return pressed;
}

void DrawSelectedItemsTooltip(const std::vector<std::string>& items) {
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

template <typename T>
bool DrawReflectedScriptValue(EditorContext& ctx, T& value) {
	return inspector::DrawReflectedContents(
		ctx, type_name_without_namespaces<T>(), std::addressof(value),
		[](void* data, ComponentReflectionVisitor visitor) {
			::ptgn::VisitReflectedValue(*static_cast<T*>(data), visitor);
		}
	);
}

[[nodiscard]] std::string ComponentLabel(const RegisteredComponent& component) {
	std::string_view name{ component.name };
	auto separator{ name.rfind("::") };

	if (separator != std::string_view::npos) {
		name.remove_prefix(separator + 2);
	}

	return inspector::PrettyName(name);
}

template <typename Predicate>
[[nodiscard]] std::vector<const RegisteredComponent*> GetSortedComponents(Predicate&& predicate) {
	std::vector<const RegisteredComponent*> result;

	for (const auto& component : ComponentRegistry::Components()) {
		if (std::invoke(predicate, component)) {
			result.push_back(std::addressof(component));
		}
	}

	std::ranges::sort(result, [](const auto* lhs, const auto* rhs) {
		return ComponentLabel(*lhs) < ComponentLabel(*rhs);
	});

	return result;
}

[[nodiscard]] bool CanAddComponentDefinition(const RegisteredComponent& component) {
	return component.is_empty || (component.default_constructible && component.serializable);
}

bool NormalizeJsonAgainstDefaults(json& value, const json& defaults) {
	json previous = value;

	if (value.is_null()) {
		value = defaults;
	} else if (value.is_object() && defaults.is_object()) {
		json normalized = defaults;
		normalized.update(value, true);
		value = std::move(normalized);
	}

	return value != previous;
}

bool DrawJsonValue(std::string_view label, json& value, const json* defaults = nullptr);

bool DrawJsonObject(json& value, const json* defaults) {
	bool changed{ false };

	for (auto it{ value.begin() }; it != value.end(); ++it) {
		const json* member_defaults{ nullptr };

		if (defaults && defaults->is_object()) {
			auto default_it{ defaults->find(it.key()) };
			if (default_it != defaults->end()) {
				member_defaults = std::addressof(*default_it);
			}
		}

		changed |= DrawJsonValue(inspector::PrettyName(it.key()), it.value(), member_defaults);
	}

	return changed;
}

bool DrawJsonArray(std::string_view label, json& value, const json* defaults) {
	std::string title{ std::string{ label } + " [" + std::to_string(value.size()) + "]" };

	if (!inspector::DrawInspectOnlyTreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) {
		return false;
	}

	bool changed{ false };

	for (std::size_t i{ 0 }; i < value.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));

		const json* item_defaults{ nullptr };
		if (defaults && defaults->is_array() && i < defaults->size()) {
			item_defaults = std::addressof((*defaults)[i]);
		}

		changed |= DrawJsonValue("Item " + std::to_string(i + 1), value[i], item_defaults);

		ImGui::PopID();
	}

	if (value.empty()) {
		ImGui::TextDisabled("No items");
	}

	ImGui::TreePop();
	return changed;
}

bool DrawJsonValue(std::string_view label, json& value, const json* defaults) {
	if (defaults) {
		NormalizeJsonAgainstDefaults(value, *defaults);
	}

	if (value.is_boolean()) {
		bool temporary{ value.get<bool>() };
		bool changed{ inspector::DrawPropertyRow(label, [&]() {
			return ImGui::Checkbox("##Value", &temporary);
		}) };

		if (changed) {
			value = temporary;
		}
		return changed;
	}

	if (value.is_number_unsigned()) {
		std::uint64_t temporary{ value.get<std::uint64_t>() };
		bool changed{ inspector::DrawPropertyRow(label, [&]() {
			return ImGui::DragScalar("##Value", ImGuiDataType_U64, &temporary, 1.0f);
		}) };

		if (changed) {
			value = temporary;
		}
		return changed;
	}

	if (value.is_number_integer()) {
		std::int64_t temporary{ value.get<std::int64_t>() };
		bool changed{ inspector::DrawPropertyRow(label, [&]() {
			return ImGui::DragScalar("##Value", ImGuiDataType_S64, &temporary, 1.0f);
		}) };

		if (changed) {
			value = temporary;
		}
		return changed;
	}

	if (value.is_number_float()) {
		double temporary{ value.get<double>() };
		bool changed{ inspector::DrawPropertyRow(label, [&]() {
			return ImGui::DragScalar(
				"##Value", ImGuiDataType_Double, &temporary, 0.1f, nullptr, nullptr, "%.6g"
			);
		}) };

		if (changed) {
			value = temporary;
		}
		return changed;
	}

	if (value.is_string()) {
		std::string temporary{ value.get<std::string>() };
		bool changed{ inspector::DrawPropertyRow(label, [&]() {
			return ImGui::InputText("##Value", &temporary);
		}) };

		if (changed) {
			value = std::move(temporary);
		}
		return changed;
	}

	if (value.is_object()) {
		std::string title{ label };
		if (!inspector::DrawInspectOnlyTreeNodeEx(
				title.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth
			)) {
			return false;
		}

		bool changed{ DrawJsonObject(value, defaults) };
		ImGui::TreePop();
		return changed;
	}

	if (value.is_array()) {
		return DrawJsonArray(label, value, defaults);
	}

	inspector::DrawPropertyRow(label, []() {
		ImGui::TextDisabled("Null");
		return false;
	});
	return false;
}

bool DrawRegisteredComponentJson(const RegisteredComponent& component, json& value) {
	json defaults = json::object();

	try {
		if (auto default_value{ component.MakeDefaultJson() }) {
			defaults = std::move(*default_value);
		}
	} catch (...) {
		defaults = json::object();
	}

	bool changed{ NormalizeJsonAgainstDefaults(value, defaults) };

	if (value.is_object()) {
		changed |= DrawJsonObject(value, std::addressof(defaults));
	} else {
		changed |= DrawJsonValue("Value", value, std::addressof(defaults));
	}

	return changed;
}

template <typename T>
[[nodiscard]] T JsonValueOr(const json& input, std::string_view key, T fallback) {
	if (!input.is_object()) {
		return fallback;
	}
	auto it{ input.find(std::string{ key }) };
	if (it == input.end() || it->is_null()) {
		return fallback;
	}
	try {
		return it->template get<T>();
	} catch (...) {
		return fallback;
	}
}

std::string KeyExpressionValue(const json& value) {
	if (auto expression{ JsonValueOr<std::string>(value, "keys", "") }; !expression.empty()) {
		return expression;
	}

	return inspector::KeyDisplayLabel(JsonValueOr<Key>(value, "key", Key::W));
}

bool DrawHeldDurationToggle(bool& require_duration) {
	bool changed{ ImGui::Checkbox("##RequireHeldDuration", &require_duration) };
	DrawItemTooltip("Checked: require the minimum held duration. Unchecked: match any held state.");
	return changed;
}

bool DrawKeyExpression(json& value, bool with_duration) {
	std::string expression{ KeyExpressionValue(value) };
	bool require_duration{ JsonValueOr<bool>(value, "require_held_duration", true) };
	float held_duration_ms{ std::max(0.0f, JsonValueOr<float>(value, "held_duration_ms", 250.0f)) };
	float spacing{ ImGui::GetStyle().ItemSpacing.x };
	float duration_width{ 112.0f };
	float checkbox_width{ ImGui::GetFrameHeight() };
	float expression_width{ with_duration
								? std::max(
									  1.0f, ImGui::GetContentRegionAvail().x - duration_width -
												checkbox_width - spacing * 2.0f
								  )
								: -FLT_MIN };

	bool changed{ inspector::DrawKeyExpression(
		expression, "##Keys", "W + X, W + Left Shift", expression_width
	) };

	if (with_duration) {
		ImGui::SameLine(0.0f, spacing);
		changed |= DrawHeldDurationToggle(require_duration);

		ImGui::SameLine(0.0f, spacing);
		ImGui::BeginDisabled(!require_duration);
		changed |= inspector::DrawDurationInput(
			"##HeldDuration", held_duration_ms, duration_width,
			"Minimum time the key expression must remain held."
		);
		ImGui::EndDisabled();
		held_duration_ms = std::max(0.0f, held_duration_ms);
	}

	if (changed) {
		value["keys"] = std::move(expression);
		value.erase("key");
		if (with_duration) {
			value["require_held_duration"] = require_duration;
			value["held_duration_ms"]	   = held_duration_ms;
		}
	}

	return changed;
}

bool DrawKey(json& value) {
	return DrawKeyExpression(value, false);
}

bool DrawHeldKey(json& value) {
	return DrawKeyExpression(value, true);
}

const char* MouseTriggerLabel(Mouse mouse) {
	switch (mouse) {
		case Mouse::Left:	return "Left";
		case Mouse::Right:	return "Right";
		case Mouse::Middle: return "Middle";
		default:			return "Left";
	}
}

bool DrawMouseTrigger(json& value, bool with_duration) {
	Mouse mouse{ JsonValueOr<Mouse>(value, "button", Mouse::Left) };
	bool changed{ false };
	if (mouse != Mouse::Left && mouse != Mouse::Right && mouse != Mouse::Middle) {
		mouse	= Mouse::Left;
		changed = true;
	}

	bool require_duration{ JsonValueOr<bool>(value, "require_held_duration", true) };
	float held_duration_ms{ std::max(0.0f, JsonValueOr<float>(value, "held_duration_ms", 250.0f)) };
	float spacing{ ImGui::GetStyle().ItemSpacing.x };
	float duration_width{ 112.0f };
	float checkbox_width{ ImGui::GetFrameHeight() };
	float mouse_width{ with_duration ? std::max(
										   1.0f, ImGui::GetContentRegionAvail().x - duration_width -
													 checkbox_width - spacing * 2.0f
									   )
									 : -FLT_MIN };

	ImGui::SetNextItemWidth(mouse_width);
	if (ImGui::BeginCombo("##Button", MouseTriggerLabel(mouse))) {
		for (Mouse candidate : { Mouse::Left, Mouse::Right, Mouse::Middle }) {
			bool selected{ candidate == mouse };
			if (ImGui::Selectable(MouseTriggerLabel(candidate), selected)) {
				mouse	= candidate;
				changed = true;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	DrawItemTooltip("Mouse button matched by this trigger.");

	if (with_duration) {
		ImGui::SameLine(0.0f, spacing);
		changed |= DrawHeldDurationToggle(require_duration);

		ImGui::SameLine(0.0f, spacing);
		ImGui::BeginDisabled(!require_duration);
		changed |= inspector::DrawDurationInput(
			"##HeldDuration", held_duration_ms, duration_width,
			"Minimum time the mouse button must remain held."
		);
		ImGui::EndDisabled();
		held_duration_ms = std::max(0.0f, held_duration_ms);
	}

	if (changed) {
		value["button"] = mouse;
		if (with_duration) {
			value["require_held_duration"] = require_duration;
			value["held_duration_ms"]	   = held_duration_ms;
		}
	}

	return changed;
}

bool DrawMouse(json& value) {
	return DrawMouseTrigger(value, false);
}

bool DrawHeldMouse(json& value) {
	return DrawMouseTrigger(value, true);
}

bool DrawSignalEvent(json& value) {
	std::string signal{ JsonValueOr<std::string>(value, "signal", "") };
	ImGui::SetNextItemWidth(-FLT_MIN);
	bool changed{ ImGui::InputTextWithHint("##Signal", "Signal name", &signal) };
	DrawItemTooltip("Signal name matched exactly.");
	if (changed) {
		value["signal"] = std::move(signal);
	}
	return changed;
}

bool DrawDialogueEventFilter(json& value, bool with_page) {
	if (!value.is_object()) {
		value = json::object();
	}

	std::string dialogue{ JsonValueOr<std::string>(value, "dialogue", "") };
	int page{ JsonValueOr<int>(value, "page", -1) };
	bool changed{ false };

	if (with_page) {
		float spacing{ ImGui::GetStyle().ItemSpacing.x };
		float page_width{ 72.0f };
		ImGui::SetNextItemWidth(
			std::max(1.0f, ImGui::GetContentRegionAvail().x - page_width - spacing)
		);
		changed |= ImGui::InputTextWithHint("##DialogueKey", "Any dialogue", &dialogue);
		DrawItemTooltip("Dialogue key to match. Empty matches any dialogue.");
		SameLineControl();
		ImGui::SetNextItemWidth(page_width);
		changed |= ImGui::InputInt("##DialoguePage", &page, 0, 0);
		DrawItemTooltip("Zero-based page index. -1 matches any page.");
		page = std::max(-1, page);
	} else {
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= ImGui::InputTextWithHint("##DialogueKey", "Any dialogue", &dialogue);
		DrawItemTooltip("Dialogue key to match. Empty matches any dialogue.");
	}

	if (changed) {
		value["dialogue"] = std::move(dialogue);
		if (with_page) {
			value["page"] = page;
		}
	}

	return changed;
}

bool DrawDialogueEvent(json& value) {
	return DrawDialogueEventFilter(value, false);
}

bool DrawDialoguePageEvent(json& value) {
	return DrawDialogueEventFilter(value, true);
}

template <typename T>
bool DrawNothing(ScriptEditorContext&, T&) {
	return false;
}

bool DrawScriptSequenceInline(ScriptEditorContext& context, Script& script) {
	const auto* selected{ script.sequence.shared_reference
							  ? context.shared_sequences.Find(script.sequence.shared_sequence_id)
							  : nullptr };
	const char* preview{ selected ? selected->name.c_str() : "Select Script Sequence" };
	bool changed{ false };
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::BeginCombo("##GlobalScriptSequence", preview)) {
		for (const auto& shared : context.shared_sequences.sequences) {
			bool is_selected{ script.sequence.shared_reference &&
							  script.sequence.shared_sequence_id == shared.id };
			if (ImGui::Selectable(shared.name.c_str(), is_selected)) {
				script.sequence.name			   = shared.name;
				script.sequence.shared_reference   = true;
				script.sequence.shared_sequence_id = shared.id;
				script.sequence.runtime			   = ScriptSequenceRuntime{};
				changed							   = true;
			}
		}
		ImGui::EndCombo();
	}
	DrawItemTooltip("Choose the global Script Sequence run by this action.");
	return changed;
}

bool DrawMoveToInline(ScriptEditorContext&, MoveToScript& script) {
	bool changed{ false };
	float available{ ImGui::GetContentRegionAvail().x };
	float spacing{ ImGui::GetStyle().ItemSpacing.x };
	float mode_width{
		std::max(ImGui::CalcTextSize("Relative").x, ImGui::CalcTextSize("Absolute").x) +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};
	float field_width{ std::max(36.0f, (available - mode_width - spacing * 2.0f) * 0.5f) };
	ImGui::SetNextItemWidth(field_width);
	changed |=
		ImGui::DragFloat("##X", &script.destination.x, 1.0f, -100000.0f, 100000.0f, "X: %.0f");
	SameLineControl();
	ImGui::SetNextItemWidth(field_width);
	changed |=
		ImGui::DragFloat("##Y", &script.destination.y, 1.0f, -100000.0f, 100000.0f, "Y: %.0f");
	SameLineControl();
	if (ImGui::Button(
			script.relative ? "Relative" : "Absolute", ImVec2{ mode_width, ImGui::GetFrameHeight() }
		)) {
		script.relative = !script.relative;
		changed			= true;
	}
	DrawItemTooltip(
		script.relative ? "Offset from the entity's current position."
						: "Use an absolute world position."
	);
	return changed;
}

bool DrawRotateToInline(ScriptEditorContext&, RotateToScript& script) {
	float available{ ImGui::GetContentRegionAvail().x };
	float spacing{ ImGui::GetStyle().ItemSpacing.x };
	float shortest_width{ ImGui::CalcTextSize("Shortest").x +
						  ImGui::GetStyle().FramePadding.x * 2.0f };
	float relative_width{ ImGui::CalcTextSize("Relative").x +
						  ImGui::GetStyle().FramePadding.x * 2.0f };
	float degrees_width{
		std::max(48.0f, available - shortest_width - relative_width - spacing * 2.0f)
	};
	bool changed{ false };
	ImGui::SetNextItemWidth(degrees_width);
	changed |= ImGui::DragFloat("##Degrees", &script.degrees, 1.0f, -3600.0f, 3600.0f, "%.1f deg");
	SameLineControl();
	changed |= DrawToggleButton(
		"Shortest", script.shortest_path, ImVec2{ shortest_width, ImGui::GetFrameHeight() },
		"Toggle the shortest rotational path."
	);
	SameLineControl();
	changed |= DrawToggleButton(
		"Relative", script.relative, ImVec2{ relative_width, ImGui::GetFrameHeight() },
		"Treat the angle as an offset from the current rotation."
	);
	return changed;
}

bool DrawScaleToInline(ScriptEditorContext&, ScaleToScript& script) {
	float available{ ImGui::GetContentRegionAvail().x };
	float spacing{ ImGui::GetStyle().ItemSpacing.x };
	float mode_width{
		std::max(ImGui::CalcTextSize("Relative").x, ImGui::CalcTextSize("Absolute").x) +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};
	float field_width{ std::max(36.0f, (available - mode_width - spacing * 2.0f) * 0.5f) };
	bool changed{ false };
	ImGui::SetNextItemWidth(field_width);
	changed |= ImGui::DragFloat("##ScaleX", &script.scale.x, 0.01f, -100.0f, 100.0f, "X: %.2f");
	SameLineControl();
	ImGui::SetNextItemWidth(field_width);
	changed |= ImGui::DragFloat("##ScaleY", &script.scale.y, 0.01f, -100.0f, 100.0f, "Y: %.2f");
	SameLineControl();
	if (ImGui::Button(
			script.relative ? "Relative" : "Absolute", ImVec2{ mode_width, ImGui::GetFrameHeight() }
		)) {
		script.relative = !script.relative;
		changed			= true;
	}
	DrawItemTooltip(
		script.relative ? "Multiply the entity's current scale." : "Use an absolute scale."
	);
	return changed;
}

Entity ResolvePrintPreviewTarget(ScriptEditorContext& context) {
	if (!context.owner) {
		return {};
	}
	if (context.sequence_target_filter && context.sequence_target_filter->has_value()) {
		auto targets{ ResolveEntityFilter(
			context.sequence_target_filter->value(), context.owner.GetScene(), context.owner
		) };
		return targets.empty() ? Entity{} : targets.front();
	}
	return context.owner;
}

bool DrawPrintConsoleInline(ScriptEditorContext& context, PrintConsoleScript& script) {
	bool changed{ false };
	float spacing{ ImGui::GetStyle().ItemSpacing.x };
	float width{ ImGui::GetContentRegionAvail().x };
	float options_width{ std::min(
		ImGui::CalcTextSize("Warning +3").x + ImGui::GetFrameHeight() +
			ImGui::GetStyle().FramePadding.x,
		std::max(1.0f, width * 0.42f)
	) };

	const char* level_label{ "Info" };
	switch (script.level) {
		case ConsoleLogLevel::Debug:   level_label = "Debug"; break;
		case ConsoleLogLevel::Info:	   level_label = "Info"; break;
		case ConsoleLogLevel::Warning: level_label = "Warning"; break;
		case ConsoleLogLevel::Error:   level_label = "Error"; break;
	}

	int includes{ static_cast<int>(script.include_entity) +
				  static_cast<int>(script.include_sequence) +
				  static_cast<int>(script.include_timestamp) };
	std::string preview{ level_label };
	if (includes > 0) {
		preview += " +" + std::to_string(includes);
	}

	ImGui::SetNextItemWidth(options_width);
	ImGui::SetNextWindowSizeConstraints(ImVec2{ 175.0f, 0.0f }, ImVec2{ FLT_MAX, FLT_MAX });
	if (ImGui::BeginCombo("##PrintOptions", preview.c_str())) {
		ImGui::TextDisabled("Log Level");
		for (const auto& [level, label] : {
				 std::pair{ ConsoleLogLevel::Debug, "Debug" },
				 std::pair{ ConsoleLogLevel::Info, "Info" },
				 std::pair{ ConsoleLogLevel::Warning, "Warning" },
				 std::pair{ ConsoleLogLevel::Error, "Error" },
			 }) {
			if (ImGui::Selectable(label, script.level == level)) {
				script.level = level;
				changed		 = true;
			}
		}
		ImGui::Separator();
		ImGui::TextDisabled("Include");
		changed |= ImGui::Checkbox("Entity", &script.include_entity);
		DrawItemTooltip("Include the target entity UUID in the message.");
		changed |= ImGui::Checkbox("Sequence", &script.include_sequence);
		DrawItemTooltip("Include the name of the sequence running this action.");
		changed |= ImGui::Checkbox("Timestamp", &script.include_timestamp);
		DrawItemTooltip("Include the elapsed game time in seconds.");
		ImGui::EndCombo();
	}
	DrawItemTooltip("Choose a log level and optional message context.");

	SameLineControl();
	changed |= ImGui::InputTextMultiline(
		"##PrintMessage", &script.message,
		ImVec2{ std::max(1.0f, width - options_width - spacing), ImGui::GetFrameHeight() },
		ImGuiInputTextFlags_CtrlEnterForNewLine
	);
	bool hovered{ ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) };
	std::string popup_title{ "Debug Message###PrintMessageEditor" +
							 std::to_string(ImGui::GetID("##PrintMessageEditor")) };
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
		ImGui::OpenPopup(popup_title.c_str());
	}

	if (hovered) {
		std::string resolved{
			ResolveConsoleMessage(script.message, ResolvePrintPreviewTarget(context))
		};
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 38.0f);
		ImGui::TextUnformatted(
			"Use {counter:Name} and {bool:Name} for live variables. Right click to edit multiple "
			"lines."
		);
		ImGui::Spacing();
		ImGui::TextUnformatted(resolved.empty() ? "(empty message)" : resolved.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	ImGui::SetNextWindowSize(ImVec2{ 460.0f, 200.0f }, ImGuiCond_Appearing);
	bool popup_open{ true };
	if (ImGui::BeginPopupModal(
			popup_title.c_str(), &popup_open,
			ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings
		)) {
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}
		changed |= ImGui::InputTextMultiline(
			"##FullPrintMessage", &script.message, ImVec2{ -FLT_MIN, -FLT_MIN }
		);
		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	return changed;
}

bool DrawSetVisibleInline(ScriptEditorContext&, SetVisibleScript& script) {
	const char* preview{ script.visible ? "True" : "False" };
	bool changed{ false };
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::BeginCombo("##VisibleValue", preview)) {
		if (ImGui::Selectable("True", script.visible)) {
			script.visible = true;
			changed		   = true;
		}
		if (ImGui::Selectable("False", !script.visible)) {
			script.visible = false;
			changed		   = true;
		}
		ImGui::EndCombo();
	}
	DrawItemTooltip("Visibility value assigned by this action.");
	return changed;
}

bool DrawPlaySoundInline(ScriptEditorContext& context, PlaySoundScript& script) {
	ImGui::SetNextItemWidth(-FLT_MIN);

	return inspector::DrawAssetKeyInline(
		context.ctx, script.sound, inspector::FieldOptions{}, "Audio key"
	);
}

bool DrawPlaySound(ScriptEditorContext&, PlaySoundScript& script) {
	script.volume = std::clamp(script.volume, kMinVolume, kMaxVolume);
	script.loops  = std::clamp(script.loops, 0, kMaxAudioPlayLoops);

	bool changed{ false };

	changed |= inspector::DrawPropertyRow("Volume", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);

		bool row_changed{ ImGui::SliderFloat(
			"##Volume", &script.volume, kMinVolume, kMaxVolume, "%.2f", ImGuiSliderFlags_AlwaysClamp
		) };
		DrawItemTooltip("Audio volume. Double click to enter an exact value.");

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			ImGui::OpenPopup("ExactVolume");
		}

		if (ImGui::BeginPopup("ExactVolume")) {
			ImGui::SetNextItemWidth(110.0f);
			row_changed |= ImGui::InputFloat("##ExactVolume", &script.volume, 0.01f, 0.1f, "%.3f");
			script.volume = std::clamp(script.volume, kMinVolume, kMaxVolume);
			ImGui::EndPopup();
		}

		return row_changed;
	});

	changed |= inspector::DrawPropertyRow("Loops", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		bool row_changed{ ImGui::DragInt(
			"##Loops", &script.loops, 1.0f, 0, kMaxAudioPlayLoops, "%d",
			ImGuiSliderFlags_AlwaysClamp
		) };
		DrawItemTooltip("Number of additional plays after the first.");
		return row_changed;
	});

	changed |= inspector::DrawPropertyRow("Frequency Ratio", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat(
			"##FrequencyRatio", &script.frequency_ratio, 0.01f, kMinFrequencyRatio,
			kMaxFrequencyRatio, "%.2f", ImGuiSliderFlags_AlwaysClamp
		);
	});
	changed |= inspector::DrawPropertyRow("Exclusive", [&]() {
		return ImGui::Checkbox("##Exclusive", &script.exclusive);
	});
	if (script.exclusive) {
		changed |= inspector::DrawPropertyRow("Force Restart", [&]() {
			return ImGui::Checkbox("##ForceRestart", &script.force_restart);
		});
	}
	return changed;
}

template <typename T, std::size_t N>
bool DrawNamedEnumCombo(
	const char* id, T& value, const std::array<std::pair<T, const char*>, N>& entries,
	const char* tooltip = nullptr
) {
	auto selected{ std::ranges::find_if(entries, [value](const auto& entry) {
		return entry.first == value;
	}) };
	const char* preview{ selected != entries.end() ? selected->second : "Unknown" };
	bool changed{ false };
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::BeginCombo(id, preview)) {
		for (const auto& [candidate, label] : entries) {
			if (ImGui::Selectable(label, candidate == value)) {
				value	= candidate;
				changed = true;
			}
		}
		ImGui::EndCombo();
	}
	DrawItemTooltip(tooltip ? tooltip : "Choose the action operation.");
	return changed;
}

inline constexpr std::array kSceneActions{
	std::pair{ SceneChangeAction::Enter, "Enter" },
	std::pair{ SceneChangeAction::Exit, "Exit" },
	std::pair{ SceneChangeAction::Switch, "Switch" },
};

inline constexpr std::array kSceneTransitions{
	std::pair{ SceneTransitionStyle::None, "None" },
	std::pair{ SceneTransitionStyle::Fade, "Fade" },
	std::pair{ SceneTransitionStyle::CrossFade, "Cross Fade" },
	std::pair{ SceneTransitionStyle::Slide, "Slide" },
};

[[nodiscard]] bool IsEnabledComponent(const RegisteredComponent& component) {
	if (!component.serializable || !component.deserializable) {
		return false;
	}

	json value;
	try {
		auto default_value{ component.MakeDefaultJson() };
		if (!default_value) {
			return false;
		}
		value = std::move(*default_value);
	} catch (...) {
		return false;
	}

	if (component.name == "Interactive" && value.is_boolean()) {
		return true;
	}
	if (!value.is_object()) {
		return false;
	}
	auto it{ value.find("enabled") };
	return it != value.end() && it->is_boolean();
}

[[nodiscard]] std::vector<TimerKey> GetTimerActionChoices(ScriptEditorContext& context) {
	if (!context.owner) {
		return {};
	}

	std::vector<Entity> targets;
	if (context.sequence_target_filter && context.sequence_target_filter->has_value()) {
		targets = ResolveEntityFilter(
			context.sequence_target_filter->value(), context.owner.GetScene(), context.owner
		);
	} else {
		targets.push_back(context.owner);
	}

	std::vector<TimerKey> result;
	for (Entity target : targets) {
		const auto* timers{ target.TryGet<::ptgn::impl::Timers>() };
		if (!timers) {
			continue;
		}

		for (const auto& entry : timers->timers) {
			if (entry.config.key.value.empty() || std::ranges::contains(result, entry.config.key)) {
				continue;
			}
			result.push_back(entry.config.key);
		}
	}

	return result;
}

bool DrawTimerKeyInline(
	const char* id, TimerKey& timer, const std::vector<TimerKey>& choices, float width
) {
	bool changed{ false };
	ImGui::SetNextItemWidth(width);

	const char* preview{ timer.value.empty() ? "No Timer" : timer.value.c_str() };

	if (ImGui::BeginCombo(id, preview)) {
		std::string custom_name{ timer.value };

		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputTextWithHint("##CustomTimerName", "Custom timer name...", &custom_name)) {
			timer.value = std::move(custom_name);
			changed		= true;
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
					timer	= choice;
					changed = true;
				}
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}

		if (choices.empty()) {
			ImGui::TextDisabled("No timers on the target.");
		}

		ImGui::EndCombo();
	}

	DrawItemTooltip(
		"Named timer on the action target. Open the combo to choose an existing timer "
		"or enter a custom name."
	);
	return changed;
}

template <typename Key>
[[nodiscard]] std::vector<Key> GetVariableActionChoices(ScriptEditorContext& context) {
	if (!context.owner) {
		return {};
	}
	std::vector<Entity> targets;
	if (context.sequence_target_filter && context.sequence_target_filter->has_value()) {
		targets = ResolveEntityFilter(
			context.sequence_target_filter->value(), context.owner.GetScene(), context.owner
		);
	} else {
		targets.push_back(context.owner);
	}
	std::vector<Key> result;
	for (Entity target : targets) {
		if constexpr (std::same_as<Key, CounterKey>) {
			if (const auto* variables{ target.TryGet<::ptgn::impl::Counters>() }) {
				for (const auto& entry : variables->counters) {
					if (!entry.config.key.value.empty() &&
						!std::ranges::contains(result, entry.config.key)) {
						result.push_back(entry.config.key);
					}
				}
			}
		} else {
			if (const auto* variables{ target.TryGet<::ptgn::impl::Booleans>() }) {
				for (const auto& entry : variables->booleans) {
					if (!entry.config.key.value.empty() &&
						!std::ranges::contains(result, entry.config.key)) {
						result.push_back(entry.config.key);
					}
				}
			}
		}
	}
	return result;
}

template <typename Key>
bool DrawVariableKeyInline(const char* id, Key& key, const std::vector<Key>& choices, float width) {
	bool changed{ false };
	const char* kind{ std::same_as<Key, CounterKey> ? "Counter" : "Boolean" };
	std::string missing_tooltip{ std::string{ kind } +
								 " key does not exist on the action target." };
	std::string key_tooltip{ std::string{ kind } + " key on the action target." };
	auto missing = [&]() {
		return !key.value.empty() && !std::ranges::contains(choices, key);
	};
	auto draw_warning_border = [&]() {
		if (missing()) {
			ImGui::GetWindowDrawList()->AddRect(
				ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
				ImGui::GetColorU32(ImVec4{ 0.92f, 0.23f, 0.25f, 1.0f }),
				ImGui::GetStyle().FrameRounding, 0, 2.0f
			);
		}
	};
	auto draw_key_tooltip = [&]() {
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip(
				"%s", missing()			  ? missing_tooltip.c_str()
					  : key.value.empty() ? "No variable key selected."
										  : key_tooltip.c_str()
			);
		}
	};

	ImGui::SetNextItemWidth(width);
	ImVec2 combo_min{ ImGui::GetCursorScreenPos() };
	ImVec2 combo_max{ combo_min.x + ImGui::CalcItemWidth(), combo_min.y + ImGui::GetFrameHeight() };
	ImDrawList* combo_draw_list{ ImGui::GetWindowDrawList() };
	bool open{ ImGui::BeginCombo(id, key.value.empty() ? "None" : key.value.c_str()) };
	if (missing()) {
		combo_draw_list->AddRect(
			combo_min, combo_max, ImGui::GetColorU32(ImVec4{ 0.92f, 0.23f, 0.25f, 1.0f }),
			ImGui::GetStyle().FrameRounding, 0, 2.0f
		);
	}
	draw_key_tooltip();
	if (open) {
		std::string custom{ key.value };
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputTextWithHint("##VariableCustomName", "Name...", &custom)) {
			key.value = std::move(custom);
			changed	  = true;
		}
		draw_warning_border();
		draw_key_tooltip();
		ImGui::Separator();
		if (ImGui::Selectable("None", key.value.empty()) && !key.value.empty()) {
			key.value.clear();
			changed = true;
		}
		for (const auto& choice : choices) {
			if (ImGui::Selectable(choice.value.c_str(), choice == key) && choice != key) {
				key		= choice;
				changed = true;
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}

[[nodiscard]] const char* SceneActionLabel(SceneChangeAction action) {
	auto it{ std::ranges::find_if(kSceneActions, [action](const auto& entry) {
		return entry.first == action;
	}) };
	return it != kSceneActions.end() ? it->second : "Scene";
}

bool DrawSetTextureInline(ScriptEditorContext& context, SetTextureScript& script) {
	ImGui::SetNextItemWidth(-FLT_MIN);

	return inspector::DrawAssetKeyInline(
		context.ctx, script.texture_key, inspector::FieldOptions{}, "Texture key"
	);
}

bool DrawSetEnabledInline(ScriptEditorContext&, SetEnabledScript& script) {
	auto components{ GetSortedComponents([](const RegisteredComponent& component) {
		return IsEnabledComponent(component);
	}) };

	float available{ ImGui::GetContentRegionAvail().x };
	float spacing{ ImGui::GetStyle().ItemSpacing.x };
	float value_width{ ImGui::CalcTextSize("Disabled").x +
					   ImGui::GetStyle().FramePadding.x * 2.0f };

	bool changed{ false };
	ImGui::SetNextItemWidth(std::max(1.0f, available - value_width - spacing));

	const auto* selected{ ComponentRegistry::Find(script.component) };
	std::string preview{ selected ? ComponentLabel(*selected) : script.component };

	if (ImGui::BeginCombo("##EnabledComponent", preview.empty() ? "Component" : preview.c_str())) {
		for (const auto* component : components) {
			std::string label{ ComponentLabel(*component) };

			if (ImGui::Selectable(label.c_str(), script.component == component->name)) {
				script.component = component->name;
				changed			 = true;
			}
		}

		ImGui::EndCombo();
	}

	DrawItemTooltip("Registered component whose enabled value is changed.");
	SameLineControl();

	if (ImGui::Button(
			script.enabled ? "Enabled" : "Disabled", ImVec2{ value_width, ImGui::GetFrameHeight() }
		)) {
		script.enabled = !script.enabled;
		changed		   = true;
	}

	DrawItemTooltip("Toggle the value assigned by this action.");
	return changed;
}

struct ProjectSceneChoice {
	std::string key{};
	std::string label{};
};

[[nodiscard]] std::vector<ProjectSceneChoice> GetProjectSceneChoices(ScriptEditorContext& context) {
	std::vector<ProjectSceneChoice> choices;

	const auto* project{ context.ctx.editor.GetProject() };

	if (!project) {
		return choices;
	}

	choices.reserve(project->scenes.size());

	for (const auto& entry : project->scenes) {
		choices.emplace_back(
			ProjectSceneChoice{
				.key   = entry.key,
				.label = entry.key + " [" + entry.display_name + "]",
			}
		);
	}

	return choices;
}

[[nodiscard]] std::optional<std::string> GetCurrentProjectSceneKey(ScriptEditorContext& context) {
	if (!context.owner) {
		return std::nullopt;
	}

	return context.owner.GetScene().GetTag();
}

[[nodiscard]] bool HasOtherProjectScene(ScriptEditorContext& context) {
	auto choices{ GetProjectSceneChoices(context) };

	if (choices.empty()) {
		return false;
	}

	auto current_scene{ GetCurrentProjectSceneKey(context) };

	if (!current_scene) {
		return true;
	}

	return std::ranges::any_of(choices, [&current_scene](const ProjectSceneChoice& choice) {
		return choice.key != *current_scene;
	});
}

bool NormalizeSceneChangeSelection(ScriptEditorContext& context, SceneChangeScript& script) {
	auto choices{ GetProjectSceneChoices(context) };

	auto current_scene{ GetCurrentProjectSceneKey(context) };

	if (script.action == SceneChangeAction::Enter) {
		auto selected{ std::ranges::find_if(choices, [&script](const ProjectSceneChoice& choice) {
			return choice.key == script.scene_key;
		}) };

		if (selected != choices.end() || choices.empty()) {
			return false;
		}

		if (current_scene) {
			auto current{ std::ranges::find_if(
				choices, [&current_scene](const ProjectSceneChoice& choice) {
					return choice.key == *current_scene;
				}
			) };

			if (current != choices.end()) {
				script.scene_key = current->key;
				return true;
			}
		}

		script.scene_key = choices.front().key;
		return true;
	}

	if (script.action != SceneChangeAction::Switch) {
		return false;
	}

	auto is_valid_destination = [&current_scene](const ProjectSceneChoice& choice) {
		return !current_scene || choice.key != *current_scene;
	};

	auto selected{ std::ranges::find_if(choices, [&](const ProjectSceneChoice& choice) {
		return choice.key == script.scene_key && is_valid_destination(choice);
	}) };

	if (selected != choices.end()) {
		return false;
	}

	auto replacement{ std::ranges::find_if(choices, is_valid_destination) };

	if (replacement != choices.end()) {
		script.scene_key = replacement->key;

		return true;
	}

	// Switching to the current scene is equivalent to entering an
	// already active scene, which reconstructs it.
	script.action = SceneChangeAction::Enter;

	if (current_scene) {
		script.scene_key = *current_scene;
	} else if (!choices.empty()) {
		script.scene_key = choices.front().key;
	}

	return true;
}

[[nodiscard]] std::string SceneSelectionTooltip(
	ScriptEditorContext& context, const SceneChangeScript& script
) {
	std::string current_scene{ GetCurrentProjectSceneKey(context).value_or("Current Scene") };

	std::string selected_scene{ script.scene_key.empty() ? "Selected Scene" : script.scene_key };

	switch (script.action) {
		case SceneChangeAction::Enter:
			if (current_scene == selected_scene) {
				return "[" + selected_scene + "]" + " will be re-entered.";
			}

			return "[" + selected_scene + "]" + " will be entered.";

		case SceneChangeAction::Exit: return "[" + selected_scene + "]" + " will be exited.";

		case SceneChangeAction::Switch:
			return "Switch from [" + current_scene + "] to [" + selected_scene + "]";
	}

	return "Choose the scene affected by this action.";
}

bool DrawProjectSceneCombo(
	const char* label, ScriptEditorContext& context, SceneChangeScript& script
) {
	auto choices{ GetProjectSceneChoices(context) };

	auto current_scene{ GetCurrentProjectSceneKey(context) };

	bool excludes_current_scene{ script.action == SceneChangeAction::Switch };

	auto is_available = [&](const ProjectSceneChoice& choice) {
		return !excludes_current_scene || !current_scene || choice.key != *current_scene;
	};

	bool has_available_choices{ std::ranges::any_of(choices, is_available) };

	std::string preview{ script.scene_key.empty() ? "Select Scene" : script.scene_key };

	bool changed{ false };

	bool open{ ImGui::BeginCombo(label, preview.c_str()) };

	std::string tooltip{ SceneSelectionTooltip(context, script) };

	DrawItemTooltip(tooltip.c_str());

	if (!open) {
		return false;
	}

	if (!has_available_choices) {
		if (excludes_current_scene) {
			ImGui::TextDisabled("No other project scenes");
		} else {
			ImGui::TextDisabled("No project scenes");
		}
	}

	for (const auto& choice : choices) {
		if (!is_available(choice)) {
			continue;
		}

		bool selected{ script.scene_key == choice.key };

		if (ImGui::Selectable(choice.label.c_str(), selected)) {
			script.scene_key = choice.key;

			changed = true;
		}

		if (selected) {
			ImGui::SetItemDefaultFocus();
		}
	}

	ImGui::EndCombo();

	return changed;
}

bool DrawSceneChangeActionAndKey(ScriptEditorContext& context, SceneChangeScript& script) {
	bool changed{ NormalizeSceneChangeSelection(context, script) };

	bool has_other_scene{ HasOtherProjectScene(context) };

	float available{ ImGui::GetContentRegionAvail().x };

	float spacing{ ImGui::GetStyle().ItemSpacing.x };

	float action_width{ std::max(90.0f, available * 0.35f) };

	ImGui::SetNextItemWidth(action_width);

	if (ImGui::BeginCombo("##SceneAction", SceneActionLabel(script.action))) {
		for (const auto& [candidate, label] : kSceneActions) {
			bool requires_other_scene{ candidate == SceneChangeAction::Switch };

			if (requires_other_scene && !has_other_scene) {
				continue;
			}

			bool selected{ candidate == script.action };

			if (ImGui::Selectable(label, selected)) {
				script.action = candidate;

				changed = true;

				changed |= NormalizeSceneChangeSelection(context, script);
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}
	DrawItemTooltip("Scene operation to perform.");

	SameLineControl();

	ImGui::SetNextItemWidth(std::max(1.0f, available - action_width - spacing));

	changed |= DrawProjectSceneCombo("##ProjectSceneKey", context, script);

	return changed;
}

bool DrawSceneChangeInline(ScriptEditorContext& context, SceneChangeScript& script) {
	return DrawSceneChangeActionAndKey(context, script);
}

bool DrawSceneChange(ScriptEditorContext&, SceneChangeScript& script) {
	bool changed{ false };

	changed |= inspector::DrawPropertyRow("Transition", [&]() {
		return DrawNamedEnumCombo(
			"##SceneTransition", script.transition, kSceneTransitions,
			"Fade is sequential for switches and entering an already active scene; Cross Fade "
			"overlaps both scenes."
		);
	});

	if (script.transition != SceneTransitionStyle::None) {
		changed |= inspector::DrawPropertyRow("Duration", [&]() {
			return inspector::DrawDurationInput(
				"##SceneTransitionDuration", script.duration, -FLT_MIN, "Transition duration."
			);
		});

		changed |= inspector::DrawPropertyRow("Delay", [&]() {
			return inspector::DrawDurationInput(
				"##SceneTransitionDelay", script.delay, -FLT_MIN,
				"Delay before the transition begins."
			);
		});

		changed |= inspector::DrawPropertyRow("Ease", [&]() {
			ImGui::SetNextItemWidth(-FLT_MIN);

			std::string preview{ magic_enum::enum_name(script.ease) };

			bool ease_changed{ false };

			if (ImGui::BeginCombo("##SceneTransitionEase", preview.c_str())) {
				for (auto candidate : magic_enum::enum_values<Ease>()) {
					std::string label{ magic_enum::enum_name(candidate) };

					if (ImGui::Selectable(label.c_str(), candidate == script.ease)) {
						script.ease = candidate;

						ease_changed = true;
					}
				}

				ImGui::EndCombo();
			}
			DrawItemTooltip("Easing used for the scene transition.");

			return ease_changed;
		});

		if (script.transition == SceneTransitionStyle::Slide) {
			script.direction.x = std::clamp(script.direction.x, -1.0f, 1.0f);

			script.direction.y = std::clamp(script.direction.y, -1.0f, 1.0f);

			changed |= inspector::DrawPropertyRow("Exit Direction", [&]() {
				ImGui::SetNextItemWidth(-FLT_MIN);

				bool direction_changed{ ImGui::DragFloat2(
					"##SceneExitDirection", &script.direction.x, 0.05f, -1.0f, 1.0f, "%.2f",
					ImGuiSliderFlags_AlwaysClamp
				) };

				DrawItemTooltip(
					"Direction the old scene exits. The new scene enters from the opposite "
					"direction."
				);

				return direction_changed;
			});
		}
	}

	changed |= inspector::DrawPropertyRow("Priority", [&]() {
		int priority{ static_cast<int>(script.priority) };

		ImGui::SetNextItemWidth(-FLT_MIN);

		bool priority_changed{ ImGui::DragInt("##SceneChangePriority", &priority, 1.0f, 0) };

		DrawItemTooltip(
			"Used to resolve competing scene changes. Higher priority takes precedence."
		);

		if (priority_changed) {
			script.priority = static_cast<std::size_t>(std::max(0, priority));
		}

		return priority_changed;
	});

	return changed;
}

bool DrawEmitSignalInline(ScriptEditorContext&, EmitSignalScript& script) {
	ImGui::SetNextItemWidth(-FLT_MIN);
	bool changed{ ImGui::InputTextWithHint("##SignalName", "Signal name", &script.signal.value) };
	DrawItemTooltip("Signal name to broadcast.");
	return changed;
}

bool DrawAddComponentsInline(ScriptEditorContext&, AddComponentsScript& script) {
	std::vector<std::string> selected_labels;
	std::string preview;

	for (const auto& definition : script.components) {
		const auto* component{ ComponentRegistry::Find(definition.type) };
		std::string label{ component ? ComponentLabel(*component) : definition.type };

		selected_labels.push_back(label);

		if (!preview.empty()) {
			preview += ", ";
		}
		preview += label;
	}

	if (preview.empty()) {
		preview = "None";
	}

	auto components{ GetSortedComponents([](const RegisteredComponent& component) {
		return CanAddComponentDefinition(component);
	}) };

	bool changed{ false };
	ImGui::SetNextItemWidth(-FLT_MIN);

	if (ImGui::BeginCombo("##AddComponents", preview.c_str())) {
		for (const auto* component : components) {
			bool selected{ std::ranges::any_of(
				script.components, [component](const ComponentDefinition& definition) {
					return definition.type == component->name;
				}
			) };

			std::string label{ ComponentLabel(*component) };

			if (ImGui::Checkbox(label.c_str(), &selected)) {
				if (selected) {
					script.components.push_back(MakeComponentDefinition(*component));
				} else {
					std::erase_if(
						script.components, [component](const ComponentDefinition& definition) {
							return definition.type == component->name;
						}
					);
				}

				changed = true;
			}

			if (ImGui::IsItemHovered()) {
				if (component->is_empty) {
					ImGui::SetTooltip("Tag component");
				} else {
					ImGui::SetTooltip("%s", component->name.c_str());
				}
			}
		}

		ImGui::EndCombo();
	}

	DrawSelectedItemsTooltip(selected_labels);
	return changed;
}

bool DrawAddComponentsDetails(ScriptEditorContext&, AddComponentsScript& script) {
	bool changed{ false };
	int remove{ -1 };

	for (int i{ 0 }; i < static_cast<int>(script.components.size()); ++i) {
		auto& definition{ script.components[static_cast<std::size_t>(i)] };
		const auto* component{ ComponentRegistry::Find(definition.type) };
		std::string label{ component ? ComponentLabel(*component) : definition.type };

		ImGui::PushID(i);

		if (ImGui::BeginTable("ComponentTitle", 2, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(
				"Remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight()
			);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
			ImGui::TableSetColumnIndex(0);
			ImGui::SeparatorText(label.c_str());

			if (component && component->is_empty) {
				DrawItemTooltip("Tag component");
			}

			ImGui::TableSetColumnIndex(1);
			if (ImGui::Button("x", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() })) {
				remove = i;
			}

			ImGui::EndTable();
		}

		if (component && !component->is_empty) {
			if (DrawRegisteredComponentJson(*component, definition.value)) {
				definition.apply_live = {};
				changed				  = true;
			}
		}

		ImGui::PopID();
	}

	if (remove >= 0) {
		script.components.erase(script.components.begin() + remove);
		changed = true;
	}

	return changed;
}

bool DrawRemoveComponentsInline(ScriptEditorContext&, RemoveComponentsScript& script) {
	std::vector<std::string> selected_labels;
	std::string preview;

	for (const auto& name : script.components) {
		const auto* component{ ComponentRegistry::Find(name) };
		std::string label{ component ? ComponentLabel(*component) : name };

		selected_labels.push_back(label);

		if (!preview.empty()) {
			preview += ", ";
		}
		preview += label;
	}

	if (preview.empty()) {
		preview = "None";
	}

	auto components{ GetSortedComponents([](const RegisteredComponent&) { return true; }) };

	bool changed{ false };
	ImGui::SetNextItemWidth(-FLT_MIN);

	if (ImGui::BeginCombo("##RemoveComponents", preview.c_str())) {
		for (const auto* component : components) {
			bool selected{ std::ranges::contains(script.components, component->name) };

			std::string label{ ComponentLabel(*component) };

			if (ImGui::Checkbox(label.c_str(), &selected)) {
				if (selected) {
					script.components.push_back(component->name);
				} else {
					std::erase(script.components, component->name);
				}

				changed = true;
			}

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", component->name.c_str());
			}
		}

		ImGui::EndCombo();
	}

	DrawSelectedItemsTooltip(selected_labels);
	return changed;
}

bool DrawMoveTo(ScriptEditorContext&, MoveToScript& script) {
	bool changed{ inspector::DrawPropertyRow("Destination", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat2("##Destination", &script.destination.x, 0.1f);
	}) };
	changed |= inspector::DrawPropertyRow("Relative", [&]() {
		return ImGui::Checkbox("##Relative", &script.relative);
	});
	return changed;
}

[[nodiscard]] std::vector<Entity> ResolveFollowPickerExcludedEntities(
	ScriptEditorContext& context
) {
	if (!context.owner) {
		return {};
	}

	if (!context.sequence_target_filter || !context.sequence_target_filter->has_value()) {
		return { context.owner };
	}

	return ResolveEntityFilter(
		context.sequence_target_filter->value(), context.owner.GetScene(), context.owner
	);
}

bool DrawFollowEntityPicker(
	ScriptEditorContext& context, UUID& target, const char* id,
	inspector::EntityFilterEditorState& state
) {
	if (!context.owner) {
		ImGui::TextDisabled("Entity target selection requires a scene instance.");
		return false;
	}

	auto& scene{ context.owner.GetScene() };
	std::vector<Entity> excluded_entities{ ResolveFollowPickerExcludedEntities(context) };
	Entity selected{ scene.GetEntity(target) };
	bool changed{ false };

	if (selected && std::ranges::contains(excluded_entities, selected)) {
		target	 = {};
		selected = {};
		changed	 = true;
	}

	EntityFilter filter;
	filter.type = EntityFilterType::Entity;

	if (selected) {
		SetEntityReference(filter.entity, selected);
	}

	inspector::EntityFilterEditorOptions options{
		.allow_select_owner		   = false,
		.single_entity_only		   = true,
		.hide_owner_descendants	   = true,
		.hide_excluded_descendants = true,
		.hide_managed_ui_parts	   = true,
		.excluded_entities		   = std::span<const Entity>{ excluded_entities },
	};

	ImGui::PushID(id);

	if (inspector::DrawEntityFilterButton(
			std::addressof(scene), context.owner, filter, state, options
		)) {
		if (filter.entity.uuid.has_value()) {
			target	= filter.entity.uuid.value();
			changed = true;
		}
	}

	ImGui::PopID();
	return changed;
}

bool DrawFollowTarget(ScriptEditorContext& context, FollowTargetScript& script) {
	static inspector::EntityFilterEditorState state;

	bool changed{ inspector::DrawPropertyRow("Target", [&]() {
		return DrawFollowEntityPicker(context, script.target, "FollowTargetTarget", state);
	}) };
	changed |= inspector::DrawPropertyRow("Speed", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat("##Speed", &script.speed, 1.0f, 0.0f);
	});
	changed |= inspector::DrawPropertyRow("Stopping Distance", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat("##StoppingDistance", &script.stopping_distance, 0.1f, 0.0f);
	});
	return changed;
}

bool DrawTintToInline(ScriptEditorContext& context, TintToScript& script) {
	ImGui::SetNextItemWidth(ImGui::GetFrameHeight());
	return DrawColorEdit(
		context.ctx, "##Tint", script.tint,
		ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
			ImGuiColorEditFlags_AlphaPreviewHalf
	);
}

bool DrawTintTo(ScriptEditorContext& context, TintToScript& script) {
	return inspector::DrawPropertyRow("Tint", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return DrawColorEdit(
			context.ctx, "##Tint", script.tint,
			ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_AlphaBar |
				ImGuiColorEditFlags_AlphaPreviewHalf
		);
	});
}

bool DrawFadeIn(ScriptEditorContext&, FadeInScript& script) {
	return inspector::DrawPropertyRow("Start Transparent", [&]() {
		return ImGui::Checkbox("##StartTransparent", &script.start_transparent);
	});
}

bool DrawFadeOut(ScriptEditorContext&, FadeOutScript& script) {
	return inspector::DrawPropertyRow("Start Opaque", [&]() {
		return ImGui::Checkbox("##StartOpaque", &script.start_opaque);
	});
}

bool DrawBounce(ScriptEditorContext&, BounceScript& script) {
	bool changed{ inspector::DrawPropertyRow("Amplitude", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat2("##Amplitude", &script.amplitude.x, 0.1f);
	}) };
	changed |= inspector::DrawPropertyRow("Static Offset", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat2("##StaticOffset", &script.static_offset.x, 0.1f);
	});
	changed |= inspector::DrawPropertyRow("Symmetrical", [&]() {
		return ImGui::Checkbox("##Symmetrical", &script.symmetrical);
	});
	return changed;
}

bool DrawStartBounce(ScriptEditorContext& context, StartBounceScript& script) {
	bool changed{ inspector::DrawPropertyRow("Amplitude", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat2("##Amplitude", &script.amplitude.x, 0.1f);
	}) };
	changed |= inspector::DrawPropertyRow("Period", [&]() {
		return inspector::DrawDurationTextInput(
			"##Period", script.period, -FLT_MIN, false, "Duration of one complete bounce cycle."
		);
	});
	changed |= inspector::DrawPropertyRow("Static Offset", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat2("##StaticOffset", &script.static_offset.x, 0.1f);
	});
	changed |= inspector::DrawPropertyRow("Symmetrical", [&]() {
		return ImGui::Checkbox("##Symmetrical", &script.symmetrical);
	});
	changed |= inspector::DrawPropertyRow("Force", [&]() {
		return ImGui::Checkbox("##Force", &script.force);
	});
	changed |= DrawReflectedScriptValue(context.ctx, script.ease);
	return changed;
}

bool DrawStopBounce(ScriptEditorContext&, StopBounceScript& script) {
	return inspector::DrawPropertyRow("Force", [&]() {
		return ImGui::Checkbox("##Force", &script.force);
	});
}

bool DrawShake(ScriptEditorContext& context, ShakeScript& script) {
	bool changed{ inspector::DrawPropertyRow("Intensity", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat("##Intensity", &script.intensity, 0.01f, 0.0f, 1.0f);
	}) };
	changed |= inspector::DrawPropertyRow("Reset On Complete", [&]() {
		return ImGui::Checkbox("##ResetOnComplete", &script.reset_on_complete);
	});
	changed |= DrawReflectedScriptValue(context.ctx, script.config);
	return changed;
}

bool DrawShakeAction(ScriptEditorContext& context, ShakeActionScript& script) {
	bool changed{ inspector::DrawPropertyRow("Intensity", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat("##Intensity", &script.intensity, 0.01f, 0.0f, 1.0f);
	}) };
	changed |= inspector::DrawPropertyRow("Force", [&]() {
		return ImGui::Checkbox("##Force", &script.force);
	});
	changed |= DrawReflectedScriptValue(context.ctx, script.config);
	return changed;
}

bool DrawStartShake(ScriptEditorContext& context, StartShakeScript& script) {
	bool changed{ inspector::DrawPropertyRow("Intensity", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat("##Intensity", &script.intensity, 0.01f, 0.0f, 1.0f);
	}) };
	changed |= inspector::DrawPropertyRow("Force", [&]() {
		return ImGui::Checkbox("##Force", &script.force);
	});
	changed |= DrawReflectedScriptValue(context.ctx, script.config);
	return changed;
}

bool DrawStopShake(ScriptEditorContext&, StopShakeScript& script) {
	return inspector::DrawPropertyRow("Force", [&]() {
		return ImGui::Checkbox("##Force", &script.force);
	});
}

bool DrawAddShakeTrauma(ScriptEditorContext&, AddShakeTraumaScript& script) {
	return inspector::DrawPropertyRow("Intensity", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat("##Intensity", &script.intensity, 0.01f, -1.0f, 1.0f);
	});
}

bool DrawRecoverShake(ScriptEditorContext& context, RecoverShakeScript& script) {
	return DrawReflectedScriptValue(context.ctx, script.config);
}

bool DrawFollowEntity(ScriptEditorContext& context, FollowEntityScript& script) {
	static inspector::EntityFilterEditorState state;

	bool changed{ inspector::DrawPropertyRow("Target", [&]() {
		return DrawFollowEntityPicker(context, script.target, "FollowEntityTarget", state);
	}) };
	changed |= DrawReflectedScriptValue(context.ctx, script.config);
	return changed;
}

bool DrawStartFollowEntity(ScriptEditorContext& context, StartFollowEntityScript& script) {
	static inspector::EntityFilterEditorState state;

	bool changed{ inspector::DrawPropertyRow("Target", [&]() {
		return DrawFollowEntityPicker(context, script.target, "StartFollowEntityTarget", state);
	}) };
	changed |= inspector::DrawPropertyRow("Force", [&]() {
		return ImGui::Checkbox("##Force", &script.force);
	});
	changed |= DrawReflectedScriptValue(context.ctx, script.config);
	return changed;
}

bool DrawWaypointRows(std::vector<V2_float>& waypoints) {
	bool changed{ false };
	int remove{ -1 };

	for (int i{ 0 }; i < static_cast<int>(waypoints.size()); ++i) {
		ImGui::PushID(i);
		std::string label{ "Waypoint " + std::to_string(i + 1) };
		changed |= inspector::DrawPropertyRow(label, [&]() {
			ImGui::SetNextItemWidth(-ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
			bool row_changed{
				ImGui::DragFloat2("##Waypoint", &waypoints[static_cast<std::size_t>(i)].x, 0.1f)
			};
			ImGui::SameLine();
			if (ImGui::Button("x", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() })) {
				remove = i;
			}
			return row_changed;
		});
		ImGui::PopID();
	}

	if (remove >= 0) {
		waypoints.erase(waypoints.begin() + remove);
		changed = true;
	}

	if (ImGui::Button("+ Waypoint", ImVec2{ -FLT_MIN, 0.0f })) {
		waypoints.emplace_back();
		changed = true;
	}

	return changed;
}

bool DrawStartFollowPath(ScriptEditorContext& context, StartFollowPathScript& script) {
	bool changed{ DrawWaypointRows(script.waypoints) };
	changed |= inspector::DrawPropertyRow("Force", [&]() {
		return ImGui::Checkbox("##Force", &script.force);
	});
	changed |= inspector::DrawPropertyRow("Reset Waypoint Index", [&]() {
		return ImGui::Checkbox("##ResetWaypointIndex", &script.reset_waypoint_index);
	});
	changed |= DrawReflectedScriptValue(context.ctx, script.config);
	return changed;
}

bool DrawStopFollow(ScriptEditorContext&, StopFollowScript& script) {
	bool changed{ inspector::DrawPropertyRow("Force", [&]() {
		return ImGui::Checkbox("##Force", &script.force);
	}) };
	changed |= inspector::DrawPropertyRow("Reset Previous Waypoints", [&]() {
		return ImGui::Checkbox("##ResetPreviousWaypoints", &script.reset_previous_waypoints);
	});
	return changed;
}

bool DrawFollowPath(ScriptEditorContext& context, FollowPathScript& script) {
	bool changed{ DrawWaypointRows(script.waypoints) };
	changed |= inspector::DrawPropertyRow("Reset Waypoint Index", [&]() {
		return ImGui::Checkbox("##ResetWaypointIndex", &script.reset_waypoint_index);
	});
	changed |= DrawReflectedScriptValue(context.ctx, script.config);
	return changed;
}
} // namespace

const EventEditorRegistration* EventEditorRegistry::Find(TypeHashValue type_hash) {
	for (const auto& entry : Entries()) {
		if (entry.type_hash == type_hash) {
			return &entry;
		}
	}
	return nullptr;
}

std::vector<EventEditorRegistration>& EventEditorRegistry::MutableEntries() {
	static std::vector<EventEditorRegistration> entries;
	return entries;
}

const std::vector<EventEditorRegistration>& EventEditorRegistry::Entries() {
	impl::EnsureEngineScriptEditorsRegistered();
	return MutableEntries();
}

const ScriptEditorRegistration* ScriptEditorRegistry::Find(TypeHashValue type_hash) {
	for (const auto& entry : Entries()) {
		if (entry.type_hash == type_hash) {
			return &entry;
		}
	}
	return nullptr;
}

std::vector<ScriptEditorRegistration>& ScriptEditorRegistry::MutableEntries() {
	static std::vector<ScriptEditorRegistration> entries;
	return entries;
}

const std::vector<ScriptEditorRegistration>& ScriptEditorRegistry::Entries() {
	impl::EnsureEngineScriptEditorsRegistered();
	return MutableEntries();
}

PTGN_REGISTER_SCRIPT(
	Script, {
				.label		 = "Script Sequence",
				.group		 = "Sequence",
				.description = "Editor authored sequence of registered scripts.",
				.type		 = ScriptType::Both,
				.draw_inline = &DrawScriptSequenceInline,
				.draw		 = &DrawNothing<Script>,
			}
);

PTGN_REGISTER_SCRIPT(
	WaitScript, {
					.label		 = "Delay",
					.group		 = "Timing",
					.description = "Wait before continuing.",
					.type		 = ScriptType::Sequence,
				}
);

PTGN_REGISTER_SCRIPT(
	MoveToScript, {
					  .label	   = "Move To",
					  .group	   = "Transform/Movement",
					  .description = "Move the owning entity.",
					  .type		   = ScriptType::Both,
					  .draw_inline = &DrawMoveToInline,
					  .draw		   = &DrawMoveTo,
				  }
);

PTGN_REGISTER_SCRIPT(
	RotateToScript, {
						.label		 = "Rotate To",
						.group		 = "Transform/Rotation",
						.description = "Rotate the owning entity.",
						.type		 = ScriptType::Sequence,
						.draw_inline = &DrawRotateToInline,
					}
);

PTGN_REGISTER_SCRIPT(
	ScaleToScript, {
					   .label		= "Scale To",
					   .group		= "Transform/Scale",
					   .description = "Scale the owning entity.",
					   .type		= ScriptType::Sequence,
					   .draw_inline = &DrawScaleToInline,
				   }
);

PTGN_REGISTER_SCRIPT(
	TintToScript, {
					  .label	   = "Tint To",
					  .group	   = "Animation/Color",
					  .description = "Animate the target tint to a color.",
					  .type		   = ScriptType::Sequence,
					  .draw_inline = &DrawTintToInline,
					  .draw		   = &DrawTintTo,
				  }
);

PTGN_REGISTER_SCRIPT(
	FadeInScript, {
					  .label	   = "Fade In",
					  .group	   = "Animation/Color",
					  .description = "Fade the target tint to white over a Tween duration.",
					  .type		   = ScriptType::Sequence,
					  .draw		   = &DrawFadeIn,
				  }
);

PTGN_REGISTER_SCRIPT(
	FadeOutScript, {
					   .label		= "Fade Out",
					   .group		= "Animation/Color",
					   .description = "Fade the target tint to transparent over a Tween duration.",
					   .type		= ScriptType::Sequence,
					   .draw		= &DrawFadeOut,
				   }
);

PTGN_REGISTER_SCRIPT(
	BounceScript, {
					  .label	   = "Bounce",
					  .group	   = "Animation/Bounce",
					  .description = "Apply a positional bounce offset.",
					  .type		   = ScriptType::Sequence,
					  .draw		   = &DrawBounce,
				  }
);

PTGN_REGISTER_SCRIPT(
	StartBounceScript, {
						   .label		= "Start Bounce",
						   .group		= "Animation/Bounce",
						   .description = "Start an indefinitely repeating channelized bounce.",
						   .type		= ScriptType::Sequence,
						   .draw		= &DrawStartBounce,
					   }
);

PTGN_REGISTER_SCRIPT(
	StopBounceScript, {
						  .label	   = "Stop Bounce",
						  .group	   = "Animation/Bounce",
						  .description = "Stop the channelized bounce on the target.",
						  .type		   = ScriptType::Sequence,
						  .draw		   = &DrawStopBounce,
					  }
);

PTGN_REGISTER_SCRIPT(
	ShakeScript, {
					 .label		  = "Shake",
					 .group		  = "Animation/Shake",
					 .description = "Apply continuous shake over the Tween duration.",
					 .type		  = ScriptType::Sequence,
					 .draw		  = &DrawShake,
				 }
);

PTGN_REGISTER_SCRIPT(
	ShakeActionScript,
	{
		.label		 = "Shake",
		.group		 = "Animation/Shake",
		.description = "Apply shake trauma instantly and recover in the background.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawShakeAction,
	}
);

PTGN_REGISTER_SCRIPT(
	AddShakeTraumaScript,
	{
		.label = "Add Shake Trauma",
		.group = "Animation/Shake",
		.description =
			"Temporarily modify a persistent shake; trauma recovers back to its baseline.",
		.type = ScriptType::Sequence,
		.draw = &DrawAddShakeTrauma,
	}
);

PTGN_REGISTER_SCRIPT(
	RecoverShakeScript,
	{
		.label		 = "Recover Shake",
		.group		 = "Animation/Shake",
		.description = "Internal recovery step that returns shake trauma to its baseline.",
		.type		 = ScriptType::Sequence,
		.hidden		 = true,
		.draw		 = &DrawRecoverShake,
	}
);

PTGN_REGISTER_SCRIPT(
	ResetShakeScript,
	{
		.label		 = "Reset Shake",
		.group		 = "Animation/Shake",
		.description = "Internal cleanup step that clears shake trauma and offsets.",
		.type		 = ScriptType::Sequence,
		.hidden		 = true,
	}
);

PTGN_REGISTER_SCRIPT(
	StartShakeScript,
	{
		.label		 = "Start Shake",
		.group		 = "Animation/Shake",
		.description = "Start persistent shake and continue immediately; it runs until Stop Shake.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawStartShake,
	}
);

PTGN_REGISTER_SCRIPT(
	StopShakeScript,
	{
		.label = "Stop Shake",
		.group = "Animation/Shake",
		.description =
			"Stop the persistent shake, clear its trauma/offsets, and continue immediately.",
		.type = ScriptType::Sequence,
		.draw = &DrawStopShake,
	}
);

PTGN_REGISTER_SCRIPT(
	FollowTargetScript, {
							.label		 = "Follow Target",
							.group		 = "Transform/Follow",
							.description = "Follow an entity until close enough.",
							.type		 = ScriptType::Both,
							.draw		 = &DrawFollowTarget,
						}
);

PTGN_REGISTER_SCRIPT(
	FollowEntityScript, {
							.label		 = "Follow Entity",
							.group		 = "Transform/Follow",
							.description = "Follow an entity using TargetFollowConfig.",
							.type		 = ScriptType::Both,
							.draw		 = &DrawFollowEntity,
						}
);

PTGN_REGISTER_SCRIPT(
	FollowPathScript, {
						  .label	   = "Follow Path",
						  .group	   = "Transform/Follow",
						  .description = "Follow a configurable waypoint path.",
						  .type		   = ScriptType::Both,
						  .draw		   = &DrawFollowPath,
					  }
);

PTGN_REGISTER_SCRIPT(
	StartFollowEntityScript,
	{
		.label		 = "Start Follow Entity",
		.group		 = "Transform/Follow",
		.description = "Start channelized entity following without blocking the sequence.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawStartFollowEntity,
	}
);

PTGN_REGISTER_SCRIPT(
	StartFollowPathScript,
	{
		.label		 = "Start Follow Path",
		.group		 = "Transform/Follow",
		.description = "Start channelized path following without blocking the sequence.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawStartFollowPath,
	}
);

PTGN_REGISTER_SCRIPT(
	StopFollowScript, {
						  .label	   = "Stop Follow",
						  .group	   = "Transform/Follow",
						  .description = "Stop channelized target or path following.",
						  .type		   = ScriptType::Sequence,
						  .draw		   = &DrawStopFollow,
					  }
);

PTGN_REGISTER_SCRIPT(
	SetVisibleScript, {
						  .label	   = "Set Visibility",
						  .group	   = "Entity/Appearance",
						  .description = "Set owner visibility.",
						  .type		   = ScriptType::Sequence,
						  .menu_order  = 3,
						  .draw_inline = &DrawSetVisibleInline,
					  }
);

PTGN_REGISTER_SCRIPT(
	PlaySoundScript, {
						 .label		  = "Play Sound",
						 .group		  = "Audio/Playback",
						 .description = "Play an audio asset.",
						 .type		  = ScriptType::Sequence,
						 .draw_inline = &DrawPlaySoundInline,
						 .draw		  = &DrawPlaySound,
					 }
);

// Reflected single member scripts share the same compact value renderer.
// Domain and menu information is deliberately not part of the script type.
template <typename T>
constexpr bool kCompactScriptValue =
	std::same_as<T, float> || std::same_as<T, int> || std::same_as<T, std::size_t> ||
	std::same_as<T, std::int64_t> || std::same_as<T, V2_float> || std::same_as<T, std::string> ||
	std::same_as<T, Color> || std::same_as<T, bool> || std::same_as<T, millisecondsf>;

template <typename T>
bool DrawSingleScriptValue(EditorContext& context, T& script) {
	if constexpr (std::tuple_size_v<decltype(ReflectMembers(script))> != 1) {
		return false;
	} else {
		auto member{ std::get<0>(ReflectMembers(script)) };
		auto& value{ member.value };
		using Value = std::remove_cvref_t<decltype(value)>;
		ImGui::SetNextItemWidth(-FLT_MIN);
		if constexpr (std::same_as<Value, float>) {
			return ImGui::DragFloat("##Value", &value, 0.1f);
		} else if constexpr (std::same_as<Value, int>) {
			return ImGui::InputInt("##Value", &value);
		} else if constexpr (std::same_as<Value, std::size_t>) {
			return ImGui::DragScalar("##Value", ImGuiDataType_U64, &value, 1.0f);
		} else if constexpr (std::same_as<Value, std::int64_t>) {
			return ImGui::DragScalar("##Value", ImGuiDataType_S64, &value, 1.0f);
		} else if constexpr (std::same_as<Value, V2_float>) {
			return ImGui::DragFloat2("##Value", &value.x, 0.25f);
		} else if constexpr (std::same_as<Value, std::string>) {
			return ImGui::InputText("##Value", &value);
		} else if constexpr (std::same_as<Value, Color>) {
			return DrawColorEdit(context, "##Color", value);
		} else if constexpr (std::same_as<Value, bool>) {
			return ImGui::Checkbox("##Value", &value);
		} else if constexpr (std::same_as<Value, millisecondsf>) {
			return inspector::DrawDurationTextInput(
				"##Duration", value, -FLT_MIN, false, "Duration of this operation."
			);
		} else {
			return false;
		}
	}
}

// All added actions are ordinary independent registered scripts.
// Their fields are exposed through the same reflected Inspector layout as components.
// Reflection determines whether a script can fit in one action row.
// Group and label information is defined only in the editor registrations below.
template <typename T>
bool DrawTypedActionInline(ScriptEditorContext& context, T& script) {
	if constexpr (std::is_same_v<T, SetUITextScript>) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::InputText("##Content", &script.content);
	} else if constexpr (std::is_same_v<T, TooltipSetPositionScript>) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat2("##Position", &script.position.x, 1.0f);
	} else if constexpr (std::is_same_v<T, TooltipSetHoverOffsetScript>) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat2("##Offset", &script.offset.x, 1.0f);
	} else if constexpr (std::is_same_v<T, DialogueGoToPageScript>) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::InputInt("##Page", &script.page_index);
	} else if constexpr (requires { script.timer; }) {
		return DrawTimerKeyInline(
			"##Timer", script.timer, GetTimerActionChoices(context), -FLT_MIN
		);
	} else if constexpr (requires { script.counter; }) {
		return DrawVariableKeyInline(
			"##Counter", script.counter, GetVariableActionChoices<CounterKey>(context), -FLT_MIN
		);
	} else if constexpr (requires { script.boolean; }) {
		return DrawVariableKeyInline(
			"##Boolean", script.boolean, GetVariableActionChoices<BooleanKey>(context), -FLT_MIN
		);
	} else if constexpr (requires { script.animation_key; }) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::InputTextWithHint(
			"##AnimationKey", "Active animation", &script.animation_key
		);
	} else if constexpr (requires { script.dialogue; }) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::InputText("##DialogueKey", &script.dialogue);
	} else if constexpr (requires { script.amount; }) {
		return inspector::DrawDurationTextInput(
			"##Duration", script.amount, -FLT_MIN, false, "Duration of this operation."
		);
	} else {
		return DrawSingleScriptValue(context.ctx, script);
	}
}

template <typename T>
bool DrawTypedActionDetails(ScriptEditorContext& context, T& script) {
	if constexpr (std::is_same_v<T, DialogueSetPortraitScript>) {
		return DrawReflectedScriptValue(context.ctx, script);
	} else if constexpr (
		std::is_same_v<T, SetUITextScript> || std::is_same_v<T, TooltipSetPositionScript> ||
		std::is_same_v<T, TooltipSetHoverOffsetScript> || std::is_same_v<T, DialogueGoToPageScript>
	) {
		return false;
	} else if constexpr (requires {
							 script.timer;
							 script.amount;
						 }) {
		return inspector::DrawPropertyRow("Duration", [&]() {
			return inspector::DrawDurationTextInput(
				"##Duration", script.amount, -FLT_MIN, false, "Duration of this operation."
			);
		});
	} else if constexpr (requires {
							 script.counter;
							 script.counter_amount;
						 }) {
		return inspector::DrawPropertyRow("Amount", [&]() {
			return ImGui::DragScalar("##Amount", ImGuiDataType_S64, &script.counter_amount, 1.0f);
		});
	} else if constexpr (requires {
							 script.animation_key;
							 script.force;
						 }) {
		return inspector::DrawPropertyRow("Force", [&]() {
			return ImGui::Checkbox("##Force", &script.force);
		});
	} else if constexpr (requires {
							 script.animation_key;
							 script.reset_on_stop;
						 }) {
		return inspector::DrawPropertyRow("Reset On Stop", [&]() {
			return ImGui::Checkbox("##Reset", &script.reset_on_stop);
		});
	} else if constexpr (requires {
							 script.animation_key;
							 script.frame;
						 }) {
		return inspector::DrawPropertyRow("Frame", [&]() {
			int value{ static_cast<int>(script.frame) };
			bool changed{ ImGui::InputInt("##Frame", &value) };
			if (changed) {
				script.frame = static_cast<std::size_t>(std::max(0, value));
			}
			return changed;
		});
	} else if constexpr (
		requires { script.timer; } || requires { script.counter; } ||
		requires { script.boolean; } || requires { script.animation_key; } ||
		requires { script.dialogue; } || requires { script.amount; }
	) {
		return false;
	} else if constexpr (std::tuple_size_v<decltype(ReflectMembers(script))> == 0) {
		return false;
	} else if constexpr (std::tuple_size_v<decltype(ReflectMembers(script))> == 1) {
		auto member{ std::get<0>(ReflectMembers(script)) };
		using Value = std::remove_cvref_t<decltype(member.value)>;
		if constexpr (kCompactScriptValue<Value>) {
			return false;
		} else {
			return DrawReflectedScriptValue(context.ctx, script);
		}
	} else {
		return DrawReflectedScriptValue(context.ctx, script);
	}
}

bool DrawShaderSetUniformScriptDetails(
	ScriptEditorContext& context, ShaderSetUniformScript& script
) {
	bool changed{ false };
	auto row = [&](std::string_view label, auto&& draw) {
		changed |= inspector::DrawPropertyRow(label, [&]() {
			ImGui::SetNextItemWidth(-FLT_MIN);
			return draw();
		});
	};
	row("Uniform", [&]() { return ImGui::InputText("##Uniform", &script.property); });
	row("Type", [&]() {
		const char* current{ script.text.empty() ? "Float" : script.text.c_str() };
		bool row_changed{ false };
		if (ImGui::BeginCombo("##UniformType", current)) {
			for (const char* type : { "Float", "Int", "Vector", "Bool" }) {
				if (ImGui::Selectable(type, current == std::string_view{ type })) {
					script.text = type;
					row_changed = true;
				}
			}
			ImGui::EndCombo();
		}
		return row_changed;
	});
	if (script.text == "Int") {
		row("Value", [&]() { return ImGui::InputInt("##UniformInt", &script.index); });
	} else if (script.text == "Vector") {
		row("Value",
			[&]() { return ImGui::DragFloat2("##UniformVector", &script.vector.x, 0.1f); });
	} else if (script.text == "Bool") {
		row("Value", [&]() { return ImGui::Checkbox("##UniformBool", &script.option); });
	} else {
		row("Value", [&]() { return ImGui::DragFloat("##UniformFloat", &script.number, 0.1f); });
	}
	return changed;
}

bool DrawEffectSetEffectPropertyScriptDetails(
	ScriptEditorContext& context, EffectSetEffectPropertyScript& script
) {
	bool changed{ false };
	auto row = [&](std::string_view label, auto&& draw) {
		changed |= inspector::DrawPropertyRow(label, [&]() {
			ImGui::SetNextItemWidth(-FLT_MIN);
			return draw();
		});
	};
	Entity target{ ResolvePrintPreviewTarget(context) };
	const ::ptgn::impl::RegisteredEffect* selected_effect{ nullptr };
	if (!script.text.empty()) {
		selected_effect = ::ptgn::impl::EffectRegistry::Find(script.text);
	} else if (target) {
		for (const auto& candidate : ::ptgn::impl::EffectRegistry::Entries()) {
			const auto* component{ ComponentRegistry::Find(candidate.type_name) };
			if (component && component->Has(target)) {
				selected_effect = &candidate;
				break;
			}
		}
	}

	row("Effect", [&]() {
		bool row_changed{ false };
		const char* preview{ script.text.empty() ? "Target Effect" : script.text.c_str() };
		if (ImGui::BeginCombo("##EffectType", preview)) {
			if (ImGui::Selectable("Target Effect", script.text.empty())) {
				script.text.clear();
				script.property.clear();
				row_changed = true;
			}
			for (const auto& candidate : ::ptgn::impl::EffectRegistry::Entries()) {
				if (ImGui::Selectable(
						candidate.display_name.c_str(), script.text == candidate.type_name
					)) {
					script.text = candidate.type_name;
					script.property.clear();
					row_changed = true;
				}
			}
			ImGui::EndCombo();
		}
		return row_changed;
	});

	// Reflect the selected effect's own parameters, including values from a live target.
	// After changing effect, resolve the selection again on the next frame.
	selected_effect =
		script.text.empty() ? selected_effect : ::ptgn::impl::EffectRegistry::Find(script.text);
	json properties = json::object();
	if (selected_effect) {
		const auto* component{ ComponentRegistry::Find(selected_effect->type_name) };
		if (target && component && component->Has(target) && selected_effect->serialize) {
			properties = selected_effect->serialize(target);
		} else if (selected_effect->make_default) {
			properties = selected_effect->make_default();
		}
	}

	row("Property", [&]() {
		bool row_changed{ false };
		if (!properties.is_object()) {
			return ImGui::InputText("##EffectProperty", &script.property);
		}
		if (ImGui::BeginCombo(
				"##EffectProperty",
				script.property.empty() ? "Select Property" : script.property.c_str()
			)) {
			for (auto it{ properties.begin() }; it != properties.end(); ++it) {
				if (ImGui::Selectable(it.key().c_str(), it.key() == script.property)) {
					script.property = it.key();
					row_changed		= true;
				}
			}
			ImGui::EndCombo();
		}
		return row_changed;
	});

	if (!properties.is_object() || !properties.contains(script.property)) {
		return changed;
	}
	const json& value{ properties.at(script.property) };
	if (script.property == "tint" || script.property == "color") {
		row("Value",
			[&]() { return DrawColorEdit(context.ctx, "##EffectColor", script.selected_color); });
	} else if (value.is_boolean()) {
		row("Value", [&]() { return ImGui::Checkbox("##EffectBool", &script.option); });
	} else if (value.is_number_integer() || value.is_number_unsigned()) {
		row("Value", [&]() { return ImGui::InputInt("##EffectInteger", &script.index); });
	} else if (value.is_number()) {
		row("Value", [&]() { return ImGui::DragFloat("##EffectNumber", &script.number, 0.1f); });
	} else if (value.is_string()) {
		row("Value", [&]() { return ImGui::InputText("##EffectString", &script.choice); });
	}
	return changed;
}

PTGN_REGISTER_SCRIPT(
	AnimationStartScript, {
							  .label	   = "Start",
							  .group	   = "Animation/Playback",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<AnimationStartScript>,
							  .draw		   = &DrawTypedActionDetails<AnimationStartScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	AnimationStopScript, {
							 .label		  = "Stop",
							 .group		  = "Animation/Playback",
							 .type		  = ScriptType::Sequence,
							 .draw_inline = &DrawTypedActionInline<AnimationStopScript>,
							 .draw		  = &DrawTypedActionDetails<AnimationStopScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	AnimationResetScript, {
							  .label	   = "Reset",
							  .group	   = "Animation/Playback",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<AnimationResetScript>,
							  .draw		   = &DrawTypedActionDetails<AnimationResetScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	AnimationPauseScript, {
							  .label	   = "Pause",
							  .group	   = "Animation/Playback",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<AnimationPauseScript>,
							  .draw		   = &DrawTypedActionDetails<AnimationPauseScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	AnimationResumeScript, {
							   .label		= "Resume",
							   .group		= "Animation/Playback",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<AnimationResumeScript>,
							   .draw		= &DrawTypedActionDetails<AnimationResumeScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	AnimationTogglePlayingScript,
	{
		.label		 = "Toggle Playing",
		.group		 = "Animation/Playback",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<AnimationTogglePlayingScript>,
		.draw		 = &DrawTypedActionDetails<AnimationTogglePlayingScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	AnimationSetFrameScript, {
								 .label		  = "Set Frame",
								 .group		  = "Animation/Playback",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<AnimationSetFrameScript>,
								 .draw		  = &DrawTypedActionDetails<AnimationSetFrameScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	AnimationNextFrameScript, {
								  .label	   = "Next Frame",
								  .group	   = "Animation/Playback",
								  .type		   = ScriptType::Sequence,
								  .draw_inline = &DrawTypedActionInline<AnimationNextFrameScript>,
								  .draw		   = &DrawTypedActionDetails<AnimationNextFrameScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	AnimationPreviousFrameScript,
	{
		.label		 = "Previous Frame",
		.group		 = "Animation/Playback",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<AnimationPreviousFrameScript>,
		.draw		 = &DrawTypedActionDetails<AnimationPreviousFrameScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	TimerStartScript, {
						  .label	   = "Start",
						  .group	   = "Timing/Timer",
						  .type		   = ScriptType::Sequence,
						  .draw_inline = &DrawTypedActionInline<TimerStartScript>,
						  .draw		   = &DrawTypedActionDetails<TimerStartScript>,
					  }
);
PTGN_REGISTER_SCRIPT(
	TimerRestartScript, {
							.label		 = "Restart",
							.group		 = "Timing/Timer",
							.type		 = ScriptType::Sequence,
							.draw_inline = &DrawTypedActionInline<TimerRestartScript>,
							.draw		 = &DrawTypedActionDetails<TimerRestartScript>,
						}
);
PTGN_REGISTER_SCRIPT(
	TimerStopScript, {
						 .label		  = "Stop",
						 .group		  = "Timing/Timer",
						 .type		  = ScriptType::Sequence,
						 .draw_inline = &DrawTypedActionInline<TimerStopScript>,
						 .draw		  = &DrawTypedActionDetails<TimerStopScript>,
					 }
);
PTGN_REGISTER_SCRIPT(
	TimerResetScript, {
						  .label	   = "Reset",
						  .group	   = "Timing/Timer",
						  .type		   = ScriptType::Sequence,
						  .draw_inline = &DrawTypedActionInline<TimerResetScript>,
						  .draw		   = &DrawTypedActionDetails<TimerResetScript>,
					  }
);
PTGN_REGISTER_SCRIPT(
	TimerPauseScript, {
						  .label	   = "Pause",
						  .group	   = "Timing/Timer",
						  .type		   = ScriptType::Sequence,
						  .draw_inline = &DrawTypedActionInline<TimerPauseScript>,
						  .draw		   = &DrawTypedActionDetails<TimerPauseScript>,
					  }
);
PTGN_REGISTER_SCRIPT(
	TimerResumeScript, {
						   .label		= "Resume",
						   .group		= "Timing/Timer",
						   .type		= ScriptType::Sequence,
						   .draw_inline = &DrawTypedActionInline<TimerResumeScript>,
						   .draw		= &DrawTypedActionDetails<TimerResumeScript>,
					   }
);
PTGN_REGISTER_SCRIPT(
	TimerTogglePausedScript, {
								 .label		  = "Toggle Paused",
								 .group		  = "Timing/Timer",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<TimerTogglePausedScript>,
								 .draw		  = &DrawTypedActionDetails<TimerTogglePausedScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	TimerAdvanceScript, {
							.label		 = "Advance",
							.group		 = "Timing/Timer",
							.type		 = ScriptType::Sequence,
							.draw_inline = &DrawTypedActionInline<TimerAdvanceScript>,
							.draw		 = &DrawTypedActionDetails<TimerAdvanceScript>,
						}
);
PTGN_REGISTER_SCRIPT(
	TimerRewindScript, {
						   .label		= "Rewind",
						   .group		= "Timing/Timer",
						   .type		= ScriptType::Sequence,
						   .draw_inline = &DrawTypedActionInline<TimerRewindScript>,
						   .draw		= &DrawTypedActionDetails<TimerRewindScript>,
					   }
);
PTGN_REGISTER_SCRIPT(
	TimerSetDurationScript, {
								.label		 = "Set Duration",
								.group		 = "Timing/Timer",
								.type		 = ScriptType::Sequence,
								.draw_inline = &DrawTypedActionInline<TimerSetDurationScript>,
								.draw		 = &DrawTypedActionDetails<TimerSetDurationScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	TimerAddDurationScript, {
								.label		 = "Add Duration",
								.group		 = "Timing/Timer",
								.type		 = ScriptType::Sequence,
								.draw_inline = &DrawTypedActionInline<TimerAddDurationScript>,
								.draw		 = &DrawTypedActionDetails<TimerAddDurationScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	TimerRemoveDurationScript, {
								   .label		= "Remove Duration",
								   .group		= "Timing/Timer",
								   .type		= ScriptType::Sequence,
								   .draw_inline = &DrawTypedActionInline<TimerRemoveDurationScript>,
								   .draw = &DrawTypedActionDetails<TimerRemoveDurationScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	LifetimeStartScript, {
							 .label		  = "Start",
							 .group		  = "Timing/Lifetime",
							 .type		  = ScriptType::Sequence,
							 .draw_inline = &DrawTypedActionInline<LifetimeStartScript>,
							 .draw		  = &DrawTypedActionDetails<LifetimeStartScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	LifetimeRestartScript, {
							   .label		= "Restart",
							   .group		= "Timing/Lifetime",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<LifetimeRestartScript>,
							   .draw		= &DrawTypedActionDetails<LifetimeRestartScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	LifetimeStopScript, {
							.label		 = "Stop",
							.group		 = "Timing/Lifetime",
							.type		 = ScriptType::Sequence,
							.draw_inline = &DrawTypedActionInline<LifetimeStopScript>,
							.draw		 = &DrawTypedActionDetails<LifetimeStopScript>,
						}
);
PTGN_REGISTER_SCRIPT(
	LifetimeResetScript, {
							 .label		  = "Reset",
							 .group		  = "Timing/Lifetime",
							 .type		  = ScriptType::Sequence,
							 .draw_inline = &DrawTypedActionInline<LifetimeResetScript>,
							 .draw		  = &DrawTypedActionDetails<LifetimeResetScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	LifetimePauseScript, {
							 .label		  = "Pause",
							 .group		  = "Timing/Lifetime",
							 .type		  = ScriptType::Sequence,
							 .draw_inline = &DrawTypedActionInline<LifetimePauseScript>,
							 .draw		  = &DrawTypedActionDetails<LifetimePauseScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	LifetimeResumeScript, {
							  .label	   = "Resume",
							  .group	   = "Timing/Lifetime",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<LifetimeResumeScript>,
							  .draw		   = &DrawTypedActionDetails<LifetimeResumeScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	LifetimeTogglePausedScript,
	{
		.label		 = "Toggle Paused",
		.group		 = "Timing/Lifetime",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<LifetimeTogglePausedScript>,
		.draw		 = &DrawTypedActionDetails<LifetimeTogglePausedScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	LifetimeAdvanceScript, {
							   .label		= "Advance",
							   .group		= "Timing/Lifetime",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<LifetimeAdvanceScript>,
							   .draw		= &DrawTypedActionDetails<LifetimeAdvanceScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	LifetimeRewindScript, {
							  .label	   = "Rewind",
							  .group	   = "Timing/Lifetime",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<LifetimeRewindScript>,
							  .draw		   = &DrawTypedActionDetails<LifetimeRewindScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	LifetimeSetDurationScript, {
								   .label		= "Set Duration",
								   .group		= "Timing/Lifetime",
								   .type		= ScriptType::Sequence,
								   .draw_inline = &DrawTypedActionInline<LifetimeSetDurationScript>,
								   .draw = &DrawTypedActionDetails<LifetimeSetDurationScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	LifetimeAddDurationScript, {
								   .label		= "Add Duration",
								   .group		= "Timing/Lifetime",
								   .type		= ScriptType::Sequence,
								   .draw_inline = &DrawTypedActionInline<LifetimeAddDurationScript>,
								   .draw = &DrawTypedActionDetails<LifetimeAddDurationScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	LifetimeRemoveDurationScript,
	{
		.label		 = "Remove Duration",
		.group		 = "Timing/Lifetime",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<LifetimeRemoveDurationScript>,
		.draw		 = &DrawTypedActionDetails<LifetimeRemoveDurationScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	CounterSetScript, {
						  .label	   = "Set",
						  .group	   = "Variables/Counter",
						  .type		   = ScriptType::Sequence,
						  .draw_inline = &DrawTypedActionInline<CounterSetScript>,
						  .draw		   = &DrawTypedActionDetails<CounterSetScript>,
					  }
);
PTGN_REGISTER_SCRIPT(
	CounterIncrementScript, {
								.label		 = "Increment",
								.group		 = "Variables/Counter",
								.type		 = ScriptType::Sequence,
								.draw_inline = &DrawTypedActionInline<CounterIncrementScript>,
								.draw		 = &DrawTypedActionDetails<CounterIncrementScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	CounterDecrementScript, {
								.label		 = "Decrement",
								.group		 = "Variables/Counter",
								.type		 = ScriptType::Sequence,
								.draw_inline = &DrawTypedActionInline<CounterDecrementScript>,
								.draw		 = &DrawTypedActionDetails<CounterDecrementScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	CounterResetScript, {
							.label		 = "Reset",
							.group		 = "Variables/Counter",
							.type		 = ScriptType::Sequence,
							.draw_inline = &DrawTypedActionInline<CounterResetScript>,
							.draw		 = &DrawTypedActionDetails<CounterResetScript>,
						}
);
PTGN_REGISTER_SCRIPT(
	BooleanSetTrueScript, {
							  .label	   = "Set True",
							  .group	   = "Variables/Boolean",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<BooleanSetTrueScript>,
							  .draw		   = &DrawTypedActionDetails<BooleanSetTrueScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	BooleanSetFalseScript, {
							   .label		= "Set False",
							   .group		= "Variables/Boolean",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<BooleanSetFalseScript>,
							   .draw		= &DrawTypedActionDetails<BooleanSetFalseScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	BooleanToggleScript, {
							 .label		  = "Toggle",
							 .group		  = "Variables/Boolean",
							 .type		  = ScriptType::Sequence,
							 .draw_inline = &DrawTypedActionInline<BooleanToggleScript>,
							 .draw		  = &DrawTypedActionDetails<BooleanToggleScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	BooleanResetScript, {
							.label		 = "Reset",
							.group		 = "Variables/Boolean",
							.type		 = ScriptType::Sequence,
							.draw_inline = &DrawTypedActionInline<BooleanResetScript>,
							.draw		 = &DrawTypedActionDetails<BooleanResetScript>,
						}
);
PTGN_REGISTER_SCRIPT(
	DialogueOpenScript, {
							.label		 = "Open",
							.group		 = "UI/Dialogue",
							.type		 = ScriptType::Sequence,
							.draw_inline = &DrawTypedActionInline<DialogueOpenScript>,
							.draw		 = &DrawTypedActionDetails<DialogueOpenScript>,
						}
);
PTGN_REGISTER_SCRIPT(
	DialogueCloseScript, {
							 .label		  = "Close",
							 .group		  = "UI/Dialogue",
							 .type		  = ScriptType::Sequence,
							 .draw_inline = &DrawTypedActionInline<DialogueCloseScript>,
							 .draw		  = &DrawTypedActionDetails<DialogueCloseScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	DialogueAdvanceScript, {
							   .label		= "Advance",
							   .group		= "UI/Dialogue",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<DialogueAdvanceScript>,
							   .draw		= &DrawTypedActionDetails<DialogueAdvanceScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	DialogueNextPageScript, {
								.label		 = "Next Page",
								.group		 = "UI/Dialogue",
								.type		 = ScriptType::Sequence,
								.draw_inline = &DrawTypedActionInline<DialogueNextPageScript>,
								.draw		 = &DrawTypedActionDetails<DialogueNextPageScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	DialogueCompletePageScript,
	{
		.label		 = "Complete Page",
		.group		 = "UI/Dialogue",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<DialogueCompletePageScript>,
		.draw		 = &DrawTypedActionDetails<DialogueCompletePageScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	DialogueChangeDialogueScript,
	{
		.label		 = "Change Dialogue",
		.group		 = "UI/Dialogue",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<DialogueChangeDialogueScript>,
		.draw		 = &DrawTypedActionDetails<DialogueChangeDialogueScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	DialogueSelectDialogueScript,
	{
		.label		 = "Select Dialogue",
		.group		 = "UI/Dialogue",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<DialogueSelectDialogueScript>,
		.draw		 = &DrawTypedActionDetails<DialogueSelectDialogueScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	DialogueNextDialogueScript,
	{
		.label		 = "Next Dialogue",
		.group		 = "UI/Dialogue",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<DialogueNextDialogueScript>,
		.draw		 = &DrawTypedActionDetails<DialogueNextDialogueScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	DialogueOpenNextDialogueScript,
	{
		.label		 = "Open Next Dialogue",
		.group		 = "UI/Dialogue",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<DialogueOpenNextDialogueScript>,
		.draw		 = &DrawTypedActionDetails<DialogueOpenNextDialogueScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	TooltipShowScript, {
						   .label		= "Show",
						   .group		= "UI/Tooltip",
						   .type		= ScriptType::Sequence,
						   .draw_inline = &DrawTypedActionInline<TooltipShowScript>,
						   .draw		= &DrawTypedActionDetails<TooltipShowScript>,
					   }
);
PTGN_REGISTER_SCRIPT(
	TooltipHideScript, {
						   .label		= "Hide",
						   .group		= "UI/Tooltip",
						   .type		= ScriptType::Sequence,
						   .draw_inline = &DrawTypedActionInline<TooltipHideScript>,
						   .draw		= &DrawTypedActionDetails<TooltipHideScript>,
					   }
);
PTGN_REGISTER_SCRIPT(
	TooltipEnableHoverScript, {
								  .label	   = "Enable Hover",
								  .group	   = "UI/Tooltip",
								  .type		   = ScriptType::Sequence,
								  .draw_inline = &DrawTypedActionInline<TooltipEnableHoverScript>,
								  .draw		   = &DrawTypedActionDetails<TooltipEnableHoverScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	TooltipDisableHoverScript, {
								   .label		= "Disable Hover",
								   .group		= "UI/Tooltip",
								   .type		= ScriptType::Sequence,
								   .draw_inline = &DrawTypedActionInline<TooltipDisableHoverScript>,
								   .draw = &DrawTypedActionDetails<TooltipDisableHoverScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	TooltipToggleHoverScript, {
								  .label	   = "Toggle Hover",
								  .group	   = "UI/Tooltip",
								  .type		   = ScriptType::Sequence,
								  .draw_inline = &DrawTypedActionInline<TooltipToggleHoverScript>,
								  .draw		   = &DrawTypedActionDetails<TooltipToggleHoverScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	TooltipToggleVisibilityScript,
	{
		.label		 = "Toggle Visibility",
		.group		 = "UI/Tooltip",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<TooltipToggleVisibilityScript>,
		.draw		 = &DrawTypedActionDetails<TooltipToggleVisibilityScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ButtonPressScript, {
						   .label		= "Press",
						   .group		= "UI/Button",
						   .type		= ScriptType::Sequence,
						   .draw_inline = &DrawTypedActionInline<ButtonPressScript>,
						   .draw		= &DrawTypedActionDetails<ButtonPressScript>,
					   }
);
PTGN_REGISTER_SCRIPT(
	ButtonHoverStartScript, {
								.label		 = "Hover Start",
								.group		 = "UI/Button",
								.type		 = ScriptType::Sequence,
								.draw_inline = &DrawTypedActionInline<ButtonHoverStartScript>,
								.draw		 = &DrawTypedActionDetails<ButtonHoverStartScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	ButtonHoverContinueScript, {
								   .label		= "Hover Continue",
								   .group		= "UI/Button",
								   .type		= ScriptType::Sequence,
								   .draw_inline = &DrawTypedActionInline<ButtonHoverContinueScript>,
								   .draw = &DrawTypedActionDetails<ButtonHoverContinueScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	ButtonHoverStopScript, {
							   .label		= "Hover Stop",
							   .group		= "UI/Button",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<ButtonHoverStopScript>,
							   .draw		= &DrawTypedActionDetails<ButtonHoverStopScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	ButtonEnableScript, {
							.label		 = "Enable",
							.group		 = "UI/Button",
							.type		 = ScriptType::Sequence,
							.draw_inline = &DrawTypedActionInline<ButtonEnableScript>,
							.draw		 = &DrawTypedActionDetails<ButtonEnableScript>,
						}
);
PTGN_REGISTER_SCRIPT(
	ButtonDisableScript, {
							 .label		  = "Disable",
							 .group		  = "UI/Button",
							 .type		  = ScriptType::Sequence,
							 .draw_inline = &DrawTypedActionInline<ButtonDisableScript>,
							 .draw		  = &DrawTypedActionDetails<ButtonDisableScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	SliderSetValueScript, {
							  .label	   = "Set Value",
							  .group	   = "UI/Slider",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<SliderSetValueScript>,
							  .draw		   = &DrawTypedActionDetails<SliderSetValueScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	SliderIncreaseScript, {
							  .label	   = "Increase",
							  .group	   = "UI/Slider",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<SliderIncreaseScript>,
							  .draw		   = &DrawTypedActionDetails<SliderIncreaseScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	SliderDecreaseScript, {
							  .label	   = "Decrease",
							  .group	   = "UI/Slider",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<SliderDecreaseScript>,
							  .draw		   = &DrawTypedActionDetails<SliderDecreaseScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	ToggleButtonOnScript, {
							  .label	   = "Turn On",
							  .group	   = "UI/Toggle Button",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<ToggleButtonOnScript>,
							  .draw		   = &DrawTypedActionDetails<ToggleButtonOnScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	ToggleButtonOffScript, {
							   .label		= "Turn Off",
							   .group		= "UI/Toggle Button",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<ToggleButtonOffScript>,
							   .draw		= &DrawTypedActionDetails<ToggleButtonOffScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	ToggleButtonToggleScript, {
								  .label	   = "Toggle",
								  .group	   = "UI/Toggle Button",
								  .type		   = ScriptType::Sequence,
								  .draw_inline = &DrawTypedActionInline<ToggleButtonToggleScript>,
								  .draw		   = &DrawTypedActionDetails<ToggleButtonToggleScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	ToggleGroupSelectScript, {
								 .label		  = "Select",
								 .group		  = "UI/Toggle Group",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<ToggleGroupSelectScript>,
								 .draw		  = &DrawTypedActionDetails<ToggleGroupSelectScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	ToggleGroupAllowNoneScript,
	{
		.label		 = "Allow None",
		.group		 = "UI/Toggle Group",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ToggleGroupAllowNoneScript>,
		.draw		 = &DrawTypedActionDetails<ToggleGroupAllowNoneScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ToggleGroupRequireActiveScript,
	{
		.label		 = "Require Active",
		.group		 = "UI/Toggle Group",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ToggleGroupRequireActiveScript>,
		.draw		 = &DrawTypedActionDetails<ToggleGroupRequireActiveScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	DropdownOpenScript, {
							.label		 = "Open",
							.group		 = "UI/Dropdown",
							.type		 = ScriptType::Sequence,
							.draw_inline = &DrawTypedActionInline<DropdownOpenScript>,
							.draw		 = &DrawTypedActionDetails<DropdownOpenScript>,
						}
);
PTGN_REGISTER_SCRIPT(
	DropdownCloseScript, {
							 .label		  = "Close",
							 .group		  = "UI/Dropdown",
							 .type		  = ScriptType::Sequence,
							 .draw_inline = &DrawTypedActionInline<DropdownCloseScript>,
							 .draw		  = &DrawTypedActionDetails<DropdownCloseScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	DropdownToggleScript, {
							  .label	   = "Toggle",
							  .group	   = "UI/Dropdown",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<DropdownToggleScript>,
							  .draw		   = &DrawTypedActionDetails<DropdownToggleScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	DropdownPressItemScript, {
								 .label		  = "Press Item",
								 .group		  = "UI/Dropdown",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<DropdownPressItemScript>,
								 .draw		  = &DrawTypedActionDetails<DropdownPressItemScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	EntityDestroyEntityScript, {
								   .label		= "Destroy Entity",
								   .group		= "Entity/Hierarchy",
								   .description = "Destroy Entity on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw_inline = &DrawTypedActionInline<EntityDestroyEntityScript>,
								   .draw = &DrawTypedActionDetails<EntityDestroyEntityScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	EntitySpawnPrefabScript, {
								 .label		  = "Spawn Prefab",
								 .group		  = "Entity/Hierarchy",
								 .description = "Spawn Prefab on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<EntitySpawnPrefabScript>,
								 .draw		  = &DrawTypedActionDetails<EntitySpawnPrefabScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	EntitySetColorScript, {
							  .label	   = "Set Color",
							  .group	   = "Entity/Appearance",
							  .description = "Set Color on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<EntitySetColorScript>,
							  .draw		   = &DrawTypedActionDetails<EntitySetColorScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	EntitySetDepthScript, {
							  .label	   = "Set Depth",
							  .group	   = "Entity/Appearance",
							  .description = "Set Depth on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<EntitySetDepthScript>,
							  .draw		   = &DrawTypedActionDetails<EntitySetDepthScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	EntitySetPositionScript, {
								 .label		  = "Set Position",
								 .group		  = "Entity/Transform",
								 .description = "Set Position on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<EntitySetPositionScript>,
								 .draw		  = &DrawTypedActionDetails<EntitySetPositionScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	EntityAddPositionScript, {
								 .label		  = "Add Position",
								 .group		  = "Entity/Transform",
								 .description = "Add Position on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<EntityAddPositionScript>,
								 .draw		  = &DrawTypedActionDetails<EntityAddPositionScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	EntitySetRotationScript, {
								 .label		  = "Set Rotation",
								 .group		  = "Entity/Transform",
								 .description = "Set Rotation on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<EntitySetRotationScript>,
								 .draw		  = &DrawTypedActionDetails<EntitySetRotationScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	EntityAddRotationScript, {
								 .label		  = "Add Rotation",
								 .group		  = "Entity/Transform",
								 .description = "Add Rotation on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<EntityAddRotationScript>,
								 .draw		  = &DrawTypedActionDetails<EntityAddRotationScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	EntitySetScaleScript, {
							  .label	   = "Set Scale",
							  .group	   = "Entity/Transform",
							  .description = "Set Scale on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<EntitySetScaleScript>,
							  .draw		   = &DrawTypedActionDetails<EntitySetScaleScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	EntityMultiplyScaleScript, {
								   .label		= "Multiply Scale",
								   .group		= "Entity/Transform",
								   .description = "Multiply Scale on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw_inline = &DrawTypedActionInline<EntityMultiplyScaleScript>,
								   .draw = &DrawTypedActionDetails<EntityMultiplyScaleScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	EntitySetRenderMaskScript, {
								   .label		= "Set Render Mask",
								   .group		= "Entity/Appearance",
								   .description = "Set Render Mask on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw_inline = &DrawTypedActionInline<EntitySetRenderMaskScript>,
								   .draw = &DrawTypedActionDetails<EntitySetRenderMaskScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	EntitySetBlendModeScript, {
								  .label	   = "Set Blend Mode",
								  .group	   = "Entity/Appearance",
								  .description = "Set Blend Mode on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw_inline = &DrawTypedActionInline<EntitySetBlendModeScript>,
								  .draw		   = &DrawTypedActionDetails<EntitySetBlendModeScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	EntitySetOriginScript, {
							   .label		= "Set Origin",
							   .group		= "Entity/Appearance",
							   .description = "Set Origin on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<EntitySetOriginScript>,
							   .draw		= &DrawTypedActionDetails<EntitySetOriginScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	EntitySetParentScript, {
							   .label		= "Set Parent",
							   .group		= "Entity/Hierarchy",
							   .description = "Set Parent on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw		= &DrawTypedActionDetails<EntitySetParentScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	EntityRemoveParentScript, {
								  .label	   = "Remove Parent",
								  .group	   = "Entity/Hierarchy",
								  .description = "Remove Parent on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw		   = &DrawTypedActionDetails<EntityRemoveParentScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	TilemapSetTileScript, {
							  .label	   = "Set Tile",
							  .group	   = "World/Tilemap",
							  .description = "Set Tile on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw		   = &DrawTypedActionDetails<TilemapSetTileScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	TilemapRemoveTileScript, {
								 .label		  = "Remove Tile",
								 .group		  = "World/Tilemap",
								 .description = "Remove Tile on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<TilemapRemoveTileScript>,
								 .draw		  = &DrawTypedActionDetails<TilemapRemoveTileScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	ShapeSetColorScript, {
							 .label		  = "Set Color",
							 .group		  = "Shapes/General",
							 .description = "Set Color on the selected target.",
							 .type		  = ScriptType::Sequence,
							 .draw_inline = &DrawTypedActionInline<ShapeSetColorScript>,
							 .draw		  = &DrawTypedActionDetails<ShapeSetColorScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	ShapeSetFillStyleScript, {
								 .label		  = "Set Fill Style",
								 .group		  = "Shapes/General",
								 .description = "Set Fill Style on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw		  = &DrawTypedActionDetails<ShapeSetFillStyleScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	ShapeSetStrokeWidthScript, {
								   .label		= "Set Stroke Width",
								   .group		= "Shapes/General",
								   .description = "Set Stroke Width on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw_inline = &DrawTypedActionInline<ShapeSetStrokeWidthScript>,
								   .draw = &DrawTypedActionDetails<ShapeSetStrokeWidthScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	ShapeSetSizeScript, {
							.label		 = "Set Size",
							.group		 = "Shapes/General",
							.description = "Set Size on the selected target.",
							.type		 = ScriptType::Sequence,
							.draw_inline = &DrawTypedActionInline<ShapeSetSizeScript>,
							.draw		 = &DrawTypedActionDetails<ShapeSetSizeScript>,
						}
);
PTGN_REGISTER_SCRIPT(
	ShapeSetRadiusScript, {
							  .label	   = "Set Radius",
							  .group	   = "Shapes/General",
							  .description = "Set Radius on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<ShapeSetRadiusScript>,
							  .draw		   = &DrawTypedActionDetails<ShapeSetRadiusScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	ShapeSetGeometryScript, {
								.label		 = "Set Geometry",
								.group		 = "Shapes/General",
								.description = "Set Geometry on the selected target.",
								.type		 = ScriptType::Sequence,
								.draw		 = &DrawTypedActionDetails<ShapeSetGeometryScript>,
							}
);

bool DrawRectBoundsDetails(ScriptEditorContext&, ShapeRectSetBoundsOrSizeScript& script) {
	bool changed{ false };
	auto row = [&](std::string_view label, auto draw) {
		changed |= inspector::DrawPropertyRow(label, [&] {
			ImGui::SetNextItemWidth(-FLT_MIN);
			return draw();
		});
	};
	row("Min", [&] { return ImGui::DragFloat2("##Min", &script.vector.x, 0.25f); });
	row("Max", [&] { return ImGui::DragFloat2("##Max", &script.vector2.x, 0.25f); });
	V2_float size{ script.vector2 - script.vector };
	bool size_changed{ false };
	row("Size", [&] {
		size_changed = ImGui::DragFloat2("##Size", &size.x, 0.25f, 0.0f, 0.0f);
		return size_changed;
	});
	if (size_changed) {
		size.x		   = std::max(0.0f, size.x);
		size.y		   = std::max(0.0f, size.y);
		script.vector2 = script.vector + size;
	} else {
		script.vector2.x = std::max(script.vector.x, script.vector2.x);
		script.vector2.y = std::max(script.vector.y, script.vector2.y);
	}
	return changed;
}

PTGN_REGISTER_SCRIPT(
	ShapeRectSetBoundsOrSizeScript,
	{
		.label		 = "Rect Set Bounds or Size",
		.group		 = "Shapes/Rect",
		.description = "Rect Set Bounds or Size on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawRectBoundsDetails,
	}
);
PTGN_REGISTER_SCRIPT(
	ShapeRectSetCenterScript, {
								  .label	   = "Rect Set Center",
								  .group	   = "Shapes/Rect",
								  .description = "Rect Set Center on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw_inline = &DrawTypedActionInline<ShapeRectSetCenterScript>,
								  .draw		   = &DrawTypedActionDetails<ShapeRectSetCenterScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	ShapeCircleSetDiameterScript,
	{
		.label		 = "Circle Set Diameter",
		.group		 = "Shapes/Circle",
		.description = "Circle Set Diameter on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ShapeCircleSetDiameterScript>,
		.draw		 = &DrawTypedActionDetails<ShapeCircleSetDiameterScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ShapeRoundedRectSetCornerRadiusScript,
	{
		.label		 = "Rounded Rect Set Corner Radius",
		.group		 = "Shapes/Rect",
		.description = "Rounded Rect Set Corner Radius on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ShapeRoundedRectSetCornerRadiusScript>,
		.draw		 = &DrawTypedActionDetails<ShapeRoundedRectSetCornerRadiusScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ShapePolygonSetVerticesScript,
	{
		.label		 = "Polygon Set Vertices",
		.group		 = "Shapes/Polygon",
		.description = "Polygon Set Vertices on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<ShapePolygonSetVerticesScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ShapePolygonSetVertexScript, {
									 .label		  = "Polygon Set Vertex",
									 .group		  = "Shapes/Polygon",
									 .description = "Polygon Set Vertex on the selected target.",
									 .type		  = ScriptType::Sequence,
									 .draw = &DrawTypedActionDetails<ShapePolygonSetVertexScript>,
								 }
);
PTGN_REGISTER_SCRIPT(
	ShapeEllipseSetRadiiScript,
	{
		.label		 = "Ellipse Set Radii",
		.group		 = "Shapes/Ellipse",
		.description = "Ellipse Set Radii on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ShapeEllipseSetRadiiScript>,
		.draw		 = &DrawTypedActionDetails<ShapeEllipseSetRadiiScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ShapeEllipseSetWidthScript,
	{
		.label		 = "Ellipse Set Width",
		.group		 = "Shapes/Ellipse",
		.description = "Ellipse Set Width on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ShapeEllipseSetWidthScript>,
		.draw		 = &DrawTypedActionDetails<ShapeEllipseSetWidthScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ShapeEllipseSetHeightScript,
	{
		.label		 = "Ellipse Set Height",
		.group		 = "Shapes/Ellipse",
		.description = "Ellipse Set Height on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ShapeEllipseSetHeightScript>,
		.draw		 = &DrawTypedActionDetails<ShapeEllipseSetHeightScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ShapeTriangleSetVerticesScript,
	{
		.label		 = "Triangle Set Vertices",
		.group		 = "Shapes/Triangle",
		.description = "Triangle Set Vertices on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<ShapeTriangleSetVerticesScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ShapeTriangleSetVertexScript, {
									  .label	   = "Triangle Set Vertex",
									  .group	   = "Shapes/Triangle",
									  .description = "Triangle Set Vertex on the selected target.",
									  .type		   = ScriptType::Sequence,
									  .draw = &DrawTypedActionDetails<ShapeTriangleSetVertexScript>,
								  }
);
PTGN_REGISTER_SCRIPT(
	ShapeLineSetStartScript, {
								 .label		  = "Line Set Start",
								 .group		  = "Shapes/Line",
								 .description = "Line Set Start on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<ShapeLineSetStartScript>,
								 .draw		  = &DrawTypedActionDetails<ShapeLineSetStartScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	ShapeLineSetEndScript, {
							   .label		= "Line Set End",
							   .group		= "Shapes/Line",
							   .description = "Line Set End on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<ShapeLineSetEndScript>,
							   .draw		= &DrawTypedActionDetails<ShapeLineSetEndScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	ShapeLineSetEndpointsScript, {
									 .label		  = "Line Set Endpoints",
									 .group		  = "Shapes/Line",
									 .description = "Line Set Endpoints on the selected target.",
									 .type		  = ScriptType::Sequence,
									 .draw = &DrawTypedActionDetails<ShapeLineSetEndpointsScript>,
								 }
);
PTGN_REGISTER_SCRIPT(
	ShapeLineSetWidthScript, {
								 .label		  = "Line Set Width",
								 .group		  = "Shapes/Line",
								 .description = "Line Set Width on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<ShapeLineSetWidthScript>,
								 .draw		  = &DrawTypedActionDetails<ShapeLineSetWidthScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	ShapeCapsuleSetStartScript,
	{
		.label		 = "Capsule Set Start",
		.group		 = "Shapes/Capsule",
		.description = "Capsule Set Start on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ShapeCapsuleSetStartScript>,
		.draw		 = &DrawTypedActionDetails<ShapeCapsuleSetStartScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ShapeCapsuleSetEndScript, {
								  .label	   = "Capsule Set End",
								  .group	   = "Shapes/Capsule",
								  .description = "Capsule Set End on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw_inline = &DrawTypedActionInline<ShapeCapsuleSetEndScript>,
								  .draw		   = &DrawTypedActionDetails<ShapeCapsuleSetEndScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	ShapeCapsuleSetRadiusScript,
	{
		.label		 = "Capsule Set Radius",
		.group		 = "Shapes/Capsule",
		.description = "Capsule Set Radius on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ShapeCapsuleSetRadiusScript>,
		.draw		 = &DrawTypedActionDetails<ShapeCapsuleSetRadiusScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ShapeCapsuleSetLengthScript,
	{
		.label		 = "Capsule Set Length",
		.group		 = "Shapes/Capsule",
		.description = "Capsule Set Length on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ShapeCapsuleSetLengthScript>,
		.draw		 = &DrawTypedActionDetails<ShapeCapsuleSetLengthScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ShapeArcSetRadiusScript, {
								 .label		  = "Arc Set Radius",
								 .group		  = "Shapes/Arc",
								 .description = "Arc Set Radius on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<ShapeArcSetRadiusScript>,
								 .draw		  = &DrawTypedActionDetails<ShapeArcSetRadiusScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	ShapeArcSetStartAngleScript,
	{
		.label		 = "Arc Set Start Angle",
		.group		 = "Shapes/Arc",
		.description = "Arc Set Start Angle on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ShapeArcSetStartAngleScript>,
		.draw		 = &DrawTypedActionDetails<ShapeArcSetStartAngleScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ShapeArcSetEndAngleScript, {
								   .label		= "Arc Set End Angle",
								   .group		= "Shapes/Arc",
								   .description = "Arc Set End Angle on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw_inline = &DrawTypedActionInline<ShapeArcSetEndAngleScript>,
								   .draw = &DrawTypedActionDetails<ShapeArcSetEndAngleScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	ShapeArcSetDirectionScript,
	{
		.label		 = "Arc Set Direction",
		.group		 = "Shapes/Arc",
		.description = "Arc Set Direction on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ShapeArcSetDirectionScript>,
		.draw		 = &DrawTypedActionDetails<ShapeArcSetDirectionScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	SpriteSetDisplaySizeScript, {
									.label		 = "Set Display Size",
									.group		 = "Rendering/Sprite",
									.description = "Set Display Size on the selected target.",
									.type		 = ScriptType::Sequence,
									.draw = &DrawTypedActionDetails<SpriteSetDisplaySizeScript>,
								}
);
PTGN_REGISTER_SCRIPT(
	SpriteSetCropScript, {
							 .label		  = "Set Crop",
							 .group		  = "Rendering/Sprite",
							 .description = "Set Crop on the selected target.",
							 .type		  = ScriptType::Sequence,
							 .draw		  = &DrawTypedActionDetails<SpriteSetCropScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	SpriteClearCropScript, {
							   .label		= "Clear Crop",
							   .group		= "Rendering/Sprite",
							   .description = "Clear Crop on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw		= &DrawTypedActionDetails<SpriteClearCropScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	SpriteSetTextureCoordinatesScript,
	{
		.label		 = "Set Texture Coordinates",
		.group		 = "Rendering/Sprite",
		.description = "Set Texture Coordinates on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<SpriteSetTextureCoordinatesScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	SpriteSetOriginScript, {
							   .label		= "Set Origin",
							   .group		= "Rendering/Sprite",
							   .description = "Set Origin on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<SpriteSetOriginScript>,
							   .draw		= &DrawTypedActionDetails<SpriteSetOriginScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	SpriteFlipXScript, {
						   .label		= "Flip X",
						   .group		= "Rendering/Sprite",
						   .description = "Flip X on the selected target.",
						   .type		= ScriptType::Sequence,
						   .draw		= &DrawTypedActionDetails<SpriteFlipXScript>,
					   }
);
PTGN_REGISTER_SCRIPT(
	SpriteFlipYScript, {
						   .label		= "Flip Y",
						   .group		= "Rendering/Sprite",
						   .description = "Flip Y on the selected target.",
						   .type		= ScriptType::Sequence,
						   .draw		= &DrawTypedActionDetails<SpriteFlipYScript>,
					   }
);
PTGN_REGISTER_SCRIPT(
	SpriteStackSetTextureScript,
	{
		.label		 = "Set Texture",
		.group		 = "Rendering/Sprite Stack",
		.description = "Set Texture on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<SpriteStackSetTextureScript>,
		.draw		 = &DrawTypedActionDetails<SpriteStackSetTextureScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	SpriteStackSetSliceCountScript,
	{
		.label		 = "Set Slice Count",
		.group		 = "Rendering/Sprite Stack",
		.description = "Set Slice Count on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<SpriteStackSetSliceCountScript>,
		.draw		 = &DrawTypedActionDetails<SpriteStackSetSliceCountScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	SpriteStackSetSliceOrderScript,
	{
		.label		 = "Set Slice Order",
		.group		 = "Rendering/Sprite Stack",
		.description = "Set Slice Order on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<SpriteStackSetSliceOrderScript>,
		.draw		 = &DrawTypedActionDetails<SpriteStackSetSliceOrderScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	SpriteStackSetLayerOffsetScript,
	{
		.label		 = "Set Layer Offset",
		.group		 = "Rendering/Sprite Stack",
		.description = "Set Layer Offset on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<SpriteStackSetLayerOffsetScript>,
		.draw		 = &DrawTypedActionDetails<SpriteStackSetLayerOffsetScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	SpriteStackSetPixelSnappingScript,
	{
		.label		 = "Set Pixel Snapping",
		.group		 = "Rendering/Sprite Stack",
		.description = "Set Pixel Snapping on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<SpriteStackSetPixelSnappingScript>,
		.draw		 = &DrawTypedActionDetails<SpriteStackSetPixelSnappingScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	TextSetTextScript, {
						   .label		= "Set Text",
						   .group		= "Rendering/Text/Content",
						   .description = "Set Text on the selected target.",
						   .type		= ScriptType::Sequence,
						   .draw_inline = &DrawTypedActionInline<TextSetTextScript>,
						   .draw		= &DrawTypedActionDetails<TextSetTextScript>,
					   }
);
PTGN_REGISTER_SCRIPT(
	TextAppendTextScript, {
							  .label	   = "Append Text",
							  .group	   = "Rendering/Text/Content",
							  .description = "Append Text on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<TextAppendTextScript>,
							  .draw		   = &DrawTypedActionDetails<TextAppendTextScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	TextClearTextScript, {
							 .label		  = "Clear Text",
							 .group		  = "Rendering/Text/Content",
							 .description = "Clear Text on the selected target.",
							 .type		  = ScriptType::Sequence,
							 .draw		  = &DrawTypedActionDetails<TextClearTextScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	TextSetRichTextScript, {
							   .label		= "Set Rich Text",
							   .group		= "Rendering/Text/Content",
							   .description = "Set Rich Text on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<TextSetRichTextScript>,
							   .draw		= &DrawTypedActionDetails<TextSetRichTextScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	TextSetTextColorScript, {
								.label		 = "Set Text Color",
								.group		 = "Rendering/Text/Content",
								.description = "Set Text Color on the selected target.",
								.type		 = ScriptType::Sequence,
								.draw_inline = &DrawTypedActionInline<TextSetTextColorScript>,
								.draw		 = &DrawTypedActionDetails<TextSetTextColorScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	TextSetFontScript, {
						   .label		= "Set Font",
						   .group		= "Rendering/Text/Style",
						   .description = "Set Font on the selected target.",
						   .type		= ScriptType::Sequence,
						   .draw_inline = &DrawTypedActionInline<TextSetFontScript>,
						   .draw		= &DrawTypedActionDetails<TextSetFontScript>,
					   }
);
PTGN_REGISTER_SCRIPT(
	TextSetFontSizeScript, {
							   .label		= "Set Font Size",
							   .group		= "Rendering/Text/Style",
							   .description = "Set Font Size on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<TextSetFontSizeScript>,
							   .draw		= &DrawTypedActionDetails<TextSetFontSizeScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	TextSetRevealCountScript, {
								  .label	   = "Set Reveal Count",
								  .group	   = "Rendering/Text/Reveal",
								  .description = "Set Reveal Count on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw_inline = &DrawTypedActionInline<TextSetRevealCountScript>,
								  .draw		   = &DrawTypedActionDetails<TextSetRevealCountScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	TextSetRevealFractionScript,
	{
		.label		 = "Set Reveal Fraction",
		.group		 = "Rendering/Text/Reveal",
		.description = "Set Reveal Fraction on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<TextSetRevealFractionScript>,
		.draw		 = &DrawTypedActionDetails<TextSetRevealFractionScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	TextRevealAllScript, {
							 .label		  = "Reveal All",
							 .group		  = "Rendering/Text/Reveal",
							 .description = "Reveal All on the selected target.",
							 .type		  = ScriptType::Sequence,
							 .draw		  = &DrawTypedActionDetails<TextRevealAllScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	TextTypewriterRevealScript,
	{
		.label		 = "Typewriter Reveal",
		.group		 = "Rendering/Text/Reveal",
		.description = "Typewriter Reveal on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<TextTypewriterRevealScript>,
		.draw		 = &DrawTypedActionDetails<TextTypewriterRevealScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	TextSetAlignmentScript, {
								.label		 = "Set Alignment",
								.group		 = "Rendering/Text/Layout",
								.description = "Set Alignment on the selected target.",
								.type		 = ScriptType::Sequence,
								.draw_inline = &DrawTypedActionInline<TextSetAlignmentScript>,
								.draw		 = &DrawTypedActionDetails<TextSetAlignmentScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	TextSetWrapModeScript, {
							   .label		= "Set Wrap Mode",
							   .group		= "Rendering/Text/Layout",
							   .description = "Set Wrap Mode on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<TextSetWrapModeScript>,
							   .draw		= &DrawTypedActionDetails<TextSetWrapModeScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	TextSetTextBoxScript, {
							  .label	   = "Set Text Box",
							  .group	   = "Rendering/Text/Content",
							  .description = "Set Text Box on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw		   = &DrawTypedActionDetails<TextSetTextBoxScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	TextSetClipScript, {
						   .label		= "Set Clip",
						   .group		= "Rendering/Text/Layout",
						   .description = "Set Clip on the selected target.",
						   .type		= ScriptType::Sequence,
						   .draw		= &DrawTypedActionDetails<TextSetClipScript>,
					   }
);
PTGN_REGISTER_SCRIPT(
	TextSetOutlineScript, {
							  .label	   = "Set Outline",
							  .group	   = "Rendering/Text/Style",
							  .description = "Set Outline on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw		   = &DrawTypedActionDetails<TextSetOutlineScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	TextSetShadowScript, {
							 .label		  = "Set Shadow",
							 .group		  = "Rendering/Text/Style",
							 .description = "Set Shadow on the selected target.",
							 .type		  = ScriptType::Sequence,
							 .draw		  = &DrawTypedActionDetails<TextSetShadowScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	TextSetGlowScript, {
						   .label		= "Set Glow",
						   .group		= "Rendering/Text/Style",
						   .description = "Set Glow on the selected target.",
						   .type		= ScriptType::Sequence,
						   .draw		= &DrawTypedActionDetails<TextSetGlowScript>,
					   }
);
PTGN_REGISTER_SCRIPT(
	TextSetTextEffectScript, {
								 .label		  = "Set Text Effect",
								 .group		  = "Rendering/Text/Content",
								 .description = "Set Text Effect on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw		  = &DrawTypedActionDetails<TextSetTextEffectScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	ParticlesStartScript, {
							  .label	   = "Start",
							  .group	   = "Rendering/Particles/Playback",
							  .description = "Start on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw		   = &DrawTypedActionDetails<ParticlesStartScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	ParticlesStopScript, {
							 .label		  = "Stop",
							 .group		  = "Rendering/Particles/Playback",
							 .description = "Stop on the selected target.",
							 .type		  = ScriptType::Sequence,
							 .draw		  = &DrawTypedActionDetails<ParticlesStopScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	ParticlesPauseScript, {
							  .label	   = "Pause",
							  .group	   = "Rendering/Particles/Playback",
							  .description = "Pause on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw		   = &DrawTypedActionDetails<ParticlesPauseScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	ParticlesResumeScript, {
							   .label		= "Resume",
							   .group		= "Rendering/Particles/Playback",
							   .description = "Resume on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw		= &DrawTypedActionDetails<ParticlesResumeScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	ParticlesToggleScript, {
							   .label		= "Toggle",
							   .group		= "Rendering/Particles/Playback",
							   .description = "Toggle on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw		= &DrawTypedActionDetails<ParticlesToggleScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	ParticlesResetScript, {
							  .label	   = "Reset",
							  .group	   = "Rendering/Particles/Playback",
							  .description = "Reset on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw		   = &DrawTypedActionDetails<ParticlesResetScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	ParticlesEmitBurstScript, {
								  .label	   = "Emit Burst",
								  .group	   = "Rendering/Particles/Emission",
								  .description = "Emit Burst on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw_inline = &DrawTypedActionInline<ParticlesEmitBurstScript>,
								  .draw		   = &DrawTypedActionDetails<ParticlesEmitBurstScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetEmissionRateScript,
	{
		.label		 = "Set Emission Rate",
		.group		 = "Rendering/Particles/Emission",
		.description = "Set Emission Rate on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ParticlesSetEmissionRateScript>,
		.draw		 = &DrawTypedActionDetails<ParticlesSetEmissionRateScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetStartColorScript, {
									  .label	   = "Set Start Color",
									  .group	   = "Rendering/Particles/Playback",
									  .description = "Set Start Color on the selected target.",
									  .type		   = ScriptType::Sequence,
									  .draw = &DrawTypedActionDetails<ParticlesSetStartColorScript>,
								  }
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetStartSpeedScript, {
									  .label	   = "Set Start Speed",
									  .group	   = "Rendering/Particles/Playback",
									  .description = "Set Start Speed on the selected target.",
									  .type		   = ScriptType::Sequence,
									  .draw = &DrawTypedActionDetails<ParticlesSetStartSpeedScript>,
								  }
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetStartSizeScript, {
									 .label		  = "Set Start Size",
									 .group		  = "Rendering/Particles/Playback",
									 .description = "Set Start Size on the selected target.",
									 .type		  = ScriptType::Sequence,
									 .draw = &DrawTypedActionDetails<ParticlesSetStartSizeScript>,
								 }
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetLifetimeScript, {
									.label		 = "Set Lifetime",
									.group		 = "Rendering/Particles/Lifetime",
									.description = "Set Lifetime on the selected target.",
									.type		 = ScriptType::Sequence,
									.draw = &DrawTypedActionDetails<ParticlesSetLifetimeScript>,
								}
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetEmissionShapeScript,
	{
		.label		 = "Set Emission Shape",
		.group		 = "Rendering/Particles/Emission",
		.description = "Set Emission Shape on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<ParticlesSetEmissionShapeScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetGravityScript, {
								   .label		= "Set Gravity",
								   .group		= "Rendering/Particles/Motion",
								   .description = "Set Gravity on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw = &DrawTypedActionDetails<ParticlesSetGravityScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetSimulationSpeedScript,
	{
		.label		 = "Set Simulation Speed",
		.group		 = "Rendering/Particles/Motion",
		.description = "Set Simulation Speed on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ParticlesSetSimulationSpeedScript>,
		.draw		 = &DrawTypedActionDetails<ParticlesSetSimulationSpeedScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetMaximumParticlesScript,
	{
		.label		 = "Set Maximum Particles",
		.group		 = "Rendering/Particles/Lifetime",
		.description = "Set Maximum Particles on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ParticlesSetMaximumParticlesScript>,
		.draw		 = &DrawTypedActionDetails<ParticlesSetMaximumParticlesScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetEndColorScript, {
									.label		 = "Set End Color",
									.group		 = "Rendering/Particles/Appearance",
									.description = "Set End Color on the selected target.",
									.type		 = ScriptType::Sequence,
									.draw = &DrawTypedActionDetails<ParticlesSetEndColorScript>,
								}
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetEndSizeScript, {
								   .label		= "Set End Size",
								   .group		= "Rendering/Particles/Appearance",
								   .description = "Set End Size on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw = &DrawTypedActionDetails<ParticlesSetEndSizeScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetEndVelocityScript,
	{
		.label		 = "Set End Velocity",
		.group		 = "Rendering/Particles/Motion",
		.description = "Set End Velocity on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<ParticlesSetEndVelocityScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ParticlesSetParticleTypeScript,
	{
		.label		 = "Set Particle Type",
		.group		 = "Rendering/Particles/Appearance",
		.description = "Set Particle Type on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<ParticlesSetParticleTypeScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	LightSetColorScript, {
							 .label		  = "Set Color",
							 .group		  = "Rendering/Light",
							 .description = "Set Color on the selected target.",
							 .type		  = ScriptType::Sequence,
							 .draw_inline = &DrawTypedActionInline<LightSetColorScript>,
							 .draw		  = &DrawTypedActionDetails<LightSetColorScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	LightSetIntensityScript, {
								 .label		  = "Set Intensity",
								 .group		  = "Rendering/Light",
								 .description = "Set Intensity on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<LightSetIntensityScript>,
								 .draw		  = &DrawTypedActionDetails<LightSetIntensityScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	LightSetRadiusScript, {
							  .label	   = "Set Radius",
							  .group	   = "Rendering/Light",
							  .description = "Set Radius on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<LightSetRadiusScript>,
							  .draw		   = &DrawTypedActionDetails<LightSetRadiusScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	LightSetFalloffScript, {
							   .label		= "Set Falloff",
							   .group		= "Rendering/Light",
							   .description = "Set Falloff on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<LightSetFalloffScript>,
							   .draw		= &DrawTypedActionDetails<LightSetFalloffScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	LightSetConeAngleScript, {
								 .label		  = "Set Cone Angle",
								 .group		  = "Rendering/Light",
								 .description = "Set Cone Angle on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw		  = &DrawTypedActionDetails<LightSetConeAngleScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	LightSetDirectionAngleScript,
	{
		.label		 = "Set Direction Angle",
		.group		 = "Rendering/Light",
		.description = "Set Direction Angle on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<LightSetDirectionAngleScript>,
		.draw		 = &DrawTypedActionDetails<LightSetDirectionAngleScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	LightSetAmbientColorScript,
	{
		.label		 = "Set Ambient Color",
		.group		 = "Rendering/Light",
		.description = "Set Ambient Color on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<LightSetAmbientColorScript>,
		.draw		 = &DrawTypedActionDetails<LightSetAmbientColorScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	LightSetAmbientIntensityScript,
	{
		.label		 = "Set Ambient Intensity",
		.group		 = "Rendering/Light",
		.description = "Set Ambient Intensity on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<LightSetAmbientIntensityScript>,
		.draw		 = &DrawTypedActionDetails<LightSetAmbientIntensityScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	LightSetLightConfigScript, {
								   .label		= "Set Light Config",
								   .group		= "Rendering/Light",
								   .description = "Set Light Config on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw = &DrawTypedActionDetails<LightSetLightConfigScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	ShaderSetUniformScript, {
								.label		 = "Set Uniform",
								.group		 = "Rendering/Shader",
								.description = "Set Uniform on the selected target.",
								.type		 = ScriptType::Sequence,
								.draw		 = &DrawShaderSetUniformScriptDetails,
							}
);
PTGN_REGISTER_SCRIPT(
	ShaderSetShaderScript, {
							   .label		= "Set Shader",
							   .group		= "Rendering/Shader",
							   .description = "Set Shader on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<ShaderSetShaderScript>,
							   .draw		= &DrawTypedActionDetails<ShaderSetShaderScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	ShaderSetMaterialScript, {
								 .label		  = "Set Material",
								 .group		  = "Rendering/Shader",
								 .description = "Set Material on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw		  = &DrawTypedActionDetails<ShaderSetMaterialScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	ShaderSetMultipleUniformsScript,
	{
		.label		 = "Set Multiple Uniforms",
		.group		 = "Rendering/Shader",
		.description = "Set Multiple Uniforms on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<ShaderSetMultipleUniformsScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RenderTargetSetClearColorScript,
	{
		.label		 = "Set Clear Color",
		.group		 = "Rendering/Render Target",
		.description = "Set Clear Color on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<RenderTargetSetClearColorScript>,
		.draw		 = &DrawTypedActionDetails<RenderTargetSetClearColorScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RenderTargetClearColorScript, {
									  .label	   = "Clear Color",
									  .group	   = "Rendering/Render Target",
									  .description = "Clear Color on the selected target.",
									  .type		   = ScriptType::Sequence,
									  .draw = &DrawTypedActionDetails<RenderTargetClearColorScript>,
								  }
);
PTGN_REGISTER_SCRIPT(
	RenderTargetClearDepthScript,
	{
		.label		 = "Clear Depth",
		.group		 = "Rendering/Render Target",
		.description = "Clear Depth on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<RenderTargetClearDepthScript>,
		.draw		 = &DrawTypedActionDetails<RenderTargetClearDepthScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RenderTargetClearStencilScript,
	{
		.label		 = "Clear Stencil",
		.group		 = "Rendering/Render Target",
		.description = "Clear Stencil on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<RenderTargetClearStencilScript>,
		.draw		 = &DrawTypedActionDetails<RenderTargetClearStencilScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RenderTargetSetSizeScript, {
								   .label		= "Set Size",
								   .group		= "Rendering/Render Target",
								   .description = "Set Size on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw_inline = &DrawTypedActionInline<RenderTargetSetSizeScript>,
								   .draw = &DrawTypedActionDetails<RenderTargetSetSizeScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	RenderTargetFollowDisplaySizeScript,
	{
		.label		 = "Follow Display Size",
		.group		 = "Rendering/Render Target",
		.description = "Follow Display Size on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<RenderTargetFollowDisplaySizeScript>,
		.draw		 = &DrawTypedActionDetails<RenderTargetFollowDisplaySizeScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RenderTargetSetClearDepthValueScript,
	{
		.label		 = "Set Clear Depth Value",
		.group		 = "Rendering/Render Target",
		.description = "Set Clear Depth Value on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<RenderTargetSetClearDepthValueScript>,
		.draw		 = &DrawTypedActionDetails<RenderTargetSetClearDepthValueScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RenderTargetSetClearStencilValueScript,
	{
		.label		 = "Set Clear Stencil Value",
		.group		 = "Rendering/Render Target",
		.description = "Set Clear Stencil Value on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<RenderTargetSetClearStencilValueScript>,
		.draw		 = &DrawTypedActionDetails<RenderTargetSetClearStencilValueScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	CameraSetZoomScript, {
							 .label		  = "Set Zoom",
							 .group		  = "Rendering/Camera/Zoom",
							 .description = "Set Zoom on the selected target.",
							 .type		  = ScriptType::Sequence,
							 .draw_inline = &DrawTypedActionInline<CameraSetZoomScript>,
							 .draw		  = &DrawTypedActionDetails<CameraSetZoomScript>,
						 }
);
PTGN_REGISTER_SCRIPT(
	CameraZoomByScript, {
							.label		 = "Zoom By",
							.group		 = "Rendering/Camera/Zoom",
							.description = "Zoom By on the selected target.",
							.type		 = ScriptType::Sequence,
							.draw_inline = &DrawTypedActionInline<CameraZoomByScript>,
							.draw		 = &DrawTypedActionDetails<CameraZoomByScript>,
						}
);
PTGN_REGISTER_SCRIPT(
	CameraZoomToScript, {
							.label		 = "Zoom To",
							.group		 = "Rendering/Camera/Zoom",
							.description = "Zoom To on the selected target.",
							.type		 = ScriptType::Sequence,
							.draw		 = &DrawTypedActionDetails<CameraZoomToScript>,
						}
);
PTGN_REGISTER_SCRIPT(
	CameraResetCameraScript, {
								 .label		  = "Reset Camera",
								 .group		  = "Rendering/Camera/View",
								 .description = "Reset Camera on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw		  = &DrawTypedActionDetails<CameraResetCameraScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	CameraSetCameraBoundsScript, {
									 .label		  = "Set Camera Bounds",
									 .group		  = "Rendering/Camera/View",
									 .description = "Set Camera Bounds on the selected target.",
									 .type		  = ScriptType::Sequence,
									 .draw = &DrawTypedActionDetails<CameraSetCameraBoundsScript>,
								 }
);
PTGN_REGISTER_SCRIPT(
	CameraSetPixelRoundingScript,
	{
		.label		 = "Set Pixel Rounding",
		.group		 = "Rendering/Camera/View",
		.description = "Set Pixel Rounding on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<CameraSetPixelRoundingScript>,
		.draw		 = &DrawTypedActionDetails<CameraSetPixelRoundingScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	CameraSetRenderMaskScript, {
								   .label		= "Set Render Mask",
								   .group		= "Rendering/Camera/Render Masks",
								   .description = "Set Render Mask on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw = &DrawTypedActionDetails<CameraSetRenderMaskScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	CameraAddRenderMaskBitsScript,
	{
		.label		 = "Add Render Mask Bits",
		.group		 = "Rendering/Camera/Render Masks",
		.description = "Add Render Mask Bits on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<CameraAddRenderMaskBitsScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	CameraRemoveRenderMaskBitsScript,
	{
		.label		 = "Remove Render Mask Bits",
		.group		 = "Rendering/Camera/Render Masks",
		.description = "Remove Render Mask Bits on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<CameraRemoveRenderMaskBitsScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	CameraSetClearColorScript, {
								   .label		= "Set Clear Color",
								   .group		= "Rendering/Camera/View",
								   .description = "Set Clear Color on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw = &DrawTypedActionDetails<CameraSetClearColorScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	CameraSetRenderTargetScript,
	{
		.label		 = "Set Render Target",
		.group		 = "Rendering/Camera/View",
		.description = "Set Render Target on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<CameraSetRenderTargetScript>,
		.draw		 = &DrawTypedActionDetails<CameraSetRenderTargetScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	CameraSetViewportScript, {
								 .label		  = "Set Viewport",
								 .group		  = "Rendering/Camera/View",
								 .description = "Set Viewport on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw		  = &DrawTypedActionDetails<CameraSetViewportScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	EffectEnableEffectScript, {
								  .label	   = "Enable Effect",
								  .group	   = "Rendering/Effects/Playback",
								  .description = "Enable Effect on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw		   = &DrawTypedActionDetails<EffectEnableEffectScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	EffectDisableEffectScript, {
								   .label		= "Disable Effect",
								   .group		= "Rendering/Effects/Playback",
								   .description = "Disable Effect on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw = &DrawTypedActionDetails<EffectDisableEffectScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	EffectToggleEffectScript, {
								  .label	   = "Toggle Effect",
								  .group	   = "Rendering/Effects/Playback",
								  .description = "Toggle Effect on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw		   = &DrawTypedActionDetails<EffectToggleEffectScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	EffectSetEffectPropertyScript, {
									   .label		= "Set Effect Property",
									   .group		= "Rendering/Effects/Properties",
									   .description = "Set Effect Property on the selected target.",
									   .type		= ScriptType::Sequence,
									   .draw		= &DrawEffectSetEffectPropertyScriptDetails,
								   }
);
PTGN_REGISTER_SCRIPT(
	EffectAddEffectScript, {
							   .label		= "Add Effect",
							   .group		= "Rendering/Effects/Entity Effects",
							   .description = "Add Effect on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<EffectAddEffectScript>,
							   .draw		= &DrawTypedActionDetails<EffectAddEffectScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	EffectRemoveEffectScript, {
								  .label	   = "Remove Effect",
								  .group	   = "Rendering/Effects/Entity Effects",
								  .description = "Remove Effect on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw_inline = &DrawTypedActionInline<EffectRemoveEffectScript>,
								  .draw		   = &DrawTypedActionDetails<EffectRemoveEffectScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	EffectAddScreenEffectScript,
	{
		.label		 = "Add Screen Effect",
		.group		 = "Rendering/Effects/Screen Effects",
		.description = "Add Screen Effect on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<EffectAddScreenEffectScript>,
		.draw		 = &DrawTypedActionDetails<EffectAddScreenEffectScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	EffectRemoveScreenEffectScript,
	{
		.label		 = "Remove Screen Effect",
		.group		 = "Rendering/Effects/Screen Effects",
		.description = "Remove Screen Effect on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<EffectRemoveScreenEffectScript>,
		.draw		 = &DrawTypedActionDetails<EffectRemoveScreenEffectScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	EffectClearEffectsScript, {
								  .label	   = "Clear Effects",
								  .group	   = "Rendering/Effects/Entity Effects",
								  .description = "Clear Effects on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw		   = &DrawTypedActionDetails<EffectClearEffectsScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	EffectClearScreenEffectsScript,
	{
		.label		 = "Clear Screen Effects",
		.group		 = "Rendering/Effects/Screen Effects",
		.description = "Clear Screen Effects on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<EffectClearScreenEffectsScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	EffectSetEffectMarginScript,
	{
		.label		 = "Set Effect Margin",
		.group		 = "Rendering/Effects/Properties",
		.description = "Set Effect Margin on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<EffectSetEffectMarginScript>,
		.draw		 = &DrawTypedActionDetails<EffectSetEffectMarginScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RigidBodySetVelocityScript,
	{
		.label		 = "Set Velocity",
		.group		 = "Physics/Rigid Body/Motion",
		.description = "Set Velocity on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<RigidBodySetVelocityScript>,
		.draw		 = &DrawTypedActionDetails<RigidBodySetVelocityScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RigidBodyAddVelocityScript,
	{
		.label		 = "Add Velocity",
		.group		 = "Physics/Rigid Body/Motion",
		.description = "Add Velocity on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<RigidBodyAddVelocityScript>,
		.draw		 = &DrawTypedActionDetails<RigidBodyAddVelocityScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RigidBodyAddImpulseScript, {
								   .label		= "Add Impulse",
								   .group		= "Physics/Rigid Body/Forces",
								   .description = "Add Impulse on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw_inline = &DrawTypedActionInline<RigidBodyAddImpulseScript>,
								   .draw = &DrawTypedActionDetails<RigidBodyAddImpulseScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	RigidBodyAddAccelerationScript,
	{
		.label		 = "Add Acceleration",
		.group		 = "Physics/Rigid Body/Forces",
		.description = "Add Acceleration on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<RigidBodyAddAccelerationScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RigidBodyStopMovementScript, {
									 .label		  = "Stop Movement",
									 .group		  = "Physics/Rigid Body/Motion",
									 .description = "Stop Movement on the selected target.",
									 .type		  = ScriptType::Sequence,
									 .draw = &DrawTypedActionDetails<RigidBodyStopMovementScript>,
								 }
);
PTGN_REGISTER_SCRIPT(
	RigidBodySetGravityScaleScript,
	{
		.label		 = "Set Gravity Scale",
		.group		 = "Physics/Rigid Body/Forces",
		.description = "Set Gravity Scale on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<RigidBodySetGravityScaleScript>,
		.draw		 = &DrawTypedActionDetails<RigidBodySetGravityScaleScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RigidBodySetDragScript, {
								.label		 = "Set Drag",
								.group		 = "Physics/Rigid Body/Settings",
								.description = "Set Drag on the selected target.",
								.type		 = ScriptType::Sequence,
								.draw_inline = &DrawTypedActionInline<RigidBodySetDragScript>,
								.draw		 = &DrawTypedActionDetails<RigidBodySetDragScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	RigidBodySetAngularVelocityScript,
	{
		.label		 = "Set Angular Velocity",
		.group		 = "Physics/Rigid Body/Motion",
		.description = "Set Angular Velocity on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<RigidBodySetAngularVelocityScript>,
		.draw		 = &DrawTypedActionDetails<RigidBodySetAngularVelocityScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RigidBodyAddAngularImpulseScript,
	{
		.label		 = "Add Angular Impulse",
		.group		 = "Physics/Rigid Body/Forces",
		.description = "Add Angular Impulse on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<RigidBodyAddAngularImpulseScript>,
		.draw		 = &DrawTypedActionDetails<RigidBodyAddAngularImpulseScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RigidBodySetMaximumSpeedScript,
	{
		.label		 = "Set Maximum Speed",
		.group		 = "Physics/Rigid Body/Motion",
		.description = "Set Maximum Speed on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<RigidBodySetMaximumSpeedScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	RigidBodySetImmovableScript,
	{
		.label		 = "Set Immovable",
		.group		 = "Physics/Rigid Body/Settings",
		.description = "Set Immovable on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<RigidBodySetImmovableScript>,
		.draw		 = &DrawTypedActionDetails<RigidBodySetImmovableScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ColliderSetCollisionModeScript,
	{
		.label		 = "Set Collision Mode",
		.group		 = "Physics/Collider",
		.description = "Set Collision Mode on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ColliderSetCollisionModeScript>,
		.draw		 = &DrawTypedActionDetails<ColliderSetCollisionModeScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ColliderSetColliderShapeScript,
	{
		.label		 = "Set Collider Shape",
		.group		 = "Physics/Collider",
		.description = "Set Collider Shape on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<ColliderSetColliderShapeScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ColliderSetCollisionResponseScript,
	{
		.label		 = "Set Collision Response",
		.group		 = "Physics/Collider",
		.description = "Set Collision Response on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<ColliderSetCollisionResponseScript>,
		.draw		 = &DrawTypedActionDetails<ColliderSetCollisionResponseScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ColliderSetCollisionFilterScript,
	{
		.label		 = "Set Collision Filter",
		.group		 = "Physics/Collider",
		.description = "Set Collision Filter on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<ColliderSetCollisionFilterScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	ColliderSetOverlapModeScript, {
									  .label	   = "Set Overlap Mode",
									  .group	   = "Physics/Collider",
									  .description = "Set Overlap Mode on the selected target.",
									  .type		   = ScriptType::Sequence,
									  .draw = &DrawTypedActionDetails<ColliderSetOverlapModeScript>,
								  }
);
PTGN_REGISTER_SCRIPT(
	DraggableSetFollowMouseScript,
	{
		.label		 = "Set Follow Mouse",
		.group		 = "Interaction/Draggable",
		.description = "Set Follow Mouse on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<DraggableSetFollowMouseScript>,
		.draw		 = &DrawTypedActionDetails<DraggableSetFollowMouseScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	DraggableSetMoveConditionScript,
	{
		.label		 = "Set Move Condition",
		.group		 = "Interaction/Draggable",
		.description = "Set Move Condition on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<DraggableSetMoveConditionScript>,
		.draw		 = &DrawTypedActionDetails<DraggableSetMoveConditionScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	DraggableSetDropConditionScript,
	{
		.label		 = "Set Drop Condition",
		.group		 = "Interaction/Draggable",
		.description = "Set Drop Condition on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<DraggableSetDropConditionScript>,
		.draw		 = &DrawTypedActionDetails<DraggableSetDropConditionScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	DraggableSetPickupConditionScript,
	{
		.label		 = "Set Pickup Condition",
		.group		 = "Interaction/Draggable",
		.description = "Set Pickup Condition on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<DraggableSetPickupConditionScript>,
		.draw		 = &DrawTypedActionDetails<DraggableSetPickupConditionScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	DropzoneSetMoveConditionScript,
	{
		.label		 = "Set Move Condition",
		.group		 = "Interaction/Dropzone",
		.description = "Set Move Condition on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<DropzoneSetMoveConditionScript>,
		.draw		 = &DrawTypedActionDetails<DropzoneSetMoveConditionScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	DropzoneSetDropConditionScript,
	{
		.label		 = "Set Drop Condition",
		.group		 = "Interaction/Dropzone",
		.description = "Set Drop Condition on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<DropzoneSetDropConditionScript>,
		.draw		 = &DrawTypedActionDetails<DropzoneSetDropConditionScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	DropzoneSetPickupConditionScript,
	{
		.label		 = "Set Pickup Condition",
		.group		 = "Interaction/Dropzone",
		.description = "Set Pickup Condition on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<DropzoneSetPickupConditionScript>,
		.draw		 = &DrawTypedActionDetails<DropzoneSetPickupConditionScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	InteractionAddInteractiveShapeScript,
	{
		.label		 = "Add Interactive Shape",
		.group		 = "Interaction/Interactive",
		.description = "Add Interactive Shape on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<InteractionAddInteractiveShapeScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	InteractionRemoveInteractiveShapeScript,
	{
		.label		 = "Remove Interactive Shape",
		.group		 = "Interaction/Interactive",
		.description = "Remove Interactive Shape on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<InteractionRemoveInteractiveShapeScript>,
		.draw		 = &DrawTypedActionDetails<InteractionRemoveInteractiveShapeScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	InteractionClearInteractiveShapesScript,
	{
		.label		 = "Clear Interactive Shapes",
		.group		 = "Interaction/Interactive",
		.description = "Clear Interactive Shapes on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<InteractionClearInteractiveShapesScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	InteractionLockInteractionsScript,
	{
		.label		 = "Lock Interactions",
		.group		 = "Interaction/Interactive",
		.description = "Lock Interactions on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<InteractionLockInteractionsScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	InteractionUnlockInteractionsScript,
	{
		.label		 = "Unlock Interactions",
		.group		 = "Interaction/Interactive",
		.description = "Unlock Interactions on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw		 = &DrawTypedActionDetails<InteractionUnlockInteractionsScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	TopDownSetMaximumSpeedScript,
	{
		.label		 = "Set Maximum Speed",
		.group		 = "Physics/Top Down",
		.description = "Set Maximum Speed on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<TopDownSetMaximumSpeedScript>,
		.draw		 = &DrawTypedActionDetails<TopDownSetMaximumSpeedScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	TopDownMoveDirectionScript,
	{
		.label		 = "Move Direction",
		.group		 = "Physics/Top Down",
		.description = "Move Direction on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<TopDownMoveDirectionScript>,
		.draw		 = &DrawTypedActionDetails<TopDownMoveDirectionScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	TopDownSetAccelerationScript,
	{
		.label		 = "Set Acceleration",
		.group		 = "Physics/Top Down",
		.description = "Set Acceleration on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<TopDownSetAccelerationScript>,
		.draw		 = &DrawTypedActionDetails<TopDownSetAccelerationScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	PlatformerSetMaximumSpeedScript,
	{
		.label		 = "Set Maximum Speed",
		.group		 = "Physics/Platformer",
		.description = "Set Maximum Speed on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<PlatformerSetMaximumSpeedScript>,
		.draw		 = &DrawTypedActionDetails<PlatformerSetMaximumSpeedScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	PlatformerSetJumpControllerScript,
	{
		.label		 = "Set Jump Controller",
		.group		 = "Physics/Platformer",
		.description = "Set Jump Controller on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<PlatformerSetJumpControllerScript>,
		.draw		 = &DrawTypedActionDetails<PlatformerSetJumpControllerScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	JumpJumpScript, {
						.label		 = "Jump",
						.group		 = "Physics/Platformer Jump",
						.description = "Jump on the selected target.",
						.type		 = ScriptType::Sequence,
						.draw		 = &DrawTypedActionDetails<JumpJumpScript>,
					}
);
PTGN_REGISTER_SCRIPT(
	JumpSetJumpHeightScript, {
								 .label		  = "Set Jump Height",
								 .group		  = "Physics/Platformer Jump",
								 .description = "Set Jump Height on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<JumpSetJumpHeightScript>,
								 .draw		  = &DrawTypedActionDetails<JumpSetJumpHeightScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	JumpSetGravityMultipliersScript,
	{
		.label		 = "Set Gravity Multipliers",
		.group		 = "Physics/Platformer Jump",
		.description = "Set Gravity Multipliers on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<JumpSetGravityMultipliersScript>,
		.draw		 = &DrawTypedActionDetails<JumpSetGravityMultipliersScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	JumpSetAirJumpsScript, {
							   .label		= "Set Air Jumps",
							   .group		= "Physics/Platformer Jump",
							   .description = "Set Air Jumps on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<JumpSetAirJumpsScript>,
							   .draw		= &DrawTypedActionDetails<JumpSetAirJumpsScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	PhysicsSetGravityScript, {
								 .label		  = "Set Gravity",
								 .group		  = "Physics/Scene",
								 .description = "Set Gravity on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw_inline = &DrawTypedActionInline<PhysicsSetGravityScript>,
								 .draw		  = &DrawTypedActionDetails<PhysicsSetGravityScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	PhysicsEnablePhysicsScript, {
									.label		 = "Enable Physics",
									.group		 = "Physics/Scene",
									.description = "Enable Physics on the selected target.",
									.type		 = ScriptType::Sequence,
									.draw = &DrawTypedActionDetails<PhysicsEnablePhysicsScript>,
								}
);
PTGN_REGISTER_SCRIPT(
	PhysicsDisablePhysicsScript, {
									 .label		  = "Disable Physics",
									 .group		  = "Physics/Scene",
									 .description = "Disable Physics on the selected target.",
									 .type		  = ScriptType::Sequence,
									 .draw = &DrawTypedActionDetails<PhysicsDisablePhysicsScript>,
								 }
);
PTGN_REGISTER_SCRIPT(
	PhysicsSetBoundsScript, {
								.label		 = "Set Bounds",
								.group		 = "Physics/Scene",
								.description = "Set Bounds on the selected target.",
								.type		 = ScriptType::Sequence,
								.draw		 = &DrawTypedActionDetails<PhysicsSetBoundsScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	PhysicsClearBoundsScript, {
								  .label	   = "Clear Bounds",
								  .group	   = "Physics/Scene",
								  .description = "Clear Bounds on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw		   = &DrawTypedActionDetails<PhysicsClearBoundsScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	AudioStopSoundScript, {
							  .label	   = "Stop Sound",
							  .group	   = "Audio/Playback",
							  .description = "Stop Sound on the selected target.",
							  .type		   = ScriptType::Sequence,
							  .draw_inline = &DrawTypedActionInline<AudioStopSoundScript>,
							  .draw		   = &DrawTypedActionDetails<AudioStopSoundScript>,
						  }
);
PTGN_REGISTER_SCRIPT(
	AudioPauseSoundScript, {
							   .label		= "Pause Sound",
							   .group		= "Audio/Playback",
							   .description = "Pause Sound on the selected target.",
							   .type		= ScriptType::Sequence,
							   .draw_inline = &DrawTypedActionInline<AudioPauseSoundScript>,
							   .draw		= &DrawTypedActionDetails<AudioPauseSoundScript>,
						   }
);
PTGN_REGISTER_SCRIPT(
	AudioResumeSoundScript, {
								.label		 = "Resume Sound",
								.group		 = "Audio/Playback",
								.description = "Resume Sound on the selected target.",
								.type		 = ScriptType::Sequence,
								.draw_inline = &DrawTypedActionInline<AudioResumeSoundScript>,
								.draw		 = &DrawTypedActionDetails<AudioResumeSoundScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	AudioToggleSoundPauseScript,
	{
		.label		 = "Toggle Sound Pause",
		.group		 = "Audio/Playback",
		.description = "Toggle Sound Pause on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<AudioToggleSoundPauseScript>,
		.draw		 = &DrawTypedActionDetails<AudioToggleSoundPauseScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	AudioSetSoundVolumeScript, {
								   .label		= "Set Sound Volume",
								   .group		= "Audio/Volume",
								   .description = "Set Sound Volume on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw = &DrawTypedActionDetails<AudioSetSoundVolumeScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	AudioToggleSoundVolumeScript, {
									  .label	   = "Toggle Sound Volume",
									  .group	   = "Audio/Volume",
									  .description = "Toggle Sound Volume on the selected target.",
									  .type		   = ScriptType::Sequence,
									  .draw = &DrawTypedActionDetails<AudioToggleSoundVolumeScript>,
								  }
);
PTGN_REGISTER_SCRIPT(
	AudioSetMasterVolumeScript,
	{
		.label		 = "Set Master Volume",
		.group		 = "Audio/Volume",
		.description = "Set Master Volume on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<AudioSetMasterVolumeScript>,
		.draw		 = &DrawTypedActionDetails<AudioSetMasterVolumeScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	AudioToggleMasterVolumeScript,
	{
		.label		 = "Toggle Master Volume",
		.group		 = "Audio/Volume",
		.description = "Toggle Master Volume on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<AudioToggleMasterVolumeScript>,
		.draw		 = &DrawTypedActionDetails<AudioToggleMasterVolumeScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	AudioStopAllAudioScript, {
								 .label		  = "Stop All Audio",
								 .group		  = "Audio/Playback",
								 .description = "Stop All Audio on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw		  = &DrawTypedActionDetails<AudioStopAllAudioScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	AudioPauseAllAudioScript, {
								  .label	   = "Pause All Audio",
								  .group	   = "Audio/Playback",
								  .description = "Pause All Audio on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw		   = &DrawTypedActionDetails<AudioPauseAllAudioScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	AudioResumeAllAudioScript, {
								   .label		= "Resume All Audio",
								   .group		= "Audio/Playback",
								   .description = "Resume All Audio on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw = &DrawTypedActionDetails<AudioResumeAllAudioScript>,
							   }
);
PTGN_REGISTER_SCRIPT(
	AudioFadeInSoundScript, {
								.label		 = "Fade In Sound",
								.group		 = "Audio/Fade",
								.description = "Fade In Sound on the selected target.",
								.type		 = ScriptType::Sequence,
								.draw		 = &DrawTypedActionDetails<AudioFadeInSoundScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	AudioFadeOutSoundScript, {
								 .label		  = "Fade Out Sound",
								 .group		  = "Audio/Fade",
								 .description = "Fade Out Sound on the selected target.",
								 .type		  = ScriptType::Sequence,
								 .draw		  = &DrawTypedActionDetails<AudioFadeOutSoundScript>,
							 }
);
PTGN_REGISTER_SCRIPT(
	AudioSetPlaybackSpeedScript, {
									 .label		  = "Set Playback Speed",
									 .group		  = "Audio/Speed",
									 .description = "Set Playback Speed on the selected target.",
									 .type		  = ScriptType::Sequence,
									 .draw = &DrawTypedActionDetails<AudioSetPlaybackSpeedScript>,
								 }
);
PTGN_REGISTER_SCRIPT(
	SetUITextScript, {
						 .label		  = "Set Text",
						 .group		  = "UI/Content",
						 .description = "Set Text on the selected target.",
						 .type		  = ScriptType::Sequence,
						 .draw_inline = &DrawTypedActionInline<SetUITextScript>,
						 .draw		  = &DrawTypedActionDetails<SetUITextScript>,
					 }
);
PTGN_REGISTER_SCRIPT(
	TooltipSetPositionScript, {
								  .label	   = "Set Position",
								  .group	   = "UI/Tooltip",
								  .description = "Set Position on the selected target.",
								  .type		   = ScriptType::Sequence,
								  .draw_inline = &DrawTypedActionInline<TooltipSetPositionScript>,
								  .draw		   = &DrawTypedActionDetails<TooltipSetPositionScript>,
							  }
);
PTGN_REGISTER_SCRIPT(
	TooltipSetHoverOffsetScript,
	{
		.label		 = "Set Hover Offset",
		.group		 = "UI/Tooltip",
		.description = "Set Hover Offset on the selected target.",
		.type		 = ScriptType::Sequence,
		.draw_inline = &DrawTypedActionInline<TooltipSetHoverOffsetScript>,
		.draw		 = &DrawTypedActionDetails<TooltipSetHoverOffsetScript>,
	}
);
PTGN_REGISTER_SCRIPT(
	DialogueGoToPageScript, {
								.label		 = "Go To Page",
								.group		 = "UI/Dialogue",
								.description = "Go To Page on the selected target.",
								.type		 = ScriptType::Sequence,
								.draw_inline = &DrawTypedActionInline<DialogueGoToPageScript>,
								.draw		 = &DrawTypedActionDetails<DialogueGoToPageScript>,
							}
);
PTGN_REGISTER_SCRIPT(
	DialogueSetPortraitScript, {
								   .label		= "Set Portrait",
								   .group		= "UI/Dialogue",
								   .description = "Set Portrait on the selected target.",
								   .type		= ScriptType::Sequence,
								   .draw = &DrawTypedActionDetails<DialogueSetPortraitScript>,
							   }
);

PTGN_REGISTER_SCRIPT(
	SetTextureScript, {
						  .label	   = "Set Texture",
						  .group	   = "Rendering/Sprite",
						  .description = "Assign a texture asset key to the owner.",
						  .type		   = ScriptType::Sequence,
						  .draw_inline = &DrawSetTextureInline,
					  }
);

PTGN_REGISTER_SCRIPT(
	SetEnabledScript,
	{
		.label		 = "Set Enabled",
		.group		 = "Entity/Components",
		.description = "Set the enabled value of a supported registered component.",
		.type		 = ScriptType::Sequence,
		.menu_order	 = 4,
		.draw_inline = &DrawSetEnabledInline,
	}
);

PTGN_REGISTER_SCRIPT(
	SceneChangeScript, {
						   .label		= "Change Scene",
						   .group		= "",
						   .description = "Enter, exit, or switch a registered scene.",
						   .type		= ScriptType::Sequence,
						   .draw_inline = &DrawSceneChangeInline,
						   .draw		= &DrawSceneChange,
					   }
);

PTGN_REGISTER_SCRIPT(
	PrintConsoleScript, {
							.label		 = "Print",
							.group		 = "",
							.description = "Print a message to the console.",
							.type		 = ScriptType::Sequence,
							.draw_inline = &DrawPrintConsoleInline,
						}
);

PTGN_REGISTER_SCRIPT(
	EmitSignalScript, {
						  .label	   = "Emit Signal",
						  .group	   = "",
						  .description = "Emit a global signal.",
						  .type		   = ScriptType::Sequence,
						  .draw_inline = &DrawEmitSignalInline,
					  }
);

PTGN_REGISTER_SCRIPT(
	AddComponentsScript, {
							 .label		  = "Add Components",
							 .group		  = "Entity/Components",
							 .description = "Add registered components to the owner.",
							 .type		  = ScriptType::Sequence,
							 .menu_order  = 1,
							 .draw_inline = &DrawAddComponentsInline,
							 .draw		  = &DrawAddComponentsDetails,
						 }
);

PTGN_REGISTER_SCRIPT(
	RemoveComponentsScript, {
								.label			 = "Remove Components",
								.group			 = "Entity/Components",
								.description	 = "Remove registered components from the owner.",
								.type			 = ScriptType::Sequence,
								.menu_order		 = 2,
								.separator_after = true,
								.draw_inline	 = &DrawRemoveComponentsInline,
							}
);

PTGN_REGISTER_EVENT(
	event::KeyPressed, {
						   .label		  = "On Key Pressed",
						   .group		  = "Key",
						   .description	  = "Matches a key expression.",
						   .inline_fields = 1,
						   .draw		  = &DrawKey,
					   }
);

PTGN_REGISTER_EVENT(
	event::KeyHeld, {
						.label		   = "On Key Held",
						.group		   = "Key",
						.description   = "Matches a held key expression.",
						.inline_fields = 2,
						.draw		   = &DrawHeldKey,
					}
);

PTGN_REGISTER_EVENT(
	event::KeyReleased, {
							.label		   = "On Key Released",
							.group		   = "Key",
							.description   = "Matches a key expression.",
							.inline_fields = 1,
							.draw		   = &DrawKey,
						}
);

PTGN_REGISTER_EVENT(
	event::MousePressed, {
							 .label			= "On Mouse Pressed",
							 .group			= "Mouse",
							 .description	= "Matches one mouse button.",
							 .inline_fields = 1,
							 .draw			= &DrawMouse,
						 }
);

PTGN_REGISTER_EVENT(
	event::MouseHeld, {
						  .label		 = "On Mouse Held",
						  .group		 = "Mouse",
						  .description	 = "Matches one mouse button.",
						  .inline_fields = 2,
						  .draw			 = &DrawHeldMouse,
					  }
);

PTGN_REGISTER_EVENT(
	event::MouseReleased, {
							  .label		 = "On Mouse Released",
							  .group		 = "Mouse",
							  .description	 = "Matches one mouse button.",
							  .inline_fields = 1,
							  .draw			 = &DrawMouse,
						  }
);

PTGN_REGISTER_EVENT(
	event::MouseMoveOver, {
							  .label	   = "On Mouse Enter",
							  .group	   = "Interaction",
							  .description = "Matches when the pointer enters the owner.",
						  }
);

PTGN_REGISTER_EVENT(
	event::MouseMoveOut, {
							 .label		  = "On Mouse Leave",
							 .group		  = "Interaction",
							 .description = "Matches when the pointer leaves the owner.",
						 }
);

PTGN_REGISTER_EVENT(
	event::MousePressedOver, {
								 .label			= "On Mouse Pressed Over",
								 .group			= "Interaction",
								 .description	= "Matches one mouse button.",
								 .inline_fields = 1,
								 .draw			= &DrawMouse,
							 }
);

PTGN_REGISTER_EVENT(
	event::MouseHeldOver, {
							  .label		 = "On Mouse Held Over",
							  .group		 = "Interaction",
							  .description	 = "Matches one mouse button.",
							  .inline_fields = 2,
							  .draw			 = &DrawHeldMouse,
						  }
);

PTGN_REGISTER_EVENT(
	event::MouseReleasedOver, {
								  .label		 = "On Mouse Released Over",
								  .group		 = "Interaction",
								  .description	 = "Matches one mouse button.",
								  .inline_fields = 1,
								  .draw			 = &DrawMouse,
							  }
);

PTGN_REGISTER_EVENT(
	event::ButtonPress, {
							.label		 = "On Button Press",
							.group		 = "Button",
							.description = "Matches when the owner emits ButtonPress.",
						}
);

PTGN_REGISTER_EVENT(
	event::ButtonHoverStart,
	{
		.label		 = "On Button Hover Start",
		.group		 = "Button",
		.description = "Matches when the pointer starts hovering over the owner button.",
	}
);

PTGN_REGISTER_EVENT(
	event::ButtonHover,
	{
		.label		 = "On Button Hover",
		.group		 = "Button",
		.description = "Matches while the pointer remains over the owner button.",
	}
);

PTGN_REGISTER_EVENT(
	event::ButtonHoverStop,
	{
		.label		 = "On Button Hover Stop",
		.group		 = "Button",
		.description = "Matches when the pointer stops hovering over the owner button.",
	}
);

PTGN_REGISTER_EVENT(
	event::ButtonEnabled, { .label		 = "On Button Enable",
							.group		 = "Button",
							.description = "Matches when the owner button becomes press enabled." }
);
PTGN_REGISTER_EVENT(
	event::ButtonDisabled,
	{ .label	   = "On Button Disable",
	  .group	   = "Button",
	  .description = "Matches when the owner button becomes press disabled." }
);
PTGN_REGISTER_EVENT(
	event::SliderChange, { .label		= "On Slider Change",
						   .group		= "Slider",
						   .description = "Matches when the owner slider value changes." }
);
PTGN_REGISTER_EVENT(
	event::TooltipShown, { .label		= "On Tooltip Show",
						   .group		= "Tooltip",
						   .description = "Matches when the owner tooltip becomes visible." }
);
PTGN_REGISTER_EVENT(
	event::TooltipHidden, { .label		 = "On Tooltip Hide",
							.group		 = "Tooltip",
							.description = "Matches when the owner tooltip becomes hidden." }
);
PTGN_REGISTER_EVENT(
	event::TooltipHoverEnabled,
	{ .label	   = "On Tooltip Hover Enable",
	  .group	   = "Tooltip",
	  .description = "Matches when automatic tooltip showing is enabled on the owner." }
);
PTGN_REGISTER_EVENT(
	event::TooltipHoverDisabled,
	{ .label	   = "On Tooltip Hover Disable",
	  .group	   = "Tooltip",
	  .description = "Matches when automatic tooltip showing is disabled on the owner." }
);
PTGN_REGISTER_EVENT(
	event::DialogueContinued,
	{ .label = "On Dialogue Continue",
	  .group = "Dialogue",
	  .description =
		  "Matches every request to advance an open dialogue, including continue key presses.",
	  .inline_fields = 2,
	  .draw			 = &DrawDialoguePageEvent }
);
PTGN_REGISTER_EVENT(
	event::DialogueOpened, {
							   .label		  = "On Dialogue Open",
							   .group		  = "Dialogue",
							   .description	  = "Matches when the owner dialogue opens.",
							   .inline_fields = 1,
							   .draw		  = &DrawDialogueEvent,
						   }
);

PTGN_REGISTER_EVENT(
	event::DialogueClosed, {
							   .label		  = "On Dialogue Close",
							   .group		  = "Dialogue",
							   .description	  = "Matches when the owner dialogue closes.",
							   .inline_fields = 1,
							   .draw		  = &DrawDialogueEvent,
						   }
);

PTGN_REGISTER_EVENT(
	event::DialogueChanged, {
								.label		   = "On Dialogue Change",
								.group		   = "Dialogue",
								.description   = "Matches when the selected dialogue key changes.",
								.inline_fields = 1,
								.draw		   = &DrawDialogueEvent,
							}
);

PTGN_REGISTER_EVENT(
	event::DialoguePageChanged,
	{
		.label		   = "On Dialogue Page Change",
		.group		   = "Dialogue",
		.description   = "Matches when the current dialogue page changes.",
		.inline_fields = 2,
		.draw		   = &DrawDialoguePageEvent,
	}
);

PTGN_REGISTER_EVENT(
	event::DialoguePageCompleted,
	{
		.label		   = "On Dialogue Page Complete",
		.group		   = "Dialogue",
		.description   = "Matches when the current page is fully revealed.",
		.inline_fields = 2,
		.draw		   = &DrawDialoguePageEvent,
	}
);

PTGN_REGISTER_EVENT(
	event::DialogueFinished,
	{
		.label		   = "On Dialogue Finish",
		.group		   = "Dialogue",
		.description   = "Matches after advancing past the final page of a dialogue.",
		.inline_fields = 1,
		.draw		   = &DrawDialogueEvent,
	}
);

PTGN_REGISTER_EVENT(
	event::ToggleButtonOn, { .label		  = "On Toggle On",
							 .group		  = "Toggle Button",
							 .description = "Matches when the owner toggle becomes active." }
);
PTGN_REGISTER_EVENT(
	event::ToggleButtonOff, { .label	   = "On Toggle Off",
							  .group	   = "Toggle Button",
							  .description = "Matches when the owner toggle becomes inactive." }
);
PTGN_REGISTER_EVENT(
	event::ToggleGroupSelectionChanged,
	{ .label	   = "On Toggle Group Selection",
	  .group	   = "Toggle Group",
	  .description = "Matches when the owner toggle group selects an item." }
);
PTGN_REGISTER_EVENT(
	event::ToggleGroupSelectionCleared,
	{ .label	   = "On Toggle Group Clear",
	  .group	   = "Toggle Group",
	  .description = "Matches when the owner toggle group clears its selection." }
);
PTGN_REGISTER_EVENT(
	event::ToggleGroupRequiredChanged,
	{ .label	   = "On Toggle Group Policy Change",
	  .group	   = "Toggle Group",
	  .description = "Matches when requiring an active toggle is enabled or disabled." }
);
PTGN_REGISTER_EVENT(
	event::ToggleButtonToggle,
	{

		.label		 = "On Toggle",
		.group		 = "Toggle Button",
		.description = "Matches when the owner toggle button changes state.",
	}
);

PTGN_REGISTER_EVENT(
	event::DropdownOpen, {
							 .label		  = "On Dropdown Open",
							 .group		  = "Dropdown",
							 .description = "Matches when the owner dropdown opens.",
						 }
);

PTGN_REGISTER_EVENT(
	event::DropdownClose, {
							  .label	   = "On Dropdown Close",
							  .group	   = "Dropdown",
							  .description = "Matches when the owner dropdown closes.",
						  }
);

PTGN_REGISTER_EVENT(
	event::DropdownToggle,
	{
		.label		 = "On Dropdown Toggle",
		.group		 = "Dropdown",
		.description = "Matches whenever the owner dropdown opens or closes.",
	}
);

PTGN_REGISTER_EVENT(
	event::DropdownItemPress,
	{
		.label		 = "On Dropdown Item Press",
		.group		 = "Dropdown",
		.description = "Matches when one of the owner dropdown's direct items is pressed.",
	}
);

PTGN_REGISTER_EVENT(
	event::DragStart, {
						  .label	   = "On Drag Start",
						  .group	   = "Drag",
						  .description = "Matches drag start.",
					  }
);

PTGN_REGISTER_EVENT(
	event::Drag, {
					 .label		  = "On Drag",
					 .group		  = "Drag",
					 .description = "Matches while dragging.",
				 }
);

PTGN_REGISTER_EVENT(
	event::DragStop, {
						 .label		  = "On Drag Stop",
						 .group		  = "Drag",
						 .description = "Matches drag stop.",
					 }
);

PTGN_REGISTER_EVENT(
	event::OverlapStart, {
							 .label		  = "On Overlap Start",
							 .group		  = "Physics",
							 .description = "Matches overlap start.",
						 }
);

PTGN_REGISTER_EVENT(
	event::Overlap, {
						.label		 = "On Overlap",
						.group		 = "Physics",
						.description = "Matches overlap.",
					}
);

PTGN_REGISTER_EVENT(
	event::OverlapStop, {
							.label		 = "On Overlap Stop",
							.group		 = "Physics",
							.description = "Matches overlap stop.",
						}
);

PTGN_REGISTER_EVENT(
	event::Collision, {
						  .label	   = "On Collision",
						  .group	   = "Physics",
						  .description = "Matches collision.",
					  }
);

PTGN_REGISTER_EVENT(
	event::AnimationStart, {
							   .label		= "On Animation Start",
							   .group		= "Animation",
							   .description = "Matches when an animation starts.",
						   }
);

PTGN_REGISTER_EVENT(
	event::AnimationStop, {
							  .label	   = "On Animation Stop",
							  .group	   = "Animation/Color",
							  .description = "Matches when an animation stops or resets.",
						  }
);

PTGN_REGISTER_EVENT(
	event::AnimationPause, {
							   .label		= "On Animation Pause",
							   .group		= "Animation",
							   .description = "Matches when an animation is paused.",
						   }
);

PTGN_REGISTER_EVENT(
	event::AnimationResume, {
								.label		 = "On Animation Resume",
								.group		 = "Animation",
								.description = "Matches when an animation is resumed.",
							}
);

PTGN_REGISTER_EVENT(
	event::AnimationFrameChange, {
									 .label		  = "On Animation Frame Change",
									 .group		  = "Animation",
									 .description = "Matches whenever the animation frame changes.",
								 }
);

PTGN_REGISTER_EVENT(
	event::AnimationUpdate, {
								.label		 = "On Animation Update",
								.group		 = "Animation",
								.description = "Matches every frame while an animation is playing.",
							}
);

PTGN_REGISTER_EVENT(
	event::AnimationFinalFrame,
	{
		.label		 = "On Animation Final Frame",
		.group		 = "Animation",
		.description = "Matches whenever the final frame of an animation plays.",
	}
);

PTGN_REGISTER_EVENT(
	event::AnimationComplete, {
								  .label	   = "On Animation Complete",
								  .group	   = "Animation/Color",
								  .description = "Matches when all animation plays complete.",
							  }
);

PTGN_REGISTER_EVENT(
	event::AnimationLoopComplete,
	{
		.label		 = "On Animation Loop Complete",
		.group		 = "Animation",
		.description = "Matches whenever one full animation loop completes.",
	}
);

PTGN_REGISTER_EVENT(
	event::TimerElapsed,
	{
		.label = "On Timer Elapsed",
		.group = "Timing",
		.description =
			"Matches when a named timer reaches its configured or overridden elapsed duration.",
		.inline_fields = 3,
	}
);

PTGN_REGISTER_EVENT(
	event::CounterChanged,
	{
		.label		   = "On Counter Changed",
		.group		   = "Variables",
		.description   = "Matches changes or threshold crossings of a named counter.",
		.inline_fields = 3,
	}
);

PTGN_REGISTER_EVENT(
	event::BooleanChanged,
	{
		.label		   = "On Boolean Changed",
		.group		   = "Variables",
		.description   = "Matches changes to a named boolean or a specific boolean transition.",
		.inline_fields = 3,
	}
);

PTGN_REGISTER_EVENT(
	Signal, {
				.label		   = "On Signal",
				.group		   = "",
				.description   = "Matches an exact signal name.",
				.inline_fields = 1,
				.draw		   = &DrawSignalEvent,
			}
);

PTGN_REGISTER_EVENT(
	event::SceneEnter,
	{
		.label		 = "On Scene Enter",
		.group		 = "Scene",
		.description = "Matches once after the scene has entered and Scene::OnEnter() has run.",
	}
);

PTGN_REGISTER_EVENT(
	event::SceneLeave,
	{
		.label		 = "On Scene Leave",
		.group		 = "Scene",
		.description = "Matches once immediately before the scene exits. If the scene has an "
					   "outgoing transition, this occurs after that transition finishes.",
	}
);

PTGN_REGISTER_EVENT(
	event::SceneTransitionStart,
	{
		.label		 = "On Scene Transition Start",
		.group		 = "Scene",
		.description = "Matches once when this scene's transition starts.",
	}
);

PTGN_REGISTER_EVENT(
	event::SceneTransitionUpdate,
	{
		.label		 = "On Scene Transition",
		.group		 = "Scene",
		.description = "Matches every frame while this scene's transition is actively running.",
	}
);

PTGN_REGISTER_EVENT(
	event::SceneTransitionFinish,
	{
		.label		 = "On Scene Transition Finish",
		.group		 = "Scene",
		.description = "Matches once when this scene's transition finishes.",
	}
);

PTGN_REGISTER_EVENT(
	event::EntityCreated,
	{
		.label		 = "On Create",
		.group		 = "",
		.description = "Matches once when the entity's scripts are created for runtime.",
	}
);

namespace impl {

void EnsureEngineScriptEditorsRegistered() {
	// Intentionally empty.
	//
	// Referencing this function forces the linker to include this object file. The namespace scope
	// PTGN_REGISTER_SCRIPT and PTGN_REGISTER_EVENT initializers then populate the editor
	// registries.
}

} // namespace impl

} // namespace ptgn::editor
