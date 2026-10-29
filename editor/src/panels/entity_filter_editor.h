#pragma once

#include <optional>
#include <span>
#include <string>

#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_filter.h"

namespace ptgn {

class Scene;

namespace editor::inspector {

struct EntityFilterEditorState {
	std::string hierarchy_filter{};
	std::string component_filter{};
	std::string tag_filter{};
	std::string query_filter{};
};

struct EntityFilterEditorOptions {
	bool show_none{ true };
	bool show_any{ true };
	bool show_entity{ true };
	bool show_components{ true };
	bool show_tags{ true };
	bool show_queries{ true };
	bool allow_select_owner{ true };
	bool exclude_owner{ false };
	bool single_entity_only{ false };
	bool hide_owner_descendants{ false };
	bool hide_excluded_descendants{ false };
	bool hide_managed_ui_parts{ false };
	float button_width{ -1.0f };
	const char* button_tooltip{ nullptr };
	bool append_filter_tooltip{ false };
	std::span<const Entity> excluded_entities{};
};

bool DrawEntityFilterButton(
	Scene* scene, Entity owner, EntityFilter& filter, EntityFilterEditorState& state,
	const EntityFilterEditorOptions& options = {}
);

bool DrawEntityFilterButton(
	Scene* scene, Entity owner, std::optional<EntityFilter>& filter, EntityFilterEditorState& state,
	const EntityFilterEditorOptions& options = {}
);

} // namespace editor::inspector

} // namespace ptgn
