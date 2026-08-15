#include "editor/editor_selection.h"

#include <imgui.h>

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>

#include "commands/editor_command.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "panels/scene_list.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn::editor {

namespace {

Scene* ResolveScene(Editor& editor, const EditorSelection& selection) {
	if (!selection.HasSceneSelection()) {
		return nullptr;
	}

	auto& scenes{ editor.GetSceneManager().GetScenes() };
	const auto it{ std::ranges::find_if(
		scenes,
		[&selection](const auto& scene) {
			return scene &&
				scene->GetTag() == selection.selected_scene_key &&
				scene->IsRuntime() == selection.selected_scene_runtime;
		}
	) };

	return it == scenes.end() ? nullptr : it->get();
}

} // namespace

void EditorSelection::Clear() {
	selected_scene_key.clear();
	selected_scene_runtime = false;
	scene_entities.clear();
	selected_prefab.reset();
	selected_prefab_entity_path.clear();
	selected_screen_effect.reset();
	mode = EditorSelectionMode::SceneHierarchy;
	scene_list_tab = SceneListTab::Scenes;
	inspector_tab = InspectorTab::Primary;
}

bool EditorSelection::HasEntitySelection(
	std::string_view scene_key,
	bool runtime
) const {
	return std::ranges::any_of(
		scene_entities,
		[scene_key, runtime](const SceneEntitySelection& selection) {
			return selection.scene_key == scene_key &&
				selection.runtime == runtime;
		}
	);
}

std::optional<UUID> EditorSelection::GetEntityUUID(
	std::string_view scene_key,
	bool runtime
) const {
	const auto it{ std::ranges::find_if(
		scene_entities,
		[scene_key, runtime](const SceneEntitySelection& selection) {
			return selection.scene_key == scene_key &&
				selection.runtime == runtime;
		}
	) };

	return it == scene_entities.end() ? std::nullopt : it->entity_uuid;
}

void EditorSelection::SetEntityUUID(
	std::string scene_key,
	bool runtime,
	std::optional<UUID> entity_uuid
) {
	const auto it{ std::ranges::find_if(
		scene_entities,
		[&scene_key, runtime](const SceneEntitySelection& selection) {
			return selection.scene_key == scene_key &&
				selection.runtime == runtime;
		}
	) };

	if (it == scene_entities.end()) {
		scene_entities.push_back(SceneEntitySelection{
			.scene_key = std::move(scene_key),
			.runtime = runtime,
			.entity_uuid = entity_uuid,
		});
		return;
	}

	it->entity_uuid = entity_uuid;
}

void EditorSelection::RemoveScene(std::string_view scene_key) {
	std::erase_if(
		scene_entities,
		[scene_key](const SceneEntitySelection& selection) {
			return selection.scene_key == scene_key;
		}
	);

	if (selected_scene_key == scene_key) {
		selected_scene_key.clear();
		selected_scene_runtime = false;
	}
}

void EditorSelection::RenameScene(
	std::string_view old_key,
	std::string_view new_key
) {
	if (selected_scene_key == old_key) {
		selected_scene_key = std::string{ new_key };
	}

	for (auto& selection : scene_entities) {
		if (selection.scene_key == old_key) {
			selection.scene_key = std::string{ new_key };
		}
	}
}

bool EditorSelection::HasSceneSelection() const {
	return !selected_scene_key.empty() || selected_scene_runtime;
}

Scene* ResolveSelectedScene(EditorContext& ctx) {
	return ResolveScene(ctx.editor, ctx.local.selection);
}

const Scene* ResolveSelectedScene(const EditorContext& ctx) {
	return ResolveScene(ctx.editor, ctx.local.selection);
}

Entity ResolveSelectedEntity(EditorContext& ctx) {
	auto* scene{ ResolveSelectedScene(ctx) };
	if (!scene) {
		return {};
	}

	const auto uuid{
		ctx.local.selection.GetEntityUUID(scene->GetTag(), scene->IsRuntime())
	};

	return uuid ? scene->GetEntity(*uuid) : Entity{};
}

