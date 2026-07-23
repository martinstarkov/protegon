#include "scripting/script_editor_registry.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#ifndef MAGIC_ENUM_RANGE_MAX
#define MAGIC_ENUM_RANGE_MAX 512
#endif
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cfloat>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/event/key_event.h"
#include "core/event/mouse_event.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "panels/component_editor_registry.h"
#include "panels/inspector_fields.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/interaction/draggable_event.h"
#include "runtime/interaction/dropzone_event.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/physics/collision_event.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/ui/button.h"
#include "scripting/script_registration_editor.h"

namespace ptgn::editor {

namespace {

void DrawItemTooltip(const char* text) {
	if (text && *text && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", text);
	}
}

void SameLineControl() {
	ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
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

[[nodiscard]] const RegisteredComponentEditor* FindComponentEditor(
	const RegisteredComponent& component
) {
	return ComponentEditorRegistry::Find(component.type_id);
}

[[nodiscard]] ResolvedComponentEditorOptions ResolveComponentEditor(
	const RegisteredComponent& component
) {
	const auto* editor{ FindComponentEditor(component) };
	if (!editor) {
		return ResolvedComponentEditorOptions{
			.label = std::string{ component.name },
			.group = component.is_empty ? std::string{ kTagComponentGroup } : std::string{},
		};
	}
	return ComponentEditorRegistry::Resolve(component, *editor);
}

[[nodiscard]] bool HasComponentJsonEditor(const RegisteredComponent& component) {
	const auto* editor{ FindComponentEditor(component) };
	return editor && editor->draw_json;
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

bool DrawKey(json& value) {
	Key key{ JsonValueOr<Key>(value, "key", Key::W) };
	bool changed{ false };
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::BeginCombo("##Key", std::string{ magic_enum::enum_name(key) }.c_str())) {
		for (const auto candidate : magic_enum::enum_values<Key>()) {
			if (ImGui::Selectable(
					std::string{ magic_enum::enum_name(candidate) }.c_str(), candidate == key
				)) {
				key = candidate;
				changed = true;
			}
		}
		ImGui::EndCombo();
	}
	DrawItemTooltip("Key matched by this trigger.");
	if (changed) {
		value["key"] = key;
	}
	return changed;
}

bool DrawMouse(json& value) {
	Mouse mouse{ JsonValueOr<Mouse>(value, "button", Mouse::Left) };
	bool changed{ false };
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::BeginCombo("##Button", std::string{ magic_enum::enum_name(mouse) }.c_str())) {
		for (const auto candidate : magic_enum::enum_values<Mouse>()) {
			if (ImGui::Selectable(
					std::string{ magic_enum::enum_name(candidate) }.c_str(), candidate == mouse
				)) {
				mouse = candidate;
				changed = true;
			}
		}
		ImGui::EndCombo();
	}
	DrawItemTooltip("Mouse button matched by this trigger.");
	if (changed) {
		value["button"] = mouse;
	}
	return changed;
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
bool DrawNothing(T&, ScriptEditorContext&) {
	return false;
}

bool DrawScriptSequenceInline(Script& script, ScriptEditorContext& context) {
	const auto* selected{
		script.sequence.shared_reference
			? context.shared_sequences.Find(script.sequence.shared_sequence_id)
			: nullptr
	};
	const char* preview{ selected ? selected->name.c_str() : "Select Script Sequence" };
	bool changed{ false };
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::BeginCombo("##GlobalScriptSequence", preview)) {
		for (const auto& shared : context.shared_sequences.sequences) {
			const bool is_selected{
				script.sequence.shared_reference &&
				script.sequence.shared_sequence_id == shared.id
			};
			if (ImGui::Selectable(shared.name.c_str(), is_selected)) {
				script.sequence.name = shared.name;
				script.sequence.shared_reference = true;
				script.sequence.shared_sequence_id = shared.id;
				script.sequence.runtime = ScriptSequenceRuntime{};
				changed = true;
			}
		}
		ImGui::EndCombo();
	}
	DrawItemTooltip("Choose the global Script Sequence run by this action.");
	return changed;
}

bool DrawMoveToInline(MoveToScript& script, ScriptEditorContext&) {
	bool changed{ false };
	const float available{ ImGui::GetContentRegionAvail().x };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float mode_width{
		std::max(ImGui::CalcTextSize("Relative").x, ImGui::CalcTextSize("Absolute").x) +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const float field_width{
		std::max(36.0f, (available - mode_width - spacing * 2.0f) * 0.5f)
	};
	ImGui::SetNextItemWidth(field_width);
	changed |= ImGui::DragFloat(
		"##X", &script.destination.x, 1.0f, -100000.0f, 100000.0f, "X: %.0f"
	);
	SameLineControl();
	ImGui::SetNextItemWidth(field_width);
	changed |= ImGui::DragFloat(
		"##Y", &script.destination.y, 1.0f, -100000.0f, 100000.0f, "Y: %.0f"
	);
	SameLineControl();
	if (ImGui::Button(
			script.relative ? "Relative" : "Absolute",
			ImVec2{ mode_width, ImGui::GetFrameHeight() }
		)) {
		script.relative = !script.relative;
		changed = true;
	}
	DrawItemTooltip(
		script.relative ? "Offset from the entity's current position."
						: "Use an absolute world position."
	);
	return changed;
}

bool DrawRotateToInline(RotateToScript& script, ScriptEditorContext&) {
	const float available{ ImGui::GetContentRegionAvail().x };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float shortest_width{
		ImGui::CalcTextSize("Shortest").x + ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const float relative_width{
		ImGui::CalcTextSize("Relative").x + ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const float degrees_width{
		std::max(48.0f, available - shortest_width - relative_width - spacing * 2.0f)
	};
	bool changed{ false };
	ImGui::SetNextItemWidth(degrees_width);
	changed |= ImGui::DragFloat(
		"##Degrees", &script.degrees, 1.0f, -3600.0f, 3600.0f, "%.1f deg"
	);
	SameLineControl();
	changed |= DrawToggleButton(
		"Shortest", script.shortest_path,
		ImVec2{ shortest_width, ImGui::GetFrameHeight() },
		"Toggle the shortest rotational path."
	);
	SameLineControl();
	changed |= DrawToggleButton(
		"Relative", script.relative,
		ImVec2{ relative_width, ImGui::GetFrameHeight() },
		"Treat the angle as an offset from the current rotation."
	);
	return changed;
}

bool DrawScaleToInline(ScaleToScript& script, ScriptEditorContext&) {
	const float available{ ImGui::GetContentRegionAvail().x };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float mode_width{
		std::max(ImGui::CalcTextSize("Relative").x, ImGui::CalcTextSize("Absolute").x) +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};
	const float field_width{
		std::max(36.0f, (available - mode_width - spacing * 2.0f) * 0.5f)
	};
	bool changed{ false };
	ImGui::SetNextItemWidth(field_width);
	changed |= ImGui::DragFloat(
		"##ScaleX", &script.scale.x, 0.01f, -100.0f, 100.0f, "X: %.2f"
	);
	SameLineControl();
	ImGui::SetNextItemWidth(field_width);
	changed |= ImGui::DragFloat(
		"##ScaleY", &script.scale.y, 0.01f, -100.0f, 100.0f, "Y: %.2f"
	);
	SameLineControl();
	if (ImGui::Button(
			script.relative ? "Relative" : "Absolute",
			ImVec2{ mode_width, ImGui::GetFrameHeight() }
		)) {
		script.relative = !script.relative;
		changed = true;
	}
	DrawItemTooltip(
		script.relative ? "Multiply the entity's current scale."
						: "Use an absolute scale."
	);
	return changed;
}

bool DrawSetVisibleInline(SetVisibleScript& script, ScriptEditorContext&) {
	const char* preview{ script.visible ? "True" : "False" };
	bool changed{ false };
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::BeginCombo("##VisibleValue", preview)) {
		if (ImGui::Selectable("True", script.visible)) {
			script.visible = true;
			changed = true;
		}
		if (ImGui::Selectable("False", !script.visible)) {
			script.visible = false;
			changed = true;
		}
		ImGui::EndCombo();
	}
	DrawItemTooltip("Visibility value assigned by this action.");
	return changed;
}

bool DrawEmitSignalInline(EmitSignalScript& script, ScriptEditorContext&) {
	ImGui::SetNextItemWidth(-FLT_MIN);
	const bool changed{ ImGui::InputTextWithHint(
		"##SignalName", "Signal name", &script.signal.value
	) };
	DrawItemTooltip("Signal name to broadcast.");
	return changed;
}

bool DrawAddComponentsInline(AddComponentsScript& script, ScriptEditorContext&) {
	std::vector<std::string> selected_labels;
	std::string preview;
	for (const auto& definition : script.components) {
		const auto* component{ ComponentRegistry::Find(definition.type) };
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
		for (const auto& component : ComponentRegistry::Components()) {
			if (!component.make_default_json || !HasComponentJsonEditor(component)) {
				continue;
			}
			auto options{ ResolveComponentEditor(component) };
			if (!options.group.empty() && !std::ranges::contains(groups, options.group)) {
				groups.push_back(options.group);
			}
		}

		auto draw_component = [&](const RegisteredComponent& component) {
			if (!component.make_default_json || !HasComponentJsonEditor(component)) {
				return;
			}
			auto options{ ResolveComponentEditor(component) };
			bool selected{ std::ranges::any_of(
				script.components, [&](const ComponentDefinition& definition) {
					return definition.type == component.name;
				}
			) };
			if (ImGui::Checkbox(options.label.c_str(), &selected)) {
				if (selected) {
					script.components.push_back(MakeComponentDefinition(component));
				} else {
					std::erase_if(script.components, [&](const ComponentDefinition& definition) {
						return definition.type == component.name;
					});
				}
				changed = true;
			}
			if (ImGui::IsItemHovered()) {
				if (component.is_empty) {
					ImGui::SetTooltip("Tag component");
				} else {
					ImGui::SetTooltip(
						"%.*s", static_cast<int>(component.name.size()), component.name.data()
					);
				}
			}
		};

		for (const auto& component : ComponentRegistry::Components()) {
			if (component.make_default_json && HasComponentJsonEditor(component) &&
				ResolveComponentEditor(component).group.empty()) {
				draw_component(component);
			}
		}
		for (const auto& group : groups) {
			if (!ImGui::BeginMenu(group.c_str())) {
				continue;
			}
			for (const auto& component : ComponentRegistry::Components()) {
				if (component.make_default_json && HasComponentJsonEditor(component) &&
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
}

bool DrawAddComponentsDetails(AddComponentsScript& script, ScriptEditorContext&) {
	bool changed{ false };
	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(script.components.size()); ++i) {
		auto& definition{ script.components[static_cast<std::size_t>(i)] };
		const auto* component{ ComponentRegistry::Find(definition.type) };
		const std::string label{
			component ? ResolveComponentEditor(*component).label : definition.type
		};
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
			if (definition.value.is_null() && component->make_default_json) {
				definition.value = component->make_default_json();
			}
			if (definition.value.is_null()) {
				definition.value = json::object();
			}
			if (ComponentEditorRegistry::DrawJson(*component, definition.value)) {
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

bool DrawRemoveComponentsInline(RemoveComponentsScript& script, ScriptEditorContext&) {
	std::vector<std::string> selected_labels;
	std::string preview;
	for (const auto& name : script.components) {
		const auto* component{ ComponentRegistry::Find(name) };
		const std::string label{ component ? ResolveComponentEditor(*component).label : name };
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
		for (const auto& component : ComponentRegistry::Components()) {
			if (!FindComponentEditor(component)) {
				continue;
			}
			auto options{ ResolveComponentEditor(component) };
			if (!options.group.empty() && !std::ranges::contains(groups, options.group)) {
				groups.push_back(options.group);
			}
		}

		auto draw_component = [&](const RegisteredComponent& component) {
			if (!FindComponentEditor(component)) {
				return;
			}
			auto options{ ResolveComponentEditor(component) };
			bool selected{ std::ranges::contains(script.components, std::string{ component.name }) };
			if (ImGui::Checkbox(options.label.c_str(), &selected)) {
				if (selected) {
					script.components.emplace_back(component.name);
				} else {
					std::erase(script.components, std::string{ component.name });
				}
				changed = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"%.*s", static_cast<int>(component.name.size()), component.name.data()
				);
			}
		};

		for (const auto& component : ComponentRegistry::Components()) {
			if (FindComponentEditor(component) && ResolveComponentEditor(component).group.empty()) {
				draw_component(component);
			}
		}
		for (const auto& group : groups) {
			if (!ImGui::BeginMenu(group.c_str())) {
				continue;
			}
			for (const auto& component : ComponentRegistry::Components()) {
				if (FindComponentEditor(component) && ResolveComponentEditor(component).group == group) {
					draw_component(component);
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndCombo();
	}
	DrawSelectedItemsTooltip(selected_labels);
	return changed;
}

bool DrawMoveTo(MoveToScript& script, ScriptEditorContext&) {
	bool changed{ ImGui::DragFloat2("Destination", &script.destination.x, 0.1f) };
	changed |= ImGui::Checkbox("Relative", &script.relative);
	return changed;
}

bool DrawFollowTarget(FollowTargetScript& script, ScriptEditorContext&) {
	bool changed{ ImGui::DragFloat("Speed", &script.speed, 1.0f, 0.0f) };
	changed |= ImGui::DragFloat("Stopping Distance", &script.stopping_distance, 0.1f, 0.0f);
	ImGui::TextDisabled("Target selection should use your UUID/entity-reference field.");
	return changed;
}

bool DrawTintTo(TintToScript& script, ScriptEditorContext&) {
	auto tint{ script.tint.Normalized() };
	if (!ImGui::ColorEdit4("Tint", tint.Data())) {
		return false;
	}
	script.tint = Color{ tint };
	return true;
}

bool DrawBounce(BounceScript& script, ScriptEditorContext&) {
	bool changed{ ImGui::DragFloat2("Amplitude", &script.amplitude.x, 0.1f) };
	changed |= ImGui::DragFloat2("Static Offset", &script.static_offset.x, 0.1f);
	changed |= ImGui::Checkbox("Symmetrical", &script.symmetrical);
	return changed;
}

bool DrawShake(ShakeScript& script, ScriptEditorContext&) {
	bool changed{ ImGui::DragFloat("Intensity", &script.intensity, 0.01f, -1.0f, 1.0f) };
	changed |= ImGui::Checkbox("Reset On Complete", &script.reset_on_complete);
	changed |= inspector::DrawComponentContents(script.config);
	return changed;
}

bool DrawAddShakeTrauma(AddShakeTraumaScript& script, ScriptEditorContext&) {
	bool changed{ ImGui::DragFloat("Intensity", &script.intensity, 0.01f, -1.0f, 1.0f) };
	changed |= inspector::DrawComponentContents(script.config);
	return changed;
}

bool DrawRecoverShake(RecoverShakeScript& script, ScriptEditorContext&) {
	return inspector::DrawComponentContents(script.config);
}

bool DrawFollowEntity(FollowEntityScript& script, ScriptEditorContext&) {
	ImGui::TextDisabled("Target selection should use your UUID/entity-reference field.");
	return inspector::DrawComponentContents(script.config);
}

bool DrawFollowPath(FollowPathScript& script, ScriptEditorContext&) {
	bool changed{ false };
	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(script.waypoints.size()); ++i) {
		ImGui::PushID(i);
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
		changed |= ImGui::DragFloat2(
			"##Waypoint", &script.waypoints[static_cast<std::size_t>(i)].x, 0.1f
		);
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
	changed |= inspector::DrawComponentContents(script.config);
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
	Script,
	{
		.label = "Script Sequence",
		.group = "Sequence",
		.description = "Editor-authored sequence of registered scripts.",
		.type = ScriptType::Both,
		.draw_inline = &DrawScriptSequenceInline,
		.draw = &DrawNothing<Script>,
	}
);

PTGN_REGISTER_SCRIPT(
	WaitScript,
	{
		.label = "Delay",
		.group = "Timing",
		.description = "Wait before continuing.",
		.type = ScriptType::Sequence,
	}
);

PTGN_REGISTER_SCRIPT(
	MoveToScript,
	{
		.label = "Move To",
		.group = "Transform",
		.description = "Move the owning entity.",
		.type = ScriptType::Both,
		.draw_inline = &DrawMoveToInline,
		.draw = &DrawMoveTo,
	}
);

PTGN_REGISTER_SCRIPT(
	RotateToScript,
	{
		.label = "Rotate To",
		.group = "Transform",
		.description = "Rotate the owning entity.",
		.type = ScriptType::Sequence,
		.draw_inline = &DrawRotateToInline,
	}
);

PTGN_REGISTER_SCRIPT(
	ScaleToScript,
	{
		.label = "Scale To",
		.group = "Transform",
		.description = "Scale the owning entity.",
		.type = ScriptType::Sequence,
		.draw_inline = &DrawScaleToInline,
	}
);

PTGN_REGISTER_SCRIPT(
	TintToScript,
	{
		.label = "Tint To",
		.group = "Animation",
		.description = "Animate the owner tint to a color.",
		.type = ScriptType::Sequence,
		.draw = &DrawTintTo,
	}
);

PTGN_REGISTER_SCRIPT(
	BounceScript,
	{
		.label = "Bounce",
		.group = "Animation",
		.description = "Apply a positional bounce offset.",
		.type = ScriptType::Sequence,
		.draw = &DrawBounce,
	}
);

PTGN_REGISTER_SCRIPT(
	ShakeScript,
	{
		.label = "Shake",
		.group = "Animation",
		.description = "Raise or lower persistent shake trauma.",
		.type = ScriptType::Sequence,
		.draw = &DrawShake,
	}
);

PTGN_REGISTER_SCRIPT(
	AddShakeTraumaScript,
	{
		.label = "Add Shake Trauma",
		.group = "Animation",
		.description = "Immediately raise or lower persistent shake trauma.",
		.type = ScriptType::Sequence,
		.draw = &DrawAddShakeTrauma,
	}
);

PTGN_REGISTER_SCRIPT(
	RecoverShakeScript,
	{
		.label = "Recover Shake",
		.group = "Animation",
		.description = "Reduce shake trauma to zero.",
		.type = ScriptType::Sequence,
		.draw = &DrawRecoverShake,
	}
);

PTGN_REGISTER_SCRIPT(
	ResetShakeScript,
	{
		.label = "Reset Shake",
		.group = "Animation",
		.description = "Immediately clear shake trauma and offsets.",
		.type = ScriptType::Sequence,
	}
);

PTGN_REGISTER_SCRIPT(
	FollowTargetScript,
	{
		.label = "Follow Target",
		.group = "Transform",
		.description = "Follow an entity until close enough.",
		.type = ScriptType::Both,
		.draw = &DrawFollowTarget,
	}
);

PTGN_REGISTER_SCRIPT(
	FollowEntityScript,
	{
		.label = "Follow Entity",
		.group = "Movement",
		.description = "Follow an entity using TargetFollowConfig.",
		.type = ScriptType::Both,
		.draw = &DrawFollowEntity,
	}
);

PTGN_REGISTER_SCRIPT(
	FollowPathScript,
	{
		.label = "Follow Path",
		.group = "Movement",
		.description = "Follow a configurable waypoint path.",
		.type = ScriptType::Both,
		.draw = &DrawFollowPath,
	}
);

PTGN_REGISTER_SCRIPT(
	SetVisibleScript,
	{
		.label = "Set Visibility",
		.group = "Entity",
		.description = "Set owner visibility.",
		.type = ScriptType::Sequence,
		.menu_order = 3,
		.draw_inline = &DrawSetVisibleInline,
	}
);

PTGN_REGISTER_SCRIPT(
	EmitSignalScript,
	{
		.label = "Emit Signal",
		.group = "",
		.description = "Emit a global signal.",
		.type = ScriptType::Sequence,
		.draw_inline = &DrawEmitSignalInline,
	}
);

PTGN_REGISTER_SCRIPT(
	AddComponentsScript,
	{
		.label = "Add Components",
		.group = "Entity",
		.description = "Add registered components to the owner.",
		.type = ScriptType::Sequence,
		.menu_order = 1,
		.draw_inline = &DrawAddComponentsInline,
		.draw = &DrawAddComponentsDetails,
	}
);

PTGN_REGISTER_SCRIPT(
	RemoveComponentsScript,
	{
		.label = "Remove Components",
		.group = "Entity",
		.description = "Remove registered components from the owner.",
		.type = ScriptType::Sequence,
		.menu_order = 2,
		.separator_after = true,
		.draw_inline = &DrawRemoveComponentsInline,
	}
);

PTGN_REGISTER_EVENT(
	event::KeyPressed,
	{
		.label = "On Key Pressed",
		.group = "Key",
		.description = "Matches one key.",
		.inline_fields = 1,
		.draw = &DrawKey,
	}
);

PTGN_REGISTER_EVENT(
	event::KeyHeld,
	{
		.label = "On Key Held",
		.group = "Key",
		.description = "Matches one key.",
		.inline_fields = 1,
		.draw = &DrawKey,
	}
);

PTGN_REGISTER_EVENT(
	event::KeyReleased,
	{
		.label = "On Key Released",
		.group = "Key",
		.description = "Matches one key.",
		.inline_fields = 1,
		.draw = &DrawKey,
	}
);

PTGN_REGISTER_EVENT(
	event::MousePressed,
	{
		.label = "On Mouse Pressed",
		.group = "Mouse",
		.description = "Matches one mouse button.",
		.inline_fields = 1,
		.draw = &DrawMouse,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseHeld,
	{
		.label = "On Mouse Held",
		.group = "Mouse",
		.description = "Matches one mouse button.",
		.inline_fields = 1,
		.draw = &DrawMouse,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseReleased,
	{
		.label = "On Mouse Released",
		.group = "Mouse",
		.description = "Matches one mouse button.",
		.inline_fields = 1,
		.draw = &DrawMouse,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseMoveOver,
	{
		.label = "On Mouse Enter",
		.group = "Interaction",
		.description = "Matches when the pointer enters the owner.",
	}
);

PTGN_REGISTER_EVENT(
	event::MouseMoveOut,
	{
		.label = "On Mouse Leave",
		.group = "Interaction",
		.description = "Matches when the pointer leaves the owner.",
	}
);

PTGN_REGISTER_EVENT(
	event::MousePressedOver,
	{
		.label = "On Mouse Pressed Over",
		.group = "Interaction",
		.description = "Matches one mouse button.",
		.inline_fields = 1,
		.draw = &DrawMouse,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseHeldOver,
	{
		.label = "On Mouse Held Over",
		.group = "Interaction",
		.description = "Matches one mouse button.",
		.inline_fields = 1,
		.draw = &DrawMouse,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseReleasedOver,
	{
		.label = "On Mouse Released Over",
		.group = "Interaction",
		.description = "Matches one mouse button.",
		.inline_fields = 1,
		.draw = &DrawMouse,
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
	event::DragStart,
	{
		.label = "On Drag Start",
		.group = "Drag",
		.description = "Matches drag start.",
	}
);

PTGN_REGISTER_EVENT(
	event::Drag,
	{
		.label = "On Drag",
		.group = "Drag",
		.description = "Matches while dragging.",
	}
);

PTGN_REGISTER_EVENT(
	event::DragStop,
	{
		.label = "On Drag Stop",
		.group = "Drag",
		.description = "Matches drag stop.",
	}
);

PTGN_REGISTER_EVENT(
	event::OverlapStart,
	{
		.label = "On Overlap Start",
		.group = "Physics",
		.description = "Matches overlap start.",
	}
);

PTGN_REGISTER_EVENT(
	event::Overlap,
	{
		.label = "On Overlap",
		.group = "Physics",
		.description = "Matches overlap.",
	}
);

PTGN_REGISTER_EVENT(
	event::OverlapStop,
	{
		.label = "On Overlap Stop",
		.group = "Physics",
		.description = "Matches overlap stop.",
	}
);

PTGN_REGISTER_EVENT(
	event::Collision,
	{
		.label = "On Collision",
		.group = "Physics",
		.description = "Matches collision.",
	}
);

PTGN_REGISTER_EVENT(
	Signal,
	{
		.label = "On Signal",
		.group = "",
		.description = "Matches an exact signal name.",
		.inline_fields = 1,
		.draw = &DrawSignalEvent,
	}
);

namespace impl {

void EnsureEngineScriptEditorsRegistered() {
	// Intentionally empty.
	//
	// Referencing this function forces the linker to include this object file. The namespace-scope
	// PTGN_REGISTER_SCRIPT and PTGN_REGISTER_EVENT initializers then populate the editor registries.
}

} // namespace impl

} // namespace ptgn::editor
