#include "panels/inspector.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <memory>
#include <string>
#include <string_view>
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
		SyncPrefabInstances(*scene, key);
	}
}

// Selecting a dock tab must not move keyboard focus away from the hierarchy or viewport.
void SelectInspectorDockTab(const char* window_name) {
	ImGuiWindow* window{ ImGui::FindWindowByName(window_name) };
	if (!window) {
		return;
	}

	if (ImGuiDockNode * dock{ window->DockNode }) {
		dock->SelectedTabId = window->TabId;
		if (dock->TabBar) {
			dock->TabBar->NextSelectedTabId = window->TabId;
		}
	} else {
		ImGui::SetWindowFocus(window_name);
	}
}

[[nodiscard]] const char* InspectorWindow(EditorSelectionMode mode) {
	switch (mode) {
		case EditorSelectionMode::SceneHierarchy: return "Entity Inspector###EntityInspector";
		case EditorSelectionMode::Prefabs:		  return "Prefab Inspector###PrefabInspector";
		case EditorSelectionMode::Tiles:		  return "Tile Inspector###TileInspector";
	}
	return "Entity Inspector###EntityInspector";
}

} // namespace

PositionPicker::Finish PreparePositionPickSession(EditorContext& ctx) {
	Editor* editor{ &ctx.editor };
	bool should_resume{ editor->CanPause() && !editor->IsPaused() };

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
	bool read_only{ target.IsReadOnly() };
	if (read_only) {
		// Native editors honor the read only item flag while tree nodes and tabs stay interactive.
		ImGui::PushItemFlag(ImGuiItemFlags_ReadOnly, true);
	}
	DrawInspectorContents(ctx, target);
	if (read_only) {
		ImGui::PopItemFlag();
	}
}

void DrawPrefabInspector(
	EditorContext& ctx, const PrefabKey& key, SerializedEntityPath entity_path
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
		.ctx		 = ctx,
		.key		 = key,
		.entity_path = entity_path,
		.prefab		 = selected_entity,
	};

	if (!DrawInspectorContents(ctx, target)) {
		return;
	}

	// The Prefabs tab edits the live AssetManager owned ECS entity. Persistence is a snapshot
	// of that entity hierarchy, not the editing model itself.
	assets.SavePrefabEntity(key);
	SyncEditedPrefabInstances(ctx, key);
	ctx.local.state.is_dirty = true;
}

void DrawPrefabInspector(EditorContext& ctx, const PrefabKey& key) {
	DrawPrefabInspector(ctx, key, ctx.local.selection.selected_prefab_entity_path);
}

} // namespace inspector

