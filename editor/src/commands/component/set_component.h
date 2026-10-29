#pragma once

#include <optional>
#include <string>
#include <utility>

#include "commands/component/component_state_command.h"

namespace ptgn::editor {

template <typename T>
class SetComponentValueCommand final : public ComponentStateCommand<T> {
public:
	SetComponentValueCommand(
		Editor& editor,
		EntityReference entity,
		std::optional<T> before,
		T after
	) :
		ComponentStateCommand<T>{
			editor,
			std::move(entity),
			std::move(before),
			std::optional<T>{ std::move(after) },
			"Set Component Value"
		} {}
};

} // namespace ptgn::editor