void ApplyEditorSelection(EditorContext& ctx, EditorSelection selection) {
	Scene* previous_scene{ ResolveSelectedScene(ctx) };

	ctx.local.position_picker.Cancel();
	ctx.local.selection = std::move(selection);

	Scene* next_scene{ ResolveSelectedScene(ctx) };
	ctx.editor.GetSceneListPanel().RefreshSelectedSceneState();

	if (previous_scene != next_scene) {
		ctx.editor.OnSelectedSceneChanged(previous_scene, next_scene);
	}
}

bool SetEditorSelection(
	EditorContext& ctx,
	EditorSelection selection,
	std::string label,
	bool allow_when_undo_disabled,
	bool transient
) {
	ctx.undo.CommitActiveEdit();

	const EditorSelection before{ ctx.local.selection };
	if (before == selection) {
		return false;
	}

	EditorContext* context{ std::addressof(ctx) };

	ctx.undo.Execute(
		std::make_unique<ActionEditorCommand>(
			std::move(label),
			[context, before]() mutable {
				ApplyEditorSelection(*context, before);
			},
			[context, after = std::move(selection)]() mutable {
				ApplyEditorSelection(*context, after);
			}
		),
		false,
		allow_when_undo_disabled,
		transient
	);

	return true;
}


namespace {

[[nodiscard]] const char* GetSceneHierarchyTabWindow(EditorSelectionMode tab) {
	switch (tab) {
		case EditorSelectionMode::SceneHierarchy:
			return "Scene Hierarchy###SceneHierarchyWindow";
		case EditorSelectionMode::Prefabs:
			return "Prefabs###PrefabsWindow";
	}
	return nullptr;
}

[[nodiscard]] const char* GetSceneListTabWindow(SceneListTab tab) {
	switch (tab) {
		case SceneListTab::Scenes: return "Scenes";
		case SceneListTab::ScreenEffects: return "Screen Effects";
	}
	return nullptr;
}

std::unordered_map<EditorContext*, SceneListTab>& PendingSceneListTabFocus() {
	static std::unordered_map<EditorContext*, SceneListTab> pending;
	return pending;
}

void RequestSceneListTabFocus(EditorContext& ctx, SceneListTab tab) {
	PendingSceneListTabFocus().insert_or_assign(std::addressof(ctx), tab);

	if (const char* window{ GetSceneListTabWindow(tab) }) {
		ImGui::SetWindowFocus(window);
	}
}

[[nodiscard]] const char* GetInspectorTabWindow(
	InspectorTab tab,
	EditorSelectionMode mode
) {
	if (tab == InspectorTab::ScreenEffect) {
		return "Screen Effect Inspector###ScreenEffectInspector";
	}

	return mode == EditorSelectionMode::Prefabs
		? "Prefab Inspector###Inspector"
		: "Entity Inspector###Inspector";
}

void ApplyEditorSelectionAndFocus(
	EditorContext& ctx,
	EditorSelection selection,
	const char* window
) {
	ApplyEditorSelection(ctx, std::move(selection));
	if (window) {
		ImGui::SetWindowFocus(window);
	}
}

} // namespace

bool SetSceneHierarchyTab(EditorContext& ctx, EditorSelectionMode tab) {
	if (ctx.local.selection.mode == tab) {
		return false;
	}

	const EditorSelection before{ ctx.local.selection };
	EditorSelection after{ before };
	after.mode = tab;

	const bool runtime{ ctx.editor.IsPlaying() };
	EditorContext* context{ std::addressof(ctx) };
	const auto before_tab{ before.mode };

	ctx.undo.Execute(
		std::make_unique<ActionEditorCommand>(
			tab == EditorSelectionMode::SceneHierarchy
				? "Select Scene Hierarchy Tab"
				: "Select Prefabs Tab",
			[context, before, before_tab]() mutable {
				ApplyEditorSelectionAndFocus(
					*context,
					before,
					GetSceneHierarchyTabWindow(before_tab)
				);
			},
			[context, after, tab]() mutable {
				ApplyEditorSelectionAndFocus(
					*context,
					after,
					GetSceneHierarchyTabWindow(tab)
				);
			}
		),
		false,
		runtime,
		runtime
	);

	return true;
}

