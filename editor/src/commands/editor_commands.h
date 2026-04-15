#pragma once

#include <concepts>
#include <memory>
#include <string>

#include "commands/component/add_component.h"
#include "commands/component/remove_component.h"
#include "commands/component/set_component.h"
#include "commands/undo_stack.h"
#include "core/editor_state.h"
#include "core/util/file.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Scene;

namespace editor {

class Editor;

class EditorCommands {
public:
	EditorCommands() = default;
	EditorCommands(UndoStack* undo_stack, EditorState* state);

	Entity CreateEntity(const std::string& name);
	void DeleteEntity(Entity entity);

	void RenameEntity(Entity entity, const std::string& new_name);

	void ReparentEntity(Entity child, Entity new_parent, bool ignore_parent_transform);

	template <typename T>
	void SetComponentValue(T* target, const T& value) {
		undo_stack_->Execute(std::make_unique<SetComponentValueCommand<T>>(target, value));
	}

	template <std::copy_constructible T>
	void RemoveComponent(Entity entity) {
		undo_stack_->Execute(std::make_unique<RemoveComponentCommand<T>>(entity));
	}

	template <std::copy_constructible T>
	void AddComponent(Entity entity, const T& component) {
		undo_stack_->Execute(std::make_unique<AddComponentCommand<T>>(entity, component));
	}

	void SaveScene(const path& path);
	void LoadScene(const path& path);

private:
	friend class Editor;

	EditorState* state_{ nullptr };
	UndoStack* undo_stack_{ nullptr };
};

} // namespace editor

} // namespace ptgn