#include "commands/editor_commands.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

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
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/scene/scene.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/ecs/entity_serialization.h"

namespace ptgn::editor {

namespace {

bool SnapshotContains(const EntitySnapshot& snapshot, UUID uuid) {
	if (snapshot.uuid == uuid) {
		return true;
	}

	for (const auto& child : snapshot.children) {
		if (SnapshotContains(child, uuid)) {
			return true;
		}
	}

	return false;
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

	Entity duplicate{
		scene.CreateEntity(source.Get<Tag>())
	};
	DeserializeEntityComponents(
		SerializeEntityComponents(source),
		duplicate
	);

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
