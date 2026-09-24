#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/math/geometry/origin.h"
#include "editor/editor_selection.h"
#include "core/math/noise.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/viewport.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity_serialization.h"
#include "runtime/ecs/uuid.h"
#include "runtime/graphics/frame_context.h"
#include "runtime/world/entity_layer.h"
#include "runtime/world/paint_generator.h"
#include "runtime/world/tilemap.h"
#include "serialization/serialize.h"

struct ImDrawList;

namespace ptgn {
class Scene;
}

namespace ptgn::editor {

class EditorContext;

enum class PaintTool : std::uint8_t {
	Select,
	Move,
	Pencil,
	Brush,
	Line,
	Rectangle,
	Fill,
	Erase,
	Eyedropper,
};
PTGN_REFLECT_ENUM(PaintTool);

enum class PaintBrushOperation : std::uint8_t {
	Paint,
	Replace,
	ExclusionMask,
};
PTGN_REFLECT_ENUM(PaintBrushOperation);

enum class PaintBrushShape : std::uint8_t {
	Circle,
	Square,
};
PTGN_REFLECT_ENUM(PaintBrushShape);

enum class PaintMoveSnapMode : std::uint8_t {
	Grid,
	Free,
};
PTGN_REFLECT_ENUM(PaintMoveSnapMode);

enum class PaintSelectMode : std::uint8_t {
	ClickMarquee,
	Brush,
};
PTGN_REFLECT_ENUM(PaintSelectMode);

enum class PaintAreaMode : std::uint8_t {
	Fill,
	Outline,
	Corners,
	RandomFill,
};
PTGN_REFLECT_ENUM(PaintAreaMode);

enum class PaintCoverageMode : std::uint8_t {
	Solid,
	RandomDensity,
	RadialFalloff,
};
PTGN_REFLECT_ENUM(PaintCoverageMode);

enum class PaintSourceKind : std::uint8_t {
	Single,
	WeightedSet,
	Checkerboard,
	Autotile,
	Noise,
};
PTGN_REFLECT_ENUM(PaintSourceKind);

enum class PaintCommitMode : std::uint8_t {
	BakeOnCommit,
	KeepGenerator,
};
PTGN_REFLECT_ENUM(PaintCommitMode);

enum class PaintTilePlacementMode : std::uint8_t {
	Grid,
	Tile,
};
PTGN_REFLECT_ENUM(PaintTilePlacementMode);

enum class PaintAutotileFormat : std::uint8_t {
	Classic15,
	Blob47,
	Subset16,
	DualGrid16,
	Wang16,
};
PTGN_REFLECT_ENUM(PaintAutotileFormat);

enum class TileImportMode : std::uint8_t {
	Auto,
	Tileset,
	Individual,
};
PTGN_REFLECT_ENUM(TileImportMode);

struct PaintTileSource {
	TextureKey texture{};
	std::array<V2_float, 4> texture_coordinates{
		V2_float{ 0.0f, 0.0f }, V2_float{ 1.0f, 0.0f },
		V2_float{ 1.0f, 1.0f }, V2_float{ 0.0f, 1.0f }
	};
	V2_int pixel_size{ 32, 32 };
	V2_int slice{};

	[[nodiscard]] explicit operator bool() const {
		return static_cast<bool>(texture);
	}

	bool operator==(const PaintTileSource&) const = default;

	PTGN_REFLECT(PaintTileSource, texture, texture_coordinates, pixel_size, slice)
};

struct PaintWeightedTileEntry {
	PaintTileSource source{};
	float weight{ 1.0f };

	bool operator==(const PaintWeightedTileEntry&) const = default;
	PTGN_REFLECT(PaintWeightedTileEntry, source, weight)
};

struct PaintWeightedPrefabEntry {
	PrefabKey prefab{};
	float weight{ 1.0f };

	bool operator==(const PaintWeightedPrefabEntry&) const = default;
	PTGN_REFLECT(PaintWeightedPrefabEntry, prefab, weight)
};

struct PaintWeightedTileSet {
	std::string name{};
	std::vector<PaintWeightedTileEntry> entries{};

	bool operator==(const PaintWeightedTileSet&) const = default;
	PTGN_REFLECT(PaintWeightedTileSet, name, entries)
};

struct PaintWeightedPrefabSet {
	std::string name{};
	std::vector<PaintWeightedPrefabEntry> entries{};