bool SetSceneListTab(EditorContext& ctx, SceneListTab tab) {
	if (ctx.local.selection.scene_list_tab == tab) {
		return false;
	}

	const EditorSelection before{ ctx.local.selection };
	EditorSelection after{ before };
	after.scene_list_tab = tab;

	if (tab == SceneListTab::Scenes) {
		after.selected_screen_effect.reset();
		after.inspector_tab = InspectorTab::Primary;
	}

	const bool runtime{ ctx.editor.IsPlaying() };
	EditorContext* context{ std::addressof(ctx) };
	const auto before_tab{ before.scene_list_tab };

	ctx.undo.Execute(
		std::make_unique<ActionEditorCommand>(
			tab == SceneListTab::Scenes
				? "Select Scenes Tab"
				: "Select Screen Effects Tab",
			[context, before, before_tab]() mutable {
				ApplyEditorSelection(*context, before);
				RequestSceneListTabFocus(*context, before_tab);
			},
			[context, after, tab]() mutable {
				ApplyEditorSelection(*context, after);
				RequestSceneListTabFocus(*context, tab);
			}
		),
		false,
		runtime,
		runtime
	);

	return true;
}

bool SyncVisibleSceneListTab(EditorContext& ctx, SceneListTab tab) {
	auto& pending{ PendingSceneListTabFocus() };
	const auto it{ pending.find(std::addressof(ctx)) };

	if (it != pending.end()) {
		if (it->second == tab) {
			pending.erase(it);
		}
		return false;
	}

	if (ctx.local.selection.scene_list_tab == tab) {
		return false;
	}

	return SetSceneListTab(ctx, tab);
}


bool SetInspectorTab(EditorContext& ctx, InspectorTab tab) {
	if (ctx.local.selection.inspector_tab == tab) {
		return false;
	}

	if (tab == InspectorTab::ScreenEffect &&
		!ctx.local.selection.selected_screen_effect.has_value()) {
		return false;
	}

	const EditorSelection before{ ctx.local.selection };
	EditorSelection after{ before };
	after.inspector_tab = tab;

	const bool runtime{ ctx.editor.IsPlaying() };
	EditorContext* context{ std::addressof(ctx) };
	const auto before_tab{ before.inspector_tab };
	const auto before_mode{ before.mode };
	const auto after_mode{ after.mode };

	ctx.undo.Execute(
		std::make_unique<ActionEditorCommand>(
			tab == InspectorTab::ScreenEffect
				? "Select Screen Effect Inspector Tab"
				: "Select Primary Inspector Tab",
			[context, before, before_tab, before_mode]() mutable {
				ApplyEditorSelectionAndFocus(
					*context,
					before,
					GetInspectorTabWindow(before_tab, before_mode)
				);
			},
			[context, after, tab, after_mode]() mutable {
				ApplyEditorSelectionAndFocus(
					*context,
					after,
					GetInspectorTabWindow(tab, after_mode)
				);
			}
		),
		false,
		runtime,
		runtime
	);

	return true;
}

bool ClearSelectedScreenEffect(
	EditorContext& ctx,
	std::string label
) {
	if (!ctx.local.selection.selected_screen_effect.has_value()) {
		return false;
	}

	EditorSelection selection{ ctx.local.selection };
	selection.selected_screen_effect.reset();
	selection.inspector_tab = InspectorTab::Primary;

	return SetEditorSelection(
		ctx,
		std::move(selection),
		std::move(label),
		true,
		ctx.editor.IsPlaying()
	);
}

} // namespace ptgn::editor
