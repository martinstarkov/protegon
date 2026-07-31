#pragma once

#include <optional>
#include <utility>

#include "commands/component/component_state_command.h"

namespace ptgn::editor {

template <typename T>
class AddComponentCommand final : public ComponentStateCommand<T> {
public:
	AddComponentCommand(
		Editor& editor,
		EntityReference entity,
		T component
	) :
		ComponentStateCommand<T>{
			editor,
			std::move(entity),
			std::nullopt,
			std::optional<T>{ std::move(component) },
			"Add Component"
		} {}
};

} // namespace ptgn::editor