	bool operator==(const PaintWeightedPrefabSet&) const = default;
	PTGN_REFLECT(PaintWeightedPrefabSet, name, entries)
};

struct PaintAutotileRuleSet {
	std::uint64_t id{};
	/// Internal/debug label only. Autotile rulesets are derived from a tilesheet + format.
	std::string name{};
	TextureKey texture{};
	PaintAutotileFormat format{ PaintAutotileFormat::Classic15 };
	std::vector<std::optional<PaintTileSource>> tiles{};

	bool operator==(const PaintAutotileRuleSet&) const = default;
	PTGN_REFLECT(PaintAutotileRuleSet, id, name, texture, format, tiles)
};

struct PaintNoiseThreshold {
	float minimum{};
	float maximum{ 1.0f };
	bool enabled{ true };
	PaintSourceKind source_kind{ PaintSourceKind::Single };
	std::optional<PaintTileSource> tile{};
	std::optional<PrefabKey> prefab{};
	std::string weighted_tile_set_name{};
	std::string weighted_prefab_set_name{};
	Origin origin{ Origin::Center };

	bool operator==(const PaintNoiseThreshold&) const = default;

	PTGN_REFLECT(PaintNoiseThreshold, minimum, maximum, enabled, source_kind, tile, prefab, weighted_tile_set_name, weighted_prefab_set_name, origin)
};

struct PaintNoiseState {
	NoiseType type{ NoiseType::Perlin };
	std::string name{ "Paint Noise" };
	int seed{ 1337 };
	float frequency{ 0.015f };
	int octaves{ 4 };
	float lacunarity{ 2.0f };
	float persistence{ 0.5f };
	V2_float offset{};
	std::vector<PaintNoiseThreshold> thresholds{};

	bool operator==(const PaintNoiseState&) const = default;

	PTGN_REFLECT(
		PaintNoiseState,
		type,
		name,
		seed,
		frequency,
		octaves,
		lacunarity,
		persistence,
		offset,
		thresholds
	)
};

struct PaintRecipeState {
	PaintSourceKind source_kind{ PaintSourceKind::Single };
	PaintCommitMode commit_mode{ PaintCommitMode::BakeOnCommit };
	PaintBrushOperation operation{ PaintBrushOperation::Paint };
	PaintCoverageMode coverage{ PaintCoverageMode::Solid };
	std::string weighted_tile_set_name{};
	std::string weighted_prefab_set_name{};
	std::optional<PaintTileSource> checker_tile{};
	std::optional<PrefabKey> checker_prefab{};
	PaintAutotileFormat autotile_format{ PaintAutotileFormat::Classic15 };
	TextureKey autotile_texture{};
	PaintTilePlacementMode tile_placement{ PaintTilePlacementMode::Tile };
	Origin tile_origin{ Origin::TopLeft };
	Origin entity_origin{ Origin::Center };
	float density{ 0.45f };
	float radial_inner{ 0.15f };
	float radial_outer{ 1.0f };
	float min_spacing{};
	bool avoid_exclusion_mask{ true };
	bool link_prefab_instances{ true };
	bool random_rotation{};
	float rotation_min{};
	float rotation_max{ 360.0f };
	bool random_scale{};
	float scale_min{ 0.8f };
	float scale_max{ 1.2f };
	PaintNoiseState noise{};
	bool show_noise_preview{};
	bool show_generated_preview{ true };
	float noise_preview_alpha{ 0.45f };

	bool operator==(const PaintRecipeState&) const = default;

	PTGN_REFLECT(
		PaintRecipeState,
		source_kind,
		commit_mode,
		operation,
		coverage,
		weighted_tile_set_name,
		weighted_prefab_set_name,
		checker_tile,
		checker_prefab,
		autotile_format,
		autotile_texture,
		tile_placement,
		tile_origin,
		entity_origin,
		density,
		radial_inner,
		radial_outer,
		min_spacing,
		avoid_exclusion_mask,
		link_prefab_instances,
		random_rotation,
		rotation_min,
		rotation_max,
		random_scale,
		scale_min,
		scale_max,
		noise,
		show_noise_preview,
		show_generated_preview,
		noise_preview_alpha
	)
};

/// @brief Editor-side authoring controller for scene-layer painting, procedural generators and
/// project paint-asset organization.
class PaintEditor {
public:
	/// @brief Top viewport row: paint-tool buttons followed by the grid/settings button.
	/// ViewportPanel owns the right-aligned runtime/camera controls on this same row.
	void DrawViewportToolButtons(EditorContext& ctx);

