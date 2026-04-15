#pragma once

#include "commands/editor_command.h"

namespace ptgn::editor {

template <typename T>
class SetComponentValueCommand : public EditorCommand {
public:
	SetComponentValueCommand(T* target, const T& value) :
		target_{ target }, new_value_{ value }, old_value_{ *target } {}

	void Execute() override {
		*target_ = new_value_;
	}

	void Undo() override {
		*target_ = old_value_;
	}

private:
	T* target_{ nullptr };
	T new_value_;
	T old_value_;
};

} // namespace ptgn::editor