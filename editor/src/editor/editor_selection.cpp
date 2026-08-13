#include "editor/editor_selection.h"

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

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
	mode = EditorSelectionMode::SceneHierarchy;
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
	std::string label
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
		false
	);

	return true;
}

} // namespace ptgn::editor