	/// @brief Second viewport row: context-sensitive settings for the active paint tool.
	bool DrawViewportOptionsToolbar(EditorContext& ctx);

	/// @brief Paint source, coverage, and infinite-generator authoring panel.
	void DrawRecipePanel(EditorContext& ctx);


	/// Draws the Tiles dock window and returns whether it was the visible dock tab this frame.
	bool DrawTilesPanel(EditorContext& ctx);
	void DrawTileInspector(EditorContext& ctx);
	void DrawGeneratorInspector(EditorContext& ctx, Entity generator);

	/// Draw tilemap/generator/grid/tool overlays and process paint input. Returns true when the paint
	/// tool consumed the current pointer interaction. Selection is handled entirely by PaintEditor so
	/// click, marquee, brush, generator, entity and tile selection all share one state machine.
	bool DrawViewportAndHandleInput(
		EditorContext& ctx,
		Scene& scene,
		Viewport image_viewport,
		Viewport presentation_viewport,
		const FrameContext& frame_context
	);

	[[nodiscard]] PaintTool GetTool() const { return tool_; }
	void SetTool(PaintTool tool);


	[[nodiscard]] SceneLayerId GetActiveLayer(const Scene& scene) const;
	void SetActiveLayer(Scene& scene, SceneLayerId layer);

	[[nodiscard]] const PaintTileSource& GetTileSource() const { return tile_source_; }
	[[nodiscard]] const PrefabKey& GetPrefabSource() const { return prefab_source_; }

	[[nodiscard]] std::optional<UUID> GetTargetTilemapUUID() const { return target_tilemap_; }
	void SetTargetTilemap(std::optional<UUID> uuid);

	/// Project-scoped prefab grouping used by the Prefabs dock tab.
	[[nodiscard]] std::vector<std::string> GetPrefabGroups(EditorContext& ctx);
	[[nodiscard]] std::string GetPrefabGroup(EditorContext& ctx, const PrefabKey& key);
	bool CreatePrefabGroup(EditorContext& ctx, std::string name);
	bool RenamePrefabGroup(EditorContext& ctx, std::string_view old_name, std::string new_name);
	bool DeletePrefabGroup(EditorContext& ctx, std::string_view name);
	void MovePrefabToGroup(EditorContext& ctx, const PrefabKey& key, std::string group);
	void OnPrefabRenamed(EditorContext& ctx, const PrefabKey& old_key, const PrefabKey& new_key);
	void OnPrefabDuplicated(EditorContext& ctx, const PrefabKey& source, const PrefabKey& duplicate);
	void OnPrefabDeleted(EditorContext& ctx, const PrefabKey& key);

	/// Called when hierarchy/layer mutations can invalidate transient paint selection.
	void ValidateSceneState(Scene& scene);
	void ClearSelection();

private:
	struct TileSliceSettings {
		V2_int tile_size{ 32, 32 };
	};

	struct TileLibraryEntry {
		std::string id{};
		std::string name{};
		TextureKey texture{};
		V2_int slice{};
		std::string group{ "Ungrouped" };
	};

	struct TileImportSettings {
		std::string path{};
		TileImportMode mode{ TileImportMode::Auto };
		int tile_width{ 32 };
		int tile_height{ 32 };
		bool use_filename_dimensions{ true };
		bool create_group_from_source{};
		std::string target_group{ "Ungrouped" };
	};

	struct DeletedEntity {
		SerializedEntity entity{};
		SceneLayerId layer{};
	};

	struct EntityStroke {
		std::vector<UUID> created_roots{};
		std::vector<DeletedEntity> deleted_roots{};
	};

	struct Stroke {
		bool active{};
		V2_float start_world{};
		V2_float current_world{};
		V2_int last_cell{ 0, 0 };
		bool has_last_cell{};
		std::optional<UUID> tilemap{};
		std::optional<::ptgn::impl::TilemapData> tilemap_before{};
		EntityStroke entities{};
		std::unordered_set<V2_int> touched_cells{};
		std::vector<V2_float> generator_points{};
	};

	struct MoveDrag {
		bool active{};
		V2_float start_mouse_world{};
		std::vector<std::pair<UUID, Transform>> entity_before{};
		std::optional<UUID> tilemap{};
		std::vector<V2_int> tile_cells{};
		std::optional<::ptgn::impl::TilemapData> tilemap_before{};
	};

