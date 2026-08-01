#include "commands/editor_commands.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "commands/editor_command.h"
#include "commands/entity/create_entity.h"
#include "commands/entity/destroy_entity.h"
#include "commands/entity/entity_reference.h"
#include "commands/entity/entity_snapshot.h"
#include "commands/entity/rename_entity.h"
#include "commands/entity/reparent_entity.h"
#include "commands/scene/load_scene.h"
#include "core/assert.h"
#include "core/editor.h"
#include "core/editor_context.h"
#include "panels/scene_list.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/entity_serialization.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn::editor {

namespace {

struct PrefabAssetState {
	PrefabKey key;
	std::optional<Prefab> prefab;
};

void SavePrefabState(
	AssetManager& assets,
	const path& project_root,
	Prefab prefab
) {
	const auto source_path{
		GetPrefabSourcePath(prefab.key)
	};

	assets.SavePrefab(
		std::move(prefab),
		project_root / source_path,
		source_path
	);
}

void ApplyPrefabAssetStates(
	EditorContext& context,
	const path& project_root,
	const std::vector<PrefabAssetState>& states,
	const EditorSelection& selection
) {
	auto& assets{
		context.editor.GetAssetManager()
	};

	// Remove every affected key first. Do not gate this on AssetManager::Has();
	// a stale catalog or an unloaded prefab can otherwise leave the source file
	// behind and cause it to be rediscovered immediately after undo.
	for (const auto& state : states) {
		(void)assets.RemovePrefab(
			state.key,
			true
		);

		std::error_code error;
		std::filesystem::remove(
			project_root /
			GetPrefabSourcePath(state.key),
			error
		);
	}

	for (const auto& state : states) {
		if (!state.prefab) {
			continue;
		}

		Prefab prefab{ *state.prefab };
		prefab.key = state.key;

		SavePrefabState(
			assets,
			project_root,
			std::move(prefab)
		);
	}

	ApplyEditorSelection(
		context,
		selection
	);

	context.local.state.is_dirty = true;
}

class PrefabAssetCommand final : public EditorCommand {
public:
	PrefabAssetCommand(
		EditorContext& context,
		path project_root,
		std::string label,
		std::vector<PrefabAssetState> before,
		std::vector<PrefabAssetState> after,
		EditorSelection before_selection,
		EditorSelection after_selection
	) :
		context_{ std::addressof(context) },
		project_root_{ std::move(project_root) },
		label_{ std::move(label) },
		before_{ std::move(before) },
		after_{ std::move(after) },
		before_selection_{ std::move(before_selection) },
		after_selection_{ std::move(after_selection) } {}

	void Undo() override {
		ApplyPrefabAssetStates(
			*context_,
			project_root_,
			before_,
			before_selection_
		);
	}

	void Redo() override {
		ApplyPrefabAssetStates(
			*context_,
			project_root_,
			after_,
			after_selection_
		);
	}

