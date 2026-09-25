#include "panels/inspector.h"

#include <imgui.h>

#include <memory>
#include <utility>

#include "editor/editor.h"
#include "editor/editor_context.h"
#include "editor/editor_selection.h"
#include "editor/paint/paint_editor.h"
#include "panels/inspector_archetype_inspector.h"
#include "panels/inspector_helpers.h"
#include "panels/inspector_screen_effects.h"
#include "panels/inspector_targets.h"
#include "panels/scene_hierarchy.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn::editor {

namespace inspector {

namespace {

template <typename Target>
bool DrawInspectorContents(EditorContext& ctx, Target& target) {
	CommitInactiveInspectorEdit(ctx);

	bool changed{ DrawName(target) };
	changed |= DrawArchetypeInspector(target);
	return changed;
}

[[nodiscard]] bool EnsurePrefabResident(EditorContext& ctx, const PrefabKey& key) {
	return ctx.editor.GetAssetManager().EnsurePrefabResident(key);
}

void SyncEditedPrefabInstances(EditorContext& ctx, const PrefabKey& key) {
	for (const auto& scene : ctx.editor.GetSceneManager().GetScenes()) {
		if (!scene || scene->IsRuntime()) {
			continue;
		}
		(void)SyncPrefabInstances(*scene, key);
	}
}

} // namespace

PositionPicker::Finish PreparePositionPickSession(EditorContext& ctx) {
	Editor* editor{ &ctx.editor };
	const bool should_resume{ editor->CanPause() && !editor->IsPaused() };

	if (should_resume) {
		editor->TogglePause();
	}

	return [editor, should_resume]() {
		if (should_resume && editor->CanPause() && editor->IsPaused()) {
			editor->TogglePause();
		}
	};
}

void DrawEntityInspector(EditorContext& ctx, Entity entity) {
	EntityInspectorTarget target{ .ctx = ctx, .entity = entity };
	DrawInspectorContents(ctx, target);
}

void DrawPrefabInspector(
	EditorContext& ctx,
	const PrefabKey& key,
	SerializedEntityPath entity_path
) {
	auto& assets{ ctx.editor.GetAssetManager() };
	if (!EnsurePrefabResident(ctx, key)) {
		ImGui::TextDisabled("Prefab is not currently available.");
		return;
	}

	Entity selected_entity{ assets.GetPrefabEntity(key, entity_path) };
	if (!selected_entity) {
		ImGui::TextDisabled("The selected prefab entity no longer exists.");
		return;
	}

	PrefabInspectorTarget target{
		.ctx = ctx,
		.key = key,
		.entity_path = entity_path,
		.prefab = selected_entity,
	};

	if (!DrawInspectorContents(ctx, target)) {
		return;
	}

	// The Prefabs tab edits the live AssetManager-owned ECS entity. Persistence is a snapshot
	// of that entity hierarchy, not the editing model itself.
	assets.SavePrefabEntity(key);
	SyncEditedPrefabInstances(ctx, key);
	ctx.local.state.is_dirty = true;
}

void DrawPrefabInspector(EditorContext& ctx, const PrefabKey& key) {
	DrawPrefabInspector(
		ctx,
		key,
		ctx.local.selection.selected_prefab_entity_path
	);
}

} // namespace inspector