void InspectorPanel::OnRender(EditorContext& ctx) {
	auto& hierarchy{ ctx.editor.GetSceneHierarchyPanel() };
	auto& paint{ ctx.editor.GetPaintEditor() };
	EditorSelection selection{ ctx.local.selection };
	EditorSelectionMode mode{ selection.mode };

	bool primary_selection_changed{ !previous_selection_.has_value() ||
									previous_selection_->mode != mode };
	if (previous_selection_.has_value()) {
		switch (mode) {
			case EditorSelectionMode::SceneHierarchy:
				primary_selection_changed |=
					previous_selection_->selected_scene_key != selection.selected_scene_key ||
					previous_selection_->selected_scene_runtime !=
						selection.selected_scene_runtime ||
					previous_selection_->scene_entities != selection.scene_entities;
				break;

			case EditorSelectionMode::Prefabs:
				primary_selection_changed |=
					previous_selection_->selected_prefab != selection.selected_prefab ||
					previous_selection_->selected_prefab_entity_path !=
						selection.selected_prefab_entity_path;
				break;

			case EditorSelectionMode::Tiles:
				primary_selection_changed |=
					previous_tile_entry_id_ != paint.GetSelectedTileEntryId();
				break;
		}
	}

	bool screen_effect_selection_changed{ previous_screen_effect_selection_ !=
										  selection.selected_screen_effect };
	std::optional<EditorSelectionMode> requested_primary{ ConsumePrimaryInspectorRequest(ctx) };
	const char* requested_window{};
	if (requested_primary.has_value()) {
		requested_window = inspector::InspectorWindow(*requested_primary);
	} else if (
		screen_effect_selection_changed && selection.selected_screen_effect.has_value() &&
		selection.inspector_tab == InspectorTab::ScreenEffect
	) {
		requested_window = "Screen Effect Inspector###ScreenEffectInspector";
	} else if (primary_selection_changed) {
		requested_window = inspector::InspectorWindow(mode);
	}

	ImGuiID inspector_dock_id{};
	bool entity_visible{ ImGui::Begin("Entity Inspector###EntityInspector") };
	inspector_dock_id = ImGui::GetWindowDockID();
	if (entity_visible) {
		Entity entity{ hierarchy.GetSelectedEntity() };
		const PrefabInstance* linked_instance{ entity ? GetPrefabInstance(entity) : nullptr };

		if (linked_instance) {
			ImGui::TextDisabled(
				IsPrefabInstanceRoot(entity) ? "Linked prefab instance (Transform editable)"
											 : "Linked prefab instance (read only)"
			);
			ImGui::Separator();
			inspector::DrawEntityInspector(ctx, entity);
		} else if (entity) {
			inspector::DrawEntityInspector(ctx, entity);
		} else {
			ImGui::TextDisabled("Select an entity to inspect it.");
		}
	}
	ImGui::End();

	bool prefab_visible{ ImGui::Begin("Prefab Inspector###PrefabInspector") };
	if (inspector_dock_id == 0) {
		inspector_dock_id = ImGui::GetWindowDockID();
	}
	if (prefab_visible) {
		if (const auto& prefab{ hierarchy.GetSelectedPrefab() }; prefab.has_value()) {
			inspector::DrawPrefabInspector(ctx, prefab.value());
		} else {
			ImGui::TextDisabled("Select a prefab to inspect it.");
		}
	}
	ImGui::End();

	bool tile_visible{ ImGui::Begin("Tile Inspector###TileInspector") };
	if (inspector_dock_id == 0) {
		inspector_dock_id = ImGui::GetWindowDockID();
	}
	if (tile_visible) {
		paint.DrawTileInspector(ctx);
	}
	ImGui::End();

	auto screen_effect_selection{ ctx.local.selection.selected_screen_effect };
	bool screen_effect_visible{};
	ImGuiID screen_effect_dock_id{};
	if (screen_effect_selection.has_value()) {
		if (inspector_dock_id != 0) {
			ImGui::SetNextWindowDockID(inspector_dock_id, ImGuiCond_Appearing);
		}

		// Keep the close button on the inspector tab, but hide the shared dock close button.
		ImGuiWindowClass window_class{};
		window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoCloseButton;
		ImGui::SetNextWindowClass(&window_class);

		bool screen_effect_open{ true };
		screen_effect_visible =
			ImGui::Begin("Screen Effect Inspector###ScreenEffectInspector", &screen_effect_open);
		screen_effect_dock_id = ImGui::GetWindowDockID();
		if (screen_effect_visible) {
			inspector::DrawScreenEffectInspector(ctx, screen_effect_selection.value());
		}
		ImGui::End();

		if (!screen_effect_open) {
			ClearSelectedScreenEffect(ctx, "Close Screen Effect Inspector");
			requested_window = inspector::InspectorWindow(mode);
		}
	}

	if (requested_window) {
		inspector::SelectInspectorDockTab(requested_window);
		if (screen_effect_selection.has_value() &&
			std::string_view{ requested_window } !=
				"Screen Effect Inspector###ScreenEffectInspector") {
			SetInspectorTab(ctx, InspectorTab::Primary);
		}
	} else if (
		screen_effect_selection.has_value() && inspector_dock_id != 0 &&
		inspector_dock_id == screen_effect_dock_id
	) {
		InspectorTab visible_tab{ screen_effect_visible ? InspectorTab::ScreenEffect
														: InspectorTab::Primary };
		if (screen_effect_visible || entity_visible || prefab_visible || tile_visible) {
			SetInspectorTab(ctx, visible_tab);
		}
	}

	previous_selection_				  = std::move(selection);
	previous_screen_effect_selection_ = previous_selection_->selected_screen_effect;
	previous_tile_entry_id_			  = std::string{ paint.GetSelectedTileEntryId() };
}

} // namespace ptgn::editor