	[[nodiscard]] SceneLayer* ResolveActiveLayer(Scene& scene);
	[[nodiscard]] const SceneLayer* ResolveActiveLayer(const Scene& scene) const;
	[[nodiscard]] Tilemap ResolveTargetTilemap(Scene& scene);
	[[nodiscard]] Tilemap ResolveTargetTilemap(const Scene& scene) const;
	void SyncHierarchySelection(EditorContext& ctx, Scene& scene);

	void DrawTilemaps(
		EditorContext& ctx,
		Scene& scene,
		ImDrawList* draw,
		Viewport presentation_viewport,
		const FrameContext& frame
	) const;
	void DrawGenerators(
		EditorContext& ctx,
		Scene& scene,
		ImDrawList* draw,
		Viewport image_viewport,
		Viewport presentation_viewport,
		const FrameContext& frame
	) const;
	void DrawActiveGeneratorPreview(
		EditorContext& ctx,
		Scene& scene,
		ImDrawList* draw,
		Viewport presentation_viewport,
		const FrameContext& frame
	);
	void DrawGrid(
		Scene& scene,
		ImDrawList* draw,
		Viewport image_viewport,
		Viewport presentation_viewport,
		const FrameContext& frame
	) const;
	void DrawSelectionOverlay(
		Scene& scene,
		ImDrawList* draw,
		Viewport presentation_viewport,
		const FrameContext& frame
	) const;
	void DrawToolPreview(
		Scene& scene,
		ImDrawList* draw,
		V2_float mouse_world,
		Viewport presentation_viewport,
		const FrameContext& frame
	) const;

	void DrawBrushSettingsToolbar(EditorContext& ctx, Scene& scene, SceneLayer& layer);
	void DrawNoiseRecipe(EditorContext& ctx, Scene& scene, SceneLayer& layer);
	void DrawExtendedSourceRecipe(EditorContext& ctx, Scene& scene, SceneLayer& layer);
	void DrawNoiseThresholdGradient(EditorContext& ctx, SceneLayer& layer);
	void DrawImportPopup(EditorContext& ctx);

	void HandleShortcuts(EditorContext& ctx, Scene& scene);
	void BeginStroke(Scene& scene, V2_float world);
	void UpdateStroke(EditorContext& ctx, Scene& scene, V2_float world);
	void EndStroke(EditorContext& ctx, Scene& scene, V2_float world);
	void CancelStroke(Scene& scene);

	void ApplyAt(EditorContext& ctx, Scene& scene, V2_float world);
	void ApplyTileAt(EditorContext& ctx, Scene& scene, Tilemap tilemap, V2_int cell);
	void ApplyEntityAt(EditorContext& ctx, Scene& scene, V2_float world);
	void EraseAt(EditorContext& ctx, Scene& scene, V2_float world);
	void FillAt(EditorContext& ctx, Scene& scene, V2_float world);
	void EyedropAt(EditorContext& ctx, Scene& scene, V2_float world);

	void ApplyLine(EditorContext& ctx, Scene& scene, V2_float a, V2_float b);
	void ApplyRectangle(EditorContext& ctx, Scene& scene, V2_float a, V2_float b);
	void CreateGeneratorForStroke(EditorContext& ctx, Scene& scene, PaintGeneratorGeometry geometry);
	void AppendBrushStrokeToGenerator(EditorContext& ctx, Scene& scene);
	void FinishActiveBrushGenerator(EditorContext& ctx, Scene& scene);
	void CancelActiveBrushGenerator(EditorContext& ctx, Scene& scene);
	void UndoActiveBrushStroke(EditorContext& ctx, Scene& scene);
	void BakeGenerator(EditorContext& ctx, Scene& scene, Entity generator);
	[[nodiscard]] bool IsActiveBrushGenerator(Entity generator) const;
	void CreateInfiniteGenerator(EditorContext& ctx, Scene& scene);

	void SnapSelectionToGrid(EditorContext& ctx, Scene& scene);
	void BeginMove(EditorContext& ctx, Scene& scene, V2_float world);
	void UpdateMove(EditorContext& ctx, Scene& scene, V2_float world);
	void EndMove(EditorContext& ctx, Scene& scene);
	void CancelMove(Scene& scene);

