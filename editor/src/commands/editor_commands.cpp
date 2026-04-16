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
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

EditorCommands::EditorCommands(UndoStack* undo_stack, EditorState* state) :
	state_{ state }, undo_stack_{ undo_stack } {}

Entity EditorCommands::CreateEntity(std::string_view name) {
	PTGN_ASSERT(state_);
	PTGN_ASSERT(undo_stack_);
	auto command = std::make_unique<CreateEntityCommand>(state_->active_scene, name);

	auto raw{ command.get() };

	undo_stack_->Execute(std::move(command));

	PTGN_ASSERT(raw);

	return raw->GetEntity();
}

void EditorCommands::DeleteEntity(Entity entity) {
	PTGN_ASSERT(state_);
	PTGN_ASSERT(undo_stack_);
	undo_stack_->Execute(std::make_unique<DeleteEntityCommand>(state_->active_scene, entity));
}

void EditorCommands::SaveScene(const path& path) {
	PTGN_ASSERT(state_);
	// Do NOT push to undo stack
	SaveSceneCommand command{ state_->active_scene, path };
	command.Execute();
}

void EditorCommands::LoadScene(const path& path) {
	PTGN_ASSERT(state_);
	PTGN_ASSERT(undo_stack_);
	undo_stack_->Execute(std::make_unique<LoadSceneCommand>(state_->active_scene, path));
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