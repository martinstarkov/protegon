#pragma once

#include <concepts>

#include "commands/editor_command.h"
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

template <std::copy_constructible T>
class AddComponentCommand : public EditorCommand {
public:
	AddComponentCommand(Entity entity, const T& component) :
		entity_{ entity }, component_{ component } {}

	void Execute() override {
		entity_.Add<T>(component_);
	}

	void Undo() override {
		entity_.Remove<T>();
	}

private:
	Entity entity_;
	T component_;
};

} // namespace ptgn::editor