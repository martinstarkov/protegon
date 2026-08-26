#pragma once

#include "serialization/serialize.h"

namespace ptgn::editor {

struct EditorSettings {
	bool entity_picking{ true };
	bool render_only_selected_scene{ true };
	bool gizmo_uses_local_orientation{ false };
	bool show_read_only_inspector_data{ false };
	bool show_read_only_scene_data{ false };
	bool show_imgui_metrics{ false };
	bool preview_screen_effects{ true };
	int content_browser_items_per_row{ 8 };
	bool content_browser_search_entire_tree{ false };

	constexpr bool operator==(const EditorSettings&) const = default;

	PTGN_REFLECT(
		EditorSettings,
		entity_picking,
		render_only_selected_scene,
		gizmo_uses_local_orientation,
		show_read_only_inspector_data,
		show_read_only_scene_data,
		show_imgui_metrics,
		preview_screen_effects,
		content_browser_items_per_row,
		content_browser_search_entire_tree
	)
};

} // namespace ptgn::editor
