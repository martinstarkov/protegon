#include "scripting/script_editor_registry.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#ifndef MAGIC_ENUM_RANGE_MAX
#define MAGIC_ENUM_RANGE_MAX 512
#endif
#include <magic_enum/magic_enum.hpp>

#include <cfloat>
#include <string>
#include <string_view>
#include <ranges>
#include <vector>
#include <utility>

#include "core/event/key_event.h"
#include "core/event/mouse_event.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "scripting/script_registration_editor.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/interaction/draggable_event.h"
#include "runtime/interaction/dropzone_event.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/physics/collision_event.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/ui/button.h"

namespace ptgn::editor::script {

const EventEditorRegistration* EventEditorRegistry::Find(TypeHashValue type_hash) {
	for (const auto& entry : Entries()) {
		if (entry.type_hash == type_hash) {
			return &entry;
		}
	}
	return nullptr;
}

const std::vector<EventEditorRegistration>& EventEditorRegistry::Entries() {
	impl::EnsureEngineScriptEditorsRegistered();
	return MutableEntries();
}

const SequenceStepEditorRegistration* SequenceStepEditorRegistry::Find(
	TypeHashValue type_hash
) {
	for (const auto& entry : Entries()) {
		if (entry.type_hash == type_hash) {
			return &entry;
		}
	}
	return nullptr;
}

const std::vector<SequenceStepEditorRegistration>& SequenceStepEditorRegistry::Entries() {
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

const std::vector<ScriptEditorRegistration>& ScriptEditorRegistry::Entries() {
	impl::EnsureEngineScriptEditorsRegistered();
	return MutableEntries();
}

} // namespace ptgn::editor::script

namespace ptgn {

namespace {

using editor::script::EventEditorRegistrationDefinition;
using editor::script::RootScriptEditor;
using editor::script::ScriptEditorContext;
using editor::script::ScriptEditorOptions;
using editor::script::SequenceStepEditor;
using editor::script::SequenceStepEditorOptions;

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

bool DrawEmptyEvent(json&) {
	return false;
}

bool DrawKey(json& value) {
	Key key{ JsonValueOr<Key>(value, "key", Key::W) };
	bool changed{ false };
	if (ImGui::BeginCombo("Key", std::string{ magic_enum::enum_name(key) }.c_str())) {
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
	if (changed) {
		value["key"] = key;
	}
	return changed;
}

bool DrawMouse(json& value) {
	Mouse mouse{ JsonValueOr<Mouse>(value, "button", Mouse::Left) };
	bool changed{ false };
	if (ImGui::BeginCombo("Button", std::string{ magic_enum::enum_name(mouse) }.c_str())) {
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
	if (changed) {
		value["button"] = mouse;
	}
	return changed;
}

bool DrawSignalEvent(json& value) {
	std::string signal{ JsonValueOr<std::string>(value, "signal", "") };
	const bool changed{ ImGui::InputText("Signal", &signal) };
	if (changed) {
		value["signal"] = std::move(signal);
	}
	return changed;
}

template <typename T>
bool DrawNothing(T&, ScriptEditorContext&) {
	return false;
}

bool DrawMoveTo(MoveToScript& script, ScriptEditorContext&) {
	bool changed{ ImGui::DragFloat2("Destination", &script.destination.x, 0.1f) };
	changed |= ImGui::Checkbox("Relative", &script.relative);
	return changed;
}

bool DrawRotateTo(RotateToScript& script, ScriptEditorContext&) {
	bool changed{ ImGui::DragFloat("Degrees", &script.degrees, 0.5f) };
	changed |= ImGui::Checkbox("Shortest Path", &script.shortest_path);
	changed |= ImGui::Checkbox("Relative", &script.relative);
	return changed;
}

bool DrawScaleTo(ScaleToScript& script, ScriptEditorContext&) {
	bool changed{ ImGui::DragFloat2("Scale", &script.scale.x, 0.01f) };
	changed |= ImGui::Checkbox("Relative", &script.relative);
	return changed;
}

bool DrawFollowTarget(FollowTargetScript& script, ScriptEditorContext&) {
	bool changed{ ImGui::DragFloat("Speed", &script.speed, 1.0f, 0.0f) };
	changed |= ImGui::DragFloat(
		"Stopping Distance", &script.stopping_distance, 0.1f, 0.0f
	);
	ImGui::TextDisabled("Target selection should use your UUID/entity-reference field.");
	return changed;
}

bool DrawFollowTargetRoot(FollowTargetScript& script, ScriptEditorContext&) {
	bool changed{ ImGui::DragFloat("Speed", &script.speed, 1.0f, 0.0f) };
	changed |= ImGui::DragFloat(
		"Stopping Distance", &script.stopping_distance, 0.1f, 0.0f
	);
	return changed;
}

bool DrawSetVisible(SetVisibleScript& script, ScriptEditorContext&) {
	return ImGui::Checkbox("Visible", &script.visible);
}

bool DrawEmitSignal(EmitSignalScript& script, ScriptEditorContext&) {
	return ImGui::InputText("Signal", &script.signal.value);
}

bool DrawAddComponents(AddComponentsScript& script, ScriptEditorContext&) {
	bool changed{ false };
	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(script.components.size()); ++i) {
		auto& component{ script.components[static_cast<std::size_t>(i)] };
		ImGui::PushID(i);
		ImGui::TextUnformatted(component.type.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("x")) {
			remove = i;
		}
		ImGui::PopID();
	}
	if (remove >= 0) {
		script.components.erase(script.components.begin() + remove);
		changed = true;
	}
	if (ImGui::Button("+ Component", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddComponentScriptComponent");
	}
	if (ImGui::BeginPopup("AddComponentScriptComponent")) {
		for (const auto& component : ComponentRegistry::Components()) {
			if (ImGui::MenuItem(std::string{ component.name }.c_str())) {
				script.components.push_back(MakeComponentDefinition(component));
				changed = true;
			}
		}
		ImGui::EndPopup();
	}
	return changed;
}

bool DrawRemoveComponents(RemoveComponentsScript& script, ScriptEditorContext&) {
	bool changed{ false };
	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(script.components.size()); ++i) {
		ImGui::PushID(i);
		changed |= ImGui::InputText(
			"##Component", &script.components[static_cast<std::size_t>(i)]
		);
		ImGui::SameLine();
		if (ImGui::SmallButton("x")) {
			remove = i;
		}
		ImGui::PopID();
	}
	if (remove >= 0) {
		script.components.erase(script.components.begin() + remove);
		changed = true;
	}
	if (ImGui::Button("+ Component", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("RemoveComponentScriptComponent");
	}
	if (ImGui::BeginPopup("RemoveComponentScriptComponent")) {
		for (const auto& component : ComponentRegistry::Components()) {
			if (ImGui::MenuItem(std::string{ component.name }.c_str())) {
				script.components.push_back(std::string{ component.name });
				changed = true;
			}
		}
		ImGui::EndPopup();
	}
	return changed;
}

} // namespace

PTGN_REGISTER_SCRIPT(
	Script,
	RootScriptEditor(
		ScriptEditorOptions{
			.label = "Script Sequence",
			.group = "Sequence",
			.description = "Editor-authored sequence of registered scripts.",
		},
		&DrawNothing<Script>
	)
);

PTGN_REGISTER_SCRIPT(
	WaitScript,
	SequenceStepEditor(
		SequenceStepEditorOptions{
			.label = "Delay",
			.group = "Timing",
			.description = "Wait before continuing.",
		},
		&DrawNothing<WaitScript>
	)
);

PTGN_REGISTER_SCRIPT(
	MoveToScript,
	SequenceStepEditor(
		SequenceStepEditorOptions{
			.label = "Move To",
			.group = "Transform",
			.description = "Move the owning entity.",
		},
		&DrawMoveTo
	),
	RootScriptEditor(
		ScriptEditorOptions{
			.label = "Move To",
			.group = "Transform",
			.description = "Root movement script.",
		},
		&DrawMoveTo
	)
);

PTGN_REGISTER_SCRIPT(
	RotateToScript,
	SequenceStepEditor(
		SequenceStepEditorOptions{
			.label = "Rotate To",
			.group = "Transform",
			.description = "Rotate the owning entity.",
		},
		&DrawRotateTo
	)
);

PTGN_REGISTER_SCRIPT(
	ScaleToScript,
	SequenceStepEditor(
		SequenceStepEditorOptions{
			.label = "Scale To",
			.group = "Transform",
			.description = "Scale the owning entity.",
		},
		&DrawScaleTo
	)
);

PTGN_REGISTER_SCRIPT(
	FollowTargetScript,
	SequenceStepEditor(
		SequenceStepEditorOptions{
			.label = "Follow Target",
			.group = "Transform",
			.description = "Follow an entity until close enough.",
		},
		&DrawFollowTarget
	),
	RootScriptEditor(
		ScriptEditorOptions{
			.label = "Follow Target",
			.group = "Transform",
			.description = "Root follow behavior.",
		},
		&DrawFollowTargetRoot
	)
);

PTGN_REGISTER_SCRIPT(
	SetVisibleScript,
	SequenceStepEditor(
		SequenceStepEditorOptions{
			.label = "Set Visibility",
			.group = "Entity",
			.description = "Set owner visibility.",
		},
		&DrawSetVisible
	)
);

PTGN_REGISTER_SCRIPT(
	EmitSignalScript,
	SequenceStepEditor(
		SequenceStepEditorOptions{
			.label = "Emit Signal",
			.group = "Events",
			.description = "Emit a global signal.",
		},
		&DrawEmitSignal
	)
);

PTGN_REGISTER_SCRIPT(
	AddComponentsScript,
	SequenceStepEditor(
		SequenceStepEditorOptions{
			.label = "Add Components",
			.group = "Entity",
			.description = "Add registered components to the owner.",
			.menu_order = 1,
		},
		&DrawAddComponents
	)
);

PTGN_REGISTER_SCRIPT(
	RemoveComponentsScript,
	SequenceStepEditor(
		SequenceStepEditorOptions{
			.label = "Remove Components",
			.group = "Entity",
			.description = "Remove registered components from the owner.",
			.menu_order = 2,
			.separator_after = true,
		},
		&DrawRemoveComponents
	)
);

#define PTGN_REGISTER_KEY_EVENT(Type, Label)                                             \
	PTGN_REGISTER_EVENT(                                                                  \
		Type,                                                                               \
		(EventEditorRegistrationDefinition{                                                 \
			.options = {                                                                      \
				.label = Label,                                                                 \
				.group = "Key",                                                                \
				.description = "Matches one key.",                                              \
			},                                                                                \
			.inline_fields = 1,                                                               \
			.draw = &DrawKey,                                                                  \
		})                                                                                  \
	)

#define PTGN_REGISTER_MOUSE_EVENT(Type, Label, Group)                                    \
	PTGN_REGISTER_EVENT(                                                                  \
		Type,                                                                               \
		(EventEditorRegistrationDefinition{                                                 \
			.options = {                                                                      \
				.label = Label,                                                                 \
				.group = Group,                                                                 \
				.description = "Matches one mouse button.",                                     \
			},                                                                                \
			.inline_fields = 1,                                                               \
			.draw = &DrawMouse,                                                                \
		})                                                                                  \
	)

#define PTGN_REGISTER_EMPTY_EVENT(Type, Label, Group, Description)                       \
	PTGN_REGISTER_EVENT(                                                                  \
		Type,                                                                               \
		(EventEditorRegistrationDefinition{                                                 \
			.options = {                                                                      \
				.label = Label,                                                                 \
				.group = Group,                                                                 \
				.description = Description,                                                     \
			},                                                                                \
			.draw = &DrawEmptyEvent,                                                          \
		})                                                                                  \
	)

PTGN_REGISTER_KEY_EVENT(event::KeyPressed, "On Key Pressed");
PTGN_REGISTER_KEY_EVENT(event::KeyHeld, "On Key Held");
PTGN_REGISTER_KEY_EVENT(event::KeyReleased, "On Key Released");
PTGN_REGISTER_MOUSE_EVENT(event::MousePressed, "On Mouse Pressed", "Mouse");
PTGN_REGISTER_MOUSE_EVENT(event::MouseHeld, "On Mouse Held", "Mouse");
PTGN_REGISTER_MOUSE_EVENT(event::MouseReleased, "On Mouse Released", "Mouse");

PTGN_REGISTER_EMPTY_EVENT(
	event::MouseMoveOver, "On Mouse Enter", "Interaction",
	"Matches when the pointer enters the owner."
);
PTGN_REGISTER_EMPTY_EVENT(
	event::MouseMoveOut, "On Mouse Leave", "Interaction",
	"Matches when the pointer leaves the owner."
);
PTGN_REGISTER_MOUSE_EVENT(
	event::MousePressedOver, "On Mouse Pressed Over", "Interaction"
);
PTGN_REGISTER_MOUSE_EVENT(event::MouseHeldOver, "On Mouse Held Over", "Interaction");
PTGN_REGISTER_MOUSE_EVENT(
	event::MouseReleasedOver, "On Mouse Released Over", "Interaction"
);
PTGN_REGISTER_EMPTY_EVENT(
	event::ButtonPress, "On Button Press", "Button",
	"Matches when the owner emits ButtonPress."
);
PTGN_REGISTER_EMPTY_EVENT(event::DragStart, "On Drag Start", "Drag", "Matches drag start.");
PTGN_REGISTER_EMPTY_EVENT(event::Drag, "On Drag", "Drag", "Matches while dragging.");
PTGN_REGISTER_EMPTY_EVENT(event::DragStop, "On Drag Stop", "Drag", "Matches drag stop.");
PTGN_REGISTER_EMPTY_EVENT(
	event::OverlapStart, "On Overlap Start", "Physics", "Matches overlap start."
);
PTGN_REGISTER_EMPTY_EVENT(event::Overlap, "On Overlap", "Physics", "Matches overlap.");
PTGN_REGISTER_EMPTY_EVENT(
	event::OverlapStop, "On Overlap Stop", "Physics", "Matches overlap stop."
);
PTGN_REGISTER_EMPTY_EVENT(
	event::Collision, "On Collision", "Physics", "Matches collision."
);

PTGN_REGISTER_EVENT(
	Signal,
	(EventEditorRegistrationDefinition{
		.options = {
			.label = "On Signal",
			.group = "",
			.description = "Matches an exact signal name.",
		},
		.inline_fields = 1,
		.draw = &DrawSignalEvent,
	})
);

#undef PTGN_REGISTER_KEY_EVENT
#undef PTGN_REGISTER_MOUSE_EVENT
#undef PTGN_REGISTER_EMPTY_EVENT

namespace editor::script::impl {

void EnsureEngineScriptEditorsRegistered() {
	// Intentionally empty.
	//
	// Referencing this function forces the linker to include this object file. The namespace-scope
	// PTGN_REGISTER_SCRIPT and PTGN_REGISTER_EVENT initializers then populate the editor registries.
}

} // namespace editor::script::impl

} // namespace ptgn