void InspectorPanel::OnRender(EditorContext& ctx) {
	auto& hierarchy{ ctx.editor.GetSceneHierarchyPanel() };
	const SceneHierarchyTab active_tab{ hierarchy.GetActiveTab() };

	Entity selected_scene_entity{
		active_tab == SceneHierarchyTab::SceneHierarchy
			? hierarchy.GetSelectedEntity()
			: Entity{}
	};
	const PrefabInstance* linked_instance{
		selected_scene_entity
			? GetPrefabInstance(selected_scene_entity)
			: nullptr
	};
	const bool inspect_linked_instance{ linked_instance != nullptr };

	// A linked scene instance remains an entity inspection surface. The prefab itself is only
	// editable after explicitly navigating to the Prefabs tab.
	const char* title{
		active_tab == SceneHierarchyTab::Prefabs
			? "Prefab Inspector###Inspector"
			: active_tab == SceneHierarchyTab::Tiles
				? "Tile Inspector###Inspector"
				: "Entity Inspector###Inspector"
	};

	const bool inspector_visible{ ImGui::Begin(title) };
	const ImGuiID inspector_dock_id{ ImGui::GetWindowDockID() };

	if (inspector_visible) {
		if (inspect_linked_instance) {
			const PrefabKey prefab_key{ linked_instance->prefab };
			const SerializedEntityPath prefab_path{ linked_instance->entity_path };

			ImGui::TextDisabled("Linked prefab instance");

			if (ImGui::SmallButton("Go to Prefab")) {
				hierarchy.SetSelectedPrefab(prefab_key, prefab_path, false);
				hierarchy.SetActiveTab(SceneHierarchyTab::Prefabs);
				ImGui::SetWindowFocus("Prefabs###PrefabsWindow");
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Open the source prefab and select the matching prefab entity."
				);
			}

			ImGui::SameLine();

			Entity instance_root{
				GetPrefabInstanceRoot(selected_scene_entity)
			};
			ImGui::BeginDisabled(!instance_root);
			const bool convert{
				ImGui::SmallButton("Convert to Entity")
			};
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip(
					"Stop syncing this instance to its prefab so this scene entity can be edited."
				);
			}

			ImGui::Separator();
			ImGui::TextDisabled(
				"This linked instance is read-only. Edit the prefab or convert the instance first."
			);

			if (convert && instance_root) {
				ctx.commands.ConvertPrefabInstanceToEntity(instance_root);
			} else {
				// Read the actual linked scene entity, not the prefab definition. Disabling ImGui
				// prevents every archetype/component editor from mutating it accidentally.
				ImGui::BeginDisabled();
				inspector::DrawEntityInspector(ctx, selected_scene_entity);
				ImGui::EndDisabled();
			}
		} else {
			switch (active_tab) {
				case SceneHierarchyTab::Prefabs:
					if (const auto& selected_prefab{ hierarchy.GetSelectedPrefab() };
						selected_prefab.has_value()) {
						inspector::DrawPrefabInspector(ctx, selected_prefab.value());
					} else {
						ImGui::TextDisabled("Select a prefab to inspect it.");
					}
					break;

				case SceneHierarchyTab::Tiles:
					ctx.editor.GetPaintEditor().DrawTileInspector(ctx);
					break;

				case SceneHierarchyTab::SceneHierarchy:
				default:
					if (auto entity{ hierarchy.GetSelectedEntity() }) {
						inspector::DrawEntityInspector(ctx, entity);
					} else {
						ImGui::TextDisabled("Select an entity to inspect it.");
					}
					break;
			}
		}
	}

	ImGui::End();

	const auto selection{ ctx.local.selection.selected_screen_effect };
	if (!selection.has_value()) {
		previous_screen_effect_selection_.reset();
		return;
	}

	const bool selection_changed{ previous_screen_effect_selection_ != selection };
	if (inspector_dock_id != 0) {
		ImGui::SetNextWindowDockID(inspector_dock_id, ImGuiCond_Appearing);
	}
	if (selection_changed) {
		ImGui::SetNextWindowFocus();
	}

	bool screen_effect_inspector_open{ true };
	const bool screen_effect_inspector_visible{
		ImGui::Begin(
			"Screen Effect Inspector###ScreenEffectInspector",
			&screen_effect_inspector_open
		)
	};
	const ImGuiID screen_effect_inspector_dock_id{ ImGui::GetWindowDockID() };

	if (screen_effect_inspector_visible) {
		inspector::DrawScreenEffectInspector(ctx, selection.value());
	}
	ImGui::End();

	if (!screen_effect_inspector_open) {
		ClearSelectedScreenEffect(ctx, "Close Screen Effect Inspector");
		previous_screen_effect_selection_.reset();
		return;
	}

	const bool shared_dock{
		inspector_dock_id != 0 && inspector_dock_id == screen_effect_inspector_dock_id
	};
	if (!selection_changed && shared_dock) {
		if (screen_effect_inspector_visible &&
			ctx.local.selection.inspector_tab != InspectorTab::ScreenEffect) {
			SetInspectorTab(ctx, InspectorTab::ScreenEffect);
		} else if (inspector_visible &&
			ctx.local.selection.inspector_tab != InspectorTab::Primary) {
			SetInspectorTab(ctx, InspectorTab::Primary);
		}
	}

	previous_screen_effect_selection_ = selection;
}

} // namespace ptgn::editor