	[[nodiscard]] std::string_view Label() const override {
		return label_;
	}

private:
	EditorContext* context_{ nullptr };
	path project_root_;
	std::string label_;
	std::vector<PrefabAssetState> before_;
	std::vector<PrefabAssetState> after_;
	EditorSelection before_selection_;
	EditorSelection after_selection_;
};

[[nodiscard]] std::optional<Prefab> CapturePrefabAsset(
	AssetManager& assets,
	const PrefabKey& key
) {
	if (!assets.Has(key)) {
		return std::nullopt;
	}

	auto prefab_asset{
		impl::AssetAccessor{ assets }.Get<Prefab>(key)
	};

	return prefab_asset.get();
}

[[nodiscard]] EditorSelection SelectPrefab(
	EditorSelection selection,
	std::optional<PrefabKey> key,
	SerializedEntityPath entity_path = {}
) {
	selection.selected_prefab = std::move(key);

	if (selection.selected_prefab) {
		selection.selected_prefab_entity_path =
			std::move(entity_path);
	} else {
		selection.selected_prefab_entity_path.clear();
	}

	selection.mode = EditorSelectionMode::Prefabs;
	return selection;
}

[[nodiscard]] bool CanCreatePrefabAtKey(
	const AssetManager& assets,
	const path& project_root,
	const PrefabKey& key
) {
	return !key.value.empty() &&
		!assets.Has(key) &&
		!assets.HasCatalogAsset(key) &&
		!FileExists(
			project_root /
			GetPrefabSourcePath(key)
		);
}

bool SerializedEntityContains(
	const SerializedEntity& serialized,
	UUID uuid
) {
	PTGN_ASSERT(
		serialized.uuid.has_value(),
		"Snapshot serialized entity must contain a UUID"
	);

	if (*serialized.uuid == uuid) {
		return true;
	}

	for (const auto& child : serialized.children) {
		if (SerializedEntityContains(child, uuid)) {
			return true;
		}
	}

	return false;
}

bool SnapshotContains(
	const EntitySnapshot& snapshot,
	UUID uuid
) {
	return SerializedEntityContains(
		snapshot.root,
		uuid
	);
}

EditorSelection SelectEntity(EditorSelection selection, Entity entity) {
	auto& scene{ entity.GetScene() };
	selection.selected_scene_key = scene.GetTag();
	selection.selected_scene_runtime = scene.IsRuntime();
	selection.SetEntityUUID(scene.GetTag(), scene.IsRuntime(), entity.Get<UUID>());
	selection.mode = EditorSelectionMode::SceneHierarchy;
	return selection;
}

std::optional<EntityReference> ParentReference(Entity entity) {
	return HasParent(entity)
		? std::optional<EntityReference>{ MakeEntityReference(GetParent(entity)) }
		: std::nullopt;
}

Entity DuplicateEntityNode(Scene& scene, Entity source) {
	PTGN_ASSERT(source);
	PTGN_ASSERT(source.Has<Tag>());
	PTGN_ASSERT(&source.GetScene() == &scene);

	Entity duplicate{
		scene.CopyEntity(
			source,
			source.Get<Tag>()
		)
	};

	// CopyEntity copied the source hierarchy references.
	// Remove them because the duplicated hierarchy is rebuilt below.
	duplicate.Remove<impl::Parent, impl::Children>();

	if (HasChildren(source)) {
		auto children{ GetChildren(source) };
		SortByLocalDepth(children);

		for (Entity child : children) {
			Entity duplicate_child{
				DuplicateEntityNode(scene, child)
			};
			SetParent(duplicate_child, duplicate);
		}
	}

	return duplicate;
}

void ApplyParent(Entity child, Entity parent, bool preserve_world_transform) {
	std::optional<Transform> world_transform;
	if (preserve_world_transform && child.Has<Transform>()) {
		world_transform = GetWorldTransform(child);
	}

	if (parent) {
		SetParent(child, parent);
	} else if (HasParent(child)) {
		RemoveParent(child);
	}

	if (world_transform) {
		SetWorldTransform(child, *world_transform);
	}
}

} // namespace

EditorCommands::EditorCommands(EditorContext* context) : context_{ context } {
	if (context_) {
		editor_ = &context_->editor;
		undo_stack_ = &context_->undo;
	}
}

void EditorCommands::Bind(EditorContext& context) {
	context_ = &context;
	editor_ = &context.editor;
	undo_stack_ = &context.undo;
}

Entity EditorCommands::CreateEntity(std::string_view name) {
	PTGN_ASSERT(context_);

	Scene* scene{ ResolveSelectedScene(*context_) };
	if (!scene) {
		return {};
	}

	const EditorSelection before{ context_->local.selection };
	Entity entity{ scene->CreateEntity(Tag{ std::string{ name } }) };
	scene->Refresh();
	return RecordCreatedEntity(entity, before);
}

Entity EditorCommands::RecordCreatedEntity(
	Entity entity,
	EditorSelection before_selection
) {
	PTGN_ASSERT(context_);

	if (!entity) {
		return {};
	}

	auto reference{ MakeEntityReference(entity) };
	auto snapshot{ CaptureEntitySnapshot(entity) };
	auto after_selection{ SelectEntity(before_selection, entity) };

	ApplyEditorSelection(*context_, after_selection);

	context_->undo.PushApplied(std::make_unique<CreateEntityCommand>(
		*context_,
		reference,
		std::move(snapshot),
		std::move(before_selection),
		std::move(after_selection)
	));

	return reference.Resolve(context_->editor);
}

Entity EditorCommands::DuplicateEntity(Entity entity) {
	PTGN_ASSERT(context_);

	if (!entity) {
		return {};
	}

	Scene& scene{ entity.GetScene() };
	const EditorSelection before{ context_->local.selection };
	Entity parent{
		HasParent(entity)
			? GetParent(entity)
			: Entity{}
	};
	Entity duplicate{
		DuplicateEntityNode(scene, entity)
	};

	if (parent) {
		SetParent(duplicate, parent);
	}

	scene.Refresh();
	return RecordCreatedEntity(duplicate, before);
}

