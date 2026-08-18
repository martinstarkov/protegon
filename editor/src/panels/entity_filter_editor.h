#pragma once

#include <optional>
#include <string>

#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_filter.h"

namespace ptgn {

class Scene;

namespace editor::inspector {

struct EntityFilterEditorState {
	std::string hierarchy_filter;
	std::string component_filter;
	std::string group_filter;
	std::string query_filter;
};

struct EntityFilterEditorOptions {
	bool show_any{ true };
	bool show_entity{ true };
	bool show_components{ true };
	bool show_groups{ true };
	bool show_queries{ true };
	bool allow_select_owner{ true };
	bool exclude_owner{ false };
};

bool DrawEntityFilterButton(
	Scene* scene,
	Entity owner,
	EntityFilter& filter,
	EntityFilterEditorState& state,
	const EntityFilterEditorOptions& options = {}
);

bool DrawEntityFilterButton(
	Scene* scene,
	Entity owner,
	std::optional<EntityFilter>& filter,
	EntityFilterEditorState& state,
	const EntityFilterEditorOptions& options = {}
);

} // namespace editor::inspector

} // namespace ptgn
