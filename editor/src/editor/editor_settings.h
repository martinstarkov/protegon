#pragma once

#include <cstdint>

#include "serialization/serialize.h"

namespace ptgn::editor {

enum class PrefabDeleteInstanceBehavior : std::uint8_t {
	Bake,
	Delete,
};
PTGN_REFLECT_ENUM(PrefabDeleteInstanceBehavior);

struct EditorSettings {
	bool entity_picking{ true };
	bool render_only_selected_scene{ true };
	bool gizmo_uses_local_orientation{ false };
	bool show_read_only_inspector_data{ false };
	bool show_read_only_scene_data{ false };
	bool show_managed_ui_parts{ false };
	bool show_imgui_metrics{ false };
	bool preview_screen_effects{ true };
	int content_browser_items_per_row{ 8 };
	bool content_browser_search_entire_tree{ false };

	/// @brief What happens to linked scene instances when their source prefab asset is deleted.
	PrefabDeleteInstanceBehavior prefab_delete_instance_behavior{
		PrefabDeleteInstanceBehavior::Bake
	};

	constexpr bool operator==(const EditorSettings&) const = default;

	PTGN_REFLECT(
		EditorSettings,
		entity_picking,
		render_only_selected_scene,
		gizmo_uses_local_orientation,
		show_read_only_inspector_data,
		show_read_only_scene_data,
		show_managed_ui_parts,
		show_imgui_metrics,
		preview_screen_effects,
		content_browser_items_per_row,
		content_browser_search_entire_tree,
		prefab_delete_instance_behavior
	)
};

} // namespace ptgn::editor