void EditorCommands::DeleteEntity(Entity entity) {
	PTGN_ASSERT(context_);

	if (!entity) {
		return;
	}

	auto reference{ MakeEntityReference(entity) };
	auto snapshot{ CaptureEntitySnapshot(entity) };
	const EditorSelection before{ context_->local.selection };
	EditorSelection after{ before };

	const auto selected_uuid{ before.GetEntityUUID(reference.scene_key, reference.runtime) };
	if (selected_uuid && SnapshotContains(snapshot, *selected_uuid)) {
		after.SetEntityUUID(reference.scene_key, reference.runtime, std::nullopt);
	}

	context_->undo.Execute(std::make_unique<DeleteEntityCommand>(
		*context_,
		std::move(reference),
		std::move(snapshot),
		before,
		std::move(after)
	));
}

void EditorCommands::RenameEntity(Entity entity, std::string_view new_name) {
	PTGN_ASSERT(context_);

	if (!entity || !entity.Has<Tag>()) {
		return;
	}

	const std::string before{ entity.Get<Tag>().value };
	const std::string after{ new_name };
	if (before == after) {
		return;
	}

	context_->undo.Execute(std::make_unique<RenameEntityCommand>(
		context_->editor,
		MakeEntityReference(entity),
		before,
		after
	));
}

void EditorCommands::ReparentEntity(
	Entity child,
	Entity new_parent,
	bool preserve_world_transform
) {
	PTGN_ASSERT(context_);

	if (!child || (new_parent && &child.GetScene() != &new_parent.GetScene())) {
		return;
	}

	const auto before_parent{ ParentReference(child) };
	const std::optional<Transform> before_transform{
		child.Has<Transform>()
			? std::optional<Transform>{ child.Get<Transform>() }
			: std::nullopt
	};

	ApplyParent(child, new_parent, preserve_world_transform);

	const auto after_parent{ ParentReference(child) };
	const std::optional<Transform> after_transform{
		child.Has<Transform>()
			? std::optional<Transform>{ child.Get<Transform>() }
			: std::nullopt
	};

	if (before_parent == after_parent && before_transform == after_transform) {
		return;
	}

	context_->undo.PushApplied(std::make_unique<ReparentEntityCommand>(
		context_->editor,
		MakeEntityReference(child),
		before_parent,
		after_parent,
		before_transform,
		after_transform
	));
}

PrefabKey EditorCommands::CreatePrefabAsset(Prefab prefab) {
	PTGN_ASSERT(context_);

	const auto project_root{
		context_->editor.GetProjectRoot()
	};

	if (!project_root ||
		!CanCreatePrefabAtKey(
			context_->editor.GetAssetManager(),
			*project_root,
			prefab.key
		)) {
		return {};
	}

	const PrefabKey key{ prefab.key };
	const EditorSelection before_selection{
		context_->local.selection
	};
	const EditorSelection after_selection{
		SelectPrefab(
			before_selection,
			key,
			{}
		)
	};

	context_->undo.Execute(
		std::make_unique<PrefabAssetCommand>(
			*context_,
			*project_root,
			"Create Prefab",
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = key,
					.prefab = std::nullopt,
				},
			},
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = key,
					.prefab = std::move(prefab),
				},
			},
			before_selection,
			after_selection
		)
	);

	return key;
}

bool EditorCommands::DeletePrefabAsset(
	const PrefabKey& key
) {
	PTGN_ASSERT(context_);

	const auto project_root{
		context_->editor.GetProjectRoot()
	};

	if (!project_root) {
		return false;
	}

	auto& assets{
		context_->editor.GetAssetManager()
	};

	auto prefab{
		CapturePrefabAsset(assets, key)
	};

	if (!prefab) {
		return false;
	}

	const EditorSelection before_selection{
		context_->local.selection
	};
	EditorSelection after_selection{
		before_selection
	};

	if (after_selection.selected_prefab == key) {
		after_selection = SelectPrefab(
			std::move(after_selection),
			std::nullopt
		);
	}

	context_->undo.Execute(
		std::make_unique<PrefabAssetCommand>(
			*context_,
			*project_root,
			"Delete Prefab",
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = key,
					.prefab = std::move(prefab),
				},
			},
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = key,
					.prefab = std::nullopt,
				},
			},
			before_selection,
			after_selection
		)
	);

	return true;
}

