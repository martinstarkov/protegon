#include "scripting/script_editor_registry.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#ifndef MAGIC_ENUM_RANGE_MAX
#define MAGIC_ENUM_RANGE_MAX 512
#endif
#include <algorithm>
#include <array>
#include <cfloat>
#include <cctype>
#include <cstdint>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <limits>
#include <vector>

#include "app/project.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "core/event/key_event.h"
#include "core/event/mouse_event.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "panels/entity_filter_editor.h"
#include "panels/inspector_fields.h"
#include "panels/content_browser.h"
#include "runtime/animation/animation_event.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity_filter.h"
#include "runtime/interaction/draggable_event.h"
#include "runtime/interaction/dropzone_event.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/physics/collision_event.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/scripting/script_event.h"
#include "runtime/timer/timer.h"
#include "runtime/timer/timer_event.h"
#include "runtime/ui/button.h"
#include "runtime/animation/animation.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/audio/audio_system.h"
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

float GetCountControlWidth(const char* label) {
	const float button_width{ ImGui::GetFrameHeight() };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const std::string widest{ std::string{ label } + ": 100" };

	return ImGui::CalcTextSize(widest.c_str()).x +
		button_width * 2.0f + spacing * 2.0f;
}

void DrawCountControl(
	const char* label,
	int& value,
	int minimum,
	int maximum = 100,
	bool disabled = false,
	const char* tooltip = nullptr
) {
	value = std::clamp(value, minimum, maximum);

	const float button_width{ ImGui::GetFrameHeight() };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
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
		ImVec2{
			start_x + text_width + spacing,
			ImGui::GetCursorScreenPos().y
		}
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
		ctx,
		type_name_without_namespaces<T>(),
		std::addressof(value),
		[](void* data, ComponentReflectionVisitor visitor) {
			::ptgn::VisitReflectedValue(*static_cast<T*>(data), visitor);
		}
	);
}

[[nodiscard]] std::string ComponentLabel(const RegisteredComponent& component) {
	std::string_view name{ component.name };
	const auto separator{ name.rfind("::") };

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
	const json previous = value;

	if (value.is_null()) {
		value = defaults;
	} else if (value.is_object() && defaults.is_object()) {
		json normalized = defaults;
		normalized.update(value, true);
		value = std::move(normalized);
	}

	return value != previous;
}

bool DrawJsonValue(
	std::string_view label,
	json& value,
	const json* defaults = nullptr
);

bool DrawJsonObject(json& value, const json* defaults) {
	bool changed{ false };

	for (auto it{ value.begin() }; it != value.end(); ++it) {
		const json* member_defaults{ nullptr };

		if (defaults && defaults->is_object()) {
			const auto default_it{ defaults->find(it.key()) };
			if (default_it != defaults->end()) {
				member_defaults = std::addressof(*default_it);
			}
		}

		changed |= DrawJsonValue(
			inspector::PrettyName(it.key()),
			it.value(),
			member_defaults
		);
	}

	return changed;
}

bool DrawJsonArray(
	std::string_view label,
	json& value,
	const json* defaults
) {
	const std::string title{
		std::string{ label } + " [" + std::to_string(value.size()) + "]"
	};

	if (!ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) {
		return false;
	}

	bool changed{ false };

	for (std::size_t i{ 0 }; i < value.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));

		const json* item_defaults{ nullptr };
		if (defaults && defaults->is_array() && i < defaults->size()) {
			item_defaults = std::addressof((*defaults)[i]);
		}

		changed |= DrawJsonValue(
			"Item " + std::to_string(i + 1),
			value[i],
			item_defaults
		);

		ImGui::PopID();
	}

	if (value.empty()) {
		ImGui::TextDisabled("No items");
	}

	ImGui::TreePop();
	return changed;
}

