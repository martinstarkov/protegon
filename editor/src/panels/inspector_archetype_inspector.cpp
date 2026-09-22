#include "panels/inspector_archetype_inspector.h"

namespace ptgn::editor::inspector {

namespace {

template <typename T, typename Target>
bool DrawAddComponentItem(
	Target& target, const char* label, bool unavailable = false
) {
	if constexpr (!Target::template Supports<T>()) {
		return false;
	} else {
		const bool exists{ target.template Capture<T>().has_value() };
		ScopedDisabled disabled{ exists || unavailable };
		if (!ImGui::MenuItem(label)) {
			return false;
		}
		return SetComponentStateUndoable<Target, T>(
			target,
			std::string{ "Add " } + label,
			ComponentState<T>{ T{} }
		);
	}
}

template <typename Target>
bool DrawAddMovementItem(Target& target) {
	if constexpr (!Target::template Supports<TopDownMovement>() &&
		!Target::template Supports<PlatformerMovement>()) {
		return false;
	} else {
		const bool exists{
			HasInspectorComponent<Target, TopDownMovement>(target) ||
			HasInspectorComponent<Target, PlatformerMovement>(target)
		};
		ScopedDisabled disabled{ exists };
		if (!ImGui::MenuItem("Movement")) {
			return false;
		}

		// Movement is one authoring concept. Top Down is the default implementation; the Movement
		// section's Type combo owns switching to every other concrete movement implementation.
		if constexpr (Target::template Supports<TopDownMovement>()) {
			return SetComponentStateUndoable<Target, TopDownMovement>(
				target,
				"Add Movement",
				ComponentState<TopDownMovement>{ TopDownMovement{} }
			);
		} else {
			return SetComponentStateUndoable<Target, PlatformerMovement>(
				target,
				"Add Movement",
				ComponentState<PlatformerMovement>{ PlatformerMovement{} }
			);
		}
	}
}

template <typename Target>
bool DrawAddComponentMenu(Target& target) {
	bool changed{ false };

	if (ImGui::Button("Add Component", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (!ImGui::BeginPopup("AddComponentPopup")) {
		return false;
	}

	const bool button_control{ HasInspectorComponent<Target, ::ptgn::impl::ButtonData>(target) };

	changed |= DrawAddComponentItem<Transform>(target, "Transform");
	changed |= DrawAddComponentItem<::ptgn::impl::Interactive>(
		target, "Interaction", button_control
	);

	if (ImGui::BeginMenu("Physics & Movement")) {
		changed |= DrawAddComponentItem<Collider>(target, "Collider");
		changed |= DrawAddComponentItem<RigidBody>(target, "Rigid Body");
		changed |= DrawAddMovementItem(target);
		ImGui::EndMenu();
	}

	changed |= DrawAddComponentItem<::ptgn::impl::Scripts>(target, "Scripts");

	if (ImGui::BeginMenu("Utilities")) {
		changed |= DrawAddComponentItem<::ptgn::impl::Timers>(target, "Timers");
		changed |= DrawAddComponentItem<Group>(target, "Groups");
		changed |= DrawAddComponentItem<Lifetime>(target, "Lifetime");
		ImGui::EndMenu();
	}

	// Identity-defining components are intentionally not offered here. Rectangle, Sprite, Button,
	// Camera, Dialogue, etc. are authored by creating/converting the entity archetype, not by
	// assembling a new identity piecemeal from the ordinary inspector.

	ImGui::EndPopup();
	return changed;
}

template <typename Target>
bool DrawCoreArchetypeSections(Target& target, InspectorArchetype archetype) {
	bool changed{ false };

	// Managed UI children keep their specialized state selector before Transform/Visual. This is
	// presentation behavior, not entity identity, so preserve the existing ordering exactly.
	if (GetButtonChildInfo(target)) {
		changed |= DrawUISection(target);
		changed |= DrawTransformSection(target);
		changed |= DrawVisualSection(target);
		return changed;
	}

	if (ArchetypeRequiresTransform(archetype) || HasAnyInspectorComponent(target, TransformSectionComponents{})) {
		changed |= DrawTransformSection(target);
	}

	if (ArchetypeOwnsUI(archetype) || HasAnyInspectorComponent(target, UISectionComponents{})) {
		changed |= DrawUISection(target);
	}

	if (ArchetypeOwnsVisual(archetype)) {
		changed |= DrawVisualSection(
			target,
			true,
			false,
			GetInspectorArchetypeLabel(target)
		);
	} else if (HasVisualSection(target)) {
		// Generic/raw entities may still carry a drawable component. They keep the renderer selector
		// because no archetype owns that renderer identity.
		changed |= DrawVisualSection(target, true, true, "Visual");
	}

	if (ArchetypeOwnsCamera(archetype) || HasCameraSection(target)) {
		changed |= DrawCameraSection(target);
	}

	return changed;
}

template <typename Target>
bool DrawArchetypeInspectorImpl(Target& target) {
	if constexpr (requires { target.entity; }) {
		ClearButtonPreviewIfDifferent(target.ctx, target.entity);
	} else {
		ClearButtonPreviewIfDifferent(target.ctx);
	}

	const InspectorArchetype archetype{ ResolveInspectorArchetype(target) };
	bool changed{ DrawCoreArchetypeSections(target, archetype) };

	// These are capabilities attached to an entity, not identity. Their presence is determined by
	// concrete ECS components and they remain independent of the archetype router above.
	changed |= DrawInteractionSection(target);
	changed |= DrawPhysicsSection(target);
	changed |= DrawScriptsSection(target);
	changed |= DrawUtilitiesSection(target);

	ImGui::Separator();
	changed |= DrawAddComponentMenu(target);
	return changed;
}

} // namespace

bool DrawArchetypeInspector(EntityInspectorTarget& target) {
	return DrawArchetypeInspectorImpl(target);
}

bool DrawArchetypeInspector(PrefabInspectorTarget& target) {
	return DrawArchetypeInspectorImpl(target);
}

} // namespace ptgn::editor::inspector
