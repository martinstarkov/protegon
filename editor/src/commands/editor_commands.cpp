#include "commands/editor_commands.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "commands/entity/create_entity.h"
#include "commands/entity/destroy_entity.h"
#include "commands/entity/rename_entity.h"
#include "commands/entity/reparent_entity.h"
#include "commands/scene/load_scene.h"
#include "commands/scene/save_scene.h"
#include "commands/undo_stack.h"
#include "core/assert.h"
#include "core/editor_state.h"
#include "core/util/file.h"
#include "panels/scene_list.h"
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

EditorCommands::EditorCommands(UndoStack* undo_stack, SceneListPanel* scene_list) :
	scene_list_{ scene_list }, undo_stack_{ undo_stack } {}

Entity EditorCommands::CreateEntity(std::string_view name) {
	PTGN_ASSERT(scene_list_);
	PTGN_ASSERT(undo_stack_);
	auto command = std::make_unique<CreateEntityCommand>(scene_list_->GetSelectedScene(), name);

	auto raw{ command.get() };

	undo_stack_->Execute(std::move(command));

	PTGN_ASSERT(raw);

	return raw->GetEntity();
}

void EditorCommands::DeleteEntity(Entity entity) {
	PTGN_ASSERT(scene_list_);
	PTGN_ASSERT(undo_stack_);
	undo_stack_->Execute(
		std::make_unique<DeleteEntityCommand>(scene_list_->GetSelectedScene(), entity)
	);
}

void EditorCommands::SaveScene(const path& path) {
	PTGN_ASSERT(scene_list_);
	// Do NOT push to undo stack
	SaveSceneCommand command{ scene_list_->GetSelectedScene(), path };
	command.Execute();
}

void EditorCommands::LoadScene(const path& path) {
	PTGN_ASSERT(scene_list_);
	PTGN_ASSERT(undo_stack_);
	undo_stack_->Execute(std::make_unique<LoadSceneCommand>(scene_list_->GetSelectedScene(), path));
}

void EditorCommands::RenameEntity(Entity entity, std::string_view new_name) {
	PTGN_ASSERT(undo_stack_);
	undo_stack_->Execute(std::make_unique<RenameEntityCommand>(entity, new_name));
}

void EditorCommands::ReparentEntity(Entity child, Entity new_parent, bool ignore_parent_transform) {
	PTGN_ASSERT(undo_stack_);
	undo_stack_->Execute(
		std::make_unique<ReparentEntityCommand>(child, new_parent, ignore_parent_transform)
	);
}

} // namespace ptgn::editor