bool EditorCommands::RenamePrefabAsset(
	const PrefabKey& old_key,
	const PrefabKey& new_key
) {
	PTGN_ASSERT(context_);

	if (old_key == new_key) {
		return false;
	}

	const auto project_root{
		context_->editor.GetProjectRoot()
	};

	if (!project_root) {
		return false;
	}

	auto& assets{
		context_->editor.GetAssetManager()
	};

	auto old_prefab{
		CapturePrefabAsset(assets, old_key)
	};

	if (!old_prefab ||
		!CanCreatePrefabAtKey(
			assets,
			*project_root,
			new_key
		)) {
		return false;
	}

	Prefab renamed{ *old_prefab };
	renamed.key = new_key;

	const EditorSelection before_selection{
		context_->local.selection
	};
	EditorSelection after_selection{
		before_selection
	};

	if (after_selection.selected_prefab == old_key) {
		after_selection.selected_prefab = new_key;
	}

	context_->undo.Execute(
		std::make_unique<PrefabAssetCommand>(
			*context_,
			*project_root,
			"Rename Prefab",
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = old_key,
					.prefab = std::move(old_prefab),
				},
				PrefabAssetState{
					.key = new_key,
					.prefab = std::nullopt,
				},
			},
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = old_key,
					.prefab = std::nullopt,
				},
				PrefabAssetState{
					.key = new_key,
					.prefab = std::move(renamed),
				},
			},
			before_selection,
			after_selection
		)
	);

	return true;
}

PrefabKey EditorCommands::DuplicatePrefabAsset(
	const PrefabKey& source_key,
	PrefabKey duplicate_key
) {
	PTGN_ASSERT(context_);

	const auto project_root{
		context_->editor.GetProjectRoot()
	};

	if (!project_root) {
		return {};
	}

	auto& assets{
		context_->editor.GetAssetManager()
	};

	auto source{
		CapturePrefabAsset(assets, source_key)
	};

	if (!source ||
		!CanCreatePrefabAtKey(
			assets,
			*project_root,
			duplicate_key
		)) {
		return {};
	}

	Prefab duplicate{ *source };
	duplicate.key = duplicate_key;

	const EditorSelection before_selection{
		context_->local.selection
	};
	const EditorSelection after_selection{
		SelectPrefab(
			before_selection,
			duplicate_key,
			{}
		)
	};

	context_->undo.Execute(
		std::make_unique<PrefabAssetCommand>(
			*context_,
			*project_root,
			"Duplicate Prefab",
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = duplicate_key,
					.prefab = std::nullopt,
				},
			},
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = duplicate_key,
					.prefab = std::move(duplicate),
				},
			},
			before_selection,
			after_selection
		)
	);

	return duplicate_key;
}

bool EditorCommands::AddPrefabChild(
	const PrefabKey& key,
	SerializedEntityPath parent_path,
	SerializedEntity child
) {
	PTGN_ASSERT(context_);

	const auto project_root{
		context_->editor.GetProjectRoot()
	};

	if (!project_root) {
		return false;
	}

	auto& assets{
		context_->editor.GetAssetManager()
	};

	auto before_prefab{
		CapturePrefabAsset(assets, key)
	};

	if (!before_prefab) {
		return false;
	}

	Prefab after_prefab{ *before_prefab };
	auto* parent{
		ResolveSerializedEntity(
			after_prefab.root,
			parent_path
		)
	};

	if (!parent) {
		return false;
	}

	const std::size_t child_index{
		parent->children.size()
	};
	parent->children.emplace_back(
		std::move(child)
	);

	auto child_path{ parent_path };
	child_path.emplace_back(child_index);

	const EditorSelection before_selection{
		context_->local.selection
	};
	const EditorSelection after_selection{
		SelectPrefab(
			before_selection,
			key,
			child_path
		)
	};

	context_->undo.Execute(
		std::make_unique<PrefabAssetCommand>(
			*context_,
			*project_root,
			"Add Prefab Child",
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = key,
					.prefab = std::move(before_prefab),
				},
			},
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = key,
					.prefab = std::move(after_prefab),
				},
			},
			before_selection,
			after_selection
		)
	);

	return true;
}