	void SelectClick(
		EditorContext& ctx, Scene& scene, V2_float world, bool additive, bool toggle
	);
	void SelectMarquee(
		EditorContext& ctx, Scene& scene, V2_float a, V2_float b, bool additive, bool toggle
	);
	void SelectBrush(EditorContext& ctx, Scene& scene, V2_float world, bool remove);
	[[nodiscard]] bool HasSelection() const;
	[[nodiscard]] bool SelectionHitAtWorld(Scene& scene, V2_float world) const;
	[[nodiscard]] Entity FindGeneratorAtWorld(Scene& scene, V2_float world) const;

	void CommitTileStroke(EditorContext& ctx, Scene& scene);
	void CommitEntityStroke(EditorContext& ctx, Scene& scene);
	void CommitMove(EditorContext& ctx, Scene& scene);

	[[nodiscard]] V2_float ActiveGridSize(const Scene& scene) const;
	[[nodiscard]] V2_float ActiveGridOrigin(const Scene& scene) const;
	[[nodiscard]] V2_int WorldToActiveCell(const Scene& scene, V2_float world) const;
	[[nodiscard]] V2_float ActiveCellToWorld(const Scene& scene, V2_int cell) const;
	[[nodiscard]] std::vector<V2_int> BrushCells(V2_int center) const;
	[[nodiscard]] std::vector<V2_int> SelectionBrushCells(V2_int center) const;
	[[nodiscard]] std::vector<V2_int> LineCells(V2_int a, V2_int b) const;
	[[nodiscard]] std::vector<V2_int> RectangleCells(V2_int a, V2_int b) const;
	[[nodiscard]] bool CoveragePass(V2_int cell, V2_int first, V2_int last) const;
	[[nodiscard]] float NoiseValue(V2_float world, const PaintNoiseState& noise) const;
	[[nodiscard]] const PaintNoiseThreshold* ResolveNoiseThreshold(V2_float world) const;
	[[nodiscard]] PaintGeneratorRecipe CaptureGeneratorRecipe(const SceneLayer& layer) const;

	void DrawTileSourceBrowser(EditorContext& ctx, PaintTileSource& source);
	bool DrawTileSourceCombo(EditorContext& ctx, const char* id, std::optional<PaintTileSource>& source);
	void DrawPrefabSourceBrowser(EditorContext& ctx, PrefabKey& source);
	bool DrawPrefabSourceCombo(EditorContext& ctx, const char* id, std::optional<PrefabKey>& source);
	void DrawWeightedTileSetEditor(EditorContext& ctx);
	void DrawWeightedPrefabSetEditor(EditorContext& ctx);
	void DrawAutotileSourceEditor(EditorContext& ctx);

	[[nodiscard]] PaintWeightedTileSet* FindWeightedTileSet(std::string_view name);
	[[nodiscard]] const PaintWeightedTileSet* FindWeightedTileSet(std::string_view name) const;
	[[nodiscard]] PaintWeightedPrefabSet* FindWeightedPrefabSet(std::string_view name);
	[[nodiscard]] const PaintWeightedPrefabSet* FindWeightedPrefabSet(std::string_view name) const;
	[[nodiscard]] PaintAutotileRuleSet* FindAutotileRuleSet(
		const TextureKey& texture,
		PaintAutotileFormat format
	);
	[[nodiscard]] const PaintAutotileRuleSet* FindAutotileRuleSet(
		const TextureKey& texture,
		PaintAutotileFormat format
	) const;
	[[nodiscard]] PaintAutotileRuleSet* FindAutotileRuleSet(std::uint64_t id);
	[[nodiscard]] const PaintAutotileRuleSet* FindAutotileRuleSet(std::uint64_t id) const;
	[[nodiscard]] std::string UniqueWeightedTileSetName() const;
	[[nodiscard]] std::string UniqueWeightedPrefabSetName() const;
	[[nodiscard]] std::uint64_t NextAutotileRuleSetId() const;
	[[nodiscard]] PaintAutotileRuleSet* ResolveAutotileRuleSet(EditorContext& ctx);
	bool RefreshAutotileRuleSet(EditorContext& ctx, PaintAutotileRuleSet& rules);
	void RecomputeAutotileAround(Tilemap tilemap, V2_int cell, const PaintAutotileRuleSet& rules);

	[[nodiscard]] PaintTileSource MakeTileSource(
		EditorContext& ctx,
		const TextureKey& texture,
		V2_int slice
	) const;
	[[nodiscard]] TileSliceSettings& GetSliceSettings(
		EditorContext& ctx,
		const TextureKey& texture
	);
	[[nodiscard]] const TileSliceSettings* FindSliceSettings(const TextureKey& texture) const;

