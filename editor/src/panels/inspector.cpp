#include "panels/inspector.h"

#include <imgui.h>

#include <memory>
#include <utility>

#include "editor/editor.h"
#include "editor/editor_context.h"
#include "editor/editor_selection.h"
#include "panels/inspector_feature_helpers.h"
#include "panels/inspector_internal.h"
#include "panels/scene_hierarchy.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"

namespace ptgn::editor {

namespace inspector {

namespace {

template <typename Target>
bool DrawInspectorContents(EditorContext& ctx, Target& target) {
	CommitInactiveInspectorEdit(ctx);

	bool changed{ DrawName(target) };
	ImGui::Separator();
	changed |= DrawFeatureInspector(target);
	return changed;
}

} // namespace

PositionPicker::Finish PreparePositionPickSession(
	EditorContext& ctx
) {
	Editor* editor{
		&ctx.editor
	};

	const bool should_resume{
		editor->CanPause() &&
		!editor->IsPaused()
	};

	if (should_resume) {
		editor->TogglePause();
	}

	return [
		editor,
		should_resume
	]() {
		if (!should_resume) {
			return;
		}

		// Only resume if this picker owns the pause and the
		// application is still paused.
		if (
			editor->CanPause() &&
			editor->IsPaused()
		) {
			editor->TogglePause();
		}
	};
}

void DrawEntityInspector(EditorContext& ctx, Entity entity) {
	EntityInspectorTarget target{
		.ctx	= ctx,
		.entity = entity,
	};

	DrawInspectorContents(ctx, target);
}

void DrawPrefabInspector(EditorContext& ctx, const PrefabKey& key) {
	auto& assets{ ctx.editor.GetAssetManager() };

	if (!assets.Has(key)) {
		ImGui::TextDisabled("Prefab is not currently loaded.");
		return;
	}

	auto prefab_asset{
		::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(key)
	};

	const SerializedEntityPath entity_path{
		ctx.local.selection.selected_prefab_entity_path
	};

	auto* selected_entity{
		ResolveSerializedEntity(
			prefab_asset.get().root,
			entity_path
		)
	};

	if (!selected_entity) {
		ImGui::TextDisabled(
			"The selected prefab entity no longer exists."
		);
		return;
	}

	PrefabInspectorTarget target{
		.ctx = ctx,
		.key = key,
		.entity_path = entity_path,
		.prefab = *selected_entity,
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
	const bool prefab_tab_active{
		hierarchy.GetActiveTab() == SceneHierarchyTab::Prefabs
	};

	const bool inspector_visible{
		ImGui::Begin(
			prefab_tab_active
				? "Prefab Inspector###Inspector"
				: "Entity Inspector###Inspector"
		)
	};

	const ImGuiID inspector_dock_id{ ImGui::GetWindowDockID() };

	if (inspector_visible) {
		if (prefab_tab_active) {
			if (const auto& selected_prefab{ hierarchy.GetSelectedPrefab() };
				selected_prefab.has_value()) {
				inspector::DrawPrefabInspector(ctx, selected_prefab.value());
			}
		} else if (auto entity{ hierarchy.GetSelectedEntity() }) {
			inspector::DrawEntityInspector(ctx, entity);
		}
	}

	ImGui::End();

	const auto selection{ ctx.local.selection.selected_screen_effect };
	if (!selection.has_value()) {
		previous_screen_effect_selection_.reset();
		return;
	}

	const bool selection_changed{
		previous_screen_effect_selection_ != selection
	};

	if (inspector_dock_id != 0) {
		ImGui::SetNextWindowDockID(
			inspector_dock_id,
			ImGuiCond_Appearing
		);
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
		inspector_dock_id != 0 &&
		inspector_dock_id == screen_effect_inspector_dock_id
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