bool EditorCommands::DuplicatePrefabEntity(
	const PrefabKey& key,
	SerializedEntityPath entity_path
) {
	PTGN_ASSERT(context_);

	if (entity_path.empty()) {
		return false;
	}

	const auto project_root{
		context_->editor.GetProjectRoot()
	};

	if (!project_root) {
		return false;
	}

	auto& assets{
		context_->editor.GetAssetManager()
	};

	auto before_prefab{
		CapturePrefabAsset(assets, key)
	};

	if (!before_prefab) {
		return false;
	}

	Prefab after_prefab{ *before_prefab };
	auto parent_path{ entity_path };
	const std::size_t source_index{
		parent_path.back()
	};
	parent_path.pop_back();

	auto* parent{
		ResolveSerializedEntity(
			after_prefab.root,
			parent_path
		)
	};

	if (!parent ||
		source_index >= parent->children.size()) {
		return false;
	}

	const std::size_t duplicate_index{
		source_index + 1
	};

	SerializedEntity duplicate{
		parent->children[source_index]
	};

	parent->children.insert(
		parent->children.begin() +
			static_cast<std::ptrdiff_t>(duplicate_index),
		std::move(duplicate)
	);

	auto duplicate_path{ parent_path };
	duplicate_path.emplace_back(duplicate_index);

	const EditorSelection before_selection{
		context_->local.selection
	};
	const EditorSelection after_selection{
		SelectPrefab(
			before_selection,
			key,
			duplicate_path
		)
	};

	context_->undo.Execute(
		std::make_unique<PrefabAssetCommand>(
			*context_,
			*project_root,
			"Duplicate Prefab Entity",
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = key,
					.prefab = std::move(before_prefab),
				},
			},
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = key,
					.prefab = std::move(after_prefab),
				},
			},
			before_selection,
			after_selection
		)
	);

	return true;
}

bool EditorCommands::DeletePrefabEntity(
	const PrefabKey& key,
	SerializedEntityPath entity_path
) {
	PTGN_ASSERT(context_);

	if (entity_path.empty()) {
		return false;
	}

	const auto project_root{
		context_->editor.GetProjectRoot()
	};

	if (!project_root) {
		return false;
	}

	auto& assets{
		context_->editor.GetAssetManager()
	};

	auto before_prefab{
		CapturePrefabAsset(assets, key)
	};

	if (!before_prefab) {
		return false;
	}

	Prefab after_prefab{ *before_prefab };
	auto parent_path{ entity_path };
	const std::size_t child_index{
		parent_path.back()
	};
	parent_path.pop_back();

	auto* parent{
		ResolveSerializedEntity(
			after_prefab.root,
			parent_path
		)
	};

	if (!parent ||
		child_index >= parent->children.size()) {
		return false;
	}

	parent->children.erase(
		parent->children.begin() +
			static_cast<std::ptrdiff_t>(child_index)
	);

	const EditorSelection before_selection{
		context_->local.selection
	};
	const EditorSelection after_selection{
		SelectPrefab(
			before_selection,
			key,
			parent_path
		)
	};

	context_->undo.Execute(
		std::make_unique<PrefabAssetCommand>(
			*context_,
			*project_root,
			"Delete Prefab Entity",
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = key,
					.prefab = std::move(before_prefab),
				},
			},
			std::vector<PrefabAssetState>{
				PrefabAssetState{
					.key = key,
					.prefab = std::move(after_prefab),
				},
			},
			before_selection,
			after_selection
		)
	);

	return true;
}

Entity EditorCommands::CreatePrefabInstance(
	Scene& scene,
	const PrefabKey& key
) {
	PTGN_ASSERT(context_);

	if (!scene.ctx().asset.Has(key)) {
		return {};
	}

	return RecordCreatedEntity(
		scene.CreatePrefab(key),
		context_->local.selection
	);
}

void EditorCommands::SaveScene(const path& path) {
	PTGN_ASSERT(context_);

	if (Scene* scene{ ResolveSelectedScene(*context_) }) {
		SaveSceneFile(path, CaptureScene(*scene));
	}
}

void EditorCommands::LoadScene(const path& path) {
	PTGN_ASSERT(context_);

	Scene* scene{ ResolveSelectedScene(*context_) };
	if (!scene) {
		return;
	}

	context_->undo.Execute(std::make_unique<LoadSceneCommand>(
		*context_,
		scene->GetTag(),
		scene->IsRuntime(),
		CaptureScene(*scene),
		LoadSceneFile(path)
	));
}

} // namespace ptgn::editor