	void EnsureLocalState(EditorContext& ctx);
	void StoreLocalState(EditorContext& ctx) const;
	void EnsureProjectLibrary(EditorContext& ctx);
	void SaveProjectLibrary(EditorContext& ctx) const;
	void ReconcileProjectLibrary(EditorContext& ctx);
	[[nodiscard]] std::vector<std::string> TileGroupNames() const;
	[[nodiscard]] TileLibraryEntry* FindTileEntry(std::string_view id);
	[[nodiscard]] const TileLibraryEntry* FindTileEntry(std::string_view id) const;
	[[nodiscard]] std::string TileEntryId(const TextureKey& texture, V2_int slice) const;
	[[nodiscard]] int ImportTiles(EditorContext& ctx, const TileImportSettings& settings);
	[[nodiscard]] int ImportImageTiles(
		EditorContext& ctx,
		const TileImportSettings& settings,
		const std::string& image_path,
		std::string group
	);

	PaintTool tool_{ PaintTool::Select };
	PaintRecipeState recipe_{};
	PaintBrushShape brush_shape_{ PaintBrushShape::Circle };
	PaintBrushShape selection_brush_shape_{ PaintBrushShape::Circle };
	PaintSelectMode select_mode_{ PaintSelectMode::ClickMarquee };
	PaintAreaMode area_mode_{ PaintAreaMode::Fill };
	PaintMoveSnapMode move_snap_{ PaintMoveSnapMode::Grid };
	int brush_diameter_{ 3 };
	int selection_diameter_{ 3 };
	int line_thickness_{ 1 };
	int line_spacing_{ 1 };
	int area_thickness_{ 1 };
	bool line_align_rotation_{};
	bool grid_visible_{ true };
	V2_float entity_grid_size_{ 32.0f, 32.0f };
	V2_float entity_grid_offset_{};
	bool grid_aspect_locked_{};
	float grid_locked_aspect_{ 1.0f };
	int grid_major_every_{ 8 };
	std::array<float, 4> grid_minor_color_{ 1.0f, 1.0f, 1.0f, 0.07f };
	std::array<float, 4> grid_major_color_{ 1.0f, 1.0f, 1.0f, 0.15f };
	float grid_minor_thickness_{ 1.0f };
	float grid_major_thickness_{ 1.0f };

	SceneLayerId active_layer_{};
	std::optional<UUID> target_tilemap_{};
	PrefabKey prefab_source_{};
	PaintTileSource tile_source_{};

	std::unordered_map<std::string, TileSliceSettings> tile_slice_settings_{};
	std::vector<TileLibraryEntry> tile_library_{};
	std::vector<std::string> tile_groups_{};
	std::vector<std::string> prefab_groups_{};
	std::unordered_map<std::string, std::string> prefab_group_by_key_{};
	std::vector<PaintWeightedTileSet> weighted_tile_sets_{};
	std::vector<PaintWeightedPrefabSet> weighted_prefab_sets_{};
	std::vector<PaintAutotileRuleSet> autotile_rule_sets_{};
	std::string loaded_project_library_{};
	bool project_library_loaded_{};
	std::string loaded_local_project_{};
	bool local_state_loaded_{};
	std::string tile_search_{};
	std::string tile_source_search_{};
	std::string prefab_source_search_{};
	std::string selected_tile_entry_id_{};
	std::optional<TextureKey> inspected_texture_{};
	V2_int inspected_slice_{};
	TileImportSettings import_settings_{};
	bool import_popup_requested_{};
	std::string import_status_{};

	std::vector<UUID> selected_entities_{};
	std::optional<UUID> selected_generator_{};
	std::optional<UUID> selected_tilemap_{};
	std::vector<V2_int> selected_tile_cells_{};
	V2_float selection_drag_start_{};
	bool selection_drag_active_{};

	Stroke stroke_{};
	MoveDrag move_{};

	// A Keep Generator Brush remains live across mouse-up events. It is a real scene
	// generator immediately so hierarchy/viewport selection and highlighting work while
	// authoring, but it is not pushed to the global undo stack until Finish Generator.
	std::optional<UUID> active_brush_generator_{};
	std::optional<EditorSelection> active_generator_before_selection_{};
	std::uint32_t next_active_brush_stroke_id_{ 1 };
	std::optional<SerializedEntity> generator_inspector_edit_before_{};
};

} // namespace ptgn::editor
