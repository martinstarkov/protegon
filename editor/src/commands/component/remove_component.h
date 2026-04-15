#pragma once

#include <concepts>

#include "commands/editor_command.h"
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

template <std::copy_constructible T>
class RemoveComponentCommand : public EditorCommand {
public:
	RemoveComponentCommand(Entity entity) : entity_{ entity }, backup_{ entity.Get<T>() } {}

	void Execute() override {
		entity_.Remove<T>();
	}

	void Undo() override {
		entity_.Add<T>(backup_);
	}

private:
	Entity entity_;
	T backup_;
};

} // namespace ptgn::editor