bool DrawJsonValue(
	std::string_view label,
	json& value,
	const json* defaults
) {
	if (defaults) {
		NormalizeJsonAgainstDefaults(value, *defaults);
	}

	if (value.is_boolean()) {
		bool temporary{ value.get<bool>() };
		const bool changed{ inspector::DrawPropertyRow(label, [&]() {
			return ImGui::Checkbox("##Value", &temporary);
		}) };

		if (changed) {
			value = temporary;
		}
		return changed;
	}

	if (value.is_number_unsigned()) {
		std::uint64_t temporary{ value.get<std::uint64_t>() };
		const bool changed{ inspector::DrawPropertyRow(label, [&]() {
			return ImGui::DragScalar(
				"##Value",
				ImGuiDataType_U64,
				&temporary,
				1.0f
			);
		}) };

		if (changed) {
			value = temporary;
		}
		return changed;
	}

	if (value.is_number_integer()) {
		std::int64_t temporary{ value.get<std::int64_t>() };
		const bool changed{ inspector::DrawPropertyRow(label, [&]() {
			return ImGui::DragScalar(
				"##Value",
				ImGuiDataType_S64,
				&temporary,
				1.0f
			);
		}) };

		if (changed) {
			value = temporary;
		}
		return changed;
	}

	if (value.is_number_float()) {
		double temporary{ value.get<double>() };
		const bool changed{ inspector::DrawPropertyRow(label, [&]() {
			return ImGui::DragScalar(
				"##Value",
				ImGuiDataType_Double,
				&temporary,
				0.1f,
				nullptr,
				nullptr,
				"%.6g"
			);
		}) };

		if (changed) {
			value = temporary;
		}
		return changed;
	}

	if (value.is_string()) {
		std::string temporary{ value.get<std::string>() };
		const bool changed{ inspector::DrawPropertyRow(label, [&]() {
			return ImGui::InputText("##Value", &temporary);
		}) };

		if (changed) {
			value = std::move(temporary);
		}
		return changed;
	}

	if (value.is_object()) {
		const std::string title{ label };
		if (!ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) {
			return false;
		}

		const bool changed{ DrawJsonObject(value, defaults) };
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

bool DrawRegisteredComponentJson(
	const RegisteredComponent& component,
	json& value
) {
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

std::string KeyExpressionValue(const json& value) {
	if (const auto expression{ JsonValueOr<std::string>(value, "keys", "") }; !expression.empty()) {
		return expression;
	}

	return inspector::KeyDisplayLabel(JsonValueOr<Key>(value, "key", Key::W));
}

[[nodiscard]] bool IsKeyExpressionWhitespace(char c) {
	switch (c) {
		case ' ':  [[fallthrough]];
		case '	': [[fallthrough]];
		case '\n': [[fallthrough]];
		case '\r': [[fallthrough]];
		case '\f': [[fallthrough]];
		case '\v': return true;
		default: return false;
	}
}

[[nodiscard]] std::string StripKeyExpressionWhitespace(const std::string& token) {
	std::size_t first{ 0 };
	while (first < token.size() && IsKeyExpressionWhitespace(token[first])) {
		++first;
	}

	std::size_t last{ token.size() };
	while (last > first && IsKeyExpressionWhitespace(token[last - 1])) {
		--last;
	}

	return token.substr(first, last - first);
}

[[nodiscard]] bool IsAsciiAlphaNumeric(char c) {
	return (c >= 'a' && c <= 'z') ||
		   (c >= 'A' && c <= 'Z') ||
		   (c >= '0' && c <= '9');
}

[[nodiscard]] char ToAsciiLower(char c) {
	return c >= 'A' && c <= 'Z'
		? static_cast<char>(c - 'A' + 'a')
		: c;
}

[[nodiscard]] std::string NormalizeKeyExpressionToken(const std::string& token) {
	std::string normalized;
	normalized.reserve(token.size());

	for (char c : token) {
		if (IsAsciiAlphaNumeric(c)) {
			normalized.push_back(ToAsciiLower(c));
		}
	}

	return normalized;
}

[[nodiscard]] std::string NormalizeKeyAlias(std::string normalized) {
	if (normalized.size() == 1 && normalized.front() >= '0' && normalized.front() <= '9') {
		normalized.insert(normalized.begin(), 'k');
	}

	if (normalized == "shift" || normalized == "lshift") {
		return "leftshift";
	}
	if (normalized == "rshift") {
		return "rightshift";
	}
	if (
		normalized == "ctrl" ||
		normalized == "control" ||
		normalized == "lctrl" ||
		normalized == "leftcontrol"
	) {
		return "leftctrl";
	}
	if (normalized == "rctrl" || normalized == "rightcontrol") {
		return "rightctrl";
	}
	if (normalized == "alt" || normalized == "option" || normalized == "lalt") {
		return "leftalt";
	}
	if (normalized == "ralt") {
		return "rightalt";
	}
	if (normalized == "super" || normalized == "cmd" || normalized == "command") {
		return "leftsuper";
	}

	return normalized;
}

[[nodiscard]] bool IsDecimalNumberInRange(
	std::string_view value,
	int minimum,
	int maximum
) {
	if (value.empty()) {
		return false;
	}

	int number{ 0 };
	for (char c : value) {
		if (c < '0' || c > '9') {
			return false;
		}

		number = number * 10 + c - '0';
		if (number > maximum) {
			return false;
		}
	}

	return number >= minimum && number <= maximum;
}

[[nodiscard]] bool IsKnownKeyExpressionToken(const std::string& token) {
	const std::string normalized{
		NormalizeKeyAlias(NormalizeKeyExpressionToken(token))
	};

	if (normalized.empty()) {
		return false;
	}

	if (
		normalized.size() == 1 &&
		normalized.front() >= 'a' &&
		normalized.front() <= 'z'
	) {
		return true;
	}

	if (
		normalized.size() == 2 &&
		normalized.front() == 'k' &&
		normalized[1] >= '0' &&
		normalized[1] <= '9'
	) {
		return true;
	}

	if (
		normalized.size() > 1 &&
		normalized.front() == 'f' &&
		IsDecimalNumberInRange(
			std::string_view{ normalized }.substr(1),
			1,
			25
		)
	) {
		return true;
	}

	if (
		normalized.size() == 3 &&
		normalized.starts_with("kp") &&
		normalized[2] >= '0' &&
		normalized[2] <= '9'
	) {
		return true;
	}

	static constexpr std::array<std::string_view, 48> kNamedKeys{
		"space",
		"apostrophe",
		"comma",
		"minus",
		"period",
		"slash",
		"semicolon",
		"equal",
		"leftbracket",
		"backslash",
		"rightbracket",
		"graveaccent",
		"world1",
		"world2",
		"escape",
		"enter",
		"tab",
		"backspace",
		"insert",
		"delete",
		"right",
		"left",
		"down",
		"up",
		"pageup",
		"pagedown",
		"home",
		"end",
		"capslock",
		"scrolllock",
		"numlock",
		"printscreen",
		"pause",
		"kpdecimal",
		"kpdivide",
		"kpmultiply",
		"kpsubtract",
		"kpadd",
		"kpenter",
		"kpequal",
		"leftshift",
		"leftctrl",
		"leftalt",
		"leftsuper",
		"rightshift",
		"rightctrl",
		"rightalt",
		"rightsuper",
	};

	return std::ranges::find(kNamedKeys, normalized) != kNamedKeys.end() ||
		normalized == "menu";
}

[[nodiscard]] std::optional<std::string> KeyExpressionError(const std::string& input) {
	const std::string expression{ StripKeyExpressionWhitespace(input) };
	if (expression.empty()) {
		return "Enter at least one key.";
	}

	std::size_t group_begin{ 0 };
	while (group_begin <= expression.size()) {
		const std::size_t comma{ expression.find(',', group_begin) };
		const std::size_t group_end{
			comma == std::string::npos ? expression.size() : comma
		};
		const std::string group{
			StripKeyExpressionWhitespace(
				expression.substr(group_begin, group_end - group_begin)
			)
		};

		if (group.empty()) {
			return "Missing a key near ','.";
		}

		std::size_t token_begin{ 0 };
		while (token_begin <= group.size()) {
			const std::size_t plus{ group.find('+', token_begin) };
			const std::size_t token_end{
				plus == std::string::npos ? group.size() : plus
			};
			const std::string token{
				StripKeyExpressionWhitespace(
					group.substr(token_begin, token_end - token_begin)
				)
			};

			if (token.empty()) {
				return plus == std::string::npos
					? "Missing a key after '+'."
					: "Missing a key near '+'.";
			}

			if (!IsKnownKeyExpressionToken(token)) {
				return "Unknown key: " + token + ".";
			}

			if (plus == std::string::npos) {
				break;
			}
			token_begin = plus + 1;
		}

		if (comma == std::string::npos) {
			break;
		}
		group_begin = comma + 1;
		if (group_begin >= expression.size()) {
			return "Missing a key after ','.";
		}
	}

	return std::nullopt;
}

void DrawInvalidKeyExpressionBorder(const std::optional<std::string>& error) {
	if (!error) {
		return;
	}

	const ImVec2 min{ ImGui::GetItemRectMin() };
	const ImVec2 max{ ImGui::GetItemRectMax() };
	ImGui::GetWindowDrawList()->AddRect(
		min,
		max,
		ImGui::GetColorU32(ImVec4{ 1.0f, 0.2f, 0.2f, 1.0f }),
		ImGui::GetStyle().FrameRounding,
		0,
		1.5f
	);

	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", error->c_str());
	}
}

bool DrawHeldDurationToggle(bool& require_duration) {
	const bool changed{ ImGui::Checkbox("##RequireHeldDuration", &require_duration) };
	DrawItemTooltip(
		"Checked: require the minimum held duration. Unchecked: match any held state."
	);
	return changed;
}

bool DrawKeyExpression(json& value, bool with_duration) {
	std::string expression{ KeyExpressionValue(value) };
	bool require_duration{
		JsonValueOr<bool>(value, "require_held_duration", true)
	};
	float held_duration_ms{
		std::max(0.0f, JsonValueOr<float>(value, "held_duration_ms", 250.0f))
	};
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float duration_width{ 112.0f };
	const float checkbox_width{ ImGui::GetFrameHeight() };
	const float expression_width{
		with_duration
			? std::max(
				1.0f,
				ImGui::GetContentRegionAvail().x - duration_width - checkbox_width - spacing * 2.0f
			)
			: -FLT_MIN
	};

	ImGui::SetNextItemWidth(expression_width);
	bool changed{ ImGui::InputTextWithHint(
		"##Keys",
		"W + X, W + Left Shift",
		&expression
	) };

	const auto expression_error{ KeyExpressionError(expression) };
	if (expression_error) {
		DrawInvalidKeyExpressionBorder(expression_error);
	} else {
		DrawItemTooltip("Use + for AND and comma for OR. Key names are case-insensitive.");
	}

	if (with_duration) {
		ImGui::SameLine(0.0f, spacing);
		changed |= DrawHeldDurationToggle(require_duration);

		ImGui::SameLine(0.0f, spacing);
		ImGui::BeginDisabled(!require_duration);
		changed |= inspector::DrawDurationInput(
			"##HeldDuration",
			held_duration_ms,
			duration_width,
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
			value["held_duration_ms"] = held_duration_ms;
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
		case Mouse::Left: return "Left";
		case Mouse::Right: return "Right";
		case Mouse::Middle: return "Middle";
		default: return "Left";
	}
}

bool DrawMouseTrigger(json& value, bool with_duration) {
	Mouse mouse{ JsonValueOr<Mouse>(value, "button", Mouse::Left) };
	bool changed{ false };
	if (mouse != Mouse::Left && mouse != Mouse::Right && mouse != Mouse::Middle) {
		mouse = Mouse::Left;
		changed = true;
	}

	bool require_duration{
		JsonValueOr<bool>(value, "require_held_duration", true)
	};
	float held_duration_ms{
		std::max(0.0f, JsonValueOr<float>(value, "held_duration_ms", 250.0f))
	};
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float duration_width{ 112.0f };
	const float checkbox_width{ ImGui::GetFrameHeight() };
	const float mouse_width{
		with_duration
			? std::max(
				1.0f,
				ImGui::GetContentRegionAvail().x - duration_width - checkbox_width - spacing * 2.0f
			)
			: -FLT_MIN
	};

	ImGui::SetNextItemWidth(mouse_width);
	if (ImGui::BeginCombo("##Button", MouseTriggerLabel(mouse))) {
		for (const Mouse candidate : { Mouse::Left, Mouse::Right, Mouse::Middle }) {
			const bool selected{ candidate == mouse };
			if (ImGui::Selectable(MouseTriggerLabel(candidate), selected)) {
				mouse = candidate;
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
			"##HeldDuration",
			held_duration_ms,
			duration_width,
			"Minimum time the mouse button must remain held."
		);
		ImGui::EndDisabled();
		held_duration_ms = std::max(0.0f, held_duration_ms);
	}

	if (changed) {
		value["button"] = mouse;
		if (with_duration) {
			value["require_held_duration"] = require_duration;
			value["held_duration_ms"] = held_duration_ms;
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
	const bool changed{ ImGui::InputTextWithHint("##Signal", "Signal name", &signal) };
	DrawItemTooltip("Signal name matched exactly.");
	if (changed) {
		value["signal"] = std::move(signal);
	}
	return changed;
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
			const bool is_selected{ script.sequence.shared_reference &&
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
	const float available{ ImGui::GetContentRegionAvail().x };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float mode_width{
		std::max(ImGui::CalcTextSize("Relative").x, ImGui::CalcTextSize("Absolute").x) +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const float field_width{ std::max(36.0f, (available - mode_width - spacing * 2.0f) * 0.5f) };
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
	const float available{ ImGui::GetContentRegionAvail().x };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float shortest_width{ ImGui::CalcTextSize("Shortest").x +
								ImGui::GetStyle().FramePadding.x * 2.0f };
	const float relative_width{ ImGui::CalcTextSize("Relative").x +
								ImGui::GetStyle().FramePadding.x * 2.0f };
	const float degrees_width{
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
	const float available{ ImGui::GetContentRegionAvail().x };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float mode_width{
		std::max(ImGui::CalcTextSize("Relative").x, ImGui::CalcTextSize("Absolute").x) +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const float field_width{ std::max(36.0f, (available - mode_width - spacing * 2.0f) * 0.5f) };
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

bool DrawPlaySoundInline(
	ScriptEditorContext& context,
	PlaySoundScript& script
) {
	ImGui::SetNextItemWidth(-FLT_MIN);

	return inspector::DrawAssetKeyInline(
		context.ctx,
		script.sound,
		inspector::FieldOptions{},
		"Audio key"
	);
}

bool DrawPlaySound(
	ScriptEditorContext&,
	PlaySoundScript& script
) {
	script.volume = std::clamp(
		script.volume,
		kMinVolume,
		kMaxVolume
	);
	script.loops = std::clamp(
		script.loops,
		0,
		kMaxAudioPlayLoops
	);

	bool changed{ false };

	if (ImGui::BeginTable(
			"PlaySoundParameters",
			2,
			ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn(
			"Volume",
			ImGuiTableColumnFlags_WidthStretch
		);
		ImGui::TableSetupColumn(
			"Loops",
			ImGuiTableColumnFlags_WidthFixed,
			GetCountControlWidth("Loops")
		);

		ImGui::TableNextRow(
			ImGuiTableRowFlags_None,
			ImGui::GetFrameHeight()
		);

		ImGui::TableSetColumnIndex(0);
		ImGui::SetNextItemWidth(-FLT_MIN);

		changed |= ImGui::SliderFloat(
			"##Volume",
			&script.volume,
			kMinVolume,
			kMaxVolume,
			"Volume: %.2f",
			ImGuiSliderFlags_AlwaysClamp
		);
		DrawItemTooltip(
			"Audio volume. Double click to enter an exact value."
		);

		if (ImGui::IsItemHovered() &&
			ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			ImGui::OpenPopup("ExactVolume");
		}

		if (ImGui::BeginPopup("ExactVolume")) {
			ImGui::SetNextItemWidth(110.0f);

			changed |= ImGui::InputFloat(
				"Volume",
				&script.volume,
				0.01f,
				0.1f,
				"%.3f"
			);

			script.volume = std::clamp(
				script.volume,
				kMinVolume,
				kMaxVolume
			);

			ImGui::EndPopup();
		}

		ImGui::TableSetColumnIndex(1);

		const int previous_loops{ script.loops };

		DrawCountControl(
			"Loops",
			script.loops,
			0,
			kMaxAudioPlayLoops,
			false,
			"Number of additional plays after the first."
		);

		changed |= previous_loops != script.loops;

		ImGui::EndTable();
	}

	return changed;
}

template <typename T, std::size_t N>
bool DrawNamedEnumCombo(
	const char* id, T& value, const std::array<std::pair<T, const char*>, N>& entries,
	const char* tooltip = nullptr
) {
	const auto selected{ std::ranges::find_if(entries, [value](const auto& entry) {
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
	DrawItemTooltip(tooltip);
	return changed;
}

inline constexpr std::array kAnimationActions{
	std::pair{ AnimationAction::Start, "Start" },
	std::pair{ AnimationAction::Stop, "Stop" },
	std::pair{ AnimationAction::Reset, "Reset" },
	std::pair{ AnimationAction::Pause, "Pause" },
	std::pair{ AnimationAction::Resume, "Resume" },
	std::pair{ AnimationAction::TogglePlaying, "Toggle Playing" },
	std::pair{ AnimationAction::SetFrame, "Set Frame" },
	std::pair{ AnimationAction::NextFrame, "Next Frame" },
	std::pair{ AnimationAction::PreviousFrame, "Previous Frame" },
};

inline constexpr std::array kTimerActions{
	std::pair{ TimerAction::Start, "Start" },
	std::pair{ TimerAction::Restart, "Restart" },
	std::pair{ TimerAction::Stop, "Stop" },
	std::pair{ TimerAction::Reset, "Reset" },
	std::pair{ TimerAction::Pause, "Pause" },
	std::pair{ TimerAction::Resume, "Resume" },
	std::pair{ TimerAction::TogglePaused, "Toggle Paused" },
	std::pair{ TimerAction::Advance, "Advance" },
	std::pair{ TimerAction::Rewind, "Rewind" },
	std::pair{ TimerAction::SetDuration, "Set Duration" },
	std::pair{ TimerAction::AddDuration, "Add Duration" },
	std::pair{ TimerAction::RemoveDuration, "Remove Duration" },
};

inline constexpr std::array kSceneActions{
	std::pair{ SceneChangeAction::Enter, "Enter" },
	std::pair{ SceneChangeAction::Exit, "Exit" },
	std::pair{ SceneChangeAction::Switch, "Switch" },
	std::pair{ SceneChangeAction::ReEnter, "Re-enter" },
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
	const auto it{ value.find("enabled") };
	return it != value.end() && it->is_boolean();
}

[[nodiscard]] const char* AnimationActionLabel(AnimationAction action) {
	const auto it{ std::ranges::find_if(kAnimationActions, [action](const auto& entry) {
		return entry.first == action;
	}) };
	return it != kAnimationActions.end() ? it->second : "Animation";
}


[[nodiscard]] const char* TimerActionLabel(TimerAction action) {
	const auto it{ std::ranges::find_if(kTimerActions, [action](const auto& entry) {
		return entry.first == action;
	}) };
	return it != kTimerActions.end() ? it->second : "Timer";
}

[[nodiscard]] bool TimerActionUsesAmount(TimerAction action) {
	switch (action) {
		case TimerAction::Advance:
		case TimerAction::Rewind:
		case TimerAction::SetDuration:
		case TimerAction::AddDuration:
		case TimerAction::RemoveDuration:
			return true;
		case TimerAction::Start:
		case TimerAction::Restart:
		case TimerAction::Stop:
		case TimerAction::Reset:
		case TimerAction::Pause:
		case TimerAction::Resume:
		case TimerAction::TogglePaused:
			return false;
	}
	return false;
}

[[nodiscard]] std::vector<TimerKey> GetTimerActionChoices(
	ScriptEditorContext& context
) {
	if (!context.owner) {
		return {};
	}

	std::vector<Entity> targets;
	if (context.sequence_target_filter && context.sequence_target_filter->has_value()) {
		targets = ResolveEntityFilter(
			context.sequence_target_filter->value(),
			context.owner.GetScene(),
			context.owner
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
			if (entry.config.key.value.empty() ||
				std::ranges::contains(result, entry.config.key)) {
				continue;
			}
			result.push_back(entry.config.key);
		}
	}

	return result;
}

bool DrawTimerKeyInline(
	const char* id,
	TimerKey& timer,
	const std::vector<TimerKey>& choices,
	float width
) {
	bool changed{ false };
	ImGui::SetNextItemWidth(width);

	const char* preview{
		timer.value.empty()
			? "No Timer"
			: timer.value.c_str()
	};

	if (ImGui::BeginCombo(id, preview)) {
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

bool DrawTimerActionInline(
	ScriptEditorContext& context,
	TimerActionScript& script
) {
	const auto choices{ GetTimerActionChoices(context) };
	float available{ ImGui::GetContentRegionAvail().x };
	float spacing{ ImGui::GetStyle().ItemSpacing.x };
	bool uses_amount{ TimerActionUsesAmount(script.action) };
	float action_width{
		std::min(130.0f, std::max(95.0f, available * 0.32f))
	};
	float amount_width{
		std::min(100.0f, std::max(72.0f, available * 0.25f))
	};
	float timer_width{
		std::max(
			1.0f,
			available - action_width - (uses_amount ? amount_width + spacing : 0.0f) - spacing
		)
	};

	bool changed{ DrawTimerKeyInline("##TimerKey", script.timer, choices, timer_width) };
	SameLineControl();

	ImGui::SetNextItemWidth(action_width);
	if (ImGui::BeginCombo("##TimerAction", TimerActionLabel(script.action))) {
		for (const auto& [candidate, label] : kTimerActions) {
			if (ImGui::Selectable(label, candidate == script.action)) {
				script.action = candidate;
				changed = true;
			}
		}
		ImGui::EndCombo();
	}
	DrawItemTooltip("Timer operation to perform.");

	if (TimerActionUsesAmount(script.action)) {
		SameLineControl();
		changed |= inspector::DrawDurationTextInput(
			"##TimerAmount",
			script.amount,
			amount_width,
			false,
			"Time amount used by this timer operation."
		);
	}

	return changed;
}

[[nodiscard]] const char* SceneActionLabel(SceneChangeAction action) {
	const auto it{ std::ranges::find_if(kSceneActions, [action](const auto& entry) {
		return entry.first == action;
	}) };
	return it != kSceneActions.end() ? it->second : "Scene";
}

[[nodiscard]] std::size_t GetAnimationActionFrameCount(
	const ScriptEditorContext& context
) {
	if (!context.owner) {
		return 0;
	}

	if (context.owner.Has<
			::ptgn::impl::AnimationData
		>()) {
		return context.owner
			.Get<::ptgn::impl::AnimationData>()
			.config
			.frame_count;
	}

	if (!context.owner.Has<
			::ptgn::impl::AnimationMapData
		>()) {
		return 0;
	}

	const auto active{
		AnimationMap{
			context.owner
		}.GetActive()
	};

	return active.has_value()
		? active->GetFrameCount()
		: 0;
}

bool DrawAnimationActionInline(
	ScriptEditorContext& context,
	AnimationActionScript& script
) {
	const float available{
		ImGui::GetContentRegionAvail().x
	};

	const float spacing{
		ImGui::GetStyle().ItemSpacing.x
	};

	const bool shows_force{
		script.action ==
		AnimationAction::Start
	};

	const bool shows_reset{
		script.action ==
		AnimationAction::Stop
	};

	const bool shows_frame{
		script.action ==
		AnimationAction::SetFrame
	};

	const bool has_inline_value{
		shows_force ||
		shows_reset ||
		shows_frame
	};

	float value_width{ 0.0f };

	if (shows_force) {
		value_width =
			ImGui::GetFrameHeight() +
			ImGui::GetStyle()
				.ItemInnerSpacing.x +
			ImGui::CalcTextSize(
				"Force"
			).x;
	} else if (shows_reset) {
		value_width =
			ImGui::GetFrameHeight() +
			ImGui::GetStyle()
				.ItemInnerSpacing.x +
			ImGui::CalcTextSize(
				"Reset"
			).x;
	} else if (shows_frame) {
		value_width =
			std::min(
				110.0f,
				available * 0.4f
			);
	}

	const float action_width{
		has_inline_value
			? std::max(
				100.0f,
				available -
					value_width -
					spacing
			)
			: available
	};

	bool changed{ false };

	ImGui::SetNextItemWidth(
		action_width
	);

	if (ImGui::BeginCombo(
			"##AnimationAction",
			AnimationActionLabel(
				script.action
			)
		)) {
		for (const auto& [
				 candidate,
				 label
			 ] : kAnimationActions) {
			if (ImGui::Selectable(
					label,
					candidate ==
						script.action
				)) {
				script.action =
					candidate;

				changed = true;
			}
		}

		ImGui::EndCombo();
	}

	DrawItemTooltip(
		"Animation operation to perform."
	);

	if (!has_inline_value) {
		return changed;
	}

	SameLineControl();

	if (shows_force) {
		changed |= ImGui::Checkbox(
			"Force##AnimationForce",
			&script.force
		);

		DrawItemTooltip(
			"Restart the animation even if it is already playing."
		);

		return changed;
	}

	if (shows_reset) {
		changed |= ImGui::Checkbox(
			"Reset##AnimationStopReset",
			&script.reset_on_stop
		);

		DrawItemTooltip(
			"Reset to the first frame when stopping."
		);

		return changed;
	}

	const std::size_t frame_count{
		GetAnimationActionFrameCount(
			context
		)
	};

	const int maximum_frame{
		frame_count > 0
			? static_cast<int>(
				std::min<std::size_t>(
					frame_count - 1,
					static_cast<
						std::size_t
					>(
						std::numeric_limits<
							int
						>::max()
					)
				)
			)
			: 0
	};

	const std::size_t clamped_frame{
		std::min(
			script.frame,
			static_cast<std::size_t>(
				maximum_frame
			)
		)
	};

	if (script.frame != clamped_frame) {
		script.frame =
			clamped_frame;

		changed = true;
	}

	int frame{
		static_cast<int>(
			script.frame
		)
	};

	ImGui::SetNextItemWidth(
		std::max(
			1.0f,
			available -
				action_width -
				spacing
		)
	);

	ImGui::BeginDisabled(
		frame_count == 0
	);

	if (ImGui::DragInt(
			"##AnimationFrame",
			&frame,
			1.0f,
			0,
			maximum_frame,
			"Frame: %d",
			ImGuiSliderFlags_AlwaysClamp
		)) {
		script.frame =
			static_cast<std::size_t>(
				frame
			);

		changed = true;
	}

	ImGui::EndDisabled();

	DrawItemTooltip(
		frame_count > 0
			? "Animation frame to select."
			: "The owner has no configured animation frames."
	);

	return changed;
}

bool DrawSetTextureInline(
	ScriptEditorContext& context,
	SetTextureScript& script
) {
	ImGui::SetNextItemWidth(-FLT_MIN);

	return inspector::DrawAssetKeyInline(
		context.ctx,
		script.texture_key,
		inspector::FieldOptions{},
		"Texture key"
	);
}

bool DrawSetEnabledInline(ScriptEditorContext&, SetEnabledScript& script) {
	auto components{ GetSortedComponents([](const RegisteredComponent& component) {
		return IsEnabledComponent(component);
	}) };

	const float available{ ImGui::GetContentRegionAvail().x };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float value_width{
		ImGui::CalcTextSize("Disabled").x +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};

	bool changed{ false };
	ImGui::SetNextItemWidth(std::max(1.0f, available - value_width - spacing));

	const auto* selected{ ComponentRegistry::Find(script.component) };
	const std::string preview{
		selected ? ComponentLabel(*selected) : script.component
	};

	if (ImGui::BeginCombo(
			"##EnabledComponent",
			preview.empty() ? "Component" : preview.c_str()
		)) {
		for (const auto* component : components) {
			const std::string label{ ComponentLabel(*component) };

			if (ImGui::Selectable(
					label.c_str(),
					script.component == component->name
				)) {
				script.component = component->name;
				changed = true;
			}
		}

		ImGui::EndCombo();
	}

	DrawItemTooltip("Registered component whose enabled value is changed.");
	SameLineControl();

	if (ImGui::Button(
			script.enabled ? "Enabled" : "Disabled",
			ImVec2{ value_width, ImGui::GetFrameHeight() }
		)) {
		script.enabled = !script.enabled;
		changed = true;
	}

	DrawItemTooltip("Toggle the value assigned by this action.");
	return changed;
}

struct ProjectSceneChoice {
	std::string key{};
	std::string label{};
};

[[nodiscard]] std::vector<ProjectSceneChoice>
GetProjectSceneChoices(
	ScriptEditorContext& context
) {
	std::vector<ProjectSceneChoice> choices;

	const auto* project{
		context.ctx.editor.GetProject()
	};

	if (!project) {
		return choices;
	}

	choices.reserve(project->scenes.size());

	for (const auto& entry :
		 project->scenes) {
		choices.emplace_back(
			ProjectSceneChoice{
				.key = entry.key,
				.label =
					entry.display_name +
					" [" + entry.key + "]",
			}
		);
	}

	return choices;
}

[[nodiscard]] std::string
GetProjectScenePreview(
	ScriptEditorContext& context,
	const SceneChangeScript& script
) {
	const auto choices{
		GetProjectSceneChoices(context)
	};

	const auto it{
		std::ranges::find_if(
			choices,
			[&script](
				const ProjectSceneChoice& choice
			) {
				return choice.key ==
					   script.scene_key;
			}
		)
	};

	return it == choices.end()
		? std::string{ "Select Project Scene" }
		: it->label;
}

bool DrawProjectSceneCombo(
	const char* label,
	ScriptEditorContext& context,
	SceneChangeScript& script
) {
	const auto choices{
		GetProjectSceneChoices(context)
	};
	const std::string preview{
		GetProjectScenePreview(
			context,
			script
		)
	};

	bool changed{ false };

	if (!ImGui::BeginCombo(
			label,
			preview.c_str()
		)) {
		return false;
	}

	if (choices.empty()) {
		ImGui::TextDisabled(
			"No project scenes"
		);
	}

	for (const auto& choice :
		 choices) {
		if (ImGui::Selectable(
				choice.label.c_str(),
				script.scene_key ==
					choice.key
			)) {
			script.scene_key =
				choice.key;
			changed = true;
		}
	}

	ImGui::EndCombo();
	return changed;
}

bool DrawSceneChangeActionAndKey(
	ScriptEditorContext& context,
	SceneChangeScript& script
) {
	const float available{
		ImGui::GetContentRegionAvail().x
	};
	const float spacing{
		ImGui::GetStyle().ItemSpacing.x
	};
	const float action_width{
		std::max(
			90.0f,
			available * 0.35f
		)
	};

	bool changed{ false };

	ImGui::SetNextItemWidth(
		action_width
	);

	if (ImGui::BeginCombo(
			"##SceneAction",
			SceneActionLabel(script.action)
		)) {
		for (const auto& [candidate, label] :
			 kSceneActions) {
			if (ImGui::Selectable(
					label,
					candidate ==
						script.action
				)) {
				script.action =
					candidate;
				changed = true;
			}
		}

		ImGui::EndCombo();
	}

	SameLineControl();

	ImGui::SetNextItemWidth(
		std::max(
			1.0f,
			available -
				action_width -
				spacing
		)
	);

	changed |= DrawProjectSceneCombo(
		"##ProjectSceneKey",
		context,
		script
	);

	return changed;
}

bool DrawSceneChangeInline(
	ScriptEditorContext& context,
	SceneChangeScript& script
) {
	return DrawSceneChangeActionAndKey(
		context,
		script
	);
}

bool DrawSceneChange(
	ScriptEditorContext& context,
	SceneChangeScript& script
) {
	bool changed{
		DrawSceneChangeActionAndKey(
			context,
			script
		)
	};

	changed |= DrawNamedEnumCombo(
		"Transition",
		script.transition,
		kSceneTransitions,
		"Fade is sequential for switches; Cross Fade overlaps both scenes."
	);

	if (script.transition !=
		SceneTransitionStyle::None) {
		changed |= inspector::DrawPropertyRow("Duration", [&]() {
			return inspector::DrawDurationInput(
				"##SceneTransitionDuration",
				script.duration_ms,
				-FLT_MIN,
				"Transition duration."
			);
		});
		changed |= inspector::DrawPropertyRow("Delay", [&]() {
			return inspector::DrawDurationInput(
				"##SceneTransitionDelay",
				script.delay_ms,
				-FLT_MIN,
				"Delay before the transition begins."
			);
		});

		if (ImGui::BeginCombo(
				"Ease",
				std::string{
					magic_enum::enum_name(
						script.ease
					)
				}.c_str()
			)) {
			for (const auto candidate :
				 magic_enum::enum_values<Ease>()) {
				const std::string label{
					magic_enum::enum_name(
						candidate
					)
				};

				if (ImGui::Selectable(
						label.c_str(),
						candidate ==
							script.ease
					)) {
					script.ease =
						candidate;
					changed = true;
				}
			}

			ImGui::EndCombo();
		}

		if (script.transition ==
			SceneTransitionStyle::Slide) {
			changed |= ImGui::DragFloat2(
				"Exit Direction",
				&script.direction.x,
				0.05f
			);
			DrawItemTooltip(
				"Direction the old scene exits. The new scene enters from the opposite direction."
			);
		}
	}

	int priority{
		static_cast<int>(
			script.priority
		)
	};

	if (ImGui::DragInt(
			"Priority",
			&priority,
			1.0f,
			0
		)) {
		script.priority =
			static_cast<std::size_t>(
				std::max(
					0,
					priority
				)
			);
		changed = true;
	}

	return changed;
}

bool DrawEmitSignalInline(ScriptEditorContext&, EmitSignalScript& script) {
	ImGui::SetNextItemWidth(-FLT_MIN);
	const bool changed{
		ImGui::InputTextWithHint("##SignalName", "Signal name", &script.signal.value)
	};
	DrawItemTooltip("Signal name to broadcast.");
	return changed;
}

bool DrawAddComponentsInline(ScriptEditorContext&, AddComponentsScript& script) {
	std::vector<std::string> selected_labels;
	std::string preview;

	for (const auto& definition : script.components) {
		const auto* component{ ComponentRegistry::Find(definition.type) };
		const std::string label{
			component ? ComponentLabel(*component) : definition.type
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

	auto components{ GetSortedComponents([](const RegisteredComponent& component) {
		return CanAddComponentDefinition(component);
	}) };

	bool changed{ false };
	ImGui::SetNextItemWidth(-FLT_MIN);

	if (ImGui::BeginCombo("##AddComponents", preview.c_str())) {
		for (const auto* component : components) {
			bool selected{ std::ranges::any_of(
				script.components,
				[component](const ComponentDefinition& definition) {
					return definition.type == component->name;
				}
			) };

			const std::string label{ ComponentLabel(*component) };

			if (ImGui::Checkbox(label.c_str(), &selected)) {
				if (selected) {
					script.components.push_back(MakeComponentDefinition(*component));
				} else {
					std::erase_if(
						script.components,
						[component](const ComponentDefinition& definition) {
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
					ImGui::SetTooltip(
						"%s",
						component->name.c_str()
					);
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
		const std::string label{
			component ? ComponentLabel(*component) : definition.type
		};

		ImGui::PushID(i);

		if (ImGui::BeginTable(
				"ComponentTitle",
				2,
				ImGuiTableFlags_SizingStretchProp
			)) {
			ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(
				"Remove",
				ImGuiTableColumnFlags_WidthFixed,
				ImGui::GetFrameHeight()
			);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
			ImGui::TableSetColumnIndex(0);
			ImGui::SeparatorText(label.c_str());

			if (component && component->is_empty) {
				DrawItemTooltip("Tag component");
			}

			ImGui::TableSetColumnIndex(1);
			if (ImGui::Button(
					"x",
					ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }
				)) {
				remove = i;
			}

			ImGui::EndTable();
		}

		if (component && !component->is_empty) {
			if (DrawRegisteredComponentJson(*component, definition.value)) {
				definition.apply_live = {};
				changed = true;
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
		const std::string label{
			component ? ComponentLabel(*component) : name
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

	auto components{ GetSortedComponents([](const RegisteredComponent&) {
		return true;
	}) };

	bool changed{ false };
	ImGui::SetNextItemWidth(-FLT_MIN);

	if (ImGui::BeginCombo("##RemoveComponents", preview.c_str())) {
		for (const auto* component : components) {
			bool selected{
				std::ranges::contains(script.components, component->name)
			};

			const std::string label{ ComponentLabel(*component) };

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
	bool changed{ ImGui::DragFloat2("Destination", &script.destination.x, 0.1f) };
	changed |= ImGui::Checkbox("Relative", &script.relative);
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
		context.sequence_target_filter->value(),
		context.owner.GetScene(),
		context.owner
	);
}

bool DrawFollowEntityPicker(
	ScriptEditorContext& context,
	UUID& target,
	const char* id,
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
		target = {};
		selected = {};
		changed = true;
	}

	EntityFilter filter;
	filter.type = EntityFilterType::Entity;

	if (selected) {
		SetEntityReference(filter.entity, selected);
	}

	inspector::EntityFilterEditorOptions options{
		.show_any = false,
		.show_entity = true,
		.show_components = false,
		.show_groups = false,
		.show_queries = false,
		.allow_select_owner = false,
		.exclude_owner = false,
		.excluded_entities = std::span<const Entity>{ excluded_entities },
	};

	ImGui::PushID(id);

	if (inspector::DrawEntityFilterButton(
			std::addressof(scene),
			context.owner,
			filter,
			state,
			options
		)) {
		if (filter.entity.uuid.has_value()) {
			target = filter.entity.uuid.value();
			changed = true;
		}
	}

	ImGui::PopID();
	return changed;
}

bool DrawFollowTarget(ScriptEditorContext& context, FollowTargetScript& script) {
	static inspector::EntityFilterEditorState state;

	bool changed{ DrawFollowEntityPicker(
		context,
		script.target,
		"FollowTargetTarget",
		state
	) };
	changed |= ImGui::DragFloat("Speed", &script.speed, 1.0f, 0.0f);
	changed |= ImGui::DragFloat("Stopping Distance", &script.stopping_distance, 0.1f, 0.0f);
	return changed;
}

bool DrawTintToInline(ScriptEditorContext&, TintToScript& script) {
	auto tint{ script.tint.Normalized() };

	ImGui::SetNextItemWidth(ImGui::GetFrameHeight());

	if (!ImGui::ColorEdit4(
			"##Tint",
			tint.Data(),
			ImGuiColorEditFlags_NoInputs
		)) {
		return false;
	}

	script.tint = Color{ tint };
	return true;
}

bool DrawTintTo(ScriptEditorContext&, TintToScript& script) {
	auto tint{ script.tint.Normalized() };
	if (!ImGui::ColorEdit4("Tint", tint.Data())) {
		return false;
	}
	script.tint = Color{ tint };
	return true;
}

bool DrawBounce(ScriptEditorContext&, BounceScript& script) {
	bool changed{ ImGui::DragFloat2("Amplitude", &script.amplitude.x, 0.1f) };
	changed |= ImGui::DragFloat2("Static Offset", &script.static_offset.x, 0.1f);
	changed |= ImGui::Checkbox("Symmetrical", &script.symmetrical);
	return changed;
}

bool DrawShake(ScriptEditorContext& context, ShakeScript& script) {
	bool changed{ ImGui::DragFloat("Intensity", &script.intensity, 0.01f, -1.0f, 1.0f) };
	changed |= ImGui::Checkbox("Reset On Complete", &script.reset_on_complete);
	changed |= DrawReflectedScriptValue(context.ctx, script.config);
	return changed;
}

bool DrawAddShakeTrauma(ScriptEditorContext& context, AddShakeTraumaScript& script) {
	bool changed{ ImGui::DragFloat("Intensity", &script.intensity, 0.01f, -1.0f, 1.0f) };
	changed |= DrawReflectedScriptValue(context.ctx, script.config);
	return changed;
}

bool DrawRecoverShake(ScriptEditorContext& context, RecoverShakeScript& script) {
	return DrawReflectedScriptValue(context.ctx, script.config);
}

bool DrawFollowEntity(ScriptEditorContext& context, FollowEntityScript& script) {
	static inspector::EntityFilterEditorState state;

	bool changed{ DrawFollowEntityPicker(
		context,
		script.target,
		"FollowEntityTarget",
		state
	) };
	changed |= DrawReflectedScriptValue(context.ctx, script.config);
	return changed;
}

bool DrawFollowPath(ScriptEditorContext& context, FollowPathScript& script) {
	bool changed{ false };
	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(script.waypoints.size()); ++i) {
		ImGui::PushID(i);
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
		changed |=
			ImGui::DragFloat2("##Waypoint", &script.waypoints[static_cast<std::size_t>(i)].x, 0.1f);
		ImGui::SameLine();
		if (ImGui::Button("x", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() })) {
			remove = i;
		}
		ImGui::PopID();
	}
	if (remove >= 0) {
		script.waypoints.erase(script.waypoints.begin() + remove);
		changed = true;
	}
	if (ImGui::Button("+ Waypoint", ImVec2{ -FLT_MIN, 0.0f })) {
		script.waypoints.emplace_back();
		changed = true;
	}
	changed |= ImGui::Checkbox("Reset Waypoint Index", &script.reset_waypoint_index);
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
					  .group	   = "Transform",
					  .description = "Move the owning entity.",
					  .type		   = ScriptType::Both,
					  .draw_inline = &DrawMoveToInline,
					  .draw		   = &DrawMoveTo,
				  }
);

PTGN_REGISTER_SCRIPT(
	RotateToScript, {
						.label		 = "Rotate To",
						.group		 = "Transform",
						.description = "Rotate the owning entity.",
						.type		 = ScriptType::Sequence,
						.draw_inline = &DrawRotateToInline,
					}
);

PTGN_REGISTER_SCRIPT(
	ScaleToScript, {
					   .label		= "Scale To",
					   .group		= "Transform",
					   .description = "Scale the owning entity.",
					   .type		= ScriptType::Sequence,
					   .draw_inline = &DrawScaleToInline,
				   }
);

PTGN_REGISTER_SCRIPT(
	TintToScript, {
					  .label	   = "Tint To",
					  .group	   = "Animation",
					  .description = "Animate the target tint to a color.",
					  .type		   = ScriptType::Sequence,
					  .draw_inline = &DrawTintToInline,
					  .draw		   = &DrawTintTo,
				  }
);

PTGN_REGISTER_SCRIPT(
	BounceScript, {
					  .label	   = "Bounce",
					  .group	   = "Animation",
					  .description = "Apply a positional bounce offset.",
					  .type		   = ScriptType::Sequence,
					  .draw		   = &DrawBounce,
				  }
);

PTGN_REGISTER_SCRIPT(
	ShakeScript, {
					 .label		  = "Shake",
					 .group		  = "Animation",
					 .description = "Raise or lower persistent shake trauma.",
					 .type		  = ScriptType::Sequence,
					 .draw		  = &DrawShake,
				 }
);

PTGN_REGISTER_SCRIPT(
	AddShakeTraumaScript, {
							  .label	   = "Add Shake Trauma",
							  .group	   = "Animation",
							  .description = "Immediately raise or lower persistent shake trauma.",
							  .type		   = ScriptType::Sequence,
							  .draw		   = &DrawAddShakeTrauma,
						  }
);

PTGN_REGISTER_SCRIPT(
	RecoverShakeScript, {
							.label		 = "Recover Shake",
							.group		 = "Animation",
							.description = "Reduce shake trauma to zero.",
							.type		 = ScriptType::Sequence,
							.draw		 = &DrawRecoverShake,
						}
);

PTGN_REGISTER_SCRIPT(
	ResetShakeScript, {
						  .label	   = "Reset Shake",
						  .group	   = "Animation",
						  .description = "Immediately clear shake trauma and offsets.",
						  .type		   = ScriptType::Sequence,
					  }
);

PTGN_REGISTER_SCRIPT(
	FollowTargetScript, {
							.label		 = "Follow Target",
							.group		 = "Transform",
							.description = "Follow an entity until close enough.",
							.type		 = ScriptType::Both,
							.draw		 = &DrawFollowTarget,
						}
);

PTGN_REGISTER_SCRIPT(
	FollowEntityScript,
	{
		.label = "Follow Entity",
		.group = "Transform",
		.description =
			"Follow an entity using TargetFollowConfig.",
		.type = ScriptType::Both,
		.draw =
			&DrawFollowEntity,
	}
);

PTGN_REGISTER_SCRIPT(
	FollowPathScript,
	{
		.label = "Follow Path",
		.group = "Transform",
		.description =
			"Follow a configurable waypoint path.",
		.type = ScriptType::Both,
		.draw =
			&DrawFollowPath,
	}
);

PTGN_REGISTER_SCRIPT(
	SetVisibleScript, {
						  .label	   = "Set Visibility",
						  .group	   = "Entity",
						  .description = "Set owner visibility.",
						  .type		   = ScriptType::Sequence,
						  .menu_order  = 3,
						  .draw_inline = &DrawSetVisibleInline,
					  }
);

PTGN_REGISTER_SCRIPT(
	PlaySoundScript,
	{
		.label = "Play Audio",
		.group = "Other",
		.description =
			"Play an audio asset.",
		.type = ScriptType::Sequence,
		.draw_inline =
			&DrawPlaySoundInline,
		.draw =
			&DrawPlaySound,
	}
);

PTGN_REGISTER_SCRIPT(
	AnimationActionScript,
	{
		.label = "Animation Action",
		.group = "Animation",
		.description =
			"Start, stop, pause, resume, or change an animation frame.",
		.type = ScriptType::Sequence,
		.menu_order = 1,
		.draw_inline = &DrawAnimationActionInline,
	}
);

PTGN_REGISTER_SCRIPT(
	TimerActionScript,
	{
		.label = "Timer Action",
		.group = "Timing",
		.description = "Control a named timer on the action target.",
		.type = ScriptType::Sequence,
		.draw_inline = &DrawTimerActionInline,
	}
);

namespace {

[[maybe_unused]] const bool kTimerActionRuntimeRegistered{
	ScriptRegistry::Register<TimerActionScript>(
		ScriptRegistrationOptions{
			.completion = ScriptCompletion::Instant,
		}
	)
};

} // namespace

PTGN_REGISTER_SCRIPT(
	SetTextureScript,
	{
		.label = "Set Texture",
		.group = "Animation",
		.description =
			"Assign a texture asset key to the owner.",
		.type = ScriptType::Sequence,
		.draw_inline =
			&DrawSetTextureInline,
	}
);

PTGN_REGISTER_SCRIPT(
	SetEnabledScript,
	{
		.label		 = "Set Enabled",
		.group		 = "Entity",
		.description = "Set the enabled value of a supported registered component.",
		.type		 = ScriptType::Sequence,
		.menu_order	 = 4,
		.draw_inline = &DrawSetEnabledInline,
	}
);

PTGN_REGISTER_SCRIPT(
	SceneChangeScript,
	{
		.label = "Change Scene",
		.group = "Other",
		.description =
			"Enter, exit, switch, or re-enter a registered scene.",
		.type = ScriptType::Sequence,
		.draw_inline =
			&DrawSceneChangeInline,
		.draw =
			&DrawSceneChange,
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
							 .group		  = "Entity",
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
								.group			 = "Entity",
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

	event::ButtonPress,

	{

		.label = "On Button Press",

		.group = "Button",

		.description = "Matches when the owner emits ButtonPress.",

	}

);

PTGN_REGISTER_EVENT(

	event::ButtonHoverStart,

	{

		.label = "On Button Hover Start",

		.group = "Button",

		.description = "Matches when the pointer starts hovering over the owner button.",

	}

);

PTGN_REGISTER_EVENT(

	event::ButtonHover,

	{

		.label = "On Button Hover",

		.group = "Button",

		.description = "Matches while the pointer remains over the owner button.",

	}

);

PTGN_REGISTER_EVENT(

	event::ButtonHoverStop,

	{

		.label = "On Button Hover Stop",

		.group = "Button",

		.description = "Matches when the pointer stops hovering over the owner button.",

	}

);

PTGN_REGISTER_EVENT(

	event::ToggleButtonToggle,

	{

		.label = "On Toggle",

		.group = "Toggle Button",

		.description = "Matches when the owner toggle button changes state.",

	}

);

PTGN_REGISTER_EVENT(

	event::DropdownOpen,

	{

		.label = "On Dropdown Open",

		.group = "Dropdown",

		.description = "Matches when the owner dropdown opens.",

	}

);

PTGN_REGISTER_EVENT(

	event::DropdownClose,

	{

		.label = "On Dropdown Close",

		.group = "Dropdown",

		.description = "Matches when the owner dropdown closes.",

	}

);

PTGN_REGISTER_EVENT(

	event::DropdownToggle,

	{

		.label = "On Dropdown Toggle",

		.group = "Dropdown",

		.description = "Matches whenever the owner dropdown opens or closes.",

	}

);

PTGN_REGISTER_EVENT(

	event::DropdownItemPress,

	{

		.label = "On Dropdown Item Press",

		.group = "Dropdown",

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
							  .group	   = "Animation",
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
	event::AnimationFinalFrame, {
								  .label	   = "On Animation Final Frame",
								  .group	   = "Animation",
								  .description = "Matches whenever the final frame of an animation plays.",
							  }
);

PTGN_REGISTER_EVENT(
	event::AnimationComplete, {
								  .label	   = "On Animation Complete",
								  .group	   = "Animation",
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
		.description = "Matches when a named timer reaches its configured or overridden elapsed duration.",
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
	event::EntityCreated,
	{
		.label = "On Create",
		.group = "",
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
