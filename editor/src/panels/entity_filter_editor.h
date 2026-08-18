#pragma once

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

bool DrawEntityFilterButton(
	Scene* scene, Entity owner, EntityFilter& filter, EntityFilterEditorState& state
);

} // namespace editor::inspector

} // namespace ptgn
