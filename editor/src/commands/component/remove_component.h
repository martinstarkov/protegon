#pragma once

#include <optional>
#include <utility>

#include "commands/component/component_state_command.h"

namespace ptgn::editor {

template <typename T>
class RemoveComponentCommand final : public ComponentStateCommand<T> {
public:
	RemoveComponentCommand(
		Editor& editor,
		EntityReference entity,
		T component
	) :
		ComponentStateCommand<T>{
			editor,
			std::move(entity),
			std::optional<T>{ std::move(component) },
			std::nullopt,
			"Remove Component"
		} {}
};

} // namespace ptgn::editor
