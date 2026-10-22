#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/math/vector2.h"
#include "editor/paint/paint_editor.h"
#include "runtime/asset/asset_key.h"
#include "serialization/serialize.h"

namespace ptgn::editor {

/// @brief One texture's slicing metadata in the shared editor only paint library.
struct PaintTileSliceSettingsState {
	TextureKey texture{};
	V2_int tile_size{ 32, 32 };

	bool operator==(const PaintTileSliceSettingsState&) const = default;

	PTGN_REFLECT(PaintTileSliceSettingsState, texture, tile_size)
};

/// @brief One authored tile entry in the shared editor only paint library.
struct PaintTileLibraryEntryState {
	std::string id{};
	std::string name{};
	TextureKey texture{};
	V2_int slice{};
	std::string group{ "Ungrouped" };

	bool operator==(const PaintTileLibraryEntryState&) const = default;

	PTGN_REFLECT(PaintTileLibraryEntryState, id, name, texture, slice, group)
};

/// @brief An imported autotile is one palette item with an authored layout.
struct PaintAutotileLibraryEntryState {
	std::string name{};
	TextureKey texture{};
	PaintAutotileFormat format{ PaintAutotileFormat::Classic15 };
	std::string group{ "Ungrouped" };
	std::optional<bool> show_slices{};

	bool operator==(const PaintAutotileLibraryEntryState&) const = default;
	PTGN_REFLECT(PaintAutotileLibraryEntryState, name, texture, format, group, show_slices)
};

struct PaintPrefabGroupAssignmentState {
	PrefabKey prefab{};
	std::string group{ "Ungrouped" };

	bool operator==(const PaintPrefabGroupAssignmentState&) const = default;

	PTGN_REFLECT(PaintPrefabGroupAssignmentState, prefab, group)
};

/// @brief Shared editor paint exclusions for one scene entity layer.
struct PaintEntityExclusionMaskState {
	std::string scene_key{};
	std::uint64_t layer_id{};
	std::vector<V2_int> cells{};

	bool operator==(const PaintEntityExclusionMaskState&) const = default;

	PTGN_REFLECT(PaintEntityExclusionMaskState, scene_key, layer_id, cells)
};

/// @brief Shared editor paint organization stored in the tracked .ptgneditor file.
struct PaintProjectState {
	std::vector<std::string> tile_groups{};
	std::optional<std::vector<std::string>> autotile_groups{};
	std::vector<std::string> prefab_groups{};
	std::vector<PaintTileSliceSettingsState> tile_slice_settings{};
	std::vector<PaintTileLibraryEntryState> tiles{};
	std::optional<std::vector<PaintAutotileLibraryEntryState>> autotiles{};
	std::vector<PaintPrefabGroupAssignmentState> prefab_group_assignments{};
	std::vector<PaintWeightedTileSet> weighted_tile_sets{};
	std::vector<PaintWeightedPrefabSet> weighted_prefab_sets{};
	std::vector<PaintAutotileRuleSet> autotile_rule_sets{};
	std::vector<PaintEntityExclusionMaskState> entity_exclusion_masks{};

	bool operator==(const PaintProjectState&) const = default;

	PTGN_REFLECT(
		PaintProjectState, tile_groups, autotile_groups, prefab_groups, tile_slice_settings, tiles,
		autotiles, prefab_group_assignments, weighted_tile_sets, weighted_prefab_sets,
		autotile_rule_sets, entity_exclusion_masks
	)
};

/// @brief User specific paint selection stored in .ptgnlocal.
struct PaintLocalState {
	PaintTool tool{ PaintTool::Select };
	PaintRecipeState recipe{};
	PaintBrushShape brush_shape{ PaintBrushShape::Circle };
	PaintBrushShape selection_brush_shape{ PaintBrushShape::Circle };
	PaintSelectMode select_mode{ PaintSelectMode::ClickMarquee };
	PaintAreaMode area_mode{ PaintAreaMode::Fill };
	PaintMoveSnapMode move_snap{ PaintMoveSnapMode::Grid };
	int brush_diameter{ 1 };
	int selection_diameter{ 1 };
	int line_thickness{ 1 };
	int line_spacing{ 1 };
	int area_thickness{ 1 };
	bool line_align_rotation{};
	bool grid_visible{ true };
	V2_float entity_grid_size{ 32.0f, 32.0f };
	V2_float entity_grid_offset{};
	bool grid_aspect_locked{};
	float grid_locked_aspect{ 1.0f };
	int grid_major_every{ 8 };
	std::array<float, 4> grid_minor_color{ 1.0f, 1.0f, 1.0f, 0.07f };
	std::array<float, 4> grid_major_color{ 1.0f, 1.0f, 1.0f, 0.15f };
	float grid_minor_thickness{ 1.0f };
	float grid_major_thickness{ 1.0f };
	std::string selected_tile_entry_id{};
	PaintTileSource tile_source{};
	PrefabKey prefab_source{};

	bool operator==(const PaintLocalState&) const = default;

	PTGN_REFLECT(
		PaintLocalState, tool, recipe, brush_shape, selection_brush_shape, select_mode, area_mode,
		move_snap, brush_diameter, selection_diameter, line_thickness, line_spacing, area_thickness,
		line_align_rotation, grid_visible, entity_grid_size, entity_grid_offset, grid_aspect_locked,
		grid_locked_aspect, grid_major_every, grid_minor_color, grid_major_color,
		grid_minor_thickness, grid_major_thickness, selected_tile_entry_id, tile_source,
		prefab_source
	)
};

} // namespace ptgn::editor
