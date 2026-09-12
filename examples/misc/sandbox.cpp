#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "platform/glfw.h"
#include "renderer/backend/gl/gl.h"

#include "stb_image.h"

namespace demo {

struct I2 {
	int x{};
	int y{};

	constexpr bool operator==(const I2&) const = default;
};

struct I2Hash {
	std::size_t operator()(const I2& v) const noexcept {
		const auto a{ static_cast<std::uint64_t>(static_cast<std::uint32_t>(v.x)) };
		const auto b{ static_cast<std::uint64_t>(static_cast<std::uint32_t>(v.y)) };
		return static_cast<std::size_t>((a << 32u) ^ b ^ (a * 0x9E3779B185EBCA87ull));
	}
};

struct F2 {
	float x{};
	float y{};

	constexpr F2 operator+(F2 rhs) const { return { x + rhs.x, y + rhs.y }; }
	constexpr F2 operator-(F2 rhs) const { return { x - rhs.x, y - rhs.y }; }
	constexpr F2 operator*(float s) const { return { x * s, y * s }; }
	constexpr F2 operator/(float s) const { return { x / s, y / s }; }
	F2& operator+=(F2 rhs) {
		x += rhs.x;
		y += rhs.y;
		return *this;
	}
};

static float Length(F2 v) {
	return std::sqrt(v.x * v.x + v.y * v.y);
}

static float Distance(F2 a, F2 b) {
	return Length(a - b);
}

static F2 Lerp(F2 a, F2 b, float t) {
	return a + (b - a) * t;
}

static int FloorDiv(int a, int b) {
	const int q{ a / b };
	const int r{ a % b };
	return (r != 0 && ((r < 0) != (b < 0))) ? q - 1 : q;
}

static int FloorMod(int a, int b) {
	const int r{ a % b };
	return r < 0 ? r + std::abs(b) : r;
}

static std::uint32_t Hash2(int x, int y, std::uint32_t seed) {
	std::uint32_t h{ seed ^ 0x9E3779B9u };
	h ^= static_cast<std::uint32_t>(x) * 0x85EBCA6Bu;
	h = (h << 13u) | (h >> 19u);
	h ^= static_cast<std::uint32_t>(y) * 0xC2B2AE35u;
	h *= 0x27D4EB2Du;
	h ^= h >> 15u;
	return h;
}

static float Fade(float t) {
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float Grad(std::uint32_t h, float x, float y) {
	switch (h & 7u) {
		case 0: return x + y;
		case 1: return -x + y;
		case 2: return x - y;
		case 3: return -x - y;
		case 4: return x;
		case 5: return -x;
		case 6: return y;
		default: return -y;
	}
}

enum class NoiseType {
	Perlin,
	Simplex,
	Value,
};

static float Perlin2(float x, float y, std::uint32_t seed) {
	const int x0{ static_cast<int>(std::floor(x)) };
	const int y0{ static_cast<int>(std::floor(y)) };
	const float tx{ x - static_cast<float>(x0) };
	const float ty{ y - static_cast<float>(y0) };
	const float u{ Fade(tx) };
	const float v{ Fade(ty) };

	const float a{ Grad(Hash2(x0, y0, seed), tx, ty) };
	const float b{ Grad(Hash2(x0 + 1, y0, seed), tx - 1.0f, ty) };
	const float c{ Grad(Hash2(x0, y0 + 1, seed), tx, ty - 1.0f) };
	const float d{ Grad(Hash2(x0 + 1, y0 + 1, seed), tx - 1.0f, ty - 1.0f) };

	const float ab{ a + (b - a) * u };
	const float cd{ c + (d - c) * u };
	return std::clamp((ab + (cd - ab) * v) * 0.5f + 0.5f, 0.0f, 1.0f);
}

static float HashValue01(int x, int y, std::uint32_t seed) {
	return static_cast<float>(Hash2(x, y, seed) & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
}

static float ValueNoise2(float x, float y, std::uint32_t seed) {
	const int x0{ static_cast<int>(std::floor(x)) };
	const int y0{ static_cast<int>(std::floor(y)) };
	const float tx{ x - static_cast<float>(x0) };
	const float ty{ y - static_cast<float>(y0) };
	const float u{ Fade(tx) };
	const float v{ Fade(ty) };
	const float a{ HashValue01(x0, y0, seed) };
	const float b{ HashValue01(x0 + 1, y0, seed) };
	const float c{ HashValue01(x0, y0 + 1, seed) };
	const float d{ HashValue01(x0 + 1, y0 + 1, seed) };
	const float ab{ a + (b - a) * u };
	const float cd{ c + (d - c) * u };
	return std::clamp(ab + (cd - ab) * v, 0.0f, 1.0f);
}

static float SimplexNoise2(float xin, float yin, std::uint32_t seed) {
	constexpr float F2s{ 0.3660254037844386f };
	constexpr float G2s{ 0.2113248654051871f };
	const float s{ (xin + yin) * F2s };
	const int i{ static_cast<int>(std::floor(xin + s)) };
	const int j{ static_cast<int>(std::floor(yin + s)) };
	const float t{ static_cast<float>(i + j) * G2s };
	const float x0{ xin - (static_cast<float>(i) - t) };
	const float y0{ yin - (static_cast<float>(j) - t) };
	const int i1{ x0 > y0 ? 1 : 0 };
	const int j1{ x0 > y0 ? 0 : 1 };
	const float x1{ x0 - static_cast<float>(i1) + G2s };
	const float y1{ y0 - static_cast<float>(j1) + G2s };
	const float x2{ x0 - 1.0f + 2.0f * G2s };
	const float y2{ y0 - 1.0f + 2.0f * G2s };
	auto contribution = [&](int gx, int gy, float x, float y) {
		float tt{ 0.5f - x * x - y * y };
		if (tt <= 0.0f) return 0.0f;
		tt *= tt;
		return tt * tt * Grad(Hash2(gx, gy, seed), x, y);
	};
	const float n0{ contribution(i, j, x0, y0) };
	const float n1{ contribution(i + i1, j + j1, x1, y1) };
	const float n2{ contribution(i + 1, j + 1, x2, y2) };
	return std::clamp(0.5f + 35.0f * (n0 + n1 + n2), 0.0f, 1.0f);
}

static float BaseNoise2(NoiseType type, float x, float y, std::uint32_t seed) {
	switch (type) {
		case NoiseType::Perlin: return Perlin2(x, y, seed);
		case NoiseType::Simplex: return SimplexNoise2(x, y, seed);
		case NoiseType::Value: return ValueNoise2(x, y, seed);
	}
	return Perlin2(x, y, seed);
}

enum class Tool {
	None,
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

enum class LayerKind {
	Entity,
	Tile,
	Noise,
};

enum class NoiseTargetKind {
	Entity,
	Tile,
};

enum class LayerMergeMode {
	Add,
	ReplaceConflicts,
	ReplaceAll,
};

enum class MoveSnapMode {
	Grid,
	Free,
};

enum class NoiseBoundaryEdge {
	None,
	Left,
	Right,
	Top,
	Bottom,
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
};

enum class LayerPurpose {
	Visual,
	Collision,
	Navigation,
	Metadata,
};

enum class PaintSourceKind {
	Single,
	WeightedSet,
	Checkerboard,
	Autotile,
	Noise,
};

enum class PaintCoverageKind {
	Solid,
	RandomDensity,
	RadialFalloff,
};

enum class PaintCommitMode {
	BakeOnCommit,
	KeepGenerator,
};

enum class GeneratorGeometryKind {
	Rectangle,
	Line,
	BrushStroke,
	Infinite,
};

enum class AutotileFormat {
	Classic15,
	Blob47,
	Subset16,
	DualGrid16,
	Wang16,
};

enum class BrushSizeSnapMode {
	Free,
	Grid,
	Tile,
};

enum class BrushShape {
	Circle,
	Square,
};

enum class TilePaintMode {
	Grid,
	Tile,
};

enum class EntityOrigin {
	TopLeft,
	Top,
	TopRight,
	Left,
	Center,
	Right,
	BottomLeft,
	Bottom,
	BottomRight,
};

enum class ImportMode {
	Auto,
	Tileset,
	Individual,
};

enum class BrushOperation {
	Paint,
	Replace,
	ExclusionMask,
};

enum class SelectMode {
	ClickMarquee,
	Brush,
};

enum class AreaMode {
	Fill,
	Outline,
	Corners,
	RandomFill,
};

static const char* ToolName(Tool tool) {
	switch (tool) {
			case Tool::None: return "None";
			case Tool::Select: return "Select";
		case Tool::Move: return "Move";
		case Tool::Pencil: return "Pencil";
		case Tool::Brush: return "Brush";
		case Tool::Line: return "Line";
		case Tool::Rectangle: return "Rectangle";
		case Tool::Fill: return "Fill";
		case Tool::Erase: return "Erase";
		case Tool::Eyedropper: return "Eyedropper";
	}
	return "Unknown";
}


static const char* ToolTooltip(Tool tool) {
	switch (tool) {
		case Tool::None:
			return "No viewport paint tool is active.";
		case Tool::Select:
			return "Click/marquee entities or tiles, or switch to the raster selection brush. Right click clears the selection.";
		case Tool::Move:
			return "Drag selected entities/tiles together. Grid snapping is the default; hold Ctrl to temporarily use free movement.";
		case Tool::Pencil:
			return "Continuously draw one tile/entity at a time while dragging.";
		case Tool::Brush:
			return "Paint a rasterized circle or square footprint over the active layer.";
		case Tool::Line:
			return "Drag a grid-rasterized line; release to apply or press Escape to cancel.";
		case Tool::Rectangle:
			return "Drag a grid-rasterized rectangular region; release to apply or press Escape to cancel.";
		case Tool::Fill:
			return "Flood-fill a connected tile region. Empty regions are bounded by the visible viewport.";
		case Tool::Erase:
			return "Hold left mouse to continuously erase entities/tiles touched by the rasterized brush footprint.";
		case Tool::Eyedropper:
			return "Hold left mouse and move over tiles/entities to continuously pick the source.";
	}
	return "Tool";
}

static const char* LayerKindName(LayerKind kind) {
	switch (kind) {
		case LayerKind::Entity: return "Entity";
		case LayerKind::Tile: return "Tile";
		case LayerKind::Noise: return "Noise";
	}
	return "Unknown";
}

static const char* PurposeName(LayerPurpose purpose) {
	switch (purpose) {
		case LayerPurpose::Visual: return "Visual";
		case LayerPurpose::Collision: return "Collision";
		case LayerPurpose::Navigation: return "Navigation";
		case LayerPurpose::Metadata: return "Metadata";
	}
	return "Unknown";
}

static const char* NoiseTypeName(NoiseType type) {
	switch (type) {
		case NoiseType::Perlin: return "Perlin";
		case NoiseType::Simplex: return "Simplex";
		case NoiseType::Value: return "Value";
	}
	return "Noise";
}

static const char* EntityOriginName(EntityOrigin origin) {
	switch (origin) {
		case EntityOrigin::TopLeft: return "Top Left";
		case EntityOrigin::Top: return "Top";
		case EntityOrigin::TopRight: return "Top Right";
		case EntityOrigin::Left: return "Left";
		case EntityOrigin::Center: return "Center";
		case EntityOrigin::Right: return "Right";
		case EntityOrigin::BottomLeft: return "Bottom Left";
		case EntityOrigin::Bottom: return "Bottom";
		case EntityOrigin::BottomRight: return "Bottom Right";
	}
	return "Top Left";
}

static F2 EntityOriginFraction(EntityOrigin origin) {
	switch (origin) {
		case EntityOrigin::TopLeft: return { 0.0f, 0.0f };
		case EntityOrigin::Top: return { 0.5f, 0.0f };
		case EntityOrigin::TopRight: return { 1.0f, 0.0f };
		case EntityOrigin::Left: return { 0.0f, 0.5f };
		case EntityOrigin::Center: return { 0.5f, 0.5f };
		case EntityOrigin::Right: return { 1.0f, 0.5f };
		case EntityOrigin::BottomLeft: return { 0.0f, 1.0f };
		case EntityOrigin::Bottom: return { 0.5f, 1.0f };
		case EntityOrigin::BottomRight: return { 1.0f, 1.0f };
	}
	return {};
}

struct TextureAsset {
	GLuint handle{};
	int width{};
	int height{};
	std::string path;
};

struct TileDefinition {
	int id{};
	std::string name;
	int texture_index{ -1 };
	ImVec2 uv0{};
	ImVec2 uv1{ 1.0f, 1.0f };
	int pixel_x{};
	int pixel_y{};
	int pixel_w{};
	int pixel_h{};
};

struct PaletteEntry {
	int tile_id{ -1 };
	float weight{ 1.0f };
};

struct TilePalette {
	int id{};
	std::string name{ "Default" };
	std::vector<PaletteEntry> entries;
};

struct WeightedTileEntry {
	int tile_id{ -1 };
	float weight{ 1.0f };
};

struct WeightedTileSet {
	int id{};
	std::string name;
	std::vector<WeightedTileEntry> entries;
};

struct TileCell {
	int tile_id{ -1 };
	bool terrain{};
	int terrain_ruleset_id{ -1 };
	EntityOrigin origin{ EntityOrigin::TopLeft };
	F2 offset{};
};

struct TileChunk {
	I2 coordinate{};
	std::vector<TileCell> cells;
	bool dirty{};
};

struct StreamingSettings {
	bool enabled{ true };
	int preload_margin{ 1 };
	int keep_alive_margin{ 2 };
	int max_loaded_chunks{ 128 };
	bool show_chunk_boundaries{ true };
	bool show_streaming_state{ true };
};

struct Tilemap {
	int id{};
	std::string name{ "World" };
	F2 cell_size{ 32.0f, 32.0f };
	F2 origin{};
	I2 chunk_size{ 16, 16 };
	StreamingSettings streaming;
	std::unordered_set<I2, I2Hash> exclusion_mask;
};

struct TileLayerData {
	int tilemap_id{};
	// Dual-grid autotiles keep logical terrain cells separate from display tiles.
	// The value is the AutotileRuleSet id assigned to that world cell.
	std::unordered_map<I2, int, I2Hash> dual_grid_terrain;
	std::unordered_map<I2, TileChunk, I2Hash> loaded_chunks;
	std::unordered_map<I2, TileChunk, I2Hash> backing_chunks;
	std::unordered_set<I2, I2Hash> preload_chunks;
	std::unordered_set<I2, I2Hash> keep_alive_chunks;
};

struct NoiseThresholdRegion {
	float minimum{};
	float maximum{ 1.0f };
	PaintSourceKind source_kind{ PaintSourceKind::Single };
	int tile_id{ -1 };
	int prefab_index{ -1 };
	int weighted_tile_set_id{ -1 };
	int weighted_prefab_set_id{ -1 };
	EntityOrigin origin{ EntityOrigin::TopLeft };
	bool enabled{};
};

struct NoiseField {
	NoiseType type{ NoiseType::Perlin };
	std::string name{ "Perlin" };
	int seed{ 1337 };
	float frequency{ 0.02f };
	int octaves{ 4 };
	float lacunarity{ 2.0f };
	float persistence{ 0.5f };
	F2 offset{};
	bool enabled{ true };
	std::vector<NoiseThresholdRegion> thresholds;
};

struct AutotileRuleSet {
	int id{};
	std::string name{ "Terrain" };
	AutotileFormat format{ AutotileFormat::DualGrid16 };
	std::vector<int> tile_ids;
};

struct PaintRecipe {
	PaintSourceKind source_kind{ PaintSourceKind::Single };
	PaintCoverageKind coverage{ PaintCoverageKind::Solid };
	PaintCommitMode commit_mode{ PaintCommitMode::BakeOnCommit };

	// Source references are captured by persistent generators so changing the
	// currently browsed source does not silently change old generator output.
	int tile_id{ -1 };
	int prefab_index{ -1 };
	int weighted_tile_set_id{ -1 };
	int weighted_prefab_set_id{ -1 };
	int secondary_tile_id{ -1 };
	int secondary_prefab_index{ -1 };
	int autotile_ruleset_id{ -1 };

	TilePaintMode tile_paint_mode{ TilePaintMode::Tile };
	EntityOrigin tile_origin{ EntityOrigin::TopLeft };
	EntityOrigin entity_origin{ EntityOrigin::TopLeft };
	float density{ 0.45f };
	float radial_inner{ 0.15f };
	float radial_outer{ 1.0f };
	float min_spacing{ 24.0f };
	bool replace_occupied_anchor{ true };
	bool allow_visual_overlap{};
	bool avoid_exclusion_mask{ true };
	bool random_rotation{};
	float rotation_min{};
	float rotation_max{ 360.0f };
	bool random_scale{};
	float scale_min{ 0.8f };
	float scale_max{ 1.2f };
	NoiseField noise;
	bool show_noise_preview{};
	bool show_generated_preview{ true };
	float noise_preview_alpha{ 0.45f };
};

struct GeneratedInstanceOverride {
	I2 cell{};
	bool suppressed{};
	F2 position_offset{};
};

struct GeneratorHit {
	int generator_id{ -1 };
	I2 cell{};
	int tile_id{ -1 };
	int prefab_index{ -1 };
};

struct PaintGenerator {
	int id{};
	int layer_id{};
	std::string name{ "Generator" };
	GeneratorGeometryKind geometry{ GeneratorGeometryKind::Rectangle };
	PaintRecipe recipe;
	F2 grid_size{ 16.0f, 16.0f };
	F2 grid_offset{};
	I2 source_footprint_cells{ 1, 1 };
	I2 lattice_origin_cell{};
	F2 start{};
	F2 end{};
	float brush_radius{ 48.0f };
	BrushShape brush_shape{ BrushShape::Circle };
	int brush_diameter_tiles{ 3 };
	int line_thickness{ 1 };
	int line_spacing_cells{ 1 };
	int area_thickness{ 1 };
	AreaMode area_mode{ AreaMode::Fill };
	std::vector<F2> stroke_points;
	std::vector<std::size_t> stroke_starts;
	std::unordered_set<I2, I2Hash> brush_cells;
	I2 brush_min_cell{};
	I2 brush_max_cell{};
	bool brush_bounds_valid{};
	std::vector<GeneratedInstanceOverride> overrides;
	bool visible{ true };
};

struct NoiseLayerData {
	NoiseTargetKind target{ NoiseTargetKind::Tile };
	int tilemap_id{};
	F2 grid_size{ 16.0f, 16.0f };
	F2 grid_offset{};
	bool grid_aspect_locked{ true };
	float grid_locked_aspect{ 1.0f };
	bool show_noise_preview{ true };
	bool show_generated_preview{ true };
	float noise_preview_alpha{ 0.55f };
	bool bounded{};
	F2 bounds_min{ -256.0f, -256.0f };
	F2 bounds_max{ 256.0f, 256.0f };
	std::vector<NoiseField> fields;
};

struct SceneLayer {
	int id{};
	std::string name;
	LayerKind kind{ LayerKind::Entity };
	LayerPurpose purpose{ LayerPurpose::Visual };
	bool visible{ true };
	bool locked{};
	bool selectable{ true };
	TileLayerData tile;
	NoiseLayerData noise;
};

struct Entity {
	int id{};
	int layer_id{};
	std::string prefab{ "Entity" };
	F2 position{};
	F2 size{ 32.0f, 32.0f };
	EntityOrigin origin{ EntityOrigin::TopLeft };
	float rotation{};
	float scale{ 1.0f };
	float depth{};
};

struct PrefabBrushEntry {
	std::string name;
	std::string group;
	F2 size{ 32.0f, 32.0f };
};

struct WeightedPrefabEntry {
	int prefab_index{ -1 };
	float weight{ 1.0f };
};

struct WeightedPrefabSet {
	int id{};
	std::string name;
	std::vector<WeightedPrefabEntry> entries;
};

struct BrushSettings {
	BrushOperation operation{ BrushOperation::Paint };
	AreaMode area_mode{ AreaMode::Fill };
	SelectMode select_mode{ SelectMode::ClickMarquee };
	BrushShape shape{ BrushShape::Circle };

	int brush_diameter_tiles{ 3 };
	int selection_diameter_cells{ 3 };
	bool line_align_rotation{};
	bool eraser_current_source_only{};
	int line_thickness{ 1 };
	int line_spacing_cells{ 1 };
	int area_thickness{ 1 };
};

struct GridSettings {
	bool visible{ true };
	bool snap{ true };
	bool aspect_locked{};
	float locked_aspect{ 1.0f };
	F2 size{ 32.0f, 32.0f };
	F2 offset{};
	int major_every{ 8 };
	ImVec4 minor_color{ 1.0f, 1.0f, 1.0f, 0.07f };
	ImVec4 major_color{ 1.0f, 1.0f, 1.0f, 0.15f };
	float minor_thickness{ 1.0f };
	float major_thickness{ 1.0f };
};

struct RuntimeState {
	bool playing{};
	bool paused{};
	bool use_editor_camera{ true };
	float speed{ 1.0f };
	float time{};
	int stepped_frames{};
};

struct SceneSnapshot {
	std::vector<SceneLayer> layers;
	std::vector<Entity> entities;
	std::vector<Tilemap> tilemaps;
	std::vector<TilePalette> palettes;
	std::vector<WeightedTileSet> weighted_tile_sets;
	std::vector<WeightedPrefabSet> weighted_prefab_sets;
	std::vector<AutotileRuleSet> autotile_rulesets;
	std::vector<PaintGenerator> generators;
	int active_layer_id{};
	int active_palette_index{};
	int active_tile_weighted_set_id{ -1 };
	int active_prefab_weighted_set_id{ -1 };
	std::unordered_set<int> selected_entities;
	int primary_entity_id{ -1 };
	int selected_generator_id{ -1 };
	int selected_tile_layer_id{ -1 };
	std::unordered_set<I2, I2Hash> selected_tile_cells;
};

struct HistoryEntry {
	std::string label;
	SceneSnapshot before;
	SceneSnapshot after;
};

struct ImportSettings {
	std::string path;
	ImportMode mode{ ImportMode::Auto };
	int tile_width{ 32 };
	int tile_height{ 32 };
	int margin_x{};
	int margin_y{};
	int spacing_x{};
	int spacing_y{};
	bool use_filename_dimensions{ true };
	bool create_palette_from_source{};
	bool open_popup{};
	int target_palette{};
};

struct StrokeState {
	bool active{};
	bool changed{};
	F2 start_world{};
	F2 last_world{};
	F2 current_world{};
	std::unordered_set<I2, I2Hash> touched_cells;
	std::unordered_set<int> touched_entities;
	std::vector<F2> points;
	std::optional<SceneSnapshot> before;
};

struct ClipboardTile {
	I2 relative_cell{};
	TileCell cell;
};

struct SelectionClipboard {
	bool valid{};
	LayerKind kind{ LayerKind::Entity };
	int source_layer_id{ -1 };
	std::vector<Entity> entities;
	std::vector<ClipboardTile> tiles;
	I2 tile_origin{};
};

struct MoveState {
	bool dragging{};
	bool changed{};
	F2 start_world{};
	F2 current_world{};
	std::optional<SceneSnapshot> before;
	MoveSnapMode snap_mode{ MoveSnapMode::Grid };
};

struct NoiseBoundaryDragState {
	bool active{};
	bool changed{};
	int layer_id{ -1 };
	NoiseBoundaryEdge edge{ NoiseBoundaryEdge::None };
	F2 start_mouse_world{};
	F2 start_bounds_min{};
	F2 start_bounds_max{};
	std::optional<SceneSnapshot> before;
};

struct ToolBinding {
	Tool tool{ Tool::Select };
	ImGuiKey key{ ImGuiKey_None };
	std::string key_name;
};

struct EditorState {
	std::vector<TextureAsset> textures;
	std::vector<TileDefinition> tiles;
	std::vector<TilePalette> palettes;
	std::vector<WeightedTileSet> weighted_tile_sets;
	std::vector<PrefabBrushEntry> prefabs;
	std::vector<WeightedPrefabSet> weighted_prefab_sets;
	std::vector<AutotileRuleSet> autotile_rulesets;
	std::vector<PaintGenerator> generators;
	std::vector<SceneLayer> layers;
	std::vector<Tilemap> tilemaps;
	std::vector<Entity> entities;

	Tool tool{ Tool::Select };
	Tool last_non_noise_tool{ Tool::Select };
	std::array<ToolBinding, 9> tool_bindings{{
		{ Tool::Select, ImGuiKey_S, "S" },
		{ Tool::Move, ImGuiKey_M, "M" },
		{ Tool::Pencil, ImGuiKey_P, "P" },
		{ Tool::Brush, ImGuiKey_B, "B" },
		{ Tool::Line, ImGuiKey_L, "L" },
		{ Tool::Rectangle, ImGuiKey_R, "R" },
		{ Tool::Fill, ImGuiKey_F, "F" },
		{ Tool::Erase, ImGuiKey_E, "E" },
		{ Tool::Eyedropper, ImGuiKey_K, "K" },
	}};
	BrushSettings brush;
	PaintRecipe recipe;
	GridSettings grid;
	RuntimeState runtime;
	ImportSettings importer;
	std::string import_status;
	StrokeState stroke;
	MoveState move;
	NoiseBoundaryDragState noise_boundary_drag;
	SelectionClipboard clipboard;

	// Line/Rectangle use a Paint.NET-style live shape after mouse release.
	// Brush Keep Generator mode also uses this pending object so consecutive
	// strokes accumulate into one live generator until Enter/checkmark commits
	// it or Escape cancels it.
	std::optional<PaintGenerator> pending_generator;
	std::optional<SceneSnapshot> pending_generator_before;

	int active_layer_id{};
	int active_palette_index{ -1 };
	int active_tile_id{ -1 };
	int active_tile_weighted_set_id{ -1 };
	int active_prefab_index{};
	int active_prefab_weighted_set_id{ -1 };

	std::vector<int> stamp_tiles;
	int stamp_width{ 1 };

	std::unordered_set<int> selected_entities;
	int primary_entity_id{ -1 };
	int selected_generator_id{ -1 };
	int selected_tile_layer_id{ -1 };
	std::unordered_set<I2, I2Hash> selected_tile_cells;

	std::vector<HistoryEntry> history;
	int history_cursor{};

	F2 view_pan{ 480.0f, 320.0f };
	float view_zoom{ 1.0f };
	F2 canvas_screen_min{};
	F2 canvas_screen_max{};
	bool canvas_hovered{};
	bool dock_layout_initialized{};

	std::mt19937 rng{ 0xC0FFEEu };
	int next_layer_id{ 1 };
	int next_tilemap_id{ 1 };
	int next_entity_id{ 1 };
	int next_tile_id{ 1 };
	int next_palette_id{ 1 };
	int next_tile_weighted_set_id{ 1 };
	int next_prefab_weighted_set_id{ 1 };
	int next_autotile_ruleset_id{ 1 };
	int next_generator_id{ 1 };
};

static EditorState* g_editor{};

static SceneSnapshot CaptureScene(const EditorState& e) {
	return {
		.layers = e.layers,
		.entities = e.entities,
		.tilemaps = e.tilemaps,
		.palettes = e.palettes,
		.weighted_tile_sets = e.weighted_tile_sets,
		.weighted_prefab_sets = e.weighted_prefab_sets,
		.autotile_rulesets = e.autotile_rulesets,
		.generators = e.generators,
		.active_layer_id = e.active_layer_id,
		.active_palette_index = e.active_palette_index,
		.active_tile_weighted_set_id = e.active_tile_weighted_set_id,
		.active_prefab_weighted_set_id = e.active_prefab_weighted_set_id,
		.selected_entities = e.selected_entities,
		.primary_entity_id = e.primary_entity_id,
		.selected_generator_id = e.selected_generator_id,
		.selected_tile_layer_id = e.selected_tile_layer_id,
		.selected_tile_cells = e.selected_tile_cells,
	};
}

static void RestoreScene(EditorState& e, const SceneSnapshot& s) {
	e.layers = s.layers;
	e.entities = s.entities;
	e.tilemaps = s.tilemaps;
	e.palettes = s.palettes;
	e.weighted_tile_sets = s.weighted_tile_sets;
	e.weighted_prefab_sets = s.weighted_prefab_sets;
	e.autotile_rulesets = s.autotile_rulesets;
	e.generators = s.generators;
	e.active_layer_id = s.active_layer_id;
	e.active_palette_index = s.active_palette_index;
	e.active_tile_weighted_set_id = s.active_tile_weighted_set_id;
	e.active_prefab_weighted_set_id = s.active_prefab_weighted_set_id;
	e.selected_entities = s.selected_entities;
	e.primary_entity_id = s.primary_entity_id;
	e.selected_generator_id = s.selected_generator_id;
	e.selected_tile_layer_id = s.selected_tile_layer_id;
	e.selected_tile_cells = s.selected_tile_cells;
}

static void PushHistory(EditorState& e, std::string label, SceneSnapshot before) {
	if (e.history_cursor < static_cast<int>(e.history.size())) {
		e.history.erase(e.history.begin() + e.history_cursor, e.history.end());
	}
	e.history.push_back({ std::move(label), std::move(before), CaptureScene(e) });
	e.history_cursor = static_cast<int>(e.history.size());
}

static void Undo(EditorState& e) {
	if (e.history_cursor <= 0) {
		return;
	}
	--e.history_cursor;
	RestoreScene(e, e.history[static_cast<std::size_t>(e.history_cursor)].before);
}

static void Redo(EditorState& e) {
	if (e.history_cursor >= static_cast<int>(e.history.size())) {
		return;
	}
	RestoreScene(e, e.history[static_cast<std::size_t>(e.history_cursor)].after);
	++e.history_cursor;
}

static void DeselectAll(EditorState& e) {
	e.selected_entities.clear();
	e.primary_entity_id = -1;
	e.selected_generator_id = -1;
	e.selected_tile_layer_id = -1;
	e.selected_tile_cells.clear();
}

static SceneLayer* FindLayer(EditorState& e, int id) {
	for (auto& layer : e.layers) {
		if (layer.id == id) {
			return &layer;
		}
	}
	return nullptr;
}

static const SceneLayer* FindLayer(const EditorState& e, int id) {
	for (const auto& layer : e.layers) {
		if (layer.id == id) {
			return &layer;
		}
	}
	return nullptr;
}

static bool ActiveLayerIsNoise(const EditorState& e) {
	const SceneLayer* layer{ FindLayer(e, e.active_layer_id) };
	return layer && layer->kind == LayerKind::Noise;
}

static void SyncViewportToolToActiveLayer(EditorState& e) {
	if (ActiveLayerIsNoise(e)) {
		if (e.tool != Tool::None) {
			e.last_non_noise_tool = e.tool;
			e.tool = Tool::None;

			// A procedural noise layer has no paint tool. Cancel any transient
			// paint/move interaction so changing layers cannot leave a stale
			// preview or commit work to the previous layer.
			e.stroke = {};
			e.move.dragging = false;
			e.move.changed = false;
			e.move.before.reset();
		}
		return;
	}

	if (e.tool == Tool::None) {
		e.tool = e.last_non_noise_tool == Tool::None
			? Tool::Select
			: e.last_non_noise_tool;
	}
}

static Tilemap* FindTilemap(EditorState& e, int id) {
	for (auto& map : e.tilemaps) {
		if (map.id == id) {
			return &map;
		}
	}
	return nullptr;
}

static const Tilemap* FindTilemap(const EditorState& e, int id) {
	for (const auto& map : e.tilemaps) {
		if (map.id == id) {
			return &map;
		}
	}
	return nullptr;
}

static TileDefinition* FindTile(EditorState& e, int id) {
	for (auto& tile : e.tiles) {
		if (tile.id == id) {
			return &tile;
		}
	}
	return nullptr;
}

static const TileDefinition* FindTile(const EditorState& e, int id) {
	for (const auto& tile : e.tiles) {
		if (tile.id == id) {
			return &tile;
		}
	}
	return nullptr;
}


static std::string LowerCopy(std::string value) {
	std::ranges::transform(value, value.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return value;
}

static bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle) {
	if (needle.empty()) {
		return true;
	}
	return LowerCopy(std::string{ haystack }).find(LowerCopy(std::string{ needle })) != std::string::npos;
}

static WeightedTileSet* FindWeightedTileSet(EditorState& e, int id) {
	for (auto& set : e.weighted_tile_sets) {
		if (set.id == id) return &set;
	}
	return nullptr;
}

static const WeightedTileSet* FindWeightedTileSet(const EditorState& e, int id) {
	for (const auto& set : e.weighted_tile_sets) {
		if (set.id == id) return &set;
	}
	return nullptr;
}

static WeightedPrefabSet* FindWeightedPrefabSet(EditorState& e, int id) {
	for (auto& set : e.weighted_prefab_sets) {
		if (set.id == id) return &set;
	}
	return nullptr;
}

static const WeightedPrefabSet* FindWeightedPrefabSet(const EditorState& e, int id) {
	for (const auto& set : e.weighted_prefab_sets) {
		if (set.id == id) return &set;
	}
	return nullptr;
}

static AutotileRuleSet* FindAutotileRuleSet(EditorState& e, int id) {
	for (auto& set : e.autotile_rulesets) {
		if (set.id == id) return &set;
	}
	return nullptr;
}

static const AutotileRuleSet* FindAutotileRuleSet(const EditorState& e, int id) {
	for (const auto& set : e.autotile_rulesets) {
		if (set.id == id) return &set;
	}
	return nullptr;
}

static PaintGenerator* FindGenerator(EditorState& e, int id) {
	for (auto& generator : e.generators) {
		if (generator.id == id) return &generator;
	}
	return nullptr;
}

static const PaintGenerator* FindGenerator(const EditorState& e, int id) {
	for (const auto& generator : e.generators) {
		if (generator.id == id) return &generator;
	}
	return nullptr;
}

static void SyncPaintPaletteToGenerator(EditorState& e, const PaintGenerator& generator) {
	e.recipe = generator.recipe;
	const SceneLayer* layer{ FindLayer(e, generator.layer_id) };
	if (!layer) return;
	e.active_layer_id = layer->id;
	if (layer->kind == LayerKind::Tile) {
		if (generator.recipe.source_kind == PaintSourceKind::Single && FindTile(e, generator.recipe.tile_id)) {
			e.active_tile_id = generator.recipe.tile_id;
		} else if (generator.recipe.source_kind == PaintSourceKind::WeightedSet) {
			e.active_tile_weighted_set_id = generator.recipe.weighted_tile_set_id;
		}
	} else if (layer->kind == LayerKind::Entity) {
		if (generator.recipe.source_kind == PaintSourceKind::Single &&
			generator.recipe.prefab_index >= 0 && generator.recipe.prefab_index < static_cast<int>(e.prefabs.size())) {
			e.active_prefab_index = generator.recipe.prefab_index;
		} else if (generator.recipe.source_kind == PaintSourceKind::WeightedSet) {
			e.active_prefab_weighted_set_id = generator.recipe.weighted_prefab_set_id;
		}
	}
}

static void SelectGenerator(EditorState& e, int generator_id) {
	const PaintGenerator* generator{ FindGenerator(e, generator_id) };
	if (!generator) return;
	e.selected_generator_id = generator_id;
	e.selected_entities.clear();
	e.primary_entity_id = -1;
	e.selected_tile_cells.clear();
	e.selected_tile_layer_id = -1;
	SyncPaintPaletteToGenerator(e, *generator);
}

static std::string UniqueWeightedTileSetName(const EditorState& e) {
	for (int number{ 1 };; ++number) {
		const std::string candidate{ "Weighted Set " + std::to_string(number) };
		const bool exists{ std::ranges::any_of(e.weighted_tile_sets, [&](const auto& set) {
			return set.name == candidate;
		}) };
		if (!exists) return candidate;
	}
}

static std::string UniqueWeightedPrefabSetName(const EditorState& e) {
	for (int number{ 1 };; ++number) {
		const std::string candidate{ "Weighted Set " + std::to_string(number) };
		const bool exists{ std::ranges::any_of(e.weighted_prefab_sets, [&](const auto& set) {
			return set.name == candidate;
		}) };
		if (!exists) return candidate;
	}
}

static std::string TileDisplayPath(const EditorState& e, int tile_id) {
	const TileDefinition* tile{ FindTile(e, tile_id) };
	if (!tile) return "<missing tile>";
	for (const auto& palette : e.palettes) {
		if (std::ranges::any_of(palette.entries, [&](const PaletteEntry& entry) { return entry.tile_id == tile_id; })) {
			return palette.name + " / " + tile->name;
		}
	}
	return tile->name;
}

static std::string PrefabDisplayPath(const EditorState& e, int prefab_index) {
	if (prefab_index < 0 || prefab_index >= static_cast<int>(e.prefabs.size())) {
		return "<missing prefab>";
	}
	const auto& prefab{ e.prefabs[static_cast<std::size_t>(prefab_index)] };
	return prefab.group.empty() ? "Ungrouped / " + prefab.name : prefab.group + " / " + prefab.name;
}

static F2 WorldToScreen(const EditorState& e, F2 p) {
	return {
		e.canvas_screen_min.x + e.view_pan.x + p.x * e.view_zoom,
		e.canvas_screen_min.y + e.view_pan.y + p.y * e.view_zoom,
	};
}

static F2 ScreenToWorld(const EditorState& e, F2 p) {
	return {
		(p.x - e.canvas_screen_min.x - e.view_pan.x) / e.view_zoom,
		(p.y - e.canvas_screen_min.y - e.view_pan.y) / e.view_zoom,
	};
}

static F2 SnapToGrid(const EditorState& e, F2 p) {
	const auto snap_axis = [](float v, float offset, float size) {
		if (size <= 0.0f) {
			return v;
		}
		return std::round((v - offset) / size) * size + offset;
	};
	return {
		snap_axis(p.x, e.grid.offset.x, e.grid.size.x),
		snap_axis(p.y, e.grid.offset.y, e.grid.size.y),
	};
}

static F2 EntityGridAnchorOffset(const EditorState& e, EntityOrigin origin) {
	const F2 f{ EntityOriginFraction(origin) };
	return { f.x * e.grid.size.x, f.y * e.grid.size.y };
}

static F2 SnapEntityPlacementToGrid(const EditorState& e, F2 p, EntityOrigin origin) {
	const float sx{ std::max(1.0f, e.grid.size.x) };
	const float sy{ std::max(1.0f, e.grid.size.y) };
	const int cell_x{ static_cast<int>(std::floor((p.x - e.grid.offset.x) / sx)) };
	const int cell_y{ static_cast<int>(std::floor((p.y - e.grid.offset.y) / sy)) };
	const F2 anchor_offset{ EntityGridAnchorOffset(e, origin) };
	return {
		e.grid.offset.x + static_cast<float>(cell_x) * sx + anchor_offset.x,
		e.grid.offset.y + static_cast<float>(cell_y) * sy + anchor_offset.y,
	};
}

static I2 EntityGridCell(const EditorState& e, F2 p) {
	return {
		static_cast<int>(std::floor((p.x - e.grid.offset.x) / std::max(1.0f, e.grid.size.x))),
		static_cast<int>(std::floor((p.y - e.grid.offset.y) / std::max(1.0f, e.grid.size.y))),
	};
}

static I2 WorldToCell(const Tilemap& map, F2 p) {
	return {
		static_cast<int>(std::floor((p.x - map.origin.x) / map.cell_size.x)),
		static_cast<int>(std::floor((p.y - map.origin.y) / map.cell_size.y)),
	};
}

static F2 CellToWorld(const Tilemap& map, I2 cell) {
	return {
		map.origin.x + static_cast<float>(cell.x) * map.cell_size.x,
		map.origin.y + static_cast<float>(cell.y) * map.cell_size.y,
	};
}

static I2 CellToChunk(const Tilemap& map, I2 cell) {
	return {
		FloorDiv(cell.x, map.chunk_size.x),
		FloorDiv(cell.y, map.chunk_size.y),
	};
}

static I2 CellToLocal(const Tilemap& map, I2 cell) {
	return {
		FloorMod(cell.x, map.chunk_size.x),
		FloorMod(cell.y, map.chunk_size.y),
	};
}

static std::size_t CellIndex(const Tilemap& map, I2 local) {
	return static_cast<std::size_t>(local.y * map.chunk_size.x + local.x);
}

static TileChunk MakeChunk(const Tilemap& map, I2 coord) {
	TileChunk chunk;
	chunk.coordinate = coord;
	chunk.cells.resize(static_cast<std::size_t>(map.chunk_size.x * map.chunk_size.y));
	return chunk;
}

static TileChunk& EnsureLoadedChunk(SceneLayer& layer, const Tilemap& map, I2 coord) {
	if (auto it = layer.tile.loaded_chunks.find(coord); it != layer.tile.loaded_chunks.end()) {
		return it->second;
	}
	if (auto it = layer.tile.backing_chunks.find(coord); it != layer.tile.backing_chunks.end()) {
		auto node{ layer.tile.backing_chunks.extract(it) };
		node.key() = coord;
		auto inserted{ layer.tile.loaded_chunks.insert(std::move(node)) };
		return inserted.position->second;
	}
	auto [it, _] = layer.tile.loaded_chunks.emplace(coord, MakeChunk(map, coord));
	return it->second;
}

static const TileCell* ReadTileCell(const SceneLayer& layer, const Tilemap& map, I2 cell) {
	const I2 chunk_coord{ CellToChunk(map, cell) };
	const I2 local{ CellToLocal(map, cell) };
	if (auto it = layer.tile.loaded_chunks.find(chunk_coord); it != layer.tile.loaded_chunks.end()) {
		return &it->second.cells[CellIndex(map, local)];
	}
	if (auto it = layer.tile.backing_chunks.find(chunk_coord); it != layer.tile.backing_chunks.end()) {
		return &it->second.cells[CellIndex(map, local)];
	}
	return nullptr;
}

static TileCell* WriteTileCell(SceneLayer& layer, const Tilemap& map, I2 cell) {
	const I2 chunk_coord{ CellToChunk(map, cell) };
	const I2 local{ CellToLocal(map, cell) };
	auto& chunk{ EnsureLoadedChunk(layer, map, chunk_coord) };
	chunk.dirty = true;
	return &chunk.cells[CellIndex(map, local)];
}

static void SetTile(
	SceneLayer& layer,
	const Tilemap& map,
	I2 cell,
	int tile_id,
	bool terrain = false,
	EntityOrigin origin = EntityOrigin::TopLeft,
	int terrain_ruleset_id = -1
) {
	auto* dst{ WriteTileCell(layer, map, cell) };
	dst->tile_id = tile_id;
	dst->terrain = terrain;
	dst->terrain_ruleset_id = terrain_ruleset_id;
	dst->origin = origin;
	dst->offset = {};
}

static void EraseTile(SceneLayer& layer, const Tilemap& map, I2 cell) {
	auto* dst{ WriteTileCell(layer, map, cell) };
	dst->tile_id = -1;
	dst->terrain = false;
	dst->terrain_ruleset_id = -1;
	dst->origin = EntityOrigin::TopLeft;
	dst->offset = {};
}

struct RectF {
	F2 min{};
	F2 max{};
};

static bool RectsOverlap(const RectF& a, const RectF& b) {
	return a.min.x < b.max.x && a.max.x > b.min.x &&
		a.min.y < b.max.y && a.max.y > b.min.y;
}

static bool CircleIntersectsRect(F2 center, float radius, const RectF& rect) {
	const float closest_x{ std::clamp(center.x, rect.min.x, rect.max.x) };
	const float closest_y{ std::clamp(center.y, rect.min.y, rect.max.y) };
	const float dx{ center.x - closest_x };
	const float dy{ center.y - closest_y };
	return dx * dx + dy * dy <= radius * radius;
}

static RectF EntityBounds(const Entity& entity) {
	const F2 size{ entity.size.x * entity.scale, entity.size.y * entity.scale };
	const F2 f{ EntityOriginFraction(entity.origin) };
	const F2 min{ entity.position.x - size.x * f.x, entity.position.y - size.y * f.y };
	return { min, min + size };
}

static F2 RectCenter(const RectF& rect) {
	return { (rect.min.x + rect.max.x) * 0.5f, (rect.min.y + rect.max.y) * 0.5f };
}

static F2 TileWorldSize(const EditorState& e, const Tilemap& map, int tile_id) {
	if (const auto* tile = FindTile(e, tile_id)) {
		return {
			std::max(1.0f, static_cast<float>(tile->pixel_w)),
			std::max(1.0f, static_cast<float>(tile->pixel_h)),
		};
	}
	return map.cell_size;
}

static I2 TileFootprintCells(const EditorState& e, const Tilemap& map, int tile_id) {
	const F2 size{ TileWorldSize(e, map, tile_id) };
	return {
		std::max(1, static_cast<int>(std::ceil(size.x / std::max(1.0f, map.cell_size.x)))),
		std::max(1, static_cast<int>(std::ceil(size.y / std::max(1.0f, map.cell_size.y)))),
	};
}

static RectF TileAnchorRect(
	const EditorState& e,
	const Tilemap& map,
	I2 cell,
	int tile_id,
	F2 local_offset = {},
	EntityOrigin origin = EntityOrigin::TopLeft
) {
	const F2 f{ EntityOriginFraction(origin) };
	const F2 size{ TileWorldSize(e, map, tile_id) };
	const F2 cell_min{ CellToWorld(map, cell) };
	const F2 anchor{
		cell_min.x + map.cell_size.x * f.x + local_offset.x,
		cell_min.y + map.cell_size.y * f.y + local_offset.y,
	};
	const F2 min{ anchor.x - size.x * f.x, anchor.y - size.y * f.y };
	return { min, min + size };
}

template <typename Fn>
static void ForEachTileAnchor(const SceneLayer& layer, const Tilemap& map, Fn&& fn) {
	auto visit = [&](const auto& chunks) {
		for (const auto& [chunk_coord, chunk] : chunks) {
			for (int y{}; y < map.chunk_size.y; ++y) {
				for (int x{}; x < map.chunk_size.x; ++x) {
					const auto& tile_cell{ chunk.cells[static_cast<std::size_t>(y * map.chunk_size.x + x)] };
					if (tile_cell.tile_id < 0) {
						continue;
					}
					fn(I2{ chunk_coord.x * map.chunk_size.x + x, chunk_coord.y * map.chunk_size.y + y }, tile_cell);
				}
			}
		}
	};
	visit(layer.tile.loaded_chunks);
	visit(layer.tile.backing_chunks);
}


static std::optional<I2> FindVisibleTileAnchorAtWorld(
	const EditorState& e,
	const SceneLayer& layer,
	const Tilemap& map,
	F2 world
) {
	std::optional<I2> found;

	ForEachTileAnchor(layer, map, [&](I2 cell, const TileCell& tile) {
		const RectF bounds{ TileAnchorRect(e, map, cell, tile.tile_id, tile.offset, tile.origin) };
		if (
			world.x < bounds.min.x || world.x > bounds.max.x ||
			world.y < bounds.min.y || world.y > bounds.max.y
		) {
			return;
		}

		// DrawTileLayer draws anchors in increasing Y/X order, so the greatest
		// Y/X candidate corresponds to the visually latest/topmost anchor.
		if (
			!found ||
			cell.y > found->y ||
			(cell.y == found->y && cell.x > found->x)
		) {
			found = cell;
		}
	});

	return found;
}

struct RasterGrid {
	F2 size{ 1.0f, 1.0f };
	F2 offset{};
};

static RasterGrid ActiveRasterGrid(const EditorState& e) {
	if (const auto* layer = FindLayer(e, e.active_layer_id)) {
		const Tilemap* map{};
		if (layer->kind == LayerKind::Tile) {
			map = FindTilemap(e, layer->tile.tilemap_id);
		} else if (layer->kind == LayerKind::Noise && layer->noise.target == NoiseTargetKind::Tile) {
			map = FindTilemap(e, layer->noise.tilemap_id);
		}
		if (map) {
			return {
				.size = {
					std::max(1.0f, map->cell_size.x),
					std::max(1.0f, map->cell_size.y),
				},
				.offset = map->origin,
			};
		}
	}

	return {
		.size = {
			std::max(1.0f, e.grid.size.x),
			std::max(1.0f, e.grid.size.y),
		},
		.offset = e.grid.offset,
	};
}

static F2 RecipeMaxSourceSize(const EditorState& e, const SceneLayer& layer, const PaintRecipe& recipe) {
	F2 result{};
	auto include_tile = [&](int tile_id) {
		if (const auto* tile = FindTile(e, tile_id)) {
			result.x = std::max(result.x, static_cast<float>(std::max(1, tile->pixel_w)));
			result.y = std::max(result.y, static_cast<float>(std::max(1, tile->pixel_h)));
		}
	};
	auto include_prefab = [&](int prefab_index) {
		if (prefab_index >= 0 && prefab_index < static_cast<int>(e.prefabs.size())) {
			const auto& prefab{ e.prefabs[static_cast<std::size_t>(prefab_index)] };
			result.x = std::max(result.x, std::max(1.0f, prefab.size.x));
			result.y = std::max(result.y, std::max(1.0f, prefab.size.y));
		}
	};

	if (layer.kind == LayerKind::Tile) {
		switch (recipe.source_kind) {
			case PaintSourceKind::Single: include_tile(recipe.tile_id); break;
			case PaintSourceKind::Checkerboard:
				include_tile(recipe.tile_id);
				include_tile(recipe.secondary_tile_id);
				break;
			case PaintSourceKind::WeightedSet:
				if (const auto* set = FindWeightedTileSet(e, recipe.weighted_tile_set_id)) {
					for (const auto& entry : set->entries) include_tile(entry.tile_id);
				}
				break;
			case PaintSourceKind::Autotile:
				if (const auto* rules = FindAutotileRuleSet(e, recipe.autotile_ruleset_id)) {
					for (int id : rules->tile_ids) include_tile(id);
				}
				break;
			case PaintSourceKind::Noise:
				for (const auto& region : recipe.noise.thresholds) {
					if (!region.enabled) continue;
					if (region.source_kind == PaintSourceKind::Single) include_tile(region.tile_id);
					else if (region.source_kind == PaintSourceKind::WeightedSet) {
						if (const auto* set = FindWeightedTileSet(e, region.weighted_tile_set_id)) {
							for (const auto& entry : set->entries) include_tile(entry.tile_id);
						}
					}
				}
				break;
		}
	} else {
		switch (recipe.source_kind) {
			case PaintSourceKind::Single: include_prefab(recipe.prefab_index); break;
			case PaintSourceKind::Checkerboard:
				include_prefab(recipe.prefab_index);
				include_prefab(recipe.secondary_prefab_index);
				break;
			case PaintSourceKind::WeightedSet:
				if (const auto* set = FindWeightedPrefabSet(e, recipe.weighted_prefab_set_id)) {
					for (const auto& entry : set->entries) include_prefab(entry.prefab_index);
				}
				break;
			case PaintSourceKind::Noise:
				for (const auto& region : recipe.noise.thresholds) {
					if (!region.enabled) continue;
					if (region.source_kind == PaintSourceKind::Single) include_prefab(region.prefab_index);
					else if (region.source_kind == PaintSourceKind::WeightedSet) {
						if (const auto* set = FindWeightedPrefabSet(e, region.weighted_prefab_set_id)) {
							for (const auto& entry : set->entries) include_prefab(entry.prefab_index);
						}
					}
				}
				break;
			case PaintSourceKind::Autotile: break;
		}
	}

	const RasterGrid grid{ ActiveRasterGrid(e) };
	if (result.x <= 0.0f) result.x = grid.size.x;
	if (result.y <= 0.0f) result.y = grid.size.y;
	return result;
}

static I2 RecipePaintFootprintCells(const EditorState& e) {
	const SceneLayer* layer{ FindLayer(e, e.active_layer_id) };
	const RasterGrid grid{ ActiveRasterGrid(e) };
	if (!layer) return { 1, 1 };
	const F2 source_size{ RecipeMaxSourceSize(e, *layer, e.recipe) };
	return {
		std::max(1, static_cast<int>(std::ceil(source_size.x / std::max(1.0f, grid.size.x)))),
		std::max(1, static_cast<int>(std::ceil(source_size.y / std::max(1.0f, grid.size.y)))),
	};
}

static float EffectiveBrushRadius(const EditorState& e) {
	const RasterGrid grid{ ActiveRasterGrid(e) };
	const bool selection_brush{ e.tool == Tool::Select };
	const I2 footprint{ selection_brush ? I2{ 1, 1 } : RecipePaintFootprintCells(e) };
	const int diameter_units{ selection_brush
		? std::max(1, e.brush.selection_diameter_cells)
		: std::max(1, e.brush.brush_diameter_tiles) };
	const float width{ grid.size.x * static_cast<float>(diameter_units * footprint.x) };
	const float height{ grid.size.y * static_cast<float>(diameter_units * footprint.y) };
	return std::max(width, height) * 0.5f;
}

static I2 WorldToRasterCell(const RasterGrid& grid, F2 world) {
	return {
		static_cast<int>(std::floor((world.x - grid.offset.x) / grid.size.x)),
		static_cast<int>(std::floor((world.y - grid.offset.y) / grid.size.y)),
	};
}

static F2 RasterCellToWorld(const RasterGrid& grid, I2 cell) {
	return {
		grid.offset.x + static_cast<float>(cell.x) * grid.size.x,
		grid.offset.y + static_cast<float>(cell.y) * grid.size.y,
	};
}

static RectF RasterCellRect(const RasterGrid& grid, I2 cell) {
	const F2 min{ RasterCellToWorld(grid, cell) };
	return { min, min + grid.size };
}

static std::vector<I2> RasterBrushCells(const EditorState& e, F2 world) {
	const RasterGrid grid{ ActiveRasterGrid(e) };
	const I2 center{ WorldToRasterCell(grid, world) };
	const bool selection_brush{ e.tool == Tool::Select };
	const I2 footprint{ selection_brush ? I2{ 1, 1 } : RecipePaintFootprintCells(e) };
	const int diameter_units{ selection_brush
		? std::max(1, e.brush.selection_diameter_cells)
		: std::max(1, e.brush.brush_diameter_tiles) };
	const int count_x{ std::max(1, diameter_units * footprint.x) };
	const int count_y{ std::max(1, diameter_units * footprint.y) };
	const int start_x{ center.x - (count_x - 1) / 2 };
	const int start_y{ center.y - (count_y - 1) / 2 };

	std::vector<I2> cells;
	cells.reserve(static_cast<std::size_t>(count_x * count_y));
	const float cx{ (static_cast<float>(count_x) - 1.0f) * 0.5f };
	const float cy{ (static_cast<float>(count_y) - 1.0f) * 0.5f };
	const float rx{ std::max(0.5f, static_cast<float>(count_x) * 0.5f) };
	const float ry{ std::max(0.5f, static_cast<float>(count_y) * 0.5f) };

	for (int y{}; y < count_y; ++y) {
		for (int x{}; x < count_x; ++x) {
			if (e.brush.shape == BrushShape::Circle) {
				const float nx{ (static_cast<float>(x) - cx) / rx };
				const float ny{ (static_cast<float>(y) - cy) / ry };
				if (nx * nx + ny * ny > 1.0f) continue;
			}
			cells.push_back({ start_x + x, start_y + y });
		}
	}
	if (cells.empty()) cells.push_back(center);
	return cells;
}

static bool RectIntersectsRasterCells(
	const EditorState& e,
	F2 brush_world,
	const RectF& rect
) {
	const RasterGrid grid{ ActiveRasterGrid(e) };
	for (const I2 cell : RasterBrushCells(e, brush_world)) {
		if (RectsOverlap(rect, RasterCellRect(grid, cell))) {
			return true;
		}
	}
	return false;
}

static std::vector<I2> ExpandRasterCells(
	const std::vector<I2>& base,
	int thickness
) {
	thickness = std::max(1, thickness);
	if (thickness == 1) {
		return base;
	}

	const int min_offset{ -(thickness - 1) / 2 };
	const int max_offset{ thickness / 2 };
	std::unordered_set<I2, I2Hash> unique;
	for (const I2 cell : base) {
		for (int y{ min_offset }; y <= max_offset; ++y) {
			for (int x{ min_offset }; x <= max_offset; ++x) {
				unique.insert({ cell.x + x, cell.y + y });
			}
		}
	}

	std::vector<I2> result;
	result.reserve(unique.size());
	for (const I2 cell : unique) {
		result.push_back(cell);
	}
	std::sort(result.begin(), result.end(), [](I2 a, I2 b) {
		return a.y != b.y ? a.y < b.y : a.x < b.x;
	});
	return result;
}

static std::vector<I2> RasterLineCells(
	const EditorState& e,
	F2 a,
	F2 b
) {
	const RasterGrid grid{ ActiveRasterGrid(e) };
	I2 start{ WorldToRasterCell(grid, a) };
	const I2 end{ WorldToRasterCell(grid, b) };
	std::vector<I2> base;
	const int dx{ std::abs(end.x - start.x) };
	const int sx{ start.x < end.x ? 1 : -1 };
	const int dy{ -std::abs(end.y - start.y) };
	const int sy{ start.y < end.y ? 1 : -1 };
	int error{ dx + dy };
	for (;;) {
		base.push_back(start);
		if (start == end) break;
		const int twice_error{ 2 * error };
		if (twice_error >= dy) { error += dy; start.x += sx; }
		if (twice_error <= dx) { error += dx; start.y += sy; }
	}
	const int spacing{ std::max(1, e.brush.line_spacing_cells) };
	if (spacing > 1 && base.size() > 2) {
		std::vector<I2> spaced;
		for (std::size_t i{}; i < base.size(); i += static_cast<std::size_t>(spacing)) spaced.push_back(base[i]);
		if (spaced.empty() || spaced.back() != base.back()) spaced.push_back(base.back());
		base = std::move(spaced);
	}
	const I2 footprint{ RecipePaintFootprintCells(e) };
	const int source_cells{ std::max(footprint.x, footprint.y) };
	return ExpandRasterCells(base, std::max(1, e.brush.line_thickness) * source_cells);
}

static I2 QuantizeAreaEndCell(const EditorState& e, I2 start, I2 raw_end) {
	const I2 footprint{ RecipePaintFootprintCells(e) };
	const EntityOrigin origin{
		FindLayer(e, e.active_layer_id) && FindLayer(e, e.active_layer_id)->kind == LayerKind::Tile
			? e.recipe.tile_origin
			: e.recipe.entity_origin
	};
	const F2 origin_fraction{ EntityOriginFraction(origin) };

	auto quantize_axis = [](int a, int b, int unit, float origin_axis) {
		unit = std::max(1, unit);
		if (unit == 1) return b;
		const int direction{ b >= a ? 1 : -1 };
		const int span{ std::abs(b - a) + 1 };
		const bool prefer_expand{
			direction > 0 ? origin_axis <= 0.5f : origin_axis >= 0.5f
		};
		int units{};
		if (prefer_expand) units = (span + unit - 1) / unit;
		else units = std::max(1, span / unit);
		return a + direction * (units * unit - 1);
	};

	return {
		quantize_axis(start.x, raw_end.x, footprint.x, origin_fraction.x),
		quantize_axis(start.y, raw_end.y, footprint.y, origin_fraction.y),
	};
}

static F2 QuantizeAreaEndWorld(const EditorState& e, F2 start_world, F2 raw_end_world) {
	const RasterGrid grid{ ActiveRasterGrid(e) };
	const I2 start{ WorldToRasterCell(grid, start_world) };
	const I2 end{ QuantizeAreaEndCell(e, start, WorldToRasterCell(grid, raw_end_world)) };
	const RectF cell{ RasterCellRect(grid, end) };
	return RectCenter(cell);
}

static std::vector<I2> RasterAreaCells(
	const EditorState& e,
	F2 a,
	F2 b
) {
	const RasterGrid grid{ ActiveRasterGrid(e) };
	const I2 ca{ WorldToRasterCell(grid, a) };
	const I2 cb{ QuantizeAreaEndCell(e, ca, WorldToRasterCell(grid, b)) };
	const int min_x{ std::min(ca.x, cb.x) };
	const int max_x{ std::max(ca.x, cb.x) };
	const int min_y{ std::min(ca.y, cb.y) };
	const int max_y{ std::max(ca.y, cb.y) };
	const I2 footprint{ RecipePaintFootprintCells(e) };
	const int thickness_x{ std::max(1, e.brush.area_thickness) * footprint.x };
	const int thickness_y{ std::max(1, e.brush.area_thickness) * footprint.y };
	std::vector<I2> cells;
	cells.reserve(static_cast<std::size_t>((max_x - min_x + 1) * (max_y - min_y + 1)));
	for (int y{ min_y }; y <= max_y; ++y) {
		for (int x{ min_x }; x <= max_x; ++x) {
			const int left{ x - min_x };
			const int right{ max_x - x };
			const int top{ y - min_y };
			const int bottom{ max_y - y };
			const bool thick_edge{ left < thickness_x || right < thickness_x || top < thickness_y || bottom < thickness_y };
			const bool thick_corner{ (left < thickness_x || right < thickness_x) && (top < thickness_y || bottom < thickness_y) };
			if (e.brush.area_mode == AreaMode::Outline && !thick_edge) continue;
			if (e.brush.area_mode == AreaMode::Corners && !thick_corner) continue;
			cells.push_back({ x, y });
		}
	}
	return cells;
}

static bool PassesAreaRandomFill(const EditorState& e, I2 cell) {
	const std::uint32_t hash{
		Hash2(
			cell.x,
			cell.y,
			static_cast<std::uint32_t>(e.recipe.noise.seed) ^ 0xA511E9B3u
		)
	};
	const float value{
		static_cast<float>(hash & 0x00FFFFFFu) /
		static_cast<float>(0x01000000u)
	};
	return value <= e.recipe.density;
}


static void AdjustBrushDiameter(EditorState& e, int direction) {
	if (e.tool == Tool::Select) {
		e.brush.selection_diameter_cells = std::clamp(e.brush.selection_diameter_cells + direction, 1, 128);
	} else {
		e.brush.brush_diameter_tiles = std::clamp(e.brush.brush_diameter_tiles + direction, 1, 128);
	}
}

static void ItemTooltip(const char* text) {
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", text);
	}
}

static bool TileFootprintOverlapsExisting(
	const EditorState& e,
	const SceneLayer& layer,
	const Tilemap& map,
	I2 candidate,
	int tile_id,
	bool ignore_same_anchor
) {
	const RectF candidate_rect{ TileAnchorRect(e, map, candidate, tile_id, {}, e.recipe.tile_origin) };
	bool overlap{};
	ForEachTileAnchor(layer, map, [&](I2 cell, const TileCell& existing) {
		if (overlap || (ignore_same_anchor && cell == candidate)) {
			return;
		}
		if (RectsOverlap(candidate_rect, TileAnchorRect(e, map, cell, existing.tile_id, existing.offset, existing.origin))) {
			overlap = true;
		}
	});
	return overlap;
}

static bool CanPlaceTileAnchor(
	const EditorState& e,
	const SceneLayer& layer,
	const Tilemap& map,
	I2 cell,
	int tile_id
) {
	const auto* old{ ReadTileCell(layer, map, cell) };
	const bool anchor_occupied{ old && old->tile_id >= 0 };
	if (anchor_occupied && !e.recipe.replace_occupied_anchor) {
		return false;
	}

	if (e.recipe.tile_paint_mode == TilePaintMode::Grid) {
		// Grid painting deliberately allows native-size tiles to overlap neighboring
		// grid cells. Only the exact anchor cell can be replaced/skipped above.
		return true;
	}

	const I2 footprint{ TileFootprintCells(e, map, tile_id) };
	const I2 lattice_origin{ e.stroke.active ? WorldToCell(map, e.stroke.start_world) : I2{} };
	if (FloorMod(cell.x - lattice_origin.x, footprint.x) != 0 ||
		FloorMod(cell.y - lattice_origin.y, footprint.y) != 0) {
		return false;
	}

	if (!e.recipe.allow_visual_overlap && TileFootprintOverlapsExisting(e, layer, map, cell, tile_id, anchor_occupied && e.recipe.replace_occupied_anchor)) {
		return false;
	}
	return true;
}

static const char* AutotileFormatName(AutotileFormat format) {
	switch (format) {
		case AutotileFormat::Classic15: return "Classic 15";
		case AutotileFormat::Blob47: return "Blob 47 (8-neighbor)";
		case AutotileFormat::Subset16: return "4-neighbor 16";
		case AutotileFormat::DualGrid16: return "Dual Grid 16";
		case AutotileFormat::Wang16: return "Wang / Edge 16";
	}
	return "Autotile";
}

static int RequiredAutotileTileCount(AutotileFormat format) {
	switch (format) {
		case AutotileFormat::Classic15: return 15;
		case AutotileFormat::Blob47: return 47;
		case AutotileFormat::Subset16:
		case AutotileFormat::DualGrid16:
		case AutotileFormat::Wang16: return 16;
	}
	return 16;
}

static int TerrainRulesetAt(const SceneLayer& layer, const Tilemap& map, I2 cell) {
	if (const auto* c = ReadTileCell(layer, map, cell); c && c->terrain) {
		return c->terrain_ruleset_id;
	}
	return -1;
}

static bool HasTerrainRule(const SceneLayer& layer, const Tilemap& map, I2 cell, int ruleset_id) {
	return TerrainRulesetAt(layer, map, cell) == ruleset_id;
}

static int CardinalTerrainMask(const SceneLayer& layer, const Tilemap& map, I2 c, int ruleset_id) {
	int mask{};
	if (HasTerrainRule(layer, map, { c.x, c.y - 1 }, ruleset_id)) mask |= 1;  // N
	if (HasTerrainRule(layer, map, { c.x + 1, c.y }, ruleset_id)) mask |= 2;  // E
	if (HasTerrainRule(layer, map, { c.x, c.y + 1 }, ruleset_id)) mask |= 4;  // S
	if (HasTerrainRule(layer, map, { c.x - 1, c.y }, ruleset_id)) mask |= 8;  // W
	return mask;
}

static int BlobTerrainMask(const SceneLayer& layer, const Tilemap& map, I2 c, int ruleset_id) {
	const bool n{ HasTerrainRule(layer, map, { c.x, c.y - 1 }, ruleset_id) };
	const bool e{ HasTerrainRule(layer, map, { c.x + 1, c.y }, ruleset_id) };
	const bool s{ HasTerrainRule(layer, map, { c.x, c.y + 1 }, ruleset_id) };
	const bool w{ HasTerrainRule(layer, map, { c.x - 1, c.y }, ruleset_id) };
	int mask{};
	if (n) mask |= 1;
	if (e) mask |= 2;
	if (s) mask |= 4;
	if (w) mask |= 8;
	// Blob/47 convention: a diagonal is meaningful only when both adjacent
	// cardinal neighbors exist. This collapses the 256 raw 8-neighbor masks to 47.
	if (n && e && HasTerrainRule(layer, map, { c.x + 1, c.y - 1 }, ruleset_id)) mask |= 16;
	if (e && s && HasTerrainRule(layer, map, { c.x + 1, c.y + 1 }, ruleset_id)) mask |= 32;
	if (s && w && HasTerrainRule(layer, map, { c.x - 1, c.y + 1 }, ruleset_id)) mask |= 64;
	if (w && n && HasTerrainRule(layer, map, { c.x - 1, c.y - 1 }, ruleset_id)) mask |= 128;
	return mask;
}

static const std::vector<int>& ValidBlob47Masks() {
	static const std::vector<int> masks = [] {
		std::vector<int> result;
		for (int mask{}; mask < 256; ++mask) {
			const bool n{ (mask & 1) != 0 };
			const bool e{ (mask & 2) != 0 };
			const bool s{ (mask & 4) != 0 };
			const bool w{ (mask & 8) != 0 };
			if ((mask & 16) && !(n && e)) continue;
			if ((mask & 32) && !(e && s)) continue;
			if ((mask & 64) && !(s && w)) continue;
			if ((mask & 128) && !(w && n)) continue;
			result.push_back(mask);
		}
		return result;
	}();
	return masks;
}

static int AutotileIndex(const SceneLayer& layer, const Tilemap& map, I2 cell, const AutotileRuleSet& rules) {
	if (rules.format == AutotileFormat::Blob47) {
		const int mask{ BlobTerrainMask(layer, map, cell, rules.id) };
		const auto& valid{ ValidBlob47Masks() };
		if (const auto it = std::find(valid.begin(), valid.end(), mask); it != valid.end()) {
			return static_cast<int>(std::distance(valid.begin(), it));
		}
		return 0;
	}
	const int mask{ CardinalTerrainMask(layer, map, cell, rules.id) };
	if (rules.format == AutotileFormat::Classic15) {
		return mask == 0 ? 0 : std::clamp(mask - 1, 0, 14);
	}
	return std::clamp(mask, 0, 15);
}

static void RecomputeAutotile(EditorState& e, SceneLayer& layer, const Tilemap& map, I2 cell) {
	static constexpr std::array<I2, 9> offsets{
		I2{ 0, 0 }, I2{ 0, -1 }, I2{ 1, 0 }, I2{ 0, 1 }, I2{ -1, 0 },
		I2{ 1, -1 }, I2{ 1, 1 }, I2{ -1, 1 }, I2{ -1, -1 }
	};
	for (const I2 o : offsets) {
		const I2 c{ cell.x + o.x, cell.y + o.y };
		const int ruleset_id{ TerrainRulesetAt(layer, map, c) };
		const auto* rules{ FindAutotileRuleSet(e, ruleset_id) };
		if (!rules || rules->format == AutotileFormat::DualGrid16 || rules->tile_ids.empty()) continue;
		const int index{ AutotileIndex(layer, map, c, *rules) };
		if (index < 0 || index >= static_cast<int>(rules->tile_ids.size())) continue;
		if (auto* dst = WriteTileCell(layer, map, c)) {
			dst->tile_id = rules->tile_ids[static_cast<std::size_t>(index)];
			dst->terrain = true;
			dst->terrain_ruleset_id = ruleset_id;
			dst->origin = EntityOrigin::TopLeft;
			dst->offset = {};
		}
	}
}

static void DeleteSelection(EditorState& e) {
	if (e.selected_entities.empty() && e.selected_tile_cells.empty()) {
		return;
	}

	const SceneSnapshot before{ CaptureScene(e) };
	bool changed{};

	if (!e.selected_entities.empty()) {
		const auto old_size{ e.entities.size() };
		e.entities.erase(
			std::remove_if(
				e.entities.begin(),
				e.entities.end(),
				[&](const Entity& entity) {
					return e.selected_entities.contains(entity.id);
				}
			),
			e.entities.end()
		);
		changed = changed || e.entities.size() != old_size;
	}

	if (!e.selected_tile_cells.empty() && e.selected_tile_layer_id >= 0) {
		if (auto* layer = FindLayer(e, e.selected_tile_layer_id);
			layer && layer->kind == LayerKind::Tile && !layer->locked) {
			if (const auto* map = FindTilemap(e, layer->tile.tilemap_id)) {
				for (const I2 cell : e.selected_tile_cells) {
					const auto* old{ ReadTileCell(*layer, *map, cell) };
					if (!old || old->tile_id < 0) {
						continue;
					}
					const bool terrain{ old->terrain };
					EraseTile(*layer, *map, cell);
					if (terrain) {
						RecomputeAutotile(e, *layer, *map, cell);
					}
					changed = true;
				}
			}
		}
	}

	if (!changed) {
		return;
	}

	DeselectAll(e);
	PushHistory(e, "Delete Selection", before);
}

static bool HasSelection(const EditorState& e) {
	return !e.selected_entities.empty() || !e.selected_tile_cells.empty();
}

static void CopySelection(EditorState& e) {
	if (!HasSelection(e)) {
		return;
	}

	e.clipboard = {};
	e.clipboard.valid = true;

	if (!e.selected_entities.empty()) {
		e.clipboard.kind = LayerKind::Entity;
		for (const auto& entity : e.entities) {
			if (e.selected_entities.contains(entity.id)) {
				e.clipboard.entities.push_back(entity);
			}
		}
		return;
	}

	if (e.selected_tile_layer_id < 0) {
		e.clipboard.valid = false;
		return;
	}

	const auto* layer{ FindLayer(e, e.selected_tile_layer_id) };
	if (!layer || layer->kind != LayerKind::Tile) {
		e.clipboard.valid = false;
		return;
	}
	const auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
	if (!map || e.selected_tile_cells.empty()) {
		e.clipboard.valid = false;
		return;
	}

	e.clipboard.kind = LayerKind::Tile;
	e.clipboard.source_layer_id = layer->id;
	I2 min_cell{ std::numeric_limits<int>::max(), std::numeric_limits<int>::max() };
	for (const I2 cell : e.selected_tile_cells) {
		min_cell.x = std::min(min_cell.x, cell.x);
		min_cell.y = std::min(min_cell.y, cell.y);
	}
	e.clipboard.tile_origin = min_cell;
	for (const I2 cell : e.selected_tile_cells) {
		if (const auto* tile = ReadTileCell(*layer, *map, cell); tile && tile->tile_id >= 0) {
			e.clipboard.tiles.push_back({ { cell.x - min_cell.x, cell.y - min_cell.y }, *tile });
		}
	}
	if (e.clipboard.tiles.empty()) {
		e.clipboard.valid = false;
	}
}

static void CutSelection(EditorState& e) {
	if (!HasSelection(e)) {
		return;
	}
	CopySelection(e);
	DeleteSelection(e);
}

static void PasteClipboard(EditorState& e) {
	if (!e.clipboard.valid) {
		return;
	}

	const SceneSnapshot before{ CaptureScene(e) };
	DeselectAll(e);
	bool changed{};

	if (e.clipboard.kind == LayerKind::Entity) {
		const F2 offset{
			std::max(1.0f, e.grid.size.x),
			std::max(1.0f, e.grid.size.y),
		};
		for (const auto& source : e.clipboard.entities) {
			Entity copy{ source };
			copy.id = e.next_entity_id++;
			if (!FindLayer(e, copy.layer_id)) {
				if (const auto* active = FindLayer(e, e.active_layer_id); active && active->kind == LayerKind::Entity) {
					copy.layer_id = active->id;
				} else {
					continue;
				}
			}
			copy.position += offset;
			e.selected_entities.insert(copy.id);
			e.primary_entity_id = copy.id;
			e.entities.push_back(std::move(copy));
			changed = true;
		}
	} else if (e.clipboard.kind == LayerKind::Tile) {
		SceneLayer* layer{};
		if (auto* active = FindLayer(e, e.active_layer_id); active && active->kind == LayerKind::Tile && !active->locked) {
			layer = active;
		} else {
			layer = FindLayer(e, e.clipboard.source_layer_id);
		}
		if (layer && layer->kind == LayerKind::Tile && !layer->locked) {
			if (const auto* map = FindTilemap(e, layer->tile.tilemap_id)) {
				const I2 origin{ e.clipboard.tile_origin.x + 1, e.clipboard.tile_origin.y + 1 };
				e.selected_tile_layer_id = layer->id;
				for (const auto& item : e.clipboard.tiles) {
					const I2 target{ origin.x + item.relative_cell.x, origin.y + item.relative_cell.y };
					*WriteTileCell(*layer, *map, target) = item.cell;
					e.selected_tile_cells.insert(target);
					changed = true;
				}
			}
		}
	}

	if (changed) {
		e.tool = Tool::Move;
		PushHistory(e, "Paste Selection", before);
	}
}

static bool SelectionHitAtWorld(const EditorState& e, F2 world) {
	for (const auto& entity : e.entities) {
		if (!e.selected_entities.contains(entity.id)) {
			continue;
		}
		const RectF bounds{ EntityBounds(entity) };
		if (world.x >= bounds.min.x && world.x <= bounds.max.x &&
			world.y >= bounds.min.y && world.y <= bounds.max.y) {
			return true;
		}
	}
	if (e.selected_tile_layer_id >= 0) {
		const auto* layer{ FindLayer(e, e.selected_tile_layer_id) };
		if (layer && layer->kind == LayerKind::Tile) {
			if (const auto* map = FindTilemap(e, layer->tile.tilemap_id)) {
				for (const I2 cell : e.selected_tile_cells) {
					if (const auto* tile = ReadTileCell(*layer, *map, cell); tile && tile->tile_id >= 0) {
						const RectF r{ TileAnchorRect(e, *map, cell, tile->tile_id, tile->offset, tile->origin) };
						if (world.x >= r.min.x && world.x <= r.max.x && world.y >= r.min.y && world.y <= r.max.y) {
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

static F2 SelectionMoveUnit(const EditorState& e) {
	if (!e.selected_tile_cells.empty() && e.selected_tile_layer_id >= 0) {
		if (const auto* layer = FindLayer(e, e.selected_tile_layer_id); layer && layer->kind == LayerKind::Tile) {
			if (const auto* map = FindTilemap(e, layer->tile.tilemap_id)) {
				return { std::max(1.0f, map->cell_size.x), std::max(1.0f, map->cell_size.y) };
			}
		}
	}
	return { std::max(1.0f, e.grid.size.x), std::max(1.0f, e.grid.size.y) };
}

static void ApplyMoveFromSnapshot(
	EditorState& e,
	const SceneSnapshot& base,
	F2 raw_delta,
	bool free_movement
) {
	RestoreScene(e, base);
	const F2 unit{ SelectionMoveUnit(e) };
	F2 delta{ raw_delta };
	if (!free_movement) {
		delta.x = std::round(delta.x / unit.x) * unit.x;
		delta.y = std::round(delta.y / unit.y) * unit.y;
	}

	if (!e.selected_entities.empty()) {
		for (auto& entity : e.entities) {
			if (e.selected_entities.contains(entity.id)) {
				entity.position += delta;
			}
		}
		return;
	}

	if (e.selected_tile_cells.empty() || e.selected_tile_layer_id < 0) {
		return;
	}
	SceneLayer* layer{ FindLayer(e, e.selected_tile_layer_id) };
	if (!layer || layer->kind != LayerKind::Tile || layer->locked) {
		return;
	}
	const auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
	if (!map) {
		return;
	}

	if (free_movement) {
		for (const I2 cell : e.selected_tile_cells) {
			if (auto* tile = WriteTileCell(*layer, *map, cell); tile && tile->tile_id >= 0) {
				tile->offset += delta;
			}
		}
		return;
	}

	const I2 cell_delta{
		static_cast<int>(std::lround(delta.x / unit.x)),
		static_cast<int>(std::lround(delta.y / unit.y)),
	};
	if (cell_delta == I2{}) {
		return;
	}

	std::vector<std::pair<I2, TileCell>> moved;
	moved.reserve(e.selected_tile_cells.size());
	for (const I2 cell : e.selected_tile_cells) {
		if (const auto* tile = ReadTileCell(*layer, *map, cell); tile && tile->tile_id >= 0) {
			moved.push_back({ cell, *tile });
		}
	}
	for (const auto& [cell, _] : moved) {
		EraseTile(*layer, *map, cell);
	}
	e.selected_tile_cells.clear();
	for (auto& [cell, tile] : moved) {
		const I2 target{ cell.x + cell_delta.x, cell.y + cell_delta.y };
		*WriteTileCell(*layer, *map, target) = tile;
		e.selected_tile_cells.insert(target);
	}
}

static void MoveSelectionByKeyboard(EditorState& e, I2 direction, bool ctrl, bool shift) {
	if (!HasSelection(e) || direction == I2{}) {
		return;
	}
	const SceneSnapshot before{ CaptureScene(e) };
	const bool default_free{ e.move.snap_mode == MoveSnapMode::Free };
	const bool free_movement{ ctrl ? !default_free : default_free };
	const float speed{ shift ? 10.0f : 1.0f };
	const F2 unit{ SelectionMoveUnit(e) };
	const F2 delta{
		static_cast<float>(direction.x) * (free_movement ? 1.0f : unit.x) * speed,
		static_cast<float>(direction.y) * (free_movement ? 1.0f : unit.y) * speed,
	};
	ApplyMoveFromSnapshot(e, before, delta, free_movement);
	PushHistory(e, "Move Selection", before);
}

static void SnapSelectionToGrid(EditorState& e) {
	if (!HasSelection(e)) return;
	const SceneSnapshot before{ CaptureScene(e) };
	bool changed{};

	for (auto& entity : e.entities) {
		if (!e.selected_entities.contains(entity.id)) continue;
		const F2 f{ EntityOriginFraction(entity.origin) };
		const F2 anchor_offset{ f.x * e.grid.size.x, f.y * e.grid.size.y };
		const F2 snapped{
			e.grid.offset.x + std::round((entity.position.x - e.grid.offset.x - anchor_offset.x) / std::max(1.0f, e.grid.size.x)) * std::max(1.0f, e.grid.size.x) + anchor_offset.x,
			e.grid.offset.y + std::round((entity.position.y - e.grid.offset.y - anchor_offset.y) / std::max(1.0f, e.grid.size.y)) * std::max(1.0f, e.grid.size.y) + anchor_offset.y,
		};
		if (Distance(entity.position, snapped) > 0.001f) {
			entity.position = snapped;
			changed = true;
		}
	}

	if (!e.selected_tile_cells.empty() && e.selected_tile_layer_id >= 0) {
		auto* layer{ FindLayer(e, e.selected_tile_layer_id) };
		const auto* map{ layer && layer->kind == LayerKind::Tile ? FindTilemap(e, layer->tile.tilemap_id) : nullptr };
		if (layer && map && !layer->locked) {
			std::vector<std::pair<I2, TileCell>> moved;
			for (const I2 cell : e.selected_tile_cells) {
				if (const auto* tile = ReadTileCell(*layer, *map, cell); tile && tile->tile_id >= 0) {
					moved.push_back({ cell, *tile });
				}
			}
			for (const auto& [cell, tile] : moved) {
				const F2 f{ EntityOriginFraction(tile.origin) };
				const F2 cell_min{ CellToWorld(*map, cell) };
				const F2 anchor{
					cell_min.x + map->cell_size.x * f.x + tile.offset.x,
					cell_min.y + map->cell_size.y * f.y + tile.offset.y,
				};
				const I2 target{
					static_cast<int>(std::lround((anchor.x - map->origin.x - map->cell_size.x * f.x) / std::max(1.0f, map->cell_size.x))),
					static_cast<int>(std::lround((anchor.y - map->origin.y - map->cell_size.y * f.y) / std::max(1.0f, map->cell_size.y))),
				};
				if (target != cell || tile.offset.x != 0.0f || tile.offset.y != 0.0f) changed = true;
			}
			if (changed && !moved.empty()) {
				for (const auto& [cell, _] : moved) EraseTile(*layer, *map, cell);
				e.selected_tile_cells.clear();
				for (auto [cell, tile] : moved) {
					const F2 f{ EntityOriginFraction(tile.origin) };
					const F2 cell_min{ CellToWorld(*map, cell) };
					const F2 anchor{ cell_min.x + map->cell_size.x * f.x + tile.offset.x, cell_min.y + map->cell_size.y * f.y + tile.offset.y };
					const I2 target{
						static_cast<int>(std::lround((anchor.x - map->origin.x - map->cell_size.x * f.x) / std::max(1.0f, map->cell_size.x))),
						static_cast<int>(std::lround((anchor.y - map->origin.y - map->cell_size.y * f.y) / std::max(1.0f, map->cell_size.y))),
					};
					tile.offset = {};
					*WriteTileCell(*layer, *map, target) = tile;
					e.selected_tile_cells.insert(target);
				}
			}
		}
	}
	if (changed) PushHistory(e, "Snap Selection to Grid", before);
} 

static bool IsExcluded(const Tilemap& map, I2 cell) {
	return map.exclusion_mask.contains(cell);
}

static float RandomRange(EditorState& e, float a, float b) {
	std::uniform_real_distribution<float> d{ a, b };
	return d(e.rng);
}

static float Hash01(I2 cell, std::uint32_t seed) {
	return static_cast<float>(Hash2(cell.x, cell.y, seed) & 0x00FFFFFFu) /
		static_cast<float>(0x01000000u);
}

static int WeightedTileFromSet(
	EditorState& e,
	int set_id,
	std::optional<float> selector = std::nullopt
) {
	const auto* set{ FindWeightedTileSet(e, set_id) };
	if (!set || set->entries.empty()) {
		return FindTile(e, e.recipe.tile_id) ? e.recipe.tile_id : e.active_tile_id;
	}
	float sum{};
	for (const auto& item : set->entries) {
		if (FindTile(e, item.tile_id)) sum += std::max(0.0f, item.weight);
	}
	if (sum <= 0.0f) return set->entries.front().tile_id;
	float r{ selector ? std::clamp(*selector, 0.0f, 0.999999f) * sum : RandomRange(e, 0.0f, sum) };
	for (const auto& item : set->entries) {
		if (!FindTile(e, item.tile_id)) continue;
		r -= std::max(0.0f, item.weight);
		if (r <= 0.0f) return item.tile_id;
	}
	return set->entries.back().tile_id;
}

static int WeightedPrefabFromSet(
	EditorState& e,
	int set_id,
	std::optional<float> selector = std::nullopt
) {
	const auto* set{ FindWeightedPrefabSet(e, set_id) };
	if (!set || set->entries.empty()) {
		return e.prefabs.empty() ? -1 : std::clamp(e.recipe.prefab_index, 0, static_cast<int>(e.prefabs.size()) - 1);
	}
	float sum{};
	for (const auto& item : set->entries) {
		if (item.prefab_index >= 0 && item.prefab_index < static_cast<int>(e.prefabs.size())) {
			sum += std::max(0.0f, item.weight);
		}
	}
	if (sum <= 0.0f) return set->entries.front().prefab_index;
	float r{ selector ? std::clamp(*selector, 0.0f, 0.999999f) * sum : RandomRange(e, 0.0f, sum) };
	for (const auto& item : set->entries) {
		if (item.prefab_index < 0 || item.prefab_index >= static_cast<int>(e.prefabs.size())) continue;
		r -= std::max(0.0f, item.weight);
		if (r <= 0.0f) return item.prefab_index;
	}
	return set->entries.back().prefab_index;
}

static const NoiseThresholdRegion* RecipeNoiseThreshold(const PaintRecipe& recipe, float value) {
	for (const auto& threshold : recipe.noise.thresholds) {
		if (!threshold.enabled) continue;
		const float lo{ std::min(threshold.minimum, threshold.maximum) };
		const float hi{ std::max(threshold.minimum, threshold.maximum) };
		if (value >= lo && (value < hi || (hi >= 0.99999f && value <= 1.0f))) {
			return &threshold;
		}
	}
	return nullptr;
}

static float RecipeNoiseValue(F2 world, const PaintRecipe& recipe) {
	const NoiseField& field{ recipe.noise };
	float frequency{ std::max(0.00001f, field.frequency) };
	float amplitude{ 1.0f };
	float total{};
	float weight{};
	for (int octave{}; octave < std::clamp(field.octaves, 1, 12); ++octave) {
		const float n{ BaseNoise2(
			field.type,
			(world.x + field.offset.x) * frequency,
			(world.y + field.offset.y) * frequency,
			static_cast<std::uint32_t>(field.seed + octave * 1013)
		) };
		total += n * amplitude;
		weight += amplitude;
		frequency *= std::max(1.0f, field.lacunarity);
		amplitude *= std::clamp(field.persistence, 0.0f, 1.0f);
	}
	return weight > 0.0f ? std::clamp(total / weight, 0.0f, 1.0f) : 0.0f;
}

static EntityOrigin RecipeSourceOrigin(
	const PaintRecipe& recipe,
	F2 world,
	bool tile_source
) {
	if (recipe.source_kind == PaintSourceKind::Noise) {
		if (const auto* threshold = RecipeNoiseThreshold(recipe, RecipeNoiseValue(world, recipe))) {
			return threshold->origin;
		}
	}
	return tile_source ? recipe.tile_origin : recipe.entity_origin;
}

static int ChooseTileFromRecipe(
	EditorState& e,
	const PaintRecipe& recipe,
	F2 world,
	I2 cell,
	bool deterministic = false
) {
	if (recipe.source_kind == PaintSourceKind::Noise) {
		const auto* threshold{ RecipeNoiseThreshold(recipe, RecipeNoiseValue(world, recipe)) };
		if (!threshold || !threshold->enabled) return -1;
		if (threshold->source_kind == PaintSourceKind::WeightedSet) {
			return WeightedTileFromSet(
				e,
				threshold->weighted_tile_set_id,
				deterministic ? std::optional<float>{ Hash01(cell, static_cast<std::uint32_t>(recipe.noise.seed) ^ 0x7251u) } : std::nullopt
			);
		}
		return threshold->source_kind == PaintSourceKind::Single ? threshold->tile_id : -1;
	}

	switch (recipe.source_kind) {
		case PaintSourceKind::Single:
			return FindTile(e, recipe.tile_id) ? recipe.tile_id : e.active_tile_id;
		case PaintSourceKind::WeightedSet:
			return WeightedTileFromSet(
				e,
				recipe.weighted_tile_set_id,
				deterministic ? std::optional<float>{ Hash01(cell, static_cast<std::uint32_t>(recipe.noise.seed) ^ 0xA17Eu) } : std::nullopt
			);
		case PaintSourceKind::Checkerboard:
			return ((cell.x + cell.y) & 1) == 0 ? recipe.tile_id : recipe.secondary_tile_id;
		case PaintSourceKind::Autotile:
			if (const auto* rules = FindAutotileRuleSet(e, recipe.autotile_ruleset_id); rules && !rules->tile_ids.empty()) return rules->tile_ids.front();
			return recipe.tile_id;
		case PaintSourceKind::Noise:
			break;
	}
	return -1;
}

static int ChoosePrefabFromRecipe(
	EditorState& e,
	const PaintRecipe& recipe,
	F2 world,
	I2 cell,
	bool deterministic = false
) {
	if (e.prefabs.empty()) return -1;
	if (recipe.source_kind == PaintSourceKind::Noise) {
		const auto* threshold{ RecipeNoiseThreshold(recipe, RecipeNoiseValue(world, recipe)) };
		if (!threshold || !threshold->enabled) return -1;
		if (threshold->source_kind == PaintSourceKind::WeightedSet) {
			return WeightedPrefabFromSet(
				e,
				threshold->weighted_prefab_set_id,
				deterministic ? std::optional<float>{ Hash01(cell, static_cast<std::uint32_t>(recipe.noise.seed) ^ 0xC341u) } : std::nullopt
			);
		}
		return threshold->source_kind == PaintSourceKind::Single ? threshold->prefab_index : -1;
	}

	switch (recipe.source_kind) {
		case PaintSourceKind::Single:
			return std::clamp(recipe.prefab_index, 0, static_cast<int>(e.prefabs.size()) - 1);
		case PaintSourceKind::WeightedSet:
			return WeightedPrefabFromSet(
				e,
				recipe.weighted_prefab_set_id,
				deterministic ? std::optional<float>{ Hash01(cell, static_cast<std::uint32_t>(recipe.noise.seed) ^ 0xBEEFu) } : std::nullopt
			);
		case PaintSourceKind::Checkerboard:
			return ((cell.x + cell.y) & 1) == 0 ? recipe.prefab_index : recipe.secondary_prefab_index;
		case PaintSourceKind::Autotile:
			return recipe.prefab_index;
		case PaintSourceKind::Noise:
			break;
	}
	return -1;
}

static int ChooseTile(EditorState& e, F2 world) {
	const RasterGrid grid{ ActiveRasterGrid(e) };
	return ChooseTileFromRecipe(e, e.recipe, world, WorldToRasterCell(grid, world));
}

static int ChoosePrefab(EditorState& e, F2 world) {
	const RasterGrid grid{ ActiveRasterGrid(e) };
	return ChoosePrefabFromRecipe(e, e.recipe, world, WorldToRasterCell(grid, world));
}

static bool PassesDistribution(EditorState& e, F2 world) {
	const PaintRecipe& recipe{ e.recipe };
	if (recipe.source_kind == PaintSourceKind::Noise) {
		const auto* threshold{ RecipeNoiseThreshold(recipe, RecipeNoiseValue(world, recipe)) };
		if (!threshold || !threshold->enabled) return false;
		if (threshold->source_kind == PaintSourceKind::Single) {
			if (threshold->tile_id < 0 && threshold->prefab_index < 0) return false;
		} else if (threshold->weighted_tile_set_id < 0 && threshold->weighted_prefab_set_id < 0) {
			return false;
		}
	}
	switch (recipe.coverage) {
		case PaintCoverageKind::Solid:
			return true;
		case PaintCoverageKind::RandomDensity:
			return RandomRange(e, 0.0f, 1.0f) <= recipe.density;
		case PaintCoverageKind::RadialFalloff: {
			const float radius{ std::max(1.0f, EffectiveBrushRadius(e)) };
			const float normalized{ std::clamp(Distance(world, e.stroke.current_world) / radius, 0.0f, 1.0f) };
			const float inner{ std::clamp(recipe.radial_inner, 0.0f, 1.0f) };
			const float outer{ std::max(inner + 0.001f, std::clamp(recipe.radial_outer, 0.0f, 1.0f)) };
			const float t{ std::clamp((normalized - inner) / (outer - inner), 0.0f, 1.0f) };
			return RandomRange(e, 0.0f, 1.0f) <= (1.0f - t) * recipe.density;
		}
	}
	return true;
}

static bool TooCloseToEntity(const EditorState& e, int layer_id, F2 p, float distance) {
	for (const auto& entity : e.entities) {
		if (entity.layer_id == layer_id && Distance(entity.position, p) < distance) {
			return true;
		}
	}
	return false;
}

static void PlaceEntity(EditorState& e, SceneLayer& layer, F2 world) {
	if (layer.locked || layer.kind != LayerKind::Entity || !PassesDistribution(e, world)) return;
	const EntityOrigin origin{ RecipeSourceOrigin(e.recipe, world, false) };
	if (e.grid.snap) {
		world = SnapEntityPlacementToGrid(e, world, origin);
	}
	if (e.recipe.min_spacing > 0.0f && TooCloseToEntity(e, layer.id, world, e.recipe.min_spacing)) return;

	const int prefab_index{ ChoosePrefab(e, world) };
	if (prefab_index < 0 || prefab_index >= static_cast<int>(e.prefabs.size())) return;
	const auto& prefab{ e.prefabs[static_cast<std::size_t>(prefab_index)] };
	Entity entity;
	entity.id = e.next_entity_id++;
	entity.layer_id = layer.id;
	entity.prefab = prefab.name;
	entity.position = world;
	entity.size = prefab.size;
	entity.origin = origin;
	if (e.recipe.random_rotation) entity.rotation = RandomRange(e, e.recipe.rotation_min, e.recipe.rotation_max);
	if (e.recipe.random_scale) entity.scale = RandomRange(e, e.recipe.scale_min, e.recipe.scale_max);
	e.entities.push_back(std::move(entity));
	e.stroke.changed = true;
}

static void PlaceAutotileCell(EditorState& e, SceneLayer& layer, Tilemap& map, I2 cell) {
	const auto* rules{ FindAutotileRuleSet(e, e.recipe.autotile_ruleset_id) };
	if (!rules || rules->tile_ids.empty()) return;
	if (rules->format == AutotileFormat::DualGrid16) {
		layer.tile.dual_grid_terrain[cell] = rules->id;
		e.stroke.changed = true;
		return;
	}
	const int tile_id{ rules->tile_ids.front() };
	if (tile_id < 0) return;
	SetTile(layer, map, cell, tile_id, true, EntityOrigin::TopLeft, rules->id);
	RecomputeAutotile(e, layer, map, cell);
	e.stroke.changed = true;
}

static void PlaceTile(EditorState& e, SceneLayer& layer, Tilemap& map, I2 cell) {
	if (layer.locked || layer.kind != LayerKind::Tile) return;
	if (e.recipe.avoid_exclusion_mask && IsExcluded(map, cell) && e.brush.operation != BrushOperation::ExclusionMask) return;

	const F2 world{ CellToWorld(map, cell) };
	if (!PassesDistribution(e, world)) return;
	if (e.brush.operation == BrushOperation::ExclusionMask) {
		if (map.exclusion_mask.insert(cell).second) e.stroke.changed = true;
		return;
	}

	if (e.recipe.source_kind == PaintSourceKind::Autotile) {
		// Replace and Paint both mean "make this logical cell part of this terrain".
		PlaceAutotileCell(e, layer, map, cell);
		return;
	}

	if (e.brush.operation == BrushOperation::Paint && !e.stamp_tiles.empty() && e.recipe.source_kind == PaintSourceKind::Single) {
		const int width{ std::max(1, e.stamp_width) };
		for (int i{}; i < static_cast<int>(e.stamp_tiles.size()); ++i) {
			const I2 target{ cell.x + i % width, cell.y + i / width };
			const int stamp_tile{ e.stamp_tiles[static_cast<std::size_t>(i)] };
			if (e.recipe.avoid_exclusion_mask && IsExcluded(map, target)) continue;
			const auto* old{ ReadTileCell(layer, map, target) };
			const bool occupied{ old && old->tile_id >= 0 };
			if (occupied && !e.recipe.replace_occupied_anchor) continue;
			SetTile(layer, map, target, stamp_tile, false, e.recipe.tile_origin);
			e.stroke.changed = true;
		}
		return;
	}

	const int tile_id{ ChooseTile(e, world) };
	if (tile_id < 0) return;
	const auto* old{ ReadTileCell(layer, map, cell) };
	const bool occupied{ old && old->tile_id >= 0 };
	if (e.brush.operation == BrushOperation::Replace) {
		if (!occupied) return;
	} else if (!CanPlaceTileAnchor(e, layer, map, cell, tile_id)) {
		return;
	}
	SetTile(layer, map, cell, tile_id, false, RecipeSourceOrigin(e.recipe, world, true));
	e.stroke.changed = true;
}

static void ReplaceEntitiesAt(EditorState& e, SceneLayer& layer, F2 world, const std::vector<I2>& raster_cells) {
	if (e.prefabs.empty()) return;
	const RasterGrid raster{ ActiveRasterGrid(e) };
	for (const I2 cell : raster_cells) {
		const F2 sample{ RectCenter(RasterCellRect(raster, cell)) };
		if (!PassesDistribution(e, sample)) continue;
		const int to_index{ ChoosePrefabFromRecipe(e, e.recipe, sample, cell) };
		if (to_index < 0 || to_index >= static_cast<int>(e.prefabs.size())) continue;
		const auto& to{ e.prefabs[static_cast<std::size_t>(to_index)] };
		const RectF target{ RasterCellRect(raster, cell) };
		for (auto& entity : e.entities) {
			if (entity.layer_id != layer.id || e.stroke.touched_entities.contains(entity.id) || !RectsOverlap(EntityBounds(entity), target)) continue;
			e.stroke.touched_entities.insert(entity.id);
			entity.prefab = to.name;
			entity.size = to.size;
			entity.origin = e.recipe.entity_origin;
			e.stroke.changed = true;
		}
	}
}

static void PaintAt(EditorState& e, F2 world) {
	auto* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->locked || layer->kind == LayerKind::Noise) return;
	e.stroke.current_world = world;

	if (e.tool == Tool::Pencil) {
		if (layer->kind == LayerKind::Tile) {
			auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
			if (!map) return;
			const I2 cell{ WorldToCell(*map, world) };
			if (!e.stroke.touched_cells.insert(cell).second) return;
			PlaceTile(e, *layer, *map, cell);
			return;
		}
		const RasterGrid raster{ ActiveRasterGrid(e) };
		const I2 cell{ WorldToRasterCell(raster, world) };
		if (e.brush.operation == BrushOperation::Replace) {
			ReplaceEntitiesAt(e, *layer, world, std::vector<I2>{ cell });
			return;
		}
		if (e.grid.snap && !e.stroke.touched_cells.insert(cell).second) return;
		PlaceEntity(e, *layer, world);
		return;
	}

	const RasterGrid raster{ ActiveRasterGrid(e) };
	const std::vector<I2> raster_cells{ RasterBrushCells(e, world) };
	if (layer->kind == LayerKind::Tile) {
		auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
		if (!map) return;
		for (const I2 cell : raster_cells) {
			if (!e.stroke.touched_cells.insert(cell).second) continue;
			PlaceTile(e, *layer, *map, cell);
		}
		return;
	}

	if (e.brush.operation == BrushOperation::ExclusionMask) return;
	if (e.brush.operation == BrushOperation::Replace) {
		ReplaceEntitiesAt(e, *layer, world, raster_cells);
		return;
	}
	for (const I2 cell : raster_cells) {
		if (!e.stroke.touched_cells.insert(cell).second) continue;
		const RectF cell_rect{ RasterCellRect(raster, cell) };
		const F2 f{ EntityOriginFraction(e.recipe.entity_origin) };
		const F2 placement{
			cell_rect.min.x + (cell_rect.max.x - cell_rect.min.x) * f.x,
			cell_rect.min.y + (cell_rect.max.y - cell_rect.min.y) * f.y,
		};
		PlaceEntity(e, *layer, placement);
	}
}

static bool GeneratorCoveragePass(const PaintGenerator& generator, I2 cell);
static bool AddGeneratorSuppression(PaintGenerator& generator, I2 cell);
static std::optional<GeneratorHit> FindTopmostGeneratorAtWorld(EditorState& e, const SceneLayer& layer, F2 world);
static std::optional<GeneratorHit> FindTopmostGeneratorSceneHit(EditorState& e, F2 world);
static int GeneratorAutotileIndex(const PaintGenerator& generator, const AutotileRuleSet& rules, I2 cell);

static void EraseAt(EditorState& e, F2 world) {
	auto* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->locked) return;

	const std::vector<I2> raster_cells{ RasterBrushCells(e, world) };
	if (raster_cells.empty()) return;
	const RasterGrid raster{ ActiveRasterGrid(e) };

	// Only process cells newly reached by this continuous eraser stroke. This is
	// the main hot-path optimization: interpolation samples heavily overlap.
	std::unordered_set<I2, I2Hash> new_cells;
	new_cells.reserve(raster_cells.size());
	for (const I2 cell : raster_cells) {
		if (e.stroke.touched_cells.insert(cell).second) new_cells.insert(cell);
	}
	if (new_cells.empty()) return;

	// Persistent generators store erasure as deterministic suppression overrides.
	for (auto& generator : e.generators) {
		if (generator.layer_id != layer->id || !generator.visible) continue;
		for (const I2 brush_cell : new_cells) {
			const F2 sample{ RectCenter(RasterCellRect(raster, brush_cell)) };
			const I2 generator_cell{
				static_cast<int>(std::floor((sample.x - generator.grid_offset.x) / std::max(1.0f, generator.grid_size.x))),
				static_cast<int>(std::floor((sample.y - generator.grid_offset.y) / std::max(1.0f, generator.grid_size.y))),
			};
			if (
				GeneratorCoveragePass(generator, generator_cell) &&
				AddGeneratorSuppression(generator, generator_cell)
			) {
				e.stroke.changed = true;
			}
		}
	}

	if (layer->kind == LayerKind::Tile) {
		auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
		if (!map) return;

		if (e.brush.operation == BrushOperation::ExclusionMask) {
			for (const I2 cell : new_cells) {
				if (map->exclusion_mask.erase(cell) > 0) e.stroke.changed = true;
			}
			return;
		}

		for (const I2 cell : new_cells) {
			if (layer->tile.dual_grid_terrain.erase(cell) > 0) e.stroke.changed = true;
		}

		// Search only anchors close enough for their native image to overlap one of
		// the newly touched cells. This replaces the old all-tiles scan per cell.
		int max_cells_x{ 1 };
		int max_cells_y{ 1 };
		for (const auto& tile : e.tiles) {
			max_cells_x = std::max(
				max_cells_x,
				static_cast<int>(std::ceil(static_cast<float>(std::max(1, tile.pixel_w)) / std::max(1.0f, map->cell_size.x)))
			);
			max_cells_y = std::max(
				max_cells_y,
				static_cast<int>(std::ceil(static_cast<float>(std::max(1, tile.pixel_h)) / std::max(1.0f, map->cell_size.y)))
			);
		}

		std::unordered_set<I2, I2Hash> anchors_to_erase;
		for (const I2 brush_cell : new_cells) {
			const RectF brush_rect{ RasterCellRect(raster, brush_cell) };
			for (int oy{ -max_cells_y }; oy <= max_cells_y; ++oy) {
				for (int ox{ -max_cells_x }; ox <= max_cells_x; ++ox) {
					const I2 anchor{ brush_cell.x + ox, brush_cell.y + oy };
					const TileCell* tile{ ReadTileCell(*layer, *map, anchor) };
					if (!tile || tile->tile_id < 0) continue;
					if (
						e.brush.eraser_current_source_only &&
						tile->tile_id != e.active_tile_id
					) {
						continue;
					}
					if (
						RectsOverlap(
							brush_rect,
							TileAnchorRect(e, *map, anchor, tile->tile_id, tile->offset, tile->origin)
						)
					) {
						anchors_to_erase.insert(anchor);
					}
				}
			}
		}

		for (const I2 anchor : anchors_to_erase) {
			const auto* old{ ReadTileCell(*layer, *map, anchor) };
			const bool was_terrain{ old && old->terrain };
			EraseTile(*layer, *map, anchor);
			e.selected_tile_cells.erase(anchor);
			if (was_terrain) RecomputeAutotile(e, *layer, *map, anchor);
			e.stroke.changed = true;
		}
		return;
	}

	const std::string current_prefab{
		e.prefabs.empty()
			? std::string{}
			: e.prefabs[static_cast<std::size_t>(
				std::clamp(e.active_prefab_index, 0, static_cast<int>(e.prefabs.size()) - 1)
			)].name
	};

	auto entity_touches_new_cell = [&](const Entity& entity) {
		const RectF bounds{ EntityBounds(entity) };
		const I2 first{ WorldToRasterCell(raster, bounds.min) };
		const F2 max_inside{
			bounds.max.x - std::max(0.0001f, raster.size.x * 0.0001f),
			bounds.max.y - std::max(0.0001f, raster.size.y * 0.0001f),
		};
		const I2 last{ WorldToRasterCell(raster, max_inside) };
		for (int y{ first.y }; y <= last.y; ++y) {
			for (int x{ first.x }; x <= last.x; ++x) {
				if (new_cells.contains({ x, y })) return true;
			}
		}
		return false;
	};

	const auto old_size{ e.entities.size() };
	e.entities.erase(
		std::remove_if(
			e.entities.begin(),
			e.entities.end(),
			[&](const Entity& entity) {
				if (entity.layer_id != layer->id || !entity_touches_new_cell(entity)) return false;
				if (e.brush.eraser_current_source_only && entity.prefab != current_prefab) return false;
				e.selected_entities.erase(entity.id);
				if (e.primary_entity_id == entity.id) e.primary_entity_id = -1;
				return true;
			}
		),
		e.entities.end()
	);
	if (e.entities.size() != old_size) e.stroke.changed = true;
}

static void FloodFill(EditorState& e, F2 world) {
	auto* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->kind != LayerKind::Tile || layer->locked) {
		return;
	}

	auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
	if (!map) {
		return;
	}

	const I2 start{ WorldToCell(*map, world) };
	const auto* start_cell{ ReadTileCell(*layer, *map, start) };
	const int old_tile{ start_cell ? start_cell->tile_id : -1 };

	const int single_new_tile{
		e.recipe.source_kind == PaintSourceKind::Single
			? e.recipe.tile_id
			: -2
	};
	if (single_new_tile >= 0 && old_tile == single_new_tile) {
		return;
	}

	// The tilemap is intentionally unbounded. For an empty-region fill, the
	// current visible viewport is the finite editing boundary.
	I2 bound_min{};
	I2 bound_max{};
	if (old_tile < 0) {
		const F2 w0{ ScreenToWorld(e, e.canvas_screen_min) };
		const F2 w1{ ScreenToWorld(e, e.canvas_screen_max) };
		const I2 c0{ WorldToCell(*map, w0) };
		const I2 c1{ WorldToCell(*map, w1) };
		bound_min = {
			std::min(c0.x, c1.x),
			std::min(c0.y, c1.y),
		};
		bound_max = {
			std::max(c0.x, c1.x),
			std::max(c0.y, c1.y),
		};
	}

	std::vector<I2> stack{ start };
	std::unordered_set<I2, I2Hash> visited;
	constexpr std::size_t kMaxFloodCells{ 50000 };

	while (!stack.empty() && visited.size() < kMaxFloodCells) {
		const I2 cell{ stack.back() };
		stack.pop_back();

		if (!visited.insert(cell).second) {
			continue;
		}

		if (old_tile < 0 &&
			(cell.x < bound_min.x || cell.x > bound_max.x ||
			 cell.y < bound_min.y || cell.y > bound_max.y)) {
			continue;
		}

		const auto* current{ ReadTileCell(*layer, *map, cell) };
		const int current_tile{ current ? current->tile_id : -1 };
		if (current_tile != old_tile) {
			continue;
		}

		if (!e.recipe.avoid_exclusion_mask || !IsExcluded(*map, cell)) {
			const int tile_id{ ChooseTile(e, CellToWorld(*map, cell)) };
			if (e.recipe.source_kind == PaintSourceKind::Autotile) {
				PlaceAutotileCell(e, *layer, *map, cell);
			} else if (tile_id >= 0) {
				// Flood fill intentionally replaces the connected source region.
				SetTile(*layer, *map, cell, tile_id, false, e.recipe.tile_origin);
				e.stroke.changed = true;
			}
		}

		stack.push_back({ cell.x + 1, cell.y });
		stack.push_back({ cell.x - 1, cell.y });
		stack.push_back({ cell.x, cell.y + 1 });
		stack.push_back({ cell.x, cell.y - 1 });
	}

}

static void Eyedrop(EditorState& e, F2 world) {
	auto* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer) return;

	if (layer->kind == LayerKind::Tile) {
		if (const auto* map = FindTilemap(e, layer->tile.tilemap_id)) {
			if (const auto anchor = FindVisibleTileAnchorAtWorld(e, *layer, *map, world)) {
				if (const auto* cell = ReadTileCell(*layer, *map, *anchor); cell && cell->tile_id >= 0) {
					e.active_tile_id = cell->tile_id;
					e.recipe.source_kind = PaintSourceKind::Single;
					e.recipe.tile_id = cell->tile_id;
					e.stamp_tiles.clear();
					return;
				}
			}
		}
		if (const auto hit = FindTopmostGeneratorSceneHit(e, world); hit && hit->tile_id >= 0) {
			e.active_tile_id = hit->tile_id;
			e.recipe.source_kind = PaintSourceKind::Single;
			e.recipe.tile_id = hit->tile_id;
			e.stamp_tiles.clear();
		}
		return;
	}

	float best{ std::numeric_limits<float>::max() };
	const Entity* found{};
	for (const auto& entity : e.entities) {
		if (entity.layer_id != layer->id) continue;
		const RectF bounds{ EntityBounds(entity) };
		const float d{ Distance(RectCenter(bounds), world) };
		const bool inside{ world.x >= bounds.min.x && world.x <= bounds.max.x && world.y >= bounds.min.y && world.y <= bounds.max.y };
		if (d < best && inside) { best = d; found = &entity; }
	}
	if (found) {
		for (int i{}; i < static_cast<int>(e.prefabs.size()); ++i) {
			if (e.prefabs[static_cast<std::size_t>(i)].name == found->prefab) {
				e.active_prefab_index = i;
				e.recipe.source_kind = PaintSourceKind::Single;
				e.recipe.prefab_index = i;
				return;
			}
		}
	}
	if (const auto hit = FindTopmostGeneratorSceneHit(e, world); hit && hit->prefab_index >= 0) {
		e.active_prefab_index = hit->prefab_index;
		e.recipe.source_kind = PaintSourceKind::Single;
		e.recipe.prefab_index = hit->prefab_index;
	}
}

static bool PointInEntity(const Entity& entity, F2 p) {
	const RectF bounds{ EntityBounds(entity) };
	return p.x >= bounds.min.x && p.x <= bounds.max.x &&
		p.y >= bounds.min.y && p.y <= bounds.max.y;
}

static void SelectClick(EditorState& e, F2 world, bool add, bool toggle) {
	const auto* active_layer{ FindLayer(e, e.active_layer_id) };
	if (active_layer && active_layer->kind == LayerKind::Noise) return;

	if (const auto hit = FindTopmostGeneratorSceneHit(e, world)) {
		if (toggle && e.selected_generator_id == hit->generator_id) {
			e.selected_generator_id = -1;
		} else {
			SelectGenerator(e, hit->generator_id);
		}
		return;
	}

	if (active_layer && active_layer->kind == LayerKind::Tile) {
		const auto* map{ FindTilemap(e, active_layer->tile.tilemap_id) };
		if (!map || active_layer->locked || !active_layer->selectable) return;
		if (!add && !toggle) e.selected_tile_cells.clear();
		e.selected_entities.clear();
		e.primary_entity_id = -1;
		e.selected_generator_id = -1;
		e.selected_tile_layer_id = active_layer->id;

		const auto anchor{ FindVisibleTileAnchorAtWorld(e, *active_layer, *map, world) };
		if (!anchor) {
			if (!add && !toggle) e.selected_tile_layer_id = -1;
			return;
		}
		if (toggle && e.selected_tile_cells.contains(*anchor)) e.selected_tile_cells.erase(*anchor);
		else e.selected_tile_cells.insert(*anchor);
		if (e.selected_tile_cells.empty()) e.selected_tile_layer_id = -1;
		return;
	}

	int found{ -1 };
	for (auto layer_it = e.layers.rbegin(); layer_it != e.layers.rend() && found < 0; ++layer_it) {
		if (!layer_it->visible || layer_it->locked || !layer_it->selectable || layer_it->kind != LayerKind::Entity) continue;
		for (auto entity_it = e.entities.rbegin(); entity_it != e.entities.rend(); ++entity_it) {
			if (entity_it->layer_id == layer_it->id && PointInEntity(*entity_it, world)) {
				found = entity_it->id;
				break;
			}
		}
	}
	if (!add && !toggle) e.selected_entities.clear();
	e.selected_generator_id = -1;
	e.selected_tile_cells.clear();
	e.selected_tile_layer_id = -1;
	if (found < 0) {
		e.primary_entity_id = -1;
		return;
	}
	if (toggle && e.selected_entities.contains(found)) {
		e.selected_entities.erase(found);
		if (e.primary_entity_id == found) e.primary_entity_id = e.selected_entities.empty() ? -1 : *e.selected_entities.begin();
	} else {
		e.selected_entities.insert(found);
		e.primary_entity_id = found;
	}
}

static void SelectMarquee(EditorState& e, F2 a, F2 b, bool add, bool toggle) {
	const auto* active_layer{ FindLayer(e, e.active_layer_id) };
	if (active_layer && active_layer->kind == LayerKind::Noise) {
		return;
	}

	if (active_layer && active_layer->kind == LayerKind::Tile) {
		const auto* map{ FindTilemap(e, active_layer->tile.tilemap_id) };
		if (!map || active_layer->locked || !active_layer->selectable) {
			return;
		}

		const RectF selection_rect{
			{ std::min(a.x, b.x), std::min(a.y, b.y) },
			{ std::max(a.x, b.x), std::max(a.y, b.y) },
		};

		if (!add && !toggle) {
			e.selected_tile_cells.clear();
		}

		e.selected_entities.clear();
		e.primary_entity_id = -1;
		e.selected_tile_layer_id = active_layer->id;

		ForEachTileAnchor(
			*active_layer,
			*map,
			[&](I2 cell, const TileCell& tile) {
				if (!RectsOverlap(
						selection_rect,
						TileAnchorRect(e, *map, cell, tile.tile_id, tile.offset, tile.origin)
					)) {
					return;
				}

				if (toggle && e.selected_tile_cells.contains(cell)) {
					e.selected_tile_cells.erase(cell);
				} else {
					e.selected_tile_cells.insert(cell);
				}
			}
		);

		if (e.selected_tile_cells.empty()) {
			e.selected_tile_layer_id = -1;
		}
		return;
	}

	const float min_x{ std::min(a.x, b.x) };
	const float max_x{ std::max(a.x, b.x) };
	const float min_y{ std::min(a.y, b.y) };
	const float max_y{ std::max(a.y, b.y) };

	if (!add && !toggle) {
		e.selected_entities.clear();
	}
	e.selected_tile_cells.clear();
	e.selected_tile_layer_id = -1;

	const RectF selection_rect{ { min_x, min_y }, { max_x, max_y } };
	for (const auto& entity : e.entities) {
		const auto* layer{ FindLayer(e, entity.layer_id) };
		if (
			!layer ||
			!layer->visible ||
			layer->locked ||
			!layer->selectable
		) {
			continue;
		}

		if (!RectsOverlap(selection_rect, EntityBounds(entity))) {
			continue;
		}

		if (toggle && e.selected_entities.contains(entity.id)) {
			e.selected_entities.erase(entity.id);
		} else {
			e.selected_entities.insert(entity.id);
			e.primary_entity_id = entity.id;
		}
	}
}

static void SelectBrush(EditorState& e, F2 world, bool remove) {
	const auto* active_layer{ FindLayer(e, e.active_layer_id) };
	if (active_layer && active_layer->kind == LayerKind::Noise) {
		return;
	}
	if (!active_layer || active_layer->locked || !active_layer->selectable) {
		return;
	}

	if (active_layer->kind == LayerKind::Tile) {
		const auto* map{ FindTilemap(e, active_layer->tile.tilemap_id) };
		if (!map) {
			return;
		}

		e.selected_entities.clear();
		e.primary_entity_id = -1;
		e.selected_tile_layer_id = active_layer->id;

		const RasterGrid raster{ ActiveRasterGrid(e) };
		const auto brush_cells{ RasterBrushCells(e, world) };

		ForEachTileAnchor(
			*active_layer,
			*map,
			[&](I2 anchor, const TileCell& tile) {
				bool overlaps{};
				const RectF tile_rect{
					TileAnchorRect(e, *map, anchor, tile.tile_id, tile.offset, tile.origin)
				};

				for (const I2 brush_cell : brush_cells) {
					if (RectsOverlap(
							tile_rect,
							RasterCellRect(raster, brush_cell)
						)) {
						overlaps = true;
						break;
					}
				}

				if (!overlaps) {
					return;
				}

				if (remove) {
					e.selected_tile_cells.erase(anchor);
				} else {
					e.selected_tile_cells.insert(anchor);
				}
			}
		);

		if (e.selected_tile_cells.empty()) {
			e.selected_tile_layer_id = -1;
		}
		return;
	}

	e.selected_tile_cells.clear();
	e.selected_tile_layer_id = -1;

	for (const auto& entity : e.entities) {
		const auto* layer{ FindLayer(e, entity.layer_id) };
		if (
			!layer ||
			!layer->visible ||
			layer->locked ||
			!layer->selectable ||
			!RectIntersectsRasterCells(e, world, EntityBounds(entity))
		) {
			continue;
		}

		if (remove) {
			e.selected_entities.erase(entity.id);
			if (e.primary_entity_id == entity.id) {
				e.primary_entity_id = -1;
			}
		} else {
			e.selected_entities.insert(entity.id);
			e.primary_entity_id = entity.id;
		}
	}
}

static void ReplaceEntitiesInRasterCells(
	EditorState& e,
	SceneLayer& layer,
	const RasterGrid& raster,
	const std::vector<I2>& cells
) {
	if (layer.kind != LayerKind::Entity || e.prefabs.empty() || cells.empty()) return;
	for (auto& entity : e.entities) {
		if (entity.layer_id != layer.id) continue;
		const RectF bounds{ EntityBounds(entity) };
		std::optional<I2> hit;
		for (const I2 cell : cells) {
			if (RectsOverlap(bounds, RasterCellRect(raster, cell))) { hit = cell; break; }
		}
		if (!hit) continue;
		const int to_index{ ChoosePrefabFromRecipe(e, e.recipe, entity.position, *hit) };
		if (to_index < 0 || to_index >= static_cast<int>(e.prefabs.size())) continue;
		const auto& to{ e.prefabs[static_cast<std::size_t>(to_index)] };
		entity.prefab = to.name;
		entity.size = to.size;
		entity.origin = e.recipe.entity_origin;
		e.stroke.changed = true;
	}
}

static void ApplyLine(EditorState& e, F2 a, F2 b) {
	auto* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->locked) {
		return;
	}

	const RasterGrid raster{ ActiveRasterGrid(e) };
	const std::vector<I2> cells{ RasterLineCells(e, a, b) };
	const float line_angle{
		std::atan2(b.y - a.y, b.x - a.x) * 57.2957795f
	};

	if (layer->kind == LayerKind::Tile) {
		auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
		if (!map) {
			return;
		}
		for (const I2 cell : cells) {
			PlaceTile(e, *layer, *map, cell);
		}
		return;
	}

	if (e.brush.operation == BrushOperation::Replace) {
		ReplaceEntitiesInRasterCells(e, *layer, raster, cells);
		return;
	}

	for (const I2 cell : cells) {
		const RectF rect{ RasterCellRect(raster, cell) };
		const F2 origin_fraction{ EntityOriginFraction(e.recipe.entity_origin) };
		const F2 placement{
			rect.min.x + (rect.max.x - rect.min.x) * origin_fraction.x,
			rect.min.y + (rect.max.y - rect.min.y) * origin_fraction.y,
		};
		const std::size_t before_count{ e.entities.size() };
		PlaceEntity(e, *layer, placement);
		if (e.brush.line_align_rotation &&
			e.brush.operation == BrushOperation::Paint) {
			for (std::size_t i{ before_count }; i < e.entities.size(); ++i) {
				e.entities[i].rotation = line_angle;
			}
		}
	}
}

static void ApplyArea(EditorState& e, F2 a, F2 b) {
	auto* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->locked) {
		return;
	}

	const RasterGrid raster{ ActiveRasterGrid(e) };
	const std::vector<I2> cells{ RasterAreaCells(e, a, b) };

	if (layer->kind == LayerKind::Tile) {
		auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
		if (!map) {
			return;
		}

		for (const I2 cell : cells) {
			if (
				e.brush.area_mode == AreaMode::RandomFill &&
				!PassesAreaRandomFill(e, cell)
			) {
				continue;
			}
			PlaceTile(e, *layer, *map, cell);
		}
		return;
	}

	std::vector<I2> filtered_cells;
	filtered_cells.reserve(cells.size());
	for (const I2 cell : cells) {
		if (e.brush.area_mode == AreaMode::RandomFill && !PassesAreaRandomFill(e, cell)) {
			continue;
		}
		filtered_cells.push_back(cell);
	}

	if (e.brush.operation == BrushOperation::Replace) {
		ReplaceEntitiesInRasterCells(e, *layer, raster, filtered_cells);
		return;
	}

	for (const I2 cell : filtered_cells) {
		const RectF rect{ RasterCellRect(raster, cell) };
		const F2 origin_fraction{ EntityOriginFraction(e.recipe.entity_origin) };
		PlaceEntity(
			e,
			*layer,
			{
				rect.min.x + (rect.max.x - rect.min.x) * origin_fraction.x,
				rect.min.y + (rect.max.y - rect.min.y) * origin_fraction.y,
			}
		);
	}
}

static float DistancePointToSegment(F2 p, F2 a, F2 b) {
	const F2 ab{ b - a };
	const float length_sq{ ab.x * ab.x + ab.y * ab.y };
	if (length_sq <= 0.00001f) return Distance(p, a);
	const F2 ap{ p - a };
	const float t{ std::clamp((ap.x * ab.x + ap.y * ab.y) / length_sq, 0.0f, 1.0f) };
	return Distance(p, a + ab * t);
}

static bool GeneratorSuppressed(const PaintGenerator& generator, I2 cell) {
	return std::ranges::any_of(generator.overrides, [&](const GeneratedInstanceOverride& item) {
		return item.cell == cell && item.suppressed;
	});
}

static F2 GeneratorCellCenter(const PaintGenerator& generator, I2 cell) {
	return {
		generator.grid_offset.x + (static_cast<float>(cell.x) + 0.5f) * generator.grid_size.x,
		generator.grid_offset.y + (static_cast<float>(cell.y) + 0.5f) * generator.grid_size.y,
	};
}

static void IncludeGeneratorBrushCell(PaintGenerator& generator, I2 cell) {
	if (!generator.brush_cells.insert(cell).second) return;
	if (!generator.brush_bounds_valid) {
		generator.brush_min_cell = cell;
		generator.brush_max_cell = cell;
		generator.brush_bounds_valid = true;
		return;
	}
	generator.brush_min_cell.x = std::min(generator.brush_min_cell.x, cell.x);
	generator.brush_min_cell.y = std::min(generator.brush_min_cell.y, cell.y);
	generator.brush_max_cell.x = std::max(generator.brush_max_cell.x, cell.x);
	generator.brush_max_cell.y = std::max(generator.brush_max_cell.y, cell.y);
}

static void StampGeneratorBrushCells(PaintGenerator& generator, F2 world) {
	const I2 center{
		static_cast<int>(std::floor((world.x - generator.grid_offset.x) / std::max(1.0f, generator.grid_size.x))),
		static_cast<int>(std::floor((world.y - generator.grid_offset.y) / std::max(1.0f, generator.grid_size.y))),
	};
	const int count_x{ std::max(1, generator.brush_diameter_tiles * std::max(1, generator.source_footprint_cells.x)) };
	const int count_y{ std::max(1, generator.brush_diameter_tiles * std::max(1, generator.source_footprint_cells.y)) };
	const int start_x{ center.x - (count_x - 1) / 2 };
	const int start_y{ center.y - (count_y - 1) / 2 };
	const float cx{ (static_cast<float>(count_x) - 1.0f) * 0.5f };
	const float cy{ (static_cast<float>(count_y) - 1.0f) * 0.5f };
	const float rx{ std::max(0.5f, static_cast<float>(count_x) * 0.5f) };
	const float ry{ std::max(0.5f, static_cast<float>(count_y) * 0.5f) };
	for (int y{}; y < count_y; ++y) {
		for (int x{}; x < count_x; ++x) {
			if (generator.brush_shape == BrushShape::Circle) {
				const float nx{ (static_cast<float>(x) - cx) / rx };
				const float ny{ (static_cast<float>(y) - cy) / ry };
				if (nx * nx + ny * ny > 1.0f) continue;
			}
			IncludeGeneratorBrushCell(generator, { start_x + x, start_y + y });
		}
	}
}

static void AppendGeneratorBrushSample(PaintGenerator& generator, F2 world, bool begin_new_stroke) {
	auto to_cell = [&](F2 p) {
		return I2{
			static_cast<int>(std::floor((p.x - generator.grid_offset.x) / std::max(1.0f, generator.grid_size.x))),
			static_cast<int>(std::floor((p.y - generator.grid_offset.y) / std::max(1.0f, generator.grid_size.y))),
		};
	};
	if (!begin_new_stroke && !generator.stroke_points.empty()) {
		// Multiple interpolation samples can land in the same raster cell. Avoid
		// restamping a potentially very large source-sized brush footprint.
		if (to_cell(generator.stroke_points.back()) == to_cell(world)) return;
	}
	if (begin_new_stroke || generator.stroke_starts.empty()) {
		generator.stroke_starts.push_back(generator.stroke_points.size());
	}
	generator.stroke_points.push_back(world);
	StampGeneratorBrushCells(generator, world);
}

static bool GeneratorAnchorCell(const PaintGenerator& generator, I2 cell) {
	if (generator.recipe.tile_paint_mode != TilePaintMode::Tile) return true;
	const int sx{ std::max(1, generator.source_footprint_cells.x) };
	const int sy{ std::max(1, generator.source_footprint_cells.y) };
	if (sx == 1 && sy == 1) return true;
	// Anchor the source-sized lattice to the generator's first authored cell.
	// The origin must never move when a Brush later expands left/up, otherwise
	// every existing oversized-tile anchor would appear to shift parity.
	const I2 origin{ generator.lattice_origin_cell };
	return FloorMod(cell.x - origin.x, sx) == 0 && FloorMod(cell.y - origin.y, sy) == 0;
}

static bool GeneratorGeometryContains(const PaintGenerator& generator, I2 cell) {
	if (generator.geometry == GeneratorGeometryKind::BrushStroke && !generator.brush_cells.empty()) return generator.brush_cells.contains(cell);
	const F2 p{ GeneratorCellCenter(generator, cell) };
	switch (generator.geometry) {
		case GeneratorGeometryKind::Infinite:
			return true;
		case GeneratorGeometryKind::Rectangle: {
			const F2 mn{ std::min(generator.start.x, generator.end.x), std::min(generator.start.y, generator.end.y) };
			const F2 mx{ std::max(generator.start.x, generator.end.x), std::max(generator.start.y, generator.end.y) };
			if (!(p.x >= mn.x && p.x < mx.x && p.y >= mn.y && p.y < mx.y)) return false;
			if (generator.area_mode == AreaMode::Fill || generator.area_mode == AreaMode::RandomFill) return true;
			const I2 first{
				static_cast<int>(std::floor((mn.x - generator.grid_offset.x) / std::max(1.0f, generator.grid_size.x))),
				static_cast<int>(std::floor((mn.y - generator.grid_offset.y) / std::max(1.0f, generator.grid_size.y))),
			};
			const I2 last{
				static_cast<int>(std::floor((mx.x - generator.grid_offset.x - generator.grid_size.x * 0.0001f) / std::max(1.0f, generator.grid_size.x))),
				static_cast<int>(std::floor((mx.y - generator.grid_offset.y - generator.grid_size.y * 0.0001f) / std::max(1.0f, generator.grid_size.y))),
			};
			const int tx{ std::max(1, generator.area_thickness) * std::max(1, generator.source_footprint_cells.x) };
			const int ty{ std::max(1, generator.area_thickness) * std::max(1, generator.source_footprint_cells.y) };
			const int left{ cell.x - first.x };
			const int right{ last.x - cell.x };
			const int top{ cell.y - first.y };
			const int bottom{ last.y - cell.y };
			const bool edge{ left < tx || right < tx || top < ty || bottom < ty };
			const bool corner{ (left < tx || right < tx) && (top < ty || bottom < ty) };
			return generator.area_mode == AreaMode::Outline ? edge : corner;
		}
		case GeneratorGeometryKind::Line: {
			const int source_cells{ std::max(generator.source_footprint_cells.x, generator.source_footprint_cells.y) };
			const float half_width{ std::max(generator.grid_size.x, generator.grid_size.y) * std::max(1, generator.line_thickness) * std::max(1, source_cells) * 0.5f };
			return DistancePointToSegment(p, generator.start, generator.end) <= half_width;
		}
		case GeneratorGeometryKind::BrushStroke: {
			if (generator.stroke_points.empty()) return Distance(p, generator.start) <= generator.brush_radius;
			for (std::size_t stroke{}; stroke < generator.stroke_starts.size(); ++stroke) {
				const std::size_t first{ generator.stroke_starts[stroke] };
				const std::size_t last{ stroke + 1 < generator.stroke_starts.size() ? generator.stroke_starts[stroke + 1] : generator.stroke_points.size() };
				if (first >= last) continue;
				if (last - first == 1 && Distance(p, generator.stroke_points[first]) <= generator.brush_radius) return true;
				for (std::size_t i{ first + 1 }; i < last; ++i) {
					if (DistancePointToSegment(p, generator.stroke_points[i - 1], generator.stroke_points[i]) <= generator.brush_radius) return true;
				}
			}
			return false;
		}
	}
	return false;
}

static bool GeneratorCoveragePass(const PaintGenerator& generator, I2 cell) {
	if (GeneratorSuppressed(generator, cell) || !GeneratorGeometryContains(generator, cell)) return false;
	if (!GeneratorAnchorCell(generator, cell)) return false;
	const PaintRecipe& recipe{ generator.recipe };
	const F2 world{ GeneratorCellCenter(generator, cell) };
	const float selector{ Hash01(cell, static_cast<std::uint32_t>(recipe.noise.seed) ^ static_cast<std::uint32_t>((generator.id + 17) * 7919)) };
	if (generator.geometry == GeneratorGeometryKind::Rectangle && generator.area_mode == AreaMode::RandomFill && selector > recipe.density) return false;

	if (recipe.source_kind == PaintSourceKind::Noise) {
		const auto* threshold{ RecipeNoiseThreshold(recipe, RecipeNoiseValue(world, recipe)) };
		if (!threshold || !threshold->enabled) return false;
		const bool has_source{ threshold->source_kind == PaintSourceKind::Single
			? (threshold->tile_id >= 0 || threshold->prefab_index >= 0)
			: (threshold->weighted_tile_set_id >= 0 || threshold->weighted_prefab_set_id >= 0) };
		if (!has_source) return false;
	}

	switch (recipe.coverage) {
		case PaintCoverageKind::Solid:
			return true;
		case PaintCoverageKind::RandomDensity:
			return selector <= recipe.density;
		case PaintCoverageKind::RadialFalloff: {
			float normalized{};
			if (generator.geometry == GeneratorGeometryKind::BrushStroke) {
				float closest{ std::numeric_limits<float>::max() };
				for (std::size_t stroke{}; stroke < generator.stroke_starts.size(); ++stroke) {
					const std::size_t first{ generator.stroke_starts[stroke] };
					const std::size_t last{ stroke + 1 < generator.stroke_starts.size() ? generator.stroke_starts[stroke + 1] : generator.stroke_points.size() };
					if (first >= last) continue;
					if (last - first == 1) closest = std::min(closest, Distance(world, generator.stroke_points[first]));
					for (std::size_t i{ first + 1 }; i < last; ++i) closest = std::min(closest, DistancePointToSegment(world, generator.stroke_points[i - 1], generator.stroke_points[i]));
				}
				if (!std::isfinite(closest)) closest = Distance(world, generator.start);
				normalized = closest / std::max(1.0f, generator.brush_radius);
			} else {
				const F2 center{ (generator.start + generator.end) * 0.5f };
				const float radius{ std::max(1.0f, Distance(generator.start, generator.end) * 0.5f) };
				normalized = Distance(world, center) / radius;
			}
			const float inner{ std::clamp(recipe.radial_inner, 0.0f, 1.0f) };
			const float outer{ std::max(inner + 0.001f, std::clamp(recipe.radial_outer, 0.0f, 1.0f)) };
			const float t{ std::clamp((normalized - inner) / (outer - inner), 0.0f, 1.0f) };
			return selector <= (1.0f - t) * recipe.density;
		}
	}
	return true;
}

static std::pair<I2, I2> GeneratorCellBounds(const PaintGenerator& generator) {
	if (generator.geometry == GeneratorGeometryKind::Infinite) return { { -32768, -32768 }, { 32767, 32767 } };
	if (generator.geometry == GeneratorGeometryKind::BrushStroke && generator.brush_bounds_valid) return { generator.brush_min_cell, generator.brush_max_cell };
	F2 mn{ std::min(generator.start.x, generator.end.x), std::min(generator.start.y, generator.end.y) };
	F2 mx{ std::max(generator.start.x, generator.end.x), std::max(generator.start.y, generator.end.y) };
	if (generator.geometry == GeneratorGeometryKind::Line) {
		const int source_cells{ std::max(generator.source_footprint_cells.x, generator.source_footprint_cells.y) };
		const float margin{ std::max(generator.grid_size.x, generator.grid_size.y) * std::max(1, generator.line_thickness) * std::max(1, source_cells) * 0.5f };
		mn.x -= margin; mn.y -= margin; mx.x += margin; mx.y += margin;
	}
	const auto to_cell = [&](F2 p) {
		return I2{
			static_cast<int>(std::floor((p.x - generator.grid_offset.x) / std::max(1.0f, generator.grid_size.x))),
			static_cast<int>(std::floor((p.y - generator.grid_offset.y) / std::max(1.0f, generator.grid_size.y))),
		};
	};
	const float eps_x{ std::max(0.0001f, generator.grid_size.x * 0.0001f) };
	const float eps_y{ std::max(0.0001f, generator.grid_size.y * 0.0001f) };
	return { to_cell(mn), to_cell({ mx.x - eps_x, mx.y - eps_y }) };
}

static PaintGenerator MakeGeneratorFromCurrentStroke(
	EditorState& e,
	GeneratorGeometryKind geometry,
	F2 end,
	int id
) {
	PaintGenerator generator;
	generator.id = id;
	generator.layer_id = e.active_layer_id;
	generator.name = id >= 0
		? std::string{ ToolName(e.tool) } + " Generator " + std::to_string(id)
		: std::string{ ToolName(e.tool) } + " (Live)";
	generator.geometry = geometry;
	generator.recipe = e.recipe;
	const RasterGrid grid{ ActiveRasterGrid(e) };
	generator.grid_size = grid.size;
	generator.grid_offset = grid.offset;
	generator.source_footprint_cells = RecipePaintFootprintCells(e);
	generator.lattice_origin_cell = WorldToRasterCell(grid, e.stroke.start_world);
	generator.start = e.stroke.start_world;
	generator.end = end;
	generator.brush_diameter_tiles = std::max(1, e.brush.brush_diameter_tiles);
	generator.brush_radius = EffectiveBrushRadius(e);
	generator.brush_shape = e.brush.shape;
	generator.line_thickness = e.brush.line_thickness;
	generator.line_spacing_cells = e.brush.line_spacing_cells;
	generator.area_thickness = e.brush.area_thickness;
	generator.area_mode = e.brush.area_mode;

	if (geometry == GeneratorGeometryKind::Rectangle) {
		const I2 a{ WorldToRasterCell(grid, generator.start) };
		const I2 b{ QuantizeAreaEndCell(e, a, WorldToRasterCell(grid, generator.end)) };
		const I2 mn{ std::min(a.x, b.x), std::min(a.y, b.y) };
		const I2 mx{ std::max(a.x, b.x), std::max(a.y, b.y) };
		generator.start = RasterCellToWorld(grid, mn);
		generator.end = RasterCellToWorld(grid, { mx.x + 1, mx.y + 1 });
	}

	if (geometry == GeneratorGeometryKind::BrushStroke) {
		const std::vector<F2> points{ e.stroke.points.empty() ? std::vector<F2>{ e.stroke.start_world } : e.stroke.points };
		bool first{ true };
		for (const F2 point : points) {
			AppendGeneratorBrushSample(generator, point, first);
			first = false;
		}
	} else {
		generator.stroke_points = e.stroke.points;
	}
	return generator;
}

static I2 GeneratorRecipeFootprintCells(const EditorState& e, const PaintGenerator& generator) {
	const SceneLayer* layer{ FindLayer(e, generator.layer_id) };
	if (!layer) return { 1, 1 };
	const F2 size{ RecipeMaxSourceSize(e, *layer, generator.recipe) };
	return {
		std::max(1, static_cast<int>(std::ceil(size.x / std::max(1.0f, generator.grid_size.x)))),
		std::max(1, static_cast<int>(std::ceil(size.y / std::max(1.0f, generator.grid_size.y)))),
	};
}

static void RebuildGeneratorBrushCache(PaintGenerator& generator) {
	if (generator.geometry != GeneratorGeometryKind::BrushStroke) return;
	generator.brush_cells.clear();
	generator.brush_bounds_valid = false;
	for (const F2 point : generator.stroke_points) StampGeneratorBrushCells(generator, point);
	const float width{ generator.grid_size.x * static_cast<float>(std::max(1, generator.brush_diameter_tiles) * std::max(1, generator.source_footprint_cells.x)) };
	const float height{ generator.grid_size.y * static_cast<float>(std::max(1, generator.brush_diameter_tiles) * std::max(1, generator.source_footprint_cells.y)) };
	generator.brush_radius = std::max(width, height) * 0.5f;
}

static void SyncGeneratorRecipeFromPalette(EditorState& e, PaintGenerator& generator) {
	generator.recipe = e.recipe;
	const I2 footprint{ GeneratorRecipeFootprintCells(e, generator) };
	if (footprint != generator.source_footprint_cells) {
		generator.source_footprint_cells = footprint;
		RebuildGeneratorBrushCache(generator);
	}
}

static bool PendingBrushGeneratorActive(const EditorState& e) {
	return e.pending_generator && e.pending_generator->geometry == GeneratorGeometryKind::BrushStroke;
}

static int AddGeneratorFromCurrentStroke(EditorState& e, GeneratorGeometryKind geometry, F2 end) {
	const int id{ e.next_generator_id++ };
	e.generators.push_back(MakeGeneratorFromCurrentStroke(e, geometry, end, id));
	e.selected_generator_id = id;
	e.primary_entity_id = -1;
	e.selected_entities.clear();
	e.selected_tile_cells.clear();
	e.stroke.changed = true;
	return id;
}

static void BeginPendingGeneratorFromCurrentStroke(
	EditorState& e,
	GeneratorGeometryKind geometry,
	F2 end
) {
	e.pending_generator = MakeGeneratorFromCurrentStroke(e, geometry, end, -1);
	e.pending_generator_before = e.stroke.before ? e.stroke.before : std::optional<SceneSnapshot>{ CaptureScene(e) };
	e.selected_generator_id = -1;
	e.primary_entity_id = -1;
	e.selected_entities.clear();
	e.selected_tile_cells.clear();
	e.stroke = {};
}

static void CreateInfiniteGenerator(EditorState& e) {
	const auto* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->kind == LayerKind::Noise || layer->locked) return;
	const SceneSnapshot before{ CaptureScene(e) };
	PaintGenerator generator;
	generator.id = e.next_generator_id++;
	generator.layer_id = layer->id;
	generator.name = "Infinite Generator " + std::to_string(generator.id);
	generator.geometry = GeneratorGeometryKind::Infinite;
	generator.recipe = e.recipe;
	const RasterGrid grid{ ActiveRasterGrid(e) };
	generator.grid_size = grid.size;
	generator.grid_offset = grid.offset;
	generator.source_footprint_cells = RecipePaintFootprintCells(e);
	generator.lattice_origin_cell = {};
	e.selected_generator_id = generator.id;
	e.generators.push_back(std::move(generator));
	PushHistory(e, "Create Infinite Generator", before);
}

static bool AddGeneratorSuppression(PaintGenerator& generator, I2 cell) {
	if (GeneratorSuppressed(generator, cell)) return false;
	generator.overrides.push_back({ .cell = cell, .suppressed = true });
	return true;
}

static bool BakePaintGeneratorIntoScene(EditorState& e, const PaintGenerator& generator) {
	auto* layer{ FindLayer(e, generator.layer_id) };
	if (!layer || layer->locked || generator.geometry == GeneratorGeometryKind::Infinite) return false;

	const auto [first, last]{ GeneratorCellBounds(generator) };
	std::size_t visited{};
	constexpr std::size_t kMaxBakeCells{ 200000 };
	const PaintRecipe saved_recipe{ e.recipe };
	const StrokeState saved_stroke{ e.stroke };
	e.recipe = generator.recipe;
	e.stroke = {};
	e.stroke.active = true;
	bool changed{};

	for (int y{ first.y }; y <= last.y && visited < kMaxBakeCells; ++y) {
		for (int x{ first.x }; x <= last.x && visited < kMaxBakeCells; ++x, ++visited) {
			const I2 cell{ x, y };
			if (!GeneratorCoveragePass(generator, cell)) continue;
			const F2 world{ GeneratorCellCenter(generator, cell) };
			if (layer->kind == LayerKind::Tile) {
				auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
				if (!map) continue;
				const I2 target{ WorldToCell(*map, world) };
				if (generator.recipe.avoid_exclusion_mask && IsExcluded(*map, target)) continue;
				if (generator.recipe.source_kind == PaintSourceKind::Autotile) {
					const bool before_changed{ e.stroke.changed };
					PlaceAutotileCell(e, *layer, *map, target);
					changed = changed || e.stroke.changed != before_changed || e.stroke.changed;
				} else {
					const int tile_id{ ChooseTileFromRecipe(e, generator.recipe, world, cell, true) };
					if (tile_id < 0) continue;
					SetTile(*layer, *map, target, tile_id, false, RecipeSourceOrigin(generator.recipe, world, true));
					changed = true;
				}
			} else if (layer->kind == LayerKind::Entity) {
				const int prefab_index{ ChoosePrefabFromRecipe(e, generator.recipe, world, cell, true) };
				if (prefab_index < 0 || prefab_index >= static_cast<int>(e.prefabs.size())) continue;
				const auto& prefab{ e.prefabs[static_cast<std::size_t>(prefab_index)] };
				Entity entity;
				entity.id = e.next_entity_id++;
				entity.layer_id = layer->id;
				entity.prefab = prefab.name;
				entity.position = world;
				entity.size = prefab.size;
				entity.origin = RecipeSourceOrigin(generator.recipe, world, false);
				if (generator.recipe.random_rotation) {
					const float t{ Hash01(cell, static_cast<std::uint32_t>(generator.recipe.noise.seed) ^ 0x41A7u) };
					entity.rotation = generator.recipe.rotation_min + (generator.recipe.rotation_max - generator.recipe.rotation_min) * t;
				}
				if (generator.recipe.random_scale) {
					const float t{ Hash01(cell, static_cast<std::uint32_t>(generator.recipe.noise.seed) ^ 0xAC31u) };
					entity.scale = generator.recipe.scale_min + (generator.recipe.scale_max - generator.recipe.scale_min) * t;
				}
				e.entities.push_back(std::move(entity));
				changed = true;
			}
		}
	}

	e.recipe = saved_recipe;
	e.stroke = saved_stroke;
	return changed;
}

static void CommitPendingGenerator(EditorState& e) {
	if (!e.pending_generator) return;
	PaintGenerator generator{ *e.pending_generator };
	if (generator.layer_id != e.active_layer_id && !FindLayer(e, generator.layer_id)) {
		e.pending_generator.reset();
		e.pending_generator_before.reset();
		return;
	}
	const SceneSnapshot before{ e.pending_generator_before.value_or(CaptureScene(e)) };
	bool changed{};
	if (generator.recipe.commit_mode == PaintCommitMode::KeepGenerator) {
		generator.id = e.next_generator_id++;
		generator.name = "Generator " + std::to_string(generator.id);
		e.selected_generator_id = generator.id;
		e.generators.push_back(std::move(generator));
		changed = true;
	} else {
		changed = BakePaintGeneratorIntoScene(e, generator);
		e.selected_generator_id = -1;
	}
	e.pending_generator.reset();
	e.pending_generator_before.reset();
	if (changed) PushHistory(e, "Commit Live Paint", before);
}

static void CancelPendingGenerator(EditorState& e) {
	e.pending_generator.reset();
	e.pending_generator_before.reset();
}

static void BakeGenerator(EditorState& e, int generator_id) {
	const PaintGenerator* source{ FindGenerator(e, generator_id) };
	if (!source || source->geometry == GeneratorGeometryKind::Infinite) return;
	const SceneSnapshot before{ CaptureScene(e) };
	const PaintGenerator generator{ *source };
	if (!BakePaintGeneratorIntoScene(e, generator)) return;
	e.generators.erase(
		std::remove_if(
			e.generators.begin(),
			e.generators.end(),
			[&](const PaintGenerator& item) { return item.id == generator_id; }
		),
		e.generators.end()
	);
	if (e.selected_generator_id == generator_id) e.selected_generator_id = -1;
	PushHistory(e, "Bake Generator", before);
}

static void DeleteGenerator(EditorState& e, int generator_id) {
	if (!FindGenerator(e, generator_id)) return;
	const SceneSnapshot before{ CaptureScene(e) };
	e.generators.erase(
		std::remove_if(
			e.generators.begin(),
			e.generators.end(),
			[&](const PaintGenerator& generator) { return generator.id == generator_id; }
		),
		e.generators.end()
	);
	if (e.selected_generator_id == generator_id) e.selected_generator_id = -1;
	PushHistory(e, "Delete Generator", before);
}

static std::optional<GeneratorHit> FindTopmostGeneratorAtWorld(EditorState& e, const SceneLayer& layer, F2 world) {
	if (!layer.visible || layer.kind == LayerKind::Noise) return std::nullopt;

	// Manually-authored content is rendered after generators, so it wins the hit test.
	if (layer.kind == LayerKind::Tile) {
		if (const auto* map = FindTilemap(e, layer.tile.tilemap_id)) {
			if (FindVisibleTileAnchorAtWorld(e, layer, *map, world)) return std::nullopt;
		}
	} else {
		for (auto it = e.entities.rbegin(); it != e.entities.rend(); ++it) {
			if (it->layer_id == layer.id && PointInEntity(*it, world)) return std::nullopt;
		}
	}

	for (auto generator_it = e.generators.rbegin(); generator_it != e.generators.rend(); ++generator_it) {
		const PaintGenerator& generator{ *generator_it };
		if (generator.layer_id != layer.id || !generator.visible) continue;
		if (generator.recipe.source_kind == PaintSourceKind::Noise && !generator.recipe.show_generated_preview) continue;
		const I2 center{
			static_cast<int>(std::floor((world.x - generator.grid_offset.x) / std::max(1.0f, generator.grid_size.x))),
			static_cast<int>(std::floor((world.y - generator.grid_offset.y) / std::max(1.0f, generator.grid_size.y))),
		};
		const int rx{ std::max(1, generator.source_footprint_cells.x) + 1 };
		const int ry{ std::max(1, generator.source_footprint_cells.y) + 1 };
		for (int y{ center.y + ry }; y >= center.y - ry; --y) {
			for (int x{ center.x + rx }; x >= center.x - rx; --x) {
				const I2 cell{ x, y };
				if (!GeneratorCoveragePass(generator, cell)) continue;
				const F2 sample{ GeneratorCellCenter(generator, cell) };
				if (layer.kind == LayerKind::Tile) {
					const auto* source_map{ FindTilemap(e, layer.tile.tilemap_id) };
					if (!source_map) continue;
					Tilemap map{ *source_map };
					map.cell_size = generator.grid_size;
					map.origin = generator.grid_offset;
					int tile_id{ -1 };
					if (generator.recipe.source_kind == PaintSourceKind::Autotile) {
						const auto* rules{ FindAutotileRuleSet(e, generator.recipe.autotile_ruleset_id) };
						if (!rules || rules->tile_ids.empty()) continue;
						if (rules->format == AutotileFormat::DualGrid16) {
							int mask{};
							if (GeneratorCoveragePass(generator, { x, y })) mask |= 1;
							if (GeneratorCoveragePass(generator, { x + 1, y })) mask |= 2;
							if (GeneratorCoveragePass(generator, { x, y + 1 })) mask |= 4;
							if (GeneratorCoveragePass(generator, { x + 1, y + 1 })) mask |= 8;
							if (mask >= static_cast<int>(rules->tile_ids.size())) continue;
							tile_id = rules->tile_ids[static_cast<std::size_t>(mask)];
							map.origin = { generator.grid_offset.x + generator.grid_size.x * 0.5f, generator.grid_offset.y + generator.grid_size.y * 0.5f };
						} else {
							const int index{ GeneratorAutotileIndex(generator, *rules, cell) };
							if (index < 0 || index >= static_cast<int>(rules->tile_ids.size())) continue;
							tile_id = rules->tile_ids[static_cast<std::size_t>(index)];
						}
					} else {
						tile_id = ChooseTileFromRecipe(e, generator.recipe, sample, cell, true);
					}
					if (tile_id < 0) continue;
					const EntityOrigin origin{ RecipeSourceOrigin(generator.recipe, sample, true) };
					const RectF rect{ TileAnchorRect(e, map, cell, tile_id, {}, origin) };
					if (world.x >= rect.min.x && world.x <= rect.max.x && world.y >= rect.min.y && world.y <= rect.max.y) {
						return GeneratorHit{ generator.id, cell, tile_id, -1 };
					}
				} else {
					const int prefab_index{ ChoosePrefabFromRecipe(e, generator.recipe, sample, cell, true) };
					if (prefab_index < 0 || prefab_index >= static_cast<int>(e.prefabs.size())) continue;
					const auto& prefab{ e.prefabs[static_cast<std::size_t>(prefab_index)] };
					const F2 f{ EntityOriginFraction(RecipeSourceOrigin(generator.recipe, sample, false)) };
					const RectF rect{
						{ sample.x - prefab.size.x * f.x, sample.y - prefab.size.y * f.y },
						{ sample.x + prefab.size.x * (1.0f - f.x), sample.y + prefab.size.y * (1.0f - f.y) },
					};
					if (world.x >= rect.min.x && world.x <= rect.max.x && world.y >= rect.min.y && world.y <= rect.max.y) {
						return GeneratorHit{ generator.id, cell, -1, prefab_index };
					}
				}
			}
		}
	}
	return std::nullopt;
}

static std::optional<GeneratorHit> FindTopmostGeneratorSceneHit(EditorState& e, F2 world) {
	for (auto layer_it = e.layers.rbegin(); layer_it != e.layers.rend(); ++layer_it) {
		const SceneLayer& layer{ *layer_it };
		if (!layer.visible || !layer.selectable || layer.locked || layer.kind == LayerKind::Noise) continue;

		// Any manually authored object on a higher layer blocks generators below it.
		if (layer.kind == LayerKind::Tile) {
			if (const auto* map = FindTilemap(e, layer.tile.tilemap_id)) {
				if (FindVisibleTileAnchorAtWorld(e, layer, *map, world)) return std::nullopt;
			}
		} else {
			for (auto entity_it = e.entities.rbegin(); entity_it != e.entities.rend(); ++entity_it) {
				if (entity_it->layer_id == layer.id && PointInEntity(*entity_it, world)) return std::nullopt;
			}
		}

		if (auto hit = FindTopmostGeneratorAtWorld(e, layer, world)) return hit;
	}
	return std::nullopt;
}

// Noise-grid helpers are defined with the procedural rendering code below,
// but viewport/layer setup needs their interface earlier.
static std::pair<F2, F2> OrderedNoiseBounds(const SceneLayer& layer);
static void SnapNoiseBoundsOutwardToGrid(
	EditorState& e,
	SceneLayer& layer,
	F2 minimum,
	F2 maximum
);

static F2 VisibleWorldMin(const EditorState& e) {
	return ScreenToWorld(e, e.canvas_screen_min);
}

static F2 VisibleWorldMax(const EditorState& e) {
	return ScreenToWorld(e, e.canvas_screen_max);
}

static void SetNoiseBoundsToCurrentViewport(EditorState& e, SceneLayer& layer) {
	F2 minimum{ -256.0f, -256.0f };
	F2 maximum{ 256.0f, 256.0f };
	if (e.canvas_screen_max.x - e.canvas_screen_min.x > 8.0f &&
		e.canvas_screen_max.y - e.canvas_screen_min.y > 8.0f) {
		const F2 a{ VisibleWorldMin(e) };
		const F2 b{ VisibleWorldMax(e) };
		minimum = { std::min(a.x, b.x), std::min(a.y, b.y) };
		maximum = { std::max(a.x, b.x), std::max(a.y, b.y) };
	}
	layer.noise.bounded = true;
	SnapNoiseBoundsOutwardToGrid(e, layer, minimum, maximum);
}

static void FrameNoiseBoundsInViewport(EditorState& e, const SceneLayer& layer) {
	if (!layer.noise.bounded) {
		return;
	}

	const auto [minimum, maximum]{ OrderedNoiseBounds(layer) };
	const float world_width{ std::max(1.0f, maximum.x - minimum.x) };
	const float world_height{ std::max(1.0f, maximum.y - minimum.y) };
	const float canvas_width{
		std::max(1.0f, e.canvas_screen_max.x - e.canvas_screen_min.x)
	};
	const float canvas_height{
		std::max(1.0f, e.canvas_screen_max.y - e.canvas_screen_min.y)
	};
	const float padding_px{ 36.0f };
	const float usable_width{ std::max(1.0f, canvas_width - padding_px * 2.0f) };
	const float usable_height{ std::max(1.0f, canvas_height - padding_px * 2.0f) };

	e.view_zoom = std::clamp(
		std::min(usable_width / world_width, usable_height / world_height),
		0.15f,
		5.0f
	);

	const F2 center{
		(minimum.x + maximum.x) * 0.5f,
		(minimum.y + maximum.y) * 0.5f
	};
	e.view_pan = {
		canvas_width * 0.5f - center.x * e.view_zoom,
		canvas_height * 0.5f - center.y * e.view_zoom,
	};
}

static void UpdateStreamingForLayer(EditorState& e, SceneLayer& layer) {
	if (layer.kind != LayerKind::Tile) {
		return;
	}
	auto* map{ FindTilemap(e, layer.tile.tilemap_id) };
	if (!map) {
		return;
	}
	if (!map->streaming.enabled) {
		while (!layer.tile.backing_chunks.empty()) {
			auto node{ layer.tile.backing_chunks.extract(layer.tile.backing_chunks.begin()) };
			layer.tile.loaded_chunks.insert(std::move(node));
		}
		return;
	}

	const F2 w0{ VisibleWorldMin(e) };
	const F2 w1{ VisibleWorldMax(e) };
	const I2 cell0{ WorldToCell(*map, w0) };
	const I2 cell1{ WorldToCell(*map, w1) };
	const I2 chunk0{ CellToChunk(*map, { std::min(cell0.x, cell1.x), std::min(cell0.y, cell1.y) }) };
	const I2 chunk1{ CellToChunk(*map, { std::max(cell0.x, cell1.x), std::max(cell0.y, cell1.y) }) };

	layer.tile.preload_chunks.clear();
	layer.tile.keep_alive_chunks.clear();
	for (int y{ chunk0.y - map->streaming.keep_alive_margin }; y <= chunk1.y + map->streaming.keep_alive_margin; ++y) {
		for (int x{ chunk0.x - map->streaming.keep_alive_margin }; x <= chunk1.x + map->streaming.keep_alive_margin; ++x) {
			layer.tile.keep_alive_chunks.insert({ x, y });
		}
	}
	for (int y{ chunk0.y - map->streaming.preload_margin }; y <= chunk1.y + map->streaming.preload_margin; ++y) {
		for (int x{ chunk0.x - map->streaming.preload_margin }; x <= chunk1.x + map->streaming.preload_margin; ++x) {
			const I2 c{ x, y };
			layer.tile.preload_chunks.insert(c);
			if (!layer.tile.loaded_chunks.contains(c) && layer.tile.backing_chunks.contains(c) &&
				layer.tile.loaded_chunks.size() < static_cast<std::size_t>(std::max(1, map->streaming.max_loaded_chunks))) {
				auto node{ layer.tile.backing_chunks.extract(c) };
				layer.tile.loaded_chunks.insert(std::move(node));
			}
		}
	}

	for (auto it = layer.tile.loaded_chunks.begin(); it != layer.tile.loaded_chunks.end();) {
		if (layer.tile.keep_alive_chunks.contains(it->first)) {
			++it;
			continue;
		}
		auto node{ layer.tile.loaded_chunks.extract(it++) };
		layer.tile.backing_chunks.insert(std::move(node));
	}
}

static int CountMergedCollisionRects(const SceneLayer& layer, const Tilemap& map) {
	std::unordered_set<I2, I2Hash> solid;
	auto collect = [&](const auto& chunks) {
		for (const auto& [coord, chunk] : chunks) {
			for (int y{}; y < map.chunk_size.y; ++y) {
				for (int x{}; x < map.chunk_size.x; ++x) {
					const auto& cell{ chunk.cells[static_cast<std::size_t>(y * map.chunk_size.x + x)] };
					if (cell.tile_id >= 0) {
						solid.insert({ coord.x * map.chunk_size.x + x, coord.y * map.chunk_size.y + y });
					}
				}
			}
		}
	};
	collect(layer.tile.loaded_chunks);
	collect(layer.tile.backing_chunks);
	int rects{};
	while (!solid.empty()) {
		const I2 start{ *solid.begin() };
		int width{ 1 };
		while (solid.contains({ start.x + width, start.y })) {
			++width;
		}
		for (int x{}; x < width; ++x) {
			solid.erase({ start.x + x, start.y });
		}
		++rects;
	}
	return rects;
}

static int AddGeneratedTile(
	EditorState& e,
	TilePalette& palette,
	std::string name,
	int width,
	int height,
	std::array<std::uint8_t, 4> base,
	std::array<std::uint8_t, 4> accent,
	int pattern
) {
	width = std::max(1, width);
	height = std::max(1, height);
	std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
	for (int y{}; y < height; ++y) {
		for (int x{}; x < width; ++x) {
			bool use_accent{};
			switch (pattern) {
				case 0: use_accent = ((x / 4) + (y / 4)) % 2 == 0; break; // checker
				case 1: use_accent = ((x * 13 + y * 7) % 19) < 3; break; // grass/speckles
				case 2: use_accent = ((y + (x / 4) * 2) % 8) < 2; break; // water waves
				case 3: use_accent = ((x / 5) + (y / 5) * 3) % 4 == 0; break; // stone blocks
				case 4: use_accent = ((x * 5 + y * 11) % 31) < 2; break; // sand dots
				default: use_accent = (y % 8 == 0) || (x % 8 == (y / 8 % 2) * 4); break; // brick
			}
			const auto& c{ use_accent ? accent : base };
			const std::size_t i{ static_cast<std::size_t>((y * width + x) * 4) };
			pixels[i + 0] = c[0];
			pixels[i + 1] = c[1];
			pixels[i + 2] = c[2];
			pixels[i + 3] = c[3];
		}
	}

	GLuint texture{};
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

	const int texture_index{ static_cast<int>(e.textures.size()) };
	e.textures.push_back({ texture, width, height, "generated://" + name });

	TileDefinition tile;
	tile.id = e.next_tile_id++;
	tile.name = std::move(name);
	tile.texture_index = texture_index;
	tile.pixel_w = width;
	tile.pixel_h = height;
	tile.uv0 = { 0.0f, 0.0f };
	tile.uv1 = { 1.0f, 1.0f };
	e.tiles.push_back(tile);
	palette.entries.push_back({ tile.id, 1.0f });
	return tile.id;
}

static bool LoadTexture(EditorState& e, const std::string& path, int& texture_index) {
	int w{}, h{}, channels{};
	stbi_uc* pixels{ stbi_load(path.c_str(), &w, &h, &channels, 4) };
	if (!pixels) {
		return false;
	}
	GLuint texture{};
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	stbi_image_free(pixels);

	texture_index = static_cast<int>(e.textures.size());
	e.textures.push_back({ texture, w, h, path });
	return true;
}

static std::string ReadTextFile(const std::filesystem::path& path) {
	std::ifstream file{ path, std::ios::binary };
	if (!file) {
		return {};
	}
	std::ostringstream stream;
	stream << file.rdbuf();
	return stream.str();
}

static std::optional<I2> FilenameTileDimensions(const std::filesystem::path& path) {
	static const std::regex dimensions{ R"((?:_|-)([0-9]+)x([0-9]+)$)", std::regex::icase };
	std::smatch match;
	const std::string stem{ path.stem().string() };
	if (!std::regex_search(stem, match, dimensions) || match.size() < 3) {
		return std::nullopt;
	}
	try {
		return I2{ std::max(1, std::stoi(match[1].str())), std::max(1, std::stoi(match[2].str())) };
	} catch (...) {
		return std::nullopt;
	}
}

static std::string SourcePaletteName(const std::filesystem::path& path) {
	static const std::regex dimensions{ R"((?:_|-)[0-9]+x[0-9]+$)", std::regex::icase };
	return std::regex_replace(path.stem().string(), dimensions, "");
}

static bool IsImageFile(const std::filesystem::path& path) {
	std::string ext{ path.extension().string() };
	std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".gif" || ext == ".psd" || ext == ".hdr" || ext == ".pic" || ext == ".pnm";
}

static int ResolveImportPalette(EditorState& e, const ImportSettings& import, const std::filesystem::path& source) {
	if (e.palettes.empty()) {
		e.palettes.push_back({ e.next_palette_id++, "Default", {} });
	}
	if (!import.create_palette_from_source) {
		return std::clamp(import.target_palette, 0, static_cast<int>(e.palettes.size()) - 1);
	}

	std::string name{ SourcePaletteName(source) };
	if (name.empty()) {
		name = "Tiles";
	}
	for (int i{}; i < static_cast<int>(e.palettes.size()); ++i) {
		if (e.palettes[static_cast<std::size_t>(i)].name == name) {
			return i;
		}
	}
	e.palettes.push_back({ e.next_palette_id++, name, {} });
	return static_cast<int>(e.palettes.size()) - 1;
}

static int ImportImageTiles(EditorState& e, const ImportSettings& import, const std::filesystem::path& image_path, int palette_index) {
	int texture_index{};
	if (!LoadTexture(e, image_path.string(), texture_index)) {
		return 0;
	}

	const auto& texture{ e.textures[static_cast<std::size_t>(texture_index)] };
	const std::string stem{ image_path.stem().string() };
	std::optional<I2> inferred;
	if (import.use_filename_dimensions) {
		inferred = FilenameTileDimensions(image_path);
	}

	bool slice{};
	I2 tile_size{ texture.width, texture.height };
	if (import.mode == ImportMode::Individual) {
		slice = false;
	} else if (import.mode == ImportMode::Tileset) {
		slice = true;
		tile_size = inferred.value_or(I2{ std::max(1, import.tile_width), std::max(1, import.tile_height) });
	} else if (inferred.has_value()) {
		slice = true;
		tile_size = *inferred;
	} else {
		// Auto mode without a filename size postfix treats an image as one tile.
		// This makes dragging individual tile textures effortless and unambiguous.
		slice = false;
	}

	palette_index = std::clamp(palette_index, 0, static_cast<int>(e.palettes.size()) - 1);
	auto& palette{ e.palettes[static_cast<std::size_t>(palette_index)] };
	const int tile_w{ slice ? std::max(1, tile_size.x) : texture.width };
	const int tile_h{ slice ? std::max(1, tile_size.y) : texture.height };
	int index{};
	int imported_count{};
	for (int y{ slice ? std::max(0, import.margin_y) : 0 }; y + tile_h <= texture.height; y += tile_h + (slice ? std::max(0, import.spacing_y) : texture.height)) {
		for (int x{ slice ? std::max(0, import.margin_x) : 0 }; x + tile_w <= texture.width; x += tile_w + (slice ? std::max(0, import.spacing_x) : texture.width)) {
			TileDefinition tile;
			tile.id = e.next_tile_id++;
			tile.name = slice ? stem + "_" + std::to_string(index++) : stem;
			tile.texture_index = texture_index;
			tile.pixel_x = x;
			tile.pixel_y = y;
			tile.pixel_w = tile_w;
			tile.pixel_h = tile_h;
			tile.uv0 = {
				static_cast<float>(x) / static_cast<float>(texture.width),
				static_cast<float>(y) / static_cast<float>(texture.height),
			};
			tile.uv1 = {
				static_cast<float>(x + tile_w) / static_cast<float>(texture.width),
				static_cast<float>(y + tile_h) / static_cast<float>(texture.height),
			};
			e.tiles.push_back(tile);
			palette.entries.push_back({ tile.id, 1.0f });
			++imported_count;
			if (e.active_tile_id < 0) {
				e.active_tile_id = tile.id;
			}
			if (!slice) {
				return imported_count;
			}
		}
	}
	return imported_count;
}

struct TiledTilesetInfo {
	int tile_width{};
	int tile_height{};
	int margin{};
	int spacing{};
	std::vector<std::string> images;
};

static int RegexInt(const std::string& text, const std::regex& pattern, int fallback = 0) {
	std::smatch match;
	if (!std::regex_search(text, match, pattern) || match.size() < 2) {
		return fallback;
	}
	try {
		return std::stoi(match[1].str());
	} catch (...) {
		return fallback;
	}
}

static TiledTilesetInfo ParseTiledTileset(const std::filesystem::path& path) {
	TiledTilesetInfo info;
	const std::string text{ ReadTextFile(path) };
	if (text.empty()) {
		return info;
	}

	std::string ext{ path.extension().string() };
	std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (ext == ".tsx") {
		info.tile_width = RegexInt(text, std::regex{ R"rx(tilewidth\s*=\s*"([0-9]+)")rx", std::regex::icase });
		info.tile_height = RegexInt(text, std::regex{ R"rx(tileheight\s*=\s*"([0-9]+)")rx", std::regex::icase });
		info.margin = RegexInt(text, std::regex{ R"rx(margin\s*=\s*"([0-9]+)")rx", std::regex::icase });
		info.spacing = RegexInt(text, std::regex{ R"rx(spacing\s*=\s*"([0-9]+)")rx", std::regex::icase });
		const std::regex image{ R"rx(<image[^>]*\bsource\s*=\s*"([^"]+)")rx", std::regex::icase };
		for (std::sregex_iterator it{ text.begin(), text.end(), image }, end; it != end; ++it) {
			info.images.push_back((*it)[1].str());
		}
	} else {
		info.tile_width = RegexInt(text, std::regex{ R"rx("tilewidth"\s*:\s*([0-9]+))rx", std::regex::icase });
		info.tile_height = RegexInt(text, std::regex{ R"rx("tileheight"\s*:\s*([0-9]+))rx", std::regex::icase });
		info.margin = RegexInt(text, std::regex{ R"rx("margin"\s*:\s*([0-9]+))rx", std::regex::icase });
		info.spacing = RegexInt(text, std::regex{ R"rx("spacing"\s*:\s*([0-9]+))rx", std::regex::icase });
		const std::regex image{ R"rx("image"\s*:\s*"([^"]+)")rx", std::regex::icase };
		for (std::sregex_iterator it{ text.begin(), text.end(), image }, end; it != end; ++it) {
			std::string value{ (*it)[1].str() };
			for (std::size_t pos{}; (pos = value.find("\\/", pos)) != std::string::npos;) {
				value.replace(pos, 2, "/");
			}
			info.images.push_back(std::move(value));
		}
	}
	return info;
}

static bool ImportTiles(EditorState& e, const ImportSettings& import) {
	if (import.path.empty()) {
		return false;
	}
	const std::filesystem::path source{ import.path };
	if (!std::filesystem::exists(source)) {
		return false;
	}

	std::string ext{ source.extension().string() };
	std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const int palette_index{ ResolveImportPalette(e, import, source) };
	if (import.create_palette_from_source) {
		e.active_palette_index = palette_index;
	}

	if (ext == ".tsx" || ext == ".tsj" || ext == ".json") {
		const TiledTilesetInfo info{ ParseTiledTileset(source) };
		if (info.images.empty()) {
			return false;
		}
		int imported{};
		for (const std::string& image_name : info.images) {
			std::filesystem::path image_path{ image_name };
			if (image_path.is_relative()) {
				image_path = source.parent_path() / image_path;
			}
			ImportSettings child{ import };
			child.create_palette_from_source = false;
			child.target_palette = palette_index;
			if (info.images.size() > 1) {
				// Tiled image-collection tilesets: each image is one native-size tile.
				child.mode = ImportMode::Individual;
			} else {
				child.mode = ImportMode::Tileset;
				child.use_filename_dimensions = false;
				child.tile_width = info.tile_width > 0 ? info.tile_width : import.tile_width;
				child.tile_height = info.tile_height > 0 ? info.tile_height : import.tile_height;
				child.margin_x = info.margin;
				child.margin_y = info.margin;
				child.spacing_x = info.spacing;
				child.spacing_y = info.spacing;
			}
			imported += ImportImageTiles(e, child, image_path.lexically_normal(), palette_index);
		}
		return imported > 0;
	}

	if (!IsImageFile(source)) {
		return false;
	}
	return ImportImageTiles(e, import, source, palette_index) > 0;
}

static void GLFWDropCallback(GLFWwindow*, int count, const char** paths) {
	if (!g_editor || count <= 0 || !paths) {
		return;
	}

	int imported{};
	std::string first_failed;
	for (int i{}; i < count; ++i) {
		if (!paths[i]) {
			continue;
		}
		ImportSettings drop{ g_editor->importer };
		drop.path = paths[i];
		drop.mode = ImportMode::Auto;
		drop.target_palette = g_editor->active_palette_index;
		drop.create_palette_from_source = false;
		if (ImportTiles(*g_editor, drop)) {
			++imported;
		} else if (first_failed.empty()) {
			first_failed = paths[i];
		}
	}

	if (imported > 0) {
		g_editor->import_status = "Imported " + std::to_string(imported) + " dropped source(s) into the active palette.";
	}
	if (!first_failed.empty()) {
		g_editor->importer.path = first_failed;
		g_editor->importer.target_palette = g_editor->active_palette_index;
		g_editor->importer.open_popup = true;
	}
}
static void AddDefaultScene(EditorState& e) {
	// Prefabs are assumed to come from a separate prefab editor. This demo only
	// browses them, grouped exactly as a real project browser would.
	e.prefabs = {
		{ "Tree", "Foliage", { 32.0f, 48.0f } },
		{ "Bush", "Foliage", { 32.0f, 24.0f } },
		{ "Rock", "Foliage", { 24.0f, 24.0f } },
		{ "Chair", "Furniture", { 24.0f, 24.0f } },
		{ "Table", "Furniture", { 48.0f, 32.0f } },
		{ "Goblin", "Monsters", { 28.0f, 36.0f } },
		{ "Slime", "Monsters", { 30.0f, 20.0f } },
		{ "Crate", "", { 32.0f, 32.0f } },
		{ "Lamp", "", { 16.0f, 40.0f } },
	};

	e.palettes.push_back({ e.next_palette_id++, "Basic Tiles", {} });
	e.palettes.push_back({ e.next_palette_id++, "Terrain", {} });
	auto& basic{ e.palettes.front() };
	const int grass{ AddGeneratedTile(e, basic, "Grass", 16, 16, { 72, 132, 70, 255 }, { 99, 160, 80, 255 }, 1) };
	const int dirt{ AddGeneratedTile(e, basic, "Dirt", 16, 16, { 126, 88, 58, 255 }, { 151, 108, 72, 255 }, 0) };
	const int water{ AddGeneratedTile(e, basic, "Water", 16, 16, { 55, 104, 171, 255 }, { 78, 139, 205, 255 }, 2) };
	const int stone{ AddGeneratedTile(e, basic, "Stone", 16, 16, { 105, 110, 116, 255 }, { 135, 141, 146, 255 }, 3) };
	const int sand{ AddGeneratedTile(e, basic, "Sand", 16, 16, { 202, 177, 112, 255 }, { 228, 204, 139, 255 }, 4) };
	const int large_stone{ AddGeneratedTile(e, basic, "Large Stone 24x24", 24, 24, { 115, 118, 124, 255 }, { 151, 154, 160, 255 }, 3) };
	const int brick{ AddGeneratedTile(e, basic, "Brick 32x32", 32, 32, { 145, 68, 55, 255 }, { 91, 49, 45, 255 }, 5) };
	(void)large_stone;
	(void)brick;

	// A second palette demonstrates that weighted brushes/noise thresholds can
	// reference tiles from more than one palette.
	auto& terrain{ e.palettes[1] };
	const int path_light{ AddGeneratedTile(e, terrain, "Path Light", 16, 16, { 146, 111, 72, 255 }, { 171, 134, 88, 255 }, 4) };
	const int path_dark{ AddGeneratedTile(e, terrain, "Path Dark", 16, 16, { 104, 76, 54, 255 }, { 130, 94, 63, 255 }, 0) };
	const int pebble{ AddGeneratedTile(e, terrain, "Path Pebble", 16, 16, { 128, 102, 75, 255 }, { 151, 143, 129, 255 }, 3) };

	e.active_tile_id = grass;
	e.active_palette_index = 0;

	WeightedTileSet dirt_path;
	dirt_path.id = e.next_tile_weighted_set_id++;
	dirt_path.name = "Dirt Path Variety";
	dirt_path.entries = {
		{ dirt, 5.0f }, { path_light, 2.5f }, { path_dark, 1.5f }, { pebble, 0.8f }, { stone, 0.25f },
	};
	e.active_tile_weighted_set_id = dirt_path.id;
	e.weighted_tile_sets.push_back(std::move(dirt_path));

	WeightedTileSet shoreline;
	shoreline.id = e.next_tile_weighted_set_id++;
	shoreline.name = "Sandy Ground Mix";
	shoreline.entries = { { sand, 6.0f }, { dirt, 2.0f }, { pebble, 0.6f } };
	e.weighted_tile_sets.push_back(std::move(shoreline));

	WeightedPrefabSet foliage;
	foliage.id = e.next_prefab_weighted_set_id++;
	foliage.name = "Foliage Mix";
	foliage.entries = { { 0, 5.0f }, { 1, 3.0f }, { 2, 1.5f } };
	e.active_prefab_weighted_set_id = foliage.id;
	e.weighted_prefab_sets.push_back(std::move(foliage));

	WeightedPrefabSet monsters;
	monsters.id = e.next_prefab_weighted_set_id++;
	monsters.name = "Small Monsters";
	monsters.entries = { { 5, 2.0f }, { 6, 4.0f } };
	e.weighted_prefab_sets.push_back(std::move(monsters));

	Tilemap map;
	map.id = e.next_tilemap_id++;
	map.name = "World";
	map.cell_size = { 16.0f, 16.0f };
	e.tilemaps.push_back(map);

	SceneLayer background;
	background.id = e.next_layer_id++;
	background.name = "Background";
	background.kind = LayerKind::Tile;
	background.tile.tilemap_id = map.id;
	e.layers.push_back(background);

	// Procedural content now lives inside ordinary Tile/Entity layers as paint
	// recipes and persistent generators. There is intentionally no default or
	// user-created Noise layer. The legacy Noise structures remain only so this
	// standalone demo can still load/inspect older serialized experiments.

	AutotileRuleSet terrain_rules;
	terrain_rules.id = e.next_autotile_ruleset_id++;
	terrain_rules.name = "Terrain Rules";
	terrain_rules.format = AutotileFormat::DualGrid16;
	terrain_rules.tile_ids.resize(16, grass);
	const std::array<int, 7> starter_tiles{ grass, dirt, water, stone, sand, path_light, path_dark };
	for (std::size_t i{}; i < terrain_rules.tile_ids.size(); ++i) {
		terrain_rules.tile_ids[i] = starter_tiles[i % starter_tiles.size()];
	}
	e.recipe.autotile_ruleset_id = terrain_rules.id;
	e.autotile_rulesets.push_back(std::move(terrain_rules));

	// Seed a useful noise recipe with an explicit empty band. Noise is coverage
	// data in the Paint Recipe, not a layer type.
	e.recipe.noise.type = NoiseType::Perlin;
	e.recipe.noise.name = "Paint Noise";
	e.recipe.noise.frequency = 0.015f;
	e.recipe.noise.octaves = 4;
	e.recipe.noise.thresholds = {
		NoiseThresholdRegion{
			.minimum = 0.0f, .maximum = 0.45f,
			.source_kind = PaintSourceKind::Single, .tile_id = -1, .prefab_index = -1,
			.enabled = true
		},
		NoiseThresholdRegion{
			.minimum = 0.45f, .maximum = 1.0f,
			.source_kind = PaintSourceKind::Single, .tile_id = grass, .prefab_index = 0,
			.enabled = true
		},
	};

	SceneLayer props;
	props.id = e.next_layer_id++;
	props.name = "Decorations";
	props.kind = LayerKind::Entity;
	e.layers.push_back(props);

	SceneLayer gameplay;
	gameplay.id = e.next_layer_id++;
	gameplay.name = "Gameplay";
	gameplay.kind = LayerKind::Entity;
	e.layers.push_back(gameplay);

	SceneLayer foreground;
	foreground.id = e.next_layer_id++;
	foreground.name = "Foreground";
	foreground.kind = LayerKind::Tile;
	foreground.tile.tilemap_id = map.id;
	e.layers.push_back(foreground);

	e.active_layer_id = background.id;
	e.recipe.tile_id = grass;
	e.recipe.prefab_index = 0;
	e.recipe.weighted_tile_set_id = e.active_tile_weighted_set_id;
	e.recipe.weighted_prefab_set_id = e.active_prefab_weighted_set_id;
	e.recipe.tile_paint_mode = TilePaintMode::Tile;
	e.recipe.tile_origin = EntityOrigin::TopLeft;
	e.recipe.entity_origin = EntityOrigin::TopLeft;
}

static void HelpMarker(const char* text) {
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip()) {
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
		ImGui::TextUnformatted(text);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

static void DrawToolIcon(ImDrawList* dl, Tool tool, ImVec2 min, ImU32 color) {
	// Intentionally simple 16x16-ish iconography: familiar paint-program metaphors,
	// drawn directly with ImDrawList so the demo has no icon/font dependency.
	const float x{ std::floor(min.x) };
	const float y{ std::floor(min.y) };
	auto P = [&](float px, float py) { return ImVec2{ x + px, y + py }; };
	switch (tool) {
		case Tool::None:
			break;
		case Tool::Select:
			dl->AddTriangleFilled(P(2, 1), P(2, 14), P(7, 10), color);
			dl->AddLine(P(6, 9), P(11, 14), color, 2.0f);
			break;
		case Tool::Move:
			dl->AddLine(P(8, 1), P(8, 15), color, 2.0f);
			dl->AddLine(P(1, 8), P(15, 8), color, 2.0f);
			dl->AddTriangleFilled(P(8, 0), P(5, 4), P(11, 4), color);
			dl->AddTriangleFilled(P(8, 16), P(5, 12), P(11, 12), color);
			dl->AddTriangleFilled(P(0, 8), P(4, 5), P(4, 11), color);
			dl->AddTriangleFilled(P(16, 8), P(12, 5), P(12, 11), color);
			break;
		case Tool::Pencil:
			dl->AddLine(P(3, 13), P(12, 4), color, 3.0f);
			dl->AddTriangleFilled(P(2, 14), P(4, 10), P(6, 12), color);
			dl->AddLine(P(10, 3), P(13, 6), color, 2.0f);
			break;
		case Tool::Brush:
			dl->AddLine(P(11, 2), P(6, 9), color, 3.0f);
			dl->AddQuadFilled(P(3, 8), P(7, 9), P(6, 14), P(2, 14), color);
			dl->AddLine(P(2, 14), P(7, 14), color, 1.0f);
			break;
		case Tool::Line:
			dl->AddCircleFilled(P(3, 13), 1.5f, color, 8);
			dl->AddLine(P(4, 12), P(12, 4), color, 2.0f);
			dl->AddCircleFilled(P(13, 3), 1.5f, color, 8);
			break;
		case Tool::Rectangle:
			dl->AddRect(P(2, 3), P(14, 13), color, 0.0f, 0, 2.0f);
			dl->AddRectFilled(P(1, 2), P(4, 5), color);
			dl->AddRectFilled(P(12, 11), P(15, 14), color);
			break;
		case Tool::Fill:
			dl->AddQuad(P(4, 4), P(10, 7), P(7, 13), P(1, 10), color, 2.0f);
			dl->AddLine(P(5, 2), P(11, 8), color, 2.0f);
			dl->AddCircleFilled(P(12, 12), 2.0f, color, 8);
			break;
		case Tool::Erase:
			dl->AddQuadFilled(P(4, 4), P(12, 7), P(8, 13), P(1, 10), color);
			break;
		case Tool::Eyedropper:
			dl->AddLine(P(4, 12), P(11, 5), color, 3.0f);
			dl->AddCircle(P(12, 4), 2.5f, color, 8, 2.0f);
			dl->AddLine(P(2, 14), P(5, 11), color, 2.0f);
			break;
	}
}

static const ToolBinding* FindToolBinding(const EditorState& e, Tool tool) {
	for (const auto& binding : e.tool_bindings) if (binding.tool == tool) return &binding;
	return nullptr;
}

static bool ToolButton(EditorState& e, Tool tool, const char* id) {
	const ImVec2 size{ 26.0f, 26.0f };
	const ImVec2 p0{ ImGui::GetCursorScreenPos() };
	ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
	ImGui::InvisibleButton(id, size);
	ImGui::PopItemFlag();
	const bool hovered{ ImGui::IsItemHovered() };
	const bool pressed{ ImGui::IsItemClicked() };
	const bool active{ e.tool == tool };
	const ImVec4 background{
		active ? ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive) :
		hovered ? ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered) :
		ImGui::GetStyleColorVec4(ImGuiCol_Button)
	};
	ImDrawList* dl{ ImGui::GetWindowDrawList() };
	dl->AddRectFilled(p0, { p0.x + size.x, p0.y + size.y }, ImGui::GetColorU32(background), 3.0f);
	if (active) {
		dl->AddRect(p0, { p0.x + size.x, p0.y + size.y }, ImGui::GetColorU32(ImGuiCol_Text), 3.0f, 0, 1.0f);
	}
	DrawToolIcon(dl, tool, { p0.x + 5.0f, p0.y + 5.0f }, ImGui::GetColorU32(ImGuiCol_Text));
	if (pressed) {
		e.tool = tool;
	}
	if (hovered) {
		if (const auto* binding = FindToolBinding(e, tool)) {
			ImGui::SetTooltip("%s (%s)\n%s", ToolName(tool), binding->key_name.c_str(), ToolTooltip(tool));
		} else {
			ImGui::SetTooltip("%s", ToolTooltip(tool));
		}
	}
	return pressed;
}
static void DrawRuntimeToolbar(EditorState& e) {
	ImGui::Checkbox("Editor Camera", &e.runtime.use_editor_camera);
	ItemTooltip("Use the editor viewport camera instead of the scene/runtime camera.");

	ImGui::SameLine();
	if (!e.runtime.playing) {
		if (ImGui::Button("Play")) {
			e.runtime.playing = true;
			e.runtime.paused = false;
			e.runtime.use_editor_camera = false;
		}
		ItemTooltip("Start runtime simulation.");
	} else {
		if (ImGui::Button("Stop")) {
			e.runtime.playing = false;
			e.runtime.paused = false;
		}
		ItemTooltip("Stop runtime simulation.");
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!e.runtime.playing);
	if (ImGui::Button(e.runtime.paused ? "Resume" : "Pause")) {
		e.runtime.paused = !e.runtime.paused;
	}
	ItemTooltip(e.runtime.paused
		? "Resume runtime simulation."
		: "Pause runtime simulation.");

	ImGui::SameLine();
	if (ImGui::Button("Step")) {
		++e.runtime.stepped_frames;
		e.runtime.time += (1.0f / 60.0f) * e.runtime.speed;
	}
	ItemTooltip("Advance the paused/running simulation by one fixed frame.");
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	const char* speeds[]{ "0.25x", "0.5x", "1.0x", "2.0x", "4.0x" };
	const float values[]{ 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
	int current{ 2 };
	for (int i{}; i < 5; ++i) {
		if (std::abs(e.runtime.speed - values[i]) < 0.001f) {
			current = i;
		}
	}
	if (ImGui::Combo("##speed", &current, speeds, 5)) {
		e.runtime.speed = values[current];
	}
	ItemTooltip("Runtime time scale.");
}

static bool LayerIconButton(const char* id, bool& value, bool eye_icon);

static void DrawViewportToolbar(EditorState& e) {
	SceneLayer* toolbar_layer{ FindLayer(e, e.active_layer_id) };
	const bool noise_layer_active{ toolbar_layer && toolbar_layer->kind == LayerKind::Noise };
	const bool tile_layer_active{ toolbar_layer && toolbar_layer->kind == LayerKind::Tile };

	if (!noise_layer_active) {
		ToolButton(e, Tool::Select, "##tool_select"); ImGui::SameLine();
		ToolButton(e, Tool::Move, "##tool_move"); ImGui::SameLine();
		ToolButton(e, Tool::Pencil, "##tool_pencil"); ImGui::SameLine();
		ToolButton(e, Tool::Brush, "##tool_brush"); ImGui::SameLine();
		ToolButton(e, Tool::Line, "##tool_line"); ImGui::SameLine();
		ToolButton(e, Tool::Rectangle, "##tool_area"); ImGui::SameLine();
		ToolButton(e, Tool::Fill, "##tool_fill"); ImGui::SameLine();
		ToolButton(e, Tool::Erase, "##tool_erase"); ImGui::SameLine();
		ToolButton(e, Tool::Eyedropper, "##tool_pick");
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
	}

	ImGui::Checkbox("Grid", &e.grid.visible);
	ItemTooltip(noise_layer_active
		? "Show or hide the active noise layer's raster grid. Grid lines are drawn over scene content."
		: tile_layer_active
			? "Show or hide the active tilemap grid. Grid lines are drawn over scene content."
			: "Show or hide the editor placement grid. Grid lines are drawn over scene content.");

	// Tile layers inherit their tilemap grid and therefore expose no redundant
	// snap/grid-size controls here. Noise layers own an editable raster grid.
	if (!tile_layer_active) {
		if (!noise_layer_active) {
			ImGui::SameLine();
			ImGui::Checkbox("Snap", &e.grid.snap);
			ItemTooltip("Snap entity placement to the editor grid.");
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(noise_layer_active && toolbar_layer && toolbar_layer->locked);
		ImGui::SetNextItemWidth(110.0f);

		F2* size_ptr{ noise_layer_active ? &toolbar_layer->noise.grid_size : &e.grid.size };
		bool* aspect_locked_ptr{ noise_layer_active ? &toolbar_layer->noise.grid_aspect_locked : &e.grid.aspect_locked };
		float* aspect_ptr{ noise_layer_active ? &toolbar_layer->noise.grid_locked_aspect : &e.grid.locked_aspect };
		const F2 old_size{ *size_ptr };
		float grid[2]{ old_size.x, old_size.y };
		if (ImGui::DragFloat2("##grid_size", grid, 0.25f, 1.0f, 2048.0f, "%.0f")) {
			grid[0] = std::max(1.0f, grid[0]);
			grid[1] = std::max(1.0f, grid[1]);
			if (*aspect_locked_ptr) {
				const float ratio{ std::max(0.0001f, *aspect_ptr) };
				const float dx{ std::abs(grid[0] - old_size.x) / std::max(1.0f, old_size.x) };
				const float dy{ std::abs(grid[1] - old_size.y) / std::max(1.0f, old_size.y) };
				if (dx >= dy) grid[1] = std::max(1.0f, grid[0] / ratio);
				else grid[0] = std::max(1.0f, grid[1] * ratio);
			}
			if (noise_layer_active && toolbar_layer) {
				const auto [old_min, old_max]{ OrderedNoiseBounds(*toolbar_layer) };
				toolbar_layer->noise.grid_size = { grid[0], grid[1] };
				if (toolbar_layer->noise.bounded) SnapNoiseBoundsOutwardToGrid(e, *toolbar_layer, old_min, old_max);
			} else {
				e.grid.size = { grid[0], grid[1] };
			}
		}
		ItemTooltip(noise_layer_active
			? "Noise raster cell width and height. Generated threshold results evaluate once per cell. Locked noise layers cannot change this."
			: "Editor grid cell width and height.");

		ImGui::SameLine();
		const bool was_locked{ *aspect_locked_ptr };
		LayerIconButton("##grid_aspect_lock", *aspect_locked_ptr, false);
		if (!was_locked && *aspect_locked_ptr) {
			*aspect_ptr = size_ptr->x / std::max(1.0f, size_ptr->y);
		}
		ItemTooltip(*aspect_locked_ptr
			? "Grid aspect ratio is locked. Editing either dimension preserves the current ratio."
			: "Grid aspect ratio is unlocked. Width and height can be edited independently.");

		ImGui::SameLine();
		if (ImGui::Button("Grid...")) ImGui::OpenPopup("Grid Settings");
		ItemTooltip(noise_layer_active
			? "Open the noise grid origin and major-line settings."
			: "Open advanced grid offset and major-line settings.");
		if (ImGui::BeginPopup("Grid Settings")) {
			F2* offset_ptr{ noise_layer_active ? &toolbar_layer->noise.grid_offset : &e.grid.offset };
			float offset[2]{ offset_ptr->x, offset_ptr->y };
			if (ImGui::DragFloat2("Offset", offset, 1.0f)) {
				*offset_ptr = { offset[0], offset[1] };
				if (noise_layer_active && toolbar_layer && toolbar_layer->noise.bounded) {
					const auto [bmin, bmax]{ OrderedNoiseBounds(*toolbar_layer) };
					SnapNoiseBoundsOutwardToGrid(e, *toolbar_layer, bmin, bmax);
				}
			}
			ItemTooltip(noise_layer_active ? "World-space origin of this noise layer's raster grid." : "World-space origin of the editor grid.");
			ImGui::DragInt("Major Line Every", &e.grid.major_every, 1.0f, 1, 64);
			ItemTooltip("Draw a major grid line every N cells.");

			ImGui::SeparatorText("Minor Lines");
			ImGui::ColorEdit4(
				"Color##minor_grid",
				&e.grid.minor_color.x,
				ImGuiColorEditFlags_AlphaBar
			);
			ItemTooltip("Color and opacity of ordinary minor grid lines.");
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(
				"Thickness##minor_grid",
				&e.grid.minor_thickness,
				0.05f,
				0.25f,
				8.0f,
				"%.2f"
			);
			e.grid.minor_thickness = std::max(0.25f, e.grid.minor_thickness);
			ItemTooltip("Screen-space thickness of minor grid lines.");

			ImGui::SeparatorText("Major Lines");
			ImGui::ColorEdit4(
				"Color##major_grid",
				&e.grid.major_color.x,
				ImGuiColorEditFlags_AlphaBar
			);
			ItemTooltip("Color and opacity of major grid lines.");
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat(
				"Thickness##major_grid",
				&e.grid.major_thickness,
				0.05f,
				0.25f,
				8.0f,
				"%.2f"
			);
			e.grid.major_thickness = std::max(0.25f, e.grid.major_thickness);
			ItemTooltip("Screen-space thickness of major grid lines.");
			ImGui::EndPopup();
		}
		ImGui::EndDisabled();
	}

	// Runtime/view controls remain right aligned regardless of how many editing
	// controls are visible on the left.
	const float runtime_width{ 430.0f };
	const float right{ ImGui::GetWindowContentRegionMax().x };
	const float runtime_x{ right - runtime_width };
	if (ImGui::GetCursorPosX() < runtime_x) ImGui::SameLine(runtime_x);
	else ImGui::SameLine();
	DrawRuntimeToolbar(e);
}

static void ToggleComboChoice(
	const char* label,
	bool& value,
	const char* tooltip,
	bool enabled = true
) {
	ImGui::BeginDisabled(!enabled);
	if (ImGui::Selectable(
			label,
			value,
			ImGuiSelectableFlags_DontClosePopups
		)) {
		value = !value;
	}
	ItemTooltip(tooltip);
	ImGui::EndDisabled();
}


static void DrawBrushSettings(EditorState& e) {
	const SceneLayer* active_layer{ FindLayer(e, e.active_layer_id) };
	if (!active_layer || active_layer->kind == LayerKind::Noise) return;
	const bool tile_layer{ active_layer->kind == LayerKind::Tile };

	bool row_has_item{};
	auto next_item = [&]() {
		if (row_has_item) ImGui::SameLine();
		row_has_item = true;
	};
	auto new_row = [&]() { row_has_item = false; };

	auto draw_operation = [&]() {
		if (e.tool == Tool::Erase) return;
		next_item();
		const char* tile_ops[]{ "Paint", "Replace", "Exclusion Mask" };
		const char* entity_ops[]{ "Paint", "Replace" };
		int operation{ static_cast<int>(e.brush.operation) };
		if (!tile_layer && operation == static_cast<int>(BrushOperation::ExclusionMask)) operation = 0;
		ImGui::SetNextItemWidth(118.0f);
		if (ImGui::Combo("Operation##paint", &operation, tile_layer ? tile_ops : entity_ops, tile_layer ? 3 : 2)) {
			e.brush.operation = static_cast<BrushOperation>(operation);
		}
		ItemTooltip("Paint adds recipe output. Replace changes existing content touched by the tool to the current recipe source. Exclusion Mask is tile-only.");
	};

	auto draw_diameter = [&]() {
		const bool selection{ e.tool == Tool::Select };
		int& diameter{ selection ? e.brush.selection_diameter_cells : e.brush.brush_diameter_tiles };
		next_item();
		ImGui::TextUnformatted(
			selection
				? "Diameter (cells)"
				: (tile_layer ? "Diameter (tiles)" : "Diameter (entities)")
		);
		ImGui::SameLine(0.0f, 3.0f);
		if (ImGui::SmallButton("-##brush_size")) AdjustBrushDiameter(e, -1);
		ImGui::SameLine(0.0f, 3.0f);
		ImGui::SetNextItemWidth(62.0f);
		if (ImGui::DragInt("##brush_diameter", &diameter, 0.15f, 1, 128)) diameter = std::clamp(diameter, 1, 128);
		ImGui::SameLine(0.0f, 3.0f);
		if (ImGui::SmallButton("+##brush_size")) AdjustBrushDiameter(e, 1);
		ItemTooltip(selection
			? "Selection Brush diameter in raster/grid cells."
			: "Brush diameter in source tile/entity sizes. A diameter of 5 means five current source widths/heights, even when the source spans multiple grid cells.");
	};

	auto draw_shape = [&]() {
		next_item();
		const char* shapes[]{ "Circle", "Square" };
		int shape{ static_cast<int>(e.brush.shape) };
		ImGui::SetNextItemWidth(96.0f);
		if (ImGui::Combo("Shape##brush", &shape, shapes, 2)) e.brush.shape = static_cast<BrushShape>(shape);
	};

	if (e.tool == Tool::Select) {
		next_item();
		const char* modes[]{ "Click + Marquee", "Selection Brush" };
		int mode{ static_cast<int>(e.brush.select_mode) };
		ImGui::SetNextItemWidth(152.0f);
		if (ImGui::Combo("Mode##select", &mode, modes, 2)) e.brush.select_mode = static_cast<SelectMode>(mode);
		ItemTooltip("Paint.NET-style selection: click/marquee replaces selection, Shift adds, Ctrl toggles/removes, right click clears. Selection Brush always selects raster cells.");
		if (e.brush.select_mode == SelectMode::Brush) {
			draw_diameter();
			draw_shape();
		}
		next_item();
		ImGui::BeginDisabled(!HasSelection(e));
		if (ImGui::SmallButton("Deselect")) DeselectAll(e);
		ImGui::EndDisabled();
		return;
	}

	if (e.tool == Tool::Move) {
		next_item();
		const char* modes[]{ "Grid", "Free" };
		int mode{ static_cast<int>(e.move.snap_mode) };
		ImGui::SetNextItemWidth(104.0f);
		if (ImGui::Combo("Move##mode", &mode, modes, 2)) e.move.snap_mode = static_cast<MoveSnapMode>(mode);
		next_item();
		ImGui::BeginDisabled(!HasSelection(e));
		if (ImGui::Button("Snap to Grid")) SnapSelectionToGrid(e);
		ImGui::EndDisabled();
		ItemTooltip("Snap every selected entity/tile to its nearest grid coordinate using that object's origin/anchor.");
		return;
	}

	if (e.tool == Tool::Fill || e.tool == Tool::Eyedropper) return;

	if (e.tool == Tool::Erase) {
		if (tile_layer) {
			next_item();
			const char* erase_modes[]{ "Erase Tiles", "Erase Mask" };
			int choice{ e.brush.operation == BrushOperation::ExclusionMask ? 1 : 0 };
			ImGui::SetNextItemWidth(116.0f);
			if (ImGui::Combo("Mode##erase", &choice, erase_modes, 2)) e.brush.operation = choice ? BrushOperation::ExclusionMask : BrushOperation::Paint;
		}
		draw_diameter();
		draw_shape();
		return;
	}

	draw_operation();
	if (e.tool == Tool::Brush) {
		new_row();
		draw_diameter();
		draw_shape();
		if (PendingBrushGeneratorActive(e)) {
			next_item();
			if (ImGui::SmallButton("✓##finish_brush_generator")) {
				e.recipe.commit_mode = PaintCommitMode::KeepGenerator;
				e.pending_generator->recipe.commit_mode = PaintCommitMode::KeepGenerator;
				CommitPendingGenerator(e);
			}
			ItemTooltip("Finish/lock in the current multi-stroke brush generator (Enter). The next brush stroke starts a new generator instead of combining with this one.");
			next_item();
			if (ImGui::SmallButton("×##cancel_brush_generator")) CancelPendingGenerator(e);
			ItemTooltip("Cancel the current multi-stroke brush generator (Escape).");
		}
	}
	if (e.tool == Tool::Line) {
		next_item();
		ImGui::SetNextItemWidth(76.0f);
		ImGui::DragInt(tile_layer ? "Thickness (tiles)##line" : "Thickness (entities)##line", &e.brush.line_thickness, 0.15f, 1, 32);
		e.brush.line_thickness = std::max(1, e.brush.line_thickness);
		next_item();
		ImGui::SetNextItemWidth(76.0f);
		ImGui::DragInt("Spacing##line", &e.brush.line_spacing_cells, 0.1f, 1, 32);
		e.brush.line_spacing_cells = std::max(1, e.brush.line_spacing_cells);
		next_item();
		ImGui::Checkbox("Align Rotation", &e.brush.line_align_rotation);
	}
	if (e.tool == Tool::Rectangle) {
		next_item();
		const char* areas[]{ "Fill", "Outline", "Corners", "Random Fill" };
		int area{ static_cast<int>(e.brush.area_mode) };
		ImGui::SetNextItemWidth(116.0f);
		if (ImGui::Combo("Mode##rectangle", &area, areas, 4)) e.brush.area_mode = static_cast<AreaMode>(area);
		if (e.brush.area_mode == AreaMode::Outline || e.brush.area_mode == AreaMode::Corners) {
			next_item();
			ImGui::SetNextItemWidth(82.0f);
			ImGui::DragInt(tile_layer ? "Thickness (tiles)##rectangle" : "Thickness (entities)##rectangle", &e.brush.area_thickness, 0.15f, 1, 32);
			e.brush.area_thickness = std::max(1, e.brush.area_thickness);
		}
		if (e.brush.area_mode == AreaMode::RandomFill) {
			next_item();
			ImGui::SetNextItemWidth(82.0f);
			ImGui::SliderFloat("Density##rectangle", &e.recipe.density, 0.01f, 1.0f, "%.2f");
		}
	}
}

static float FractalNoiseValue(F2 world, const NoiseField& field) {
	float frequency{ std::max(0.00001f, field.frequency) };
	float amplitude{ 1.0f };
	float total{};
	float weight{};
	const int octaves{ std::clamp(field.octaves, 1, 12) };
	for (int octave{}; octave < octaves; ++octave) {
		const float n{ BaseNoise2(
			field.type,
			(world.x + field.offset.x) * frequency,
			(world.y + field.offset.y) * frequency,
			static_cast<std::uint32_t>(field.seed + octave * 1013)
		) };
		total += n * amplitude;
		weight += amplitude;
		frequency *= std::max(1.0f, field.lacunarity);
		amplitude *= std::clamp(field.persistence, 0.0f, 1.0f);
	}
	return weight > 0.0f ? std::clamp(total / weight, 0.0f, 1.0f) : 0.0f;
}

static const NoiseThresholdRegion* FindNoiseThreshold(
	const NoiseField& field,
	float value
) {
	for (const auto& threshold : field.thresholds) {
		if (!threshold.enabled) {
			continue;
		}
		const float lo{ std::min(threshold.minimum, threshold.maximum) };
		const float hi{ std::max(threshold.minimum, threshold.maximum) };
		if (value >= lo && (value < hi || (hi >= 0.99999f && value <= 1.0f))) {
			return &threshold;
		}
	}
	return nullptr;
}

static RasterGrid NoiseRasterGrid(const EditorState&, const SceneLayer& layer) {
	return {
		.size = {
			std::max(1.0f, layer.noise.grid_size.x),
			std::max(1.0f, layer.noise.grid_size.y),
		},
		.offset = layer.noise.grid_offset,
	};
}

static float SnapToGridLine(float value, float offset, float step) {
	step = std::max(1.0f, step);
	return std::round((value - offset) / step) * step + offset;
}

static std::pair<F2, F2> OrderedNoiseBounds(const SceneLayer& layer) {
	return {
		{
			std::min(layer.noise.bounds_min.x, layer.noise.bounds_max.x),
			std::min(layer.noise.bounds_min.y, layer.noise.bounds_max.y),
		},
		{
			std::max(layer.noise.bounds_min.x, layer.noise.bounds_max.x),
			std::max(layer.noise.bounds_min.y, layer.noise.bounds_max.y),
		},
	};
}

static void SnapNoiseBoundsOutwardToGrid(EditorState& e, SceneLayer& layer, F2 minimum, F2 maximum) {
	const RasterGrid grid{ NoiseRasterGrid(e, layer) };
	const F2 ordered_min{ std::min(minimum.x, maximum.x), std::min(minimum.y, maximum.y) };
	const F2 ordered_max{ std::max(minimum.x, maximum.x), std::max(minimum.y, maximum.y) };
	minimum = ordered_min;
	maximum = ordered_max;
	const auto floor_axis = [](float value, float offset, float step) {
		step = std::max(1.0f, step);
		return std::floor((value - offset) / step) * step + offset;
	};
	const auto ceil_axis = [](float value, float offset, float step) {
		step = std::max(1.0f, step);
		return std::ceil((value - offset) / step) * step + offset;
	};
	layer.noise.bounds_min = {
		floor_axis(minimum.x, grid.offset.x, grid.size.x),
		floor_axis(minimum.y, grid.offset.y, grid.size.y),
	};
	layer.noise.bounds_max = {
		ceil_axis(maximum.x, grid.offset.x, grid.size.x),
		ceil_axis(maximum.y, grid.offset.y, grid.size.y),
	};
	if (layer.noise.bounds_max.x <= layer.noise.bounds_min.x) layer.noise.bounds_max.x = layer.noise.bounds_min.x + grid.size.x;
	if (layer.noise.bounds_max.y <= layer.noise.bounds_min.y) layer.noise.bounds_max.y = layer.noise.bounds_min.y + grid.size.y;
}

static std::pair<I2, I2> NoiseBoundedCellRange(const EditorState& e, const SceneLayer& layer) {
	const RasterGrid grid{ NoiseRasterGrid(e, layer) };
	const auto [bmin, bmax]{ OrderedNoiseBounds(layer) };
	const I2 first{ WorldToRasterCell(grid, bmin) };
	const F2 epsilon{ std::max(0.001f, grid.size.x * 0.0001f), std::max(0.001f, grid.size.y * 0.0001f) };
	const I2 last{ WorldToRasterCell(grid, { bmax.x - epsilon.x, bmax.y - epsilon.y }) };
	return { first, last };
}

static std::pair<I2, I2> NoiseBoundaryCellRect(const EditorState& e, const SceneLayer& layer) {
	const RasterGrid grid{ NoiseRasterGrid(e, layer) };
	const auto [minimum, maximum]{ OrderedNoiseBounds(layer) };
	const I2 origin{
		static_cast<int>(std::lround((minimum.x - grid.offset.x) / grid.size.x)),
		static_cast<int>(std::lround((minimum.y - grid.offset.y) / grid.size.y)),
	};
	const I2 size{
		std::max(1, static_cast<int>(std::lround((maximum.x - minimum.x) / grid.size.x))),
		std::max(1, static_cast<int>(std::lround((maximum.y - minimum.y) / grid.size.y))),
	};
	return { origin, size };
}

static void SetNoiseBoundaryFromCellRect(
	EditorState& e,
	SceneLayer& layer,
	I2 origin,
	I2 size
) {
	const RasterGrid grid{ NoiseRasterGrid(e, layer) };
	size.x = std::max(1, size.x);
	size.y = std::max(1, size.y);
	layer.noise.bounds_min = {
		grid.offset.x + static_cast<float>(origin.x) * grid.size.x,
		grid.offset.y + static_cast<float>(origin.y) * grid.size.y,
	};
	layer.noise.bounds_max = {
		layer.noise.bounds_min.x + static_cast<float>(size.x) * grid.size.x,
		layer.noise.bounds_min.y + static_cast<float>(size.y) * grid.size.y,
	};
}

static std::pair<ImVec2, ImVec2> PixelCoveredScreenRect(const EditorState& e, F2 world_min, F2 world_max) {
	const F2 a{ WorldToScreen(e, world_min) };
	const F2 b{ WorldToScreen(e, world_max) };
	const float min_x{ std::min(a.x, b.x) };
	const float min_y{ std::min(a.y, b.y) };
	const float max_x{ std::max(a.x, b.x) };
	const float max_y{ std::max(a.y, b.y) };
	return {
		ImVec2{ std::floor(min_x), std::floor(min_y) },
		ImVec2{ std::ceil(max_x), std::ceil(max_y) },
	};
}

static F2 NoiseEntityPlacement(const RasterGrid& grid, I2 cell, EntityOrigin origin) {
	const RectF rect{ RasterCellRect(grid, cell) };
	const F2 f{ EntityOriginFraction(origin) };
	return {
		rect.min.x + (rect.max.x - rect.min.x) * f.x,
		rect.min.y + (rect.max.y - rect.min.y) * f.y,
	};
}

static F2 NoiseTileOffset(
	const EditorState& e,
	const RasterGrid& grid,
	const Tilemap& map,
	int tile_id,
	EntityOrigin origin
) {
	const F2 tile_size{ TileWorldSize(e, map, tile_id) };
	const F2 f{ EntityOriginFraction(origin) };
	return {
		(grid.size.x - tile_size.x) * f.x,
		(grid.size.y - tile_size.y) * f.y,
	};
}

static F2 NoiseTileWorldMinimum(
	const EditorState& e,
	const RasterGrid& grid,
	const Tilemap& map,
	I2 cell,
	int tile_id,
	EntityOrigin origin
) {
	const RectF rect{ RasterCellRect(grid, cell) };
	return rect.min + NoiseTileOffset(e, grid, map, tile_id, origin);
}

static void DrawNoiseLayer(EditorState& e, const SceneLayer& layer, ImDrawList* dl) {
	if (!layer.visible || layer.kind != LayerKind::Noise || layer.noise.fields.empty()) {
		return;
	}
	if (!layer.noise.show_noise_preview && !layer.noise.show_generated_preview) {
		return;
	}

	const RasterGrid grid{ NoiseRasterGrid(e, layer) };
	const F2 w0{ VisibleWorldMin(e) };
	const F2 w1{ VisibleWorldMax(e) };
	I2 c0{ WorldToRasterCell(grid, { std::min(w0.x, w1.x), std::min(w0.y, w1.y) }) };
	I2 c1{ WorldToRasterCell(grid, { std::max(w0.x, w1.x), std::max(w0.y, w1.y) }) };
	std::optional<std::pair<F2, F2>> noise_bounds_screen;
	if (layer.noise.bounded) {
		const auto [bmin, bmax]{ OrderedNoiseBounds(layer) };
		const auto [bc0, bc1]{ NoiseBoundedCellRange(e, layer) };
		c0.x = std::max(c0.x, bc0.x);
		c0.y = std::max(c0.y, bc0.y);
		c1.x = std::min(c1.x, bc1.x);
		c1.y = std::min(c1.y, bc1.y);
		if (c1.x < c0.x || c1.y < c0.y) return;
		noise_bounds_screen = std::pair{ WorldToScreen(e, bmin), WorldToScreen(e, bmax) };
	}
	const std::int64_t total_cells{
		static_cast<std::int64_t>(c1.x - c0.x + 1) *
		static_cast<std::int64_t>(c1.y - c0.y + 1)
	};
	const int stride{ total_cells > 50000
		? std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(total_cells) / 50000.0))))
		: 1 };

	const Tilemap* tilemap{};
	if (layer.noise.target == NoiseTargetKind::Tile) {
		tilemap = FindTilemap(e, layer.noise.tilemap_id);
		if (!tilemap) {
			return;
		}
	}

	int enabled_fields{};
	for (const auto& field : layer.noise.fields) {
		if (field.enabled) {
			++enabled_fields;
		}
	}
	const float field_alpha{
		layer.noise.noise_preview_alpha /
		static_cast<float>(std::max(1, enabled_fields))
	};

	for (int y{ c0.y }; y <= c1.y; y += stride) {
		for (int x{ c0.x }; x <= c1.x; x += stride) {
			const I2 cell{ x, y };
			const RectF cell_rect{ RasterCellRect(grid, cell) };
			const F2 sample{
				(cell_rect.min.x + cell_rect.max.x) * 0.5f,
				(cell_rect.min.y + cell_rect.max.y) * 0.5f,
			};
			const auto [screen0_px, screen1_px]{ PixelCoveredScreenRect(e, cell_rect.min, cell_rect.max) };
			const F2 screen0{ screen0_px.x, screen0_px.y };
			const F2 screen1{ screen1_px.x, screen1_px.y };

			for (const auto& field : layer.noise.fields) {
				if (!field.enabled) {
					continue;
				}

				const float value{ FractalNoiseValue(sample, field) };
				const auto* region{ FindNoiseThreshold(field, value) };

				// Draw the generated result first. The grayscale noise overlay is
				// intentionally drawn afterwards so both previews remain visible
				// when the user enables them simultaneously.
				if (layer.noise.show_generated_preview && region) {
					if (layer.noise.target == NoiseTargetKind::Tile) {
						if (region->tile_id >= 0 && tilemap) {
							const F2 draw_size{ TileWorldSize(e, *tilemap, region->tile_id) };
							const F2 draw_min{
								NoiseTileWorldMinimum(
									e,
									grid,
									*tilemap,
									cell,
									region->tile_id,
									region->origin
								)
							};
							const auto [p0_px, p1_px]{
								PixelCoveredScreenRect(e, draw_min, draw_min + draw_size)
							};
							const F2 p0{ p0_px.x, p0_px.y };
							const F2 p1{ p1_px.x, p1_px.y };
							if (const auto* tile = FindTile(e, region->tile_id);
								tile && tile->texture_index >= 0) {
								const auto& texture{
									e.textures[static_cast<std::size_t>(tile->texture_index)]
								};
								dl->AddImage(
									(ImTextureID)(intptr_t)texture.handle,
									{ p0.x, p0.y },
									{ p1.x, p1.y },
									tile->uv0,
									tile->uv1
								);
							}
						}
					} else if (
						region->prefab_index >= 0 &&
						region->prefab_index < static_cast<int>(e.prefabs.size())
					) {
						const auto& prefab{
							e.prefabs[static_cast<std::size_t>(region->prefab_index)]
						};
						const F2 placement{ NoiseEntityPlacement(grid, cell, region->origin) };
						const F2 f{ EntityOriginFraction(region->origin) };
						const F2 minimum{ placement.x - prefab.size.x * f.x, placement.y - prefab.size.y * f.y };
						const RectF bounds{ minimum, minimum + prefab.size };
						const F2 p0{ WorldToScreen(e, bounds.min) };
						const F2 p1{ WorldToScreen(e, bounds.max) };
						dl->AddRectFilled(
							{ p0.x, p0.y },
							{ p1.x, p1.y },
							ImGui::GetColorU32(ImVec4(0.30f, 0.58f, 0.86f, 0.72f)),
							2.0f
						);
						if ((p1.x - p0.x) > 30.0f && (p1.y - p0.y) > 18.0f) {
							dl->AddText(
								{ p0.x + 3.0f, p0.y + 3.0f },
								ImGui::GetColorU32(ImVec4(1, 1, 1, 0.9f)),
								prefab.name.c_str()
							);
						}
					}
				}

				if (layer.noise.show_noise_preview) {
					dl->AddRectFilled(
						{ screen0.x, screen0.y },
						{ screen1.x, screen1.y },
						ImGui::GetColorU32(ImVec4(value, value, value, field_alpha))
					);
				}
			}
		}
	}

	// Draw finite boundaries after the field itself so the outline cannot be
	// hidden by the grayscale/generated preview cells.
	if (noise_bounds_screen) {
		const auto& [p0, p1]{ *noise_bounds_screen };
		dl->AddRect(
			{ p0.x, p0.y },
			{ p1.x, p1.y },
			ImGui::GetColorU32(ImVec4(1.0f, 0.78f, 0.20f, 0.9f)),
			0.0f,
			0,
			1.5f
		);
	}
}

static ImGuiMouseCursor NoiseBoundaryCursor(NoiseBoundaryEdge edge) {
	switch (edge) {
		case NoiseBoundaryEdge::Left:
		case NoiseBoundaryEdge::Right:
			return ImGuiMouseCursor_ResizeEW;
		case NoiseBoundaryEdge::Top:
		case NoiseBoundaryEdge::Bottom:
			return ImGuiMouseCursor_ResizeNS;
		case NoiseBoundaryEdge::TopLeft:
		case NoiseBoundaryEdge::BottomRight:
			return ImGuiMouseCursor_ResizeNWSE;
		case NoiseBoundaryEdge::TopRight:
		case NoiseBoundaryEdge::BottomLeft:
			return ImGuiMouseCursor_ResizeNESW;
		case NoiseBoundaryEdge::None:
			return ImGuiMouseCursor_Arrow;
	}
	return ImGuiMouseCursor_Arrow;
}

static NoiseBoundaryEdge HitNoiseBoundaryEdge(
	const EditorState& e,
	const SceneLayer& layer,
	ImVec2 mouse
) {
	if (layer.kind != LayerKind::Noise || !layer.noise.bounded) {
		return NoiseBoundaryEdge::None;
	}

	const auto [bmin, bmax]{ OrderedNoiseBounds(layer) };
	const F2 a{ WorldToScreen(e, bmin) };
	const F2 b{ WorldToScreen(e, bmax) };
	const float left{ std::min(a.x, b.x) };
	const float right{ std::max(a.x, b.x) };
	const float top{ std::min(a.y, b.y) };
	const float bottom{ std::max(a.y, b.y) };
	constexpr float tolerance{ 9.0f };
	constexpr float corner_extent{ 15.0f };

	auto in_corner_box = [&](float x, float y) {
		return std::abs(mouse.x - x) <= corner_extent &&
			std::abs(mouse.y - y) <= corner_extent;
	};

	// Give corners a larger rectangular hit box than the edge tolerance and test
	// them first. This ensures every visible corner-handle pixel resolves to a
	// diagonal resize cursor instead of falling through to ResizeEW/ResizeNS.
	if (in_corner_box(left, top)) return NoiseBoundaryEdge::TopLeft;
	if (in_corner_box(right, top)) return NoiseBoundaryEdge::TopRight;
	if (in_corner_box(left, bottom)) return NoiseBoundaryEdge::BottomLeft;
	if (in_corner_box(right, bottom)) return NoiseBoundaryEdge::BottomRight;

	if (std::abs(mouse.x - left) <= tolerance && mouse.y >= top && mouse.y <= bottom) {
		return NoiseBoundaryEdge::Left;
	}
	if (std::abs(mouse.x - right) <= tolerance && mouse.y >= top && mouse.y <= bottom) {
		return NoiseBoundaryEdge::Right;
	}
	if (std::abs(mouse.y - top) <= tolerance && mouse.x >= left && mouse.x <= right) {
		return NoiseBoundaryEdge::Top;
	}
	if (std::abs(mouse.y - bottom) <= tolerance && mouse.x >= left && mouse.x <= right) {
		return NoiseBoundaryEdge::Bottom;
	}
	return NoiseBoundaryEdge::None;
}

static bool NoiseBoundaryMovesLeft(NoiseBoundaryEdge edge) {
	return edge == NoiseBoundaryEdge::Left ||
		edge == NoiseBoundaryEdge::TopLeft ||
		edge == NoiseBoundaryEdge::BottomLeft;
}

static bool NoiseBoundaryMovesRight(NoiseBoundaryEdge edge) {
	return edge == NoiseBoundaryEdge::Right ||
		edge == NoiseBoundaryEdge::TopRight ||
		edge == NoiseBoundaryEdge::BottomRight;
}

static bool NoiseBoundaryMovesTop(NoiseBoundaryEdge edge) {
	return edge == NoiseBoundaryEdge::Top ||
		edge == NoiseBoundaryEdge::TopLeft ||
		edge == NoiseBoundaryEdge::TopRight;
}

static bool NoiseBoundaryMovesBottom(NoiseBoundaryEdge edge) {
	return edge == NoiseBoundaryEdge::Bottom ||
		edge == NoiseBoundaryEdge::BottomLeft ||
		edge == NoiseBoundaryEdge::BottomRight;
}

static void SnapNoiseBoundaryDragToGrid(
	SceneLayer& layer,
	NoiseBoundaryEdge edge,
	const RasterGrid& grid
) {
	F2 minimum{ layer.noise.bounds_min };
	F2 maximum{ layer.noise.bounds_max };

	if (NoiseBoundaryMovesLeft(edge)) {
		minimum.x = SnapToGridLine(minimum.x, grid.offset.x, grid.size.x);
	}
	if (NoiseBoundaryMovesRight(edge)) {
		maximum.x = SnapToGridLine(maximum.x, grid.offset.x, grid.size.x);
	}
	if (NoiseBoundaryMovesTop(edge)) {
		minimum.y = SnapToGridLine(minimum.y, grid.offset.y, grid.size.y);
	}
	if (NoiseBoundaryMovesBottom(edge)) {
		maximum.y = SnapToGridLine(maximum.y, grid.offset.y, grid.size.y);
	}

	minimum.x = std::min(minimum.x, maximum.x - grid.size.x);
	minimum.y = std::min(minimum.y, maximum.y - grid.size.y);
	maximum.x = std::max(maximum.x, minimum.x + grid.size.x);
	maximum.y = std::max(maximum.y, minimum.y + grid.size.y);

	layer.noise.bounds_min = minimum;
	layer.noise.bounds_max = maximum;
}

static bool HandleNoiseBoundaryInput(EditorState& e, F2 mouse_world) {
	if (e.noise_boundary_drag.active) {
		SceneLayer* layer{ FindLayer(e, e.noise_boundary_drag.layer_id) };
		if (!layer || layer->kind != LayerKind::Noise || !layer->noise.bounded || layer->locked) {
			e.noise_boundary_drag = {};
			return true;
		}

		ImGui::SetMouseCursor(NoiseBoundaryCursor(e.noise_boundary_drag.edge));

		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
			if (e.noise_boundary_drag.before) {
				RestoreScene(e, *e.noise_boundary_drag.before);
			}
			e.noise_boundary_drag = {};
			return true;
		}

		const RasterGrid grid{ NoiseRasterGrid(e, *layer) };
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			// Snap by delta from the original edge. The cursor must travel one full
			// grid-cell in world space before the boundary advances one cell, which
			// avoids the old high-speed/half-cell jump behavior.
			const F2 delta{ mouse_world - e.noise_boundary_drag.start_mouse_world };
			const int delta_cells_x{
				static_cast<int>(std::lround(delta.x / std::max(1.0f, grid.size.x)))
			};
			const int delta_cells_y{
				static_cast<int>(std::lround(delta.y / std::max(1.0f, grid.size.y)))
			};
			F2 minimum{ e.noise_boundary_drag.start_bounds_min };
			F2 maximum{ e.noise_boundary_drag.start_bounds_max };
			const NoiseBoundaryEdge edge{ e.noise_boundary_drag.edge };

			if (NoiseBoundaryMovesLeft(edge)) {
				minimum.x = std::min(
					e.noise_boundary_drag.start_bounds_min.x +
						static_cast<float>(delta_cells_x) * grid.size.x,
					maximum.x - grid.size.x
				);
			}
			if (NoiseBoundaryMovesRight(edge)) {
				maximum.x = std::max(
					e.noise_boundary_drag.start_bounds_max.x +
						static_cast<float>(delta_cells_x) * grid.size.x,
					minimum.x + grid.size.x
				);
			}
			if (NoiseBoundaryMovesTop(edge)) {
				minimum.y = std::min(
					e.noise_boundary_drag.start_bounds_min.y +
						static_cast<float>(delta_cells_y) * grid.size.y,
					maximum.y - grid.size.y
				);
			}
			if (NoiseBoundaryMovesBottom(edge)) {
				maximum.y = std::max(
					e.noise_boundary_drag.start_bounds_max.y +
						static_cast<float>(delta_cells_y) * grid.size.y,
					minimum.y + grid.size.y
				);
			}

			layer->noise.bounds_min = minimum;
			layer->noise.bounds_max = maximum;
			e.noise_boundary_drag.changed |=
				Distance(e.noise_boundary_drag.start_bounds_min, minimum) > 0.001f ||
				Distance(e.noise_boundary_drag.start_bounds_max, maximum) > 0.001f;
		}

		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (e.noise_boundary_drag.changed && e.noise_boundary_drag.before) {
				PushHistory(
					e,
					"Resize Noise Bounds",
					std::move(*e.noise_boundary_drag.before)
				);
			}
			e.noise_boundary_drag = {};
		}

		return true;
	}

	SceneLayer* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->kind != LayerKind::Noise || !layer->noise.bounded ||
		layer->locked || !e.canvas_hovered) {
		return false;
	}

	const NoiseBoundaryEdge edge{
		HitNoiseBoundaryEdge(e, *layer, ImGui::GetMousePos())
	};
	if (edge == NoiseBoundaryEdge::None) {
		return false;
	}

	ImGui::SetMouseCursor(NoiseBoundaryCursor(edge));
	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		return false;
	}

	const auto [minimum, maximum]{ OrderedNoiseBounds(*layer) };
	e.noise_boundary_drag.active = true;
	e.noise_boundary_drag.layer_id = layer->id;
	e.noise_boundary_drag.edge = edge;
	e.noise_boundary_drag.start_mouse_world = mouse_world;
	e.noise_boundary_drag.start_bounds_min = minimum;
	e.noise_boundary_drag.start_bounds_max = maximum;
	e.noise_boundary_drag.before = CaptureScene(e);
	return true;
}

static void DrawActiveNoiseBoundaryHandles(EditorState& e, ImDrawList* dl) {
	const SceneLayer* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->kind != LayerKind::Noise || !layer->noise.bounded) {
		return;
	}

	const auto [bmin, bmax]{ OrderedNoiseBounds(*layer) };
	const F2 a{ WorldToScreen(e, bmin) };
	const F2 b{ WorldToScreen(e, bmax) };
	const float left{ std::min(a.x, b.x) };
	const float right{ std::max(a.x, b.x) };
	const float top{ std::min(a.y, b.y) };
	const float bottom{ std::max(a.y, b.y) };
	const ImU32 color{ ImGui::GetColorU32(
		layer->locked
			? ImVec4(1.0f, 0.78f, 0.20f, 0.45f)
			: ImVec4(1.0f, 0.78f, 0.20f, 1.0f)
	) };
	const float thickness{ layer->locked ? 1.5f : 2.0f };
	dl->AddRect({ left, top }, { right, bottom }, color, 0.0f, 0, thickness);
	if (layer->locked) {
		return;
	}

	constexpr float half{ 4.0f };
	for (ImVec2 center : {
		ImVec2{ left, top },
		ImVec2{ right, top },
		ImVec2{ left, bottom },
		ImVec2{ right, bottom },
		ImVec2{ left, (top + bottom) * 0.5f },
		ImVec2{ right, (top + bottom) * 0.5f },
		ImVec2{ (left + right) * 0.5f, top },
		ImVec2{ (left + right) * 0.5f, bottom },
	}) {
		dl->AddRectFilled(
			{ center.x - half, center.y - half },
			{ center.x + half, center.y + half },
			color
		);
		dl->AddRect(
			{ center.x - half, center.y - half },
			{ center.x + half, center.y + half },
			ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, 0.9f))
		);
	}
}

static void DrawGrid(EditorState& e, ImDrawList* dl) {
	if (!e.grid.visible) {
		return;
	}
	const F2 w0{ VisibleWorldMin(e) };
	const F2 w1{ VisibleWorldMax(e) };
	F2 grid_size{ e.grid.size };
	F2 grid_offset{ e.grid.offset };
	if (const SceneLayer* active = FindLayer(e, e.active_layer_id); active) {
		if (active->kind == LayerKind::Tile) {
			if (const Tilemap* map = FindTilemap(e, active->tile.tilemap_id)) {
				grid_size = map->cell_size;
				grid_offset = map->origin;
			}
		} else if (active->kind == LayerKind::Noise) {
			grid_size = active->noise.grid_size;
			grid_offset = active->noise.grid_offset;
		}
	}
	const float sx{ std::max(1.0f, grid_size.x) };
	const float sy{ std::max(1.0f, grid_size.y) };
	const int x0{ static_cast<int>(std::floor((std::min(w0.x, w1.x) - grid_offset.x) / sx)) - 1 };
	const int x1{ static_cast<int>(std::ceil((std::max(w0.x, w1.x) - grid_offset.x) / sx)) + 1 };
	const int y0{ static_cast<int>(std::floor((std::min(w0.y, w1.y) - grid_offset.y) / sy)) - 1 };
	const int y1{ static_cast<int>(std::ceil((std::max(w0.y, w1.y) - grid_offset.y) / sy)) + 1 };
	const ImU32 minor{ ImGui::GetColorU32(e.grid.minor_color) };
	const ImU32 major{ ImGui::GetColorU32(e.grid.major_color) };
	for (int x{ x0 }; x <= x1; ++x) {
		const float wx{ grid_offset.x + static_cast<float>(x) * sx };
		const auto p0{ WorldToScreen(e, { wx, w0.y }) };
		const auto p1{ WorldToScreen(e, { wx, w1.y }) };
		const bool is_major{ x % std::max(1, e.grid.major_every) == 0 };
		dl->AddLine(
			{ p0.x, p0.y },
			{ p1.x, p1.y },
			is_major ? major : minor,
			is_major ? e.grid.major_thickness : e.grid.minor_thickness
		);
	}
	for (int y{ y0 }; y <= y1; ++y) {
		const float wy{ grid_offset.y + static_cast<float>(y) * sy };
		const auto p0{ WorldToScreen(e, { w0.x, wy }) };
		const auto p1{ WorldToScreen(e, { w1.x, wy }) };
		const bool is_major{ y % std::max(1, e.grid.major_every) == 0 };
		dl->AddLine(
			{ p0.x, p0.y },
			{ p1.x, p1.y },
			is_major ? major : minor,
			is_major ? e.grid.major_thickness : e.grid.minor_thickness
		);
	}
}

static void DrawTileVisual(
	EditorState& e,
	ImDrawList* dl,
	const Tilemap& map,
	I2 cell,
	int tile_id,
	EntityOrigin origin,
	F2 offset = {},
	float alpha = 1.0f
) {
	if (tile_id < 0) return;
	const RectF rect{ TileAnchorRect(e, map, cell, tile_id, offset, origin) };
	const auto [p0_px, p1_px]{ PixelCoveredScreenRect(e, rect.min, rect.max) };
	if (p1_px.x < e.canvas_screen_min.x || p0_px.x > e.canvas_screen_max.x || p1_px.y < e.canvas_screen_min.y || p0_px.y > e.canvas_screen_max.y) return;
	if (const auto* tile = FindTile(e, tile_id); tile && tile->texture_index >= 0) {
		const auto& tex{ e.textures[static_cast<std::size_t>(tile->texture_index)] };
		dl->AddImage((ImTextureID)(intptr_t)tex.handle, { p0_px.x, p0_px.y }, { p1_px.x, p1_px.y }, tile->uv0, tile->uv1, ImGui::GetColorU32(ImVec4(1, 1, 1, alpha)));
	} else {
		dl->AddRectFilled({ p0_px.x, p0_px.y }, { p1_px.x, p1_px.y }, ImGui::GetColorU32(ImVec4(0.5f, 0.55f, 0.65f, alpha)));
	}
}

static int GeneratorAutotileIndex(const PaintGenerator& generator, const AutotileRuleSet& rules, I2 cell) {
	auto filled = [&](I2 c) { return GeneratorCoveragePass(generator, c); };
	const bool n{ filled({ cell.x, cell.y - 1 }) };
	const bool e{ filled({ cell.x + 1, cell.y }) };
	const bool ss{ filled({ cell.x, cell.y + 1 }) };
	const bool w{ filled({ cell.x - 1, cell.y }) };
	int mask{};
	if (n) mask |= 1;
	if (e) mask |= 2;
	if (ss) mask |= 4;
	if (w) mask |= 8;
	if (rules.format == AutotileFormat::Blob47) {
		if (n && e && filled({ cell.x + 1, cell.y - 1 })) mask |= 16;
		if (e && ss && filled({ cell.x + 1, cell.y + 1 })) mask |= 32;
		if (ss && w && filled({ cell.x - 1, cell.y + 1 })) mask |= 64;
		if (w && n && filled({ cell.x - 1, cell.y - 1 })) mask |= 128;
		const auto& valid{ ValidBlob47Masks() };
		if (const auto it = std::find(valid.begin(), valid.end(), mask); it != valid.end()) return static_cast<int>(std::distance(valid.begin(), it));
		return 0;
	}
	if (rules.format == AutotileFormat::Classic15) return mask == 0 ? 0 : std::clamp(mask - 1, 0, 14);
	return std::clamp(mask, 0, 15);
}

static void DrawGeneratorsForLayer(EditorState& e, const SceneLayer& layer, ImDrawList* dl) {
	if (!layer.visible || layer.kind == LayerKind::Noise) return;

	std::vector<const PaintGenerator*> generators;
	generators.reserve(e.generators.size() + 1);
	for (const auto& generator : e.generators) {
		if (generator.layer_id == layer.id && generator.visible) generators.push_back(&generator);
	}
	if (
		e.pending_generator &&
		e.pending_generator->layer_id == layer.id &&
		e.pending_generator->visible
	) {
		generators.push_back(&*e.pending_generator);
	}
	if (generators.empty()) return;

	const F2 wa{ ScreenToWorld(e, e.canvas_screen_min) };
	const F2 wb{ ScreenToWorld(e, e.canvas_screen_max) };

	auto visible_bounds = [&](const PaintGenerator& generator) {
		auto world_to_cell = [&](F2 p) {
			return I2{
				static_cast<int>(std::floor((p.x - generator.grid_offset.x) / std::max(1.0f, generator.grid_size.x))),
				static_cast<int>(std::floor((p.y - generator.grid_offset.y) / std::max(1.0f, generator.grid_size.y))),
			};
		};
		I2 first{ world_to_cell({ std::min(wa.x, wb.x), std::min(wa.y, wb.y) }) };
		I2 last{ world_to_cell({ std::max(wa.x, wb.x), std::max(wa.y, wb.y) }) };
		first.x -= 2;
		first.y -= 2;
		last.x += 2;
		last.y += 2;
		if (generator.geometry != GeneratorGeometryKind::Infinite) {
			const auto [gf, gl]{ GeneratorCellBounds(generator) };
			first.x = std::max(first.x, gf.x);
			first.y = std::max(first.y, gf.y);
			last.x = std::min(last.x, gl.x);
			last.y = std::min(last.y, gl.y);
		}
		return std::pair<I2, I2>{ first, last };
	};

	auto for_each_visible_geometry_cell = [&](const PaintGenerator& generator, auto&& fn) {
		const auto [first, last]{ visible_bounds(generator) };
		if (first.x > last.x || first.y > last.y) return;
		if (generator.geometry == GeneratorGeometryKind::BrushStroke && !generator.brush_cells.empty()) {
			const std::size_t visible_cell_count{
				static_cast<std::size_t>(last.x - first.x + 1) *
				static_cast<std::size_t>(last.y - first.y + 1)
			};
			if (visible_cell_count < generator.brush_cells.size()) {
				for (int y{ first.y }; y <= last.y; ++y) {
					for (int x{ first.x }; x <= last.x; ++x) {
						const I2 cell{ x, y };
						if (generator.brush_cells.contains(cell)) fn(cell);
					}
				}
			} else {
				for (const I2 cell : generator.brush_cells) {
					if (cell.x < first.x || cell.x > last.x || cell.y < first.y || cell.y > last.y) continue;
					fn(cell);
				}
			}
			return;
		}
		for (int y{ first.y }; y <= last.y; ++y) {
			for (int x{ first.x }; x <= last.x; ++x) {
				const I2 cell{ x, y };
				if (GeneratorGeometryContains(generator, cell)) fn(cell);
			}
		}
	};

	// Pass 1: raw noise overlays. These deliberately render before all generated
	// tile/entity previews in the layer; the scene grid is drawn after the layer.
	for (const PaintGenerator* generator_ptr : generators) {
		const PaintGenerator& generator{ *generator_ptr };
		if (
			generator.recipe.source_kind != PaintSourceKind::Noise ||
			!generator.recipe.show_noise_preview ||
			generator.recipe.noise_preview_alpha <= 0.0f
		) {
			continue;
		}
		for_each_visible_geometry_cell(generator, [&](I2 cell) {
			if (GeneratorSuppressed(generator, cell)) return;
			const F2 center{ GeneratorCellCenter(generator, cell) };
			const float value{ RecipeNoiseValue(center, generator.recipe) };
			const F2 mn{
				generator.grid_offset.x + static_cast<float>(cell.x) * generator.grid_size.x,
				generator.grid_offset.y + static_cast<float>(cell.y) * generator.grid_size.y,
			};
			const F2 mx{ mn + generator.grid_size };
			const F2 p0{ WorldToScreen(e, mn) };
			const F2 p1{ WorldToScreen(e, mx) };
			dl->AddRectFilled(
				{ p0.x, p0.y },
				{ p1.x, p1.y },
				ImGui::GetColorU32(ImVec4(value, value, value, std::clamp(generator.recipe.noise_preview_alpha, 0.0f, 1.0f)))
			);
		});
	}

	// Pass 2: threshold/source-resolved tile/entity previews.
	for (const PaintGenerator* generator_ptr : generators) {
		const PaintGenerator& generator{ *generator_ptr };
		const bool live_preview{ generator.id < 0 };
		const float generator_alpha{ live_preview ? 0.70f : 0.92f };
		if (generator.recipe.source_kind == PaintSourceKind::Noise && !generator.recipe.show_generated_preview) continue;

		Tilemap generator_map;
		if (layer.kind == LayerKind::Tile) {
			const auto* map{ FindTilemap(e, layer.tile.tilemap_id) };
			if (!map) continue;
			generator_map = *map;
			generator_map.cell_size = generator.grid_size;
			generator_map.origin = generator.grid_offset;
		}

		for_each_visible_geometry_cell(generator, [&](I2 cell) {
			if (!GeneratorCoveragePass(generator, cell)) return;
			const F2 world{ GeneratorCellCenter(generator, cell) };
			if (layer.kind == LayerKind::Tile) {
				int tile_id{ -1 };
				EntityOrigin origin{ RecipeSourceOrigin(generator.recipe, world, true) };
				if (generator.recipe.source_kind == PaintSourceKind::Autotile) {
					const auto* rules{ FindAutotileRuleSet(e, generator.recipe.autotile_ruleset_id) };
					if (!rules || rules->tile_ids.empty()) return;
					if (rules->format == AutotileFormat::DualGrid16) {
						int mask{};
						if (GeneratorCoveragePass(generator, { cell.x, cell.y })) mask |= 1;
						if (GeneratorCoveragePass(generator, { cell.x + 1, cell.y })) mask |= 2;
						if (GeneratorCoveragePass(generator, { cell.x, cell.y + 1 })) mask |= 4;
						if (GeneratorCoveragePass(generator, { cell.x + 1, cell.y + 1 })) mask |= 8;
						if (mask < 0 || mask >= static_cast<int>(rules->tile_ids.size())) return;
						tile_id = rules->tile_ids[static_cast<std::size_t>(mask)];
						Tilemap display_map{ generator_map };
						display_map.origin = {
							generator.grid_offset.x + generator.grid_size.x * 0.5f,
							generator.grid_offset.y + generator.grid_size.y * 0.5f
						};
						DrawTileVisual(e, dl, display_map, cell, tile_id, EntityOrigin::TopLeft, {}, generator_alpha);
						return;
					}
					const int index{ GeneratorAutotileIndex(generator, *rules, cell) };
					if (index < 0 || index >= static_cast<int>(rules->tile_ids.size())) return;
					tile_id = rules->tile_ids[static_cast<std::size_t>(index)];
					origin = EntityOrigin::TopLeft;
				} else {
					tile_id = ChooseTileFromRecipe(e, generator.recipe, world, cell, true);
				}
				if (tile_id < 0) return;
				DrawTileVisual(e, dl, generator_map, cell, tile_id, origin, {}, generator_alpha);
			} else {
				const int prefab_index{ ChoosePrefabFromRecipe(e, generator.recipe, world, cell, true) };
				if (prefab_index < 0 || prefab_index >= static_cast<int>(e.prefabs.size())) return;
				const auto& prefab{ e.prefabs[static_cast<std::size_t>(prefab_index)] };
				const EntityOrigin origin{ RecipeSourceOrigin(generator.recipe, world, false) };
				const F2 f{ EntityOriginFraction(origin) };
				const F2 min{
					world.x - prefab.size.x * f.x,
					world.y - prefab.size.y * f.y
				};
				const F2 p0{ WorldToScreen(e, min) };
				const F2 p1{ WorldToScreen(e, min + prefab.size) };
				dl->AddRectFilled(
					{ p0.x, p0.y },
					{ p1.x, p1.y },
					ImGui::GetColorU32(ImVec4(0.25f, 0.72f, 0.42f, live_preview ? 0.42f : 0.58f)),
					3.0f
				);
				dl->AddText(
					{ p0.x + 3.0f, p0.y + 3.0f },
					ImGui::GetColorU32(ImVec4(1, 1, 1, 0.8f)),
					prefab.name.c_str()
				);
			}
		});
	}

	// Pass 3: generator bounds. Live generators are yellow; a selected persistent
	// generator uses cyan so selection is unmistakable in the viewport.
	for (const PaintGenerator* generator_ptr : generators) {
		const PaintGenerator& generator{ *generator_ptr };
		const bool live_preview{ generator.id < 0 };
		const bool selected{ generator.id >= 0 && e.selected_generator_id == generator.id };
		if ((!live_preview && !selected) || generator.geometry == GeneratorGeometryKind::Infinite) continue;
		const auto [first, last]{ GeneratorCellBounds(generator) };
		const F2 mn{
			generator.grid_offset.x + static_cast<float>(first.x) * generator.grid_size.x,
			generator.grid_offset.y + static_cast<float>(first.y) * generator.grid_size.y,
		};
		const F2 mx{
			generator.grid_offset.x + static_cast<float>(last.x + 1) * generator.grid_size.x,
			generator.grid_offset.y + static_cast<float>(last.y + 1) * generator.grid_size.y,
		};
		const F2 p0{ WorldToScreen(e, mn) };
		const F2 p1{ WorldToScreen(e, mx) };
		const ImVec4 border_color{
			selected
				? ImVec4(0.25f, 0.82f, 1.0f, 1.0f)
				: ImVec4(1.0f, 0.75f, 0.18f, 0.98f)
		};
		dl->AddRect(
			{ p0.x, p0.y },
			{ p1.x, p1.y },
			ImGui::GetColorU32(border_color),
			0.0f,
			0,
			2.0f
		);
	}
}

static void DrawTileLayer(EditorState& e, const SceneLayer& layer, ImDrawList* dl) {
	if (!layer.visible || layer.kind != LayerKind::Tile) {
		return;
	}
	const auto* map{ FindTilemap(e, layer.tile.tilemap_id) };
	if (!map) {
		return;
	}

	struct DrawAnchor {
		I2 cell{};
		const TileCell* data{};
	};
	std::vector<DrawAnchor> anchors;
	for (const auto& [chunk_coord, chunk] : layer.tile.loaded_chunks) {
		for (int y{}; y < map->chunk_size.y; ++y) {
			for (int x{}; x < map->chunk_size.x; ++x) {
				const auto& cell{ chunk.cells[static_cast<std::size_t>(y * map->chunk_size.x + x)] };
				if (cell.tile_id < 0) {
					continue;
				}
				anchors.push_back({ { chunk_coord.x * map->chunk_size.x + x, chunk_coord.y * map->chunk_size.y + y }, &cell });
			}
		}
	}
	std::stable_sort(anchors.begin(), anchors.end(), [](const DrawAnchor& a, const DrawAnchor& b) {
		return a.cell.y != b.cell.y ? a.cell.y < b.cell.y : a.cell.x < b.cell.x;
	});

	for (const DrawAnchor& anchor : anchors) {
		const TileCell& cell{ *anchor.data };
		const RectF tile_rect{ TileAnchorRect(e, *map, anchor.cell, cell.tile_id, cell.offset, cell.origin) };
		const auto [p0_px, p1_px]{ PixelCoveredScreenRect(e, tile_rect.min, tile_rect.max) };
		const F2 p0{ p0_px.x, p0_px.y };
		const F2 p1{ p1_px.x, p1_px.y };
		if (p1.x < e.canvas_screen_min.x || p0.x > e.canvas_screen_max.x || p1.y < e.canvas_screen_min.y || p0.y > e.canvas_screen_max.y) {
			continue;
		}
		if (const auto* tile = FindTile(e, cell.tile_id); tile && tile->texture_index >= 0) {
			const auto& tex{ e.textures[static_cast<std::size_t>(tile->texture_index)] };
			dl->AddImage((ImTextureID)(intptr_t)tex.handle, { p0.x, p0.y }, { p1.x, p1.y }, tile->uv0, tile->uv1);
		} else {
			dl->AddRectFilled({ p0.x, p0.y }, { p1.x, p1.y }, ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.55f, 1.0f)));
		}
		if (layer.purpose == LayerPurpose::Collision) {
			dl->AddRectFilled({ p0.x, p0.y }, { p1.x, p1.y }, ImGui::GetColorU32(ImVec4(1.0f, 0.2f, 0.2f, 0.18f)));
		}
		if (e.selected_tile_layer_id == layer.id &&
			e.selected_tile_cells.contains(anchor.cell)) {
			dl->AddRectFilled(
				{ p0.x, p0.y },
				{ p1.x, p1.y },
				ImGui::GetColorU32(ImVec4(1.0f, 0.72f, 0.12f, 0.22f))
			);
			dl->AddRect(
				{ p0.x, p0.y },
				{ p1.x, p1.y },
				ImGui::GetColorU32(ImVec4(1.0f, 0.78f, 0.18f, 0.95f)),
				0.0f,
				0,
				2.0f
			);
		}
	}

	// Dual-grid terrain stores the logical world grid and derives a display grid
	// offset by half a tile. Each displayed tile depends on the four overlapping
	// logical cells, yielding the complete 16-case dual-grid set.
	if (!layer.tile.dual_grid_terrain.empty()) {
		const F2 wa{ ScreenToWorld(e, e.canvas_screen_min) };
		const F2 wb{ ScreenToWorld(e, e.canvas_screen_max) };
		const I2 first{ WorldToCell(*map, { std::min(wa.x, wb.x), std::min(wa.y, wb.y) }) };
		const I2 last{ WorldToCell(*map, { std::max(wa.x, wb.x), std::max(wa.y, wb.y) }) };
		Tilemap display_map{ *map };
		display_map.origin = { map->origin.x + map->cell_size.x * 0.5f, map->origin.y + map->cell_size.y * 0.5f };
		for (int y{ first.y - 2 }; y <= last.y + 1; ++y) {
			for (int x{ first.x - 2 }; x <= last.x + 1; ++x) {
				const I2 display_cell{ x, y };
				const std::array<I2, 4> logical{{ { x, y }, { x + 1, y }, { x, y + 1 }, { x + 1, y + 1 } }};
				int ruleset_id{ -1 };
				for (const I2 c : logical) {
					if (const auto it = layer.tile.dual_grid_terrain.find(c); it != layer.tile.dual_grid_terrain.end()) { ruleset_id = it->second; break; }
				}
				const auto* rules{ FindAutotileRuleSet(e, ruleset_id) };
				if (!rules || rules->format != AutotileFormat::DualGrid16 || rules->tile_ids.size() < 16) continue;
				int mask{};
				for (int i{}; i < 4; ++i) {
					if (const auto it = layer.tile.dual_grid_terrain.find(logical[static_cast<std::size_t>(i)]); it != layer.tile.dual_grid_terrain.end() && it->second == ruleset_id) mask |= 1 << i;
				}
				DrawTileVisual(e, dl, display_map, display_cell, rules->tile_ids[static_cast<std::size_t>(mask)], EntityOrigin::TopLeft);
			}
		}
	}

	if (map->streaming.show_chunk_boundaries) {
		const float chunk_w{ map->cell_size.x * static_cast<float>(map->chunk_size.x) };
		const float chunk_h{ map->cell_size.y * static_cast<float>(map->chunk_size.y) };
		for (const auto& [coord, _] : layer.tile.loaded_chunks) {
			const F2 w0{ map->origin.x + coord.x * chunk_w, map->origin.y + coord.y * chunk_h };
			const F2 w1{ w0.x + chunk_w, w0.y + chunk_h };
			const F2 p0{ WorldToScreen(e, w0) };
			const F2 p1{ WorldToScreen(e, w1) };
			dl->AddRect({ p0.x, p0.y }, { p1.x, p1.y }, ImGui::GetColorU32(ImVec4(1.0f, 0.8f, 0.2f, 0.45f)), 0.0f, 0, 1.5f);
			if (map->streaming.show_streaming_state) {
				const std::string label{ "chunk " + std::to_string(coord.x) + "," + std::to_string(coord.y) };
				dl->AddText({ p0.x + 4.0f, p0.y + 4.0f }, ImGui::GetColorU32(ImVec4(1.0f, 0.9f, 0.45f, 0.8f)), label.c_str());
			}
		}
	}

	if (map->streaming.show_streaming_state) {
		for (const auto& cell : map->exclusion_mask) {
			const F2 w0{ CellToWorld(*map, cell) };
			const F2 w1{ w0 + map->cell_size };
			const F2 p0{ WorldToScreen(e, w0) };
			const F2 p1{ WorldToScreen(e, w1) };
			dl->AddRectFilled({ p0.x, p0.y }, { p1.x, p1.y }, ImGui::GetColorU32(ImVec4(1.0f, 0.1f, 0.1f, 0.18f)));
		}
	}
}

static void DrawEntityLayer(EditorState& e, const SceneLayer& layer, ImDrawList* dl) {
	if (!layer.visible || layer.kind != LayerKind::Entity) {
		return;
	}
	std::vector<const Entity*> sorted;
	for (const auto& entity : e.entities) {
		if (entity.layer_id == layer.id) {
			sorted.push_back(&entity);
		}
	}
	std::stable_sort(sorted.begin(), sorted.end(), [](const Entity* a, const Entity* b) {
		return a->depth < b->depth;
	});
	for (const Entity* entity : sorted) {
		const RectF bounds{ EntityBounds(*entity) };
		const F2 p0{ WorldToScreen(e, bounds.min) };
		const F2 p1{ WorldToScreen(e, bounds.max) };
		const bool selected{ e.selected_entities.contains(entity->id) };
		const ImU32 fill{ ImGui::GetColorU32(selected ? ImVec4(0.95f, 0.7f, 0.15f, 0.65f) : ImVec4(0.3f, 0.55f, 0.8f, 0.65f)) };
		dl->AddRectFilled({ p0.x, p0.y }, { p1.x, p1.y }, fill, 3.0f);
		dl->AddRect({ p0.x, p0.y }, { p1.x, p1.y }, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, selected ? 0.9f : 0.35f)), 3.0f, 0, selected ? 2.0f : 1.0f);
		dl->AddText({ p0.x + 4.0f, p0.y + 4.0f }, ImGui::GetColorU32(ImVec4(1, 1, 1, 0.9f)), entity->prefab.c_str());
	}
}

static F2 ConstrainSquareDrag(F2 start, F2 current) {
	const float dx{ current.x - start.x };
	const float dy{ current.y - start.y };
	const float side{ std::max(std::abs(dx), std::abs(dy)) };
	return {
		start.x + (dx < 0.0f ? -side : side),
		start.y + (dy < 0.0f ? -side : side),
	};
}

static void DrawToolPreview(EditorState& e, ImDrawList* dl, F2 mouse_world) {
	const auto* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->kind == LayerKind::Noise || e.tool == Tool::None) {
		return;
	}

	const RasterGrid raster{ ActiveRasterGrid(e) };
	const bool preview_uses_operation{
		e.tool == Tool::Pencil || e.tool == Tool::Brush ||
		e.tool == Tool::Line || e.tool == Tool::Rectangle ||
		e.tool == Tool::Erase
	};
	const BrushOperation preview_operation{
		preview_uses_operation
			? e.brush.operation
			: BrushOperation::Paint
	};

	auto draw_raster_cells = [&](const std::vector<I2>& cells, ImVec4 fill_color, ImVec4 line_color) {
		for (const I2 cell : cells) {
			const RectF rect{ RasterCellRect(raster, cell) };
			const F2 p0{ WorldToScreen(e, rect.min) };
			const F2 p1{ WorldToScreen(e, rect.max) };
			dl->AddRectFilled(
				{ p0.x, p0.y },
				{ p1.x, p1.y },
				ImGui::GetColorU32(fill_color)
			);
			dl->AddRect(
				{ p0.x, p0.y },
				{ p1.x, p1.y },
				ImGui::GetColorU32(line_color)
			);
		}
	};

	auto tile_anchor_allowed = [&](const SceneLayer& tile_layer, const Tilemap& map, I2 cell) {
		if (preview_operation == BrushOperation::ExclusionMask) {
			return true;
		}
		if (preview_operation == BrushOperation::Replace) {
			const auto* old{ ReadTileCell(tile_layer, map, cell) };
			return old && old->tile_id >= 0;
		}
		const int preview_tile_id{ ChooseTileFromRecipe(e, e.recipe, CellToWorld(map, cell), cell, true) };
		if (preview_tile_id < 0 && e.recipe.source_kind != PaintSourceKind::Autotile) return false;

		if (!e.stroke.active) {
			const auto* old{ ReadTileCell(tile_layer, map, cell) };
			const bool occupied{ old && old->tile_id >= 0 };
			if (occupied && !e.recipe.replace_occupied_anchor) {
				return false;
			}

			if (e.recipe.tile_paint_mode == TilePaintMode::Tile) {
				const I2 footprint{
					TileFootprintCells(e, map, preview_tile_id)
				};
				const I2 origin{ WorldToCell(map, mouse_world) };
				if (
					FloorMod(cell.x - origin.x, footprint.x) != 0 ||
					FloorMod(cell.y - origin.y, footprint.y) != 0
				) {
					return false;
				}

				if (
					!e.recipe.allow_visual_overlap &&
					TileFootprintOverlapsExisting(
						e,
						tile_layer,
						map,
						cell,
						preview_tile_id,
						occupied && e.recipe.replace_occupied_anchor
					)
				) {
					return false;
				}
			}
			return true;
		}

		return CanPlaceTileAnchor(
			e,
			tile_layer,
			map,
			cell,
			preview_tile_id
		);
	};

	auto draw_tile_preview = [&](const SceneLayer& tile_layer, const Tilemap& map, const std::vector<I2>& cells) {
		for (const I2 cell : cells) {
			if (!tile_anchor_allowed(tile_layer, map, cell)) {
				continue;
			}

			if (preview_operation == BrushOperation::ExclusionMask) {
				const F2 w0{ CellToWorld(map, cell) };
				const F2 p0{ WorldToScreen(e, w0) };
				const F2 p1{ WorldToScreen(e, w0 + map.cell_size) };
				dl->AddRectFilled(
					{ p0.x, p0.y },
					{ p1.x, p1.y },
					ImGui::GetColorU32(ImVec4(1.0f, 0.25f, 0.2f, 0.24f))
				);
				continue;
			}

			const int tile_id{ ChooseTileFromRecipe(e, e.recipe, CellToWorld(map, cell), cell, true) };
			const auto* tile{ FindTile(e, tile_id) };
			const F2 source_world{ CellToWorld(map, cell) };
			const EntityOrigin preview_origin{ RecipeSourceOrigin(e.recipe, source_world, true) };
			const RectF preview_rect{ TileAnchorRect(e, map, cell, tile_id, {}, preview_origin) };
			const F2 p0{ WorldToScreen(e, preview_rect.min) };
			const F2 p1{ WorldToScreen(e, preview_rect.max) };

			if (tile && tile->texture_index >= 0) {
				const auto& texture{
					e.textures[static_cast<std::size_t>(tile->texture_index)]
				};
				dl->AddImage(
					(ImTextureID)(intptr_t)texture.handle,
					{ p0.x, p0.y },
					{ p1.x, p1.y },
					tile->uv0,
					tile->uv1,
					ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.48f))
				);
			} else {
				dl->AddRectFilled(
					{ p0.x, p0.y },
					{ p1.x, p1.y },
					ImGui::GetColorU32(ImVec4(0.75f, 0.8f, 0.9f, 0.28f))
				);
			}
			dl->AddRect(
				{ p0.x, p0.y },
				{ p1.x, p1.y },
				ImGui::GetColorU32(ImVec4(1.0f, 0.85f, 0.3f, 0.7f))
			);
		}
	};

	if (e.tool == Tool::Brush ||
		e.tool == Tool::Erase ||
		(e.tool == Tool::Select && e.brush.select_mode == SelectMode::Brush)) {
		const std::vector<I2> cells{ RasterBrushCells(e, mouse_world) };

		if (layer && layer->kind == LayerKind::Tile && e.tool == Tool::Brush) {
			if (const auto* map = FindTilemap(e, layer->tile.tilemap_id)) {
				draw_tile_preview(*layer, *map, cells);
			}
		} else {
			draw_raster_cells(
				cells,
				ImVec4(1.0f, 0.82f, 0.22f, 0.10f),
				ImVec4(1.0f, 0.88f, 0.35f, 0.42f)
			);
		}
	}

	if (e.stroke.active && (e.tool == Tool::Line || e.tool == Tool::Rectangle)) {
		std::vector<I2> cells{
			e.tool == Tool::Line
				? RasterLineCells(e, e.stroke.start_world, mouse_world)
				: RasterAreaCells(e, e.stroke.start_world, mouse_world)
		};

		if (
			e.tool == Tool::Rectangle &&
			e.brush.area_mode == AreaMode::RandomFill
		) {
			cells.erase(
				std::remove_if(
					cells.begin(),
					cells.end(),
					[&](I2 cell) {
						return !PassesAreaRandomFill(e, cell);
					}
				),
				cells.end()
			);
		}

		if (layer && layer->kind == LayerKind::Tile) {
			if (const auto* map = FindTilemap(e, layer->tile.tilemap_id)) {
				draw_tile_preview(*layer, *map, cells);
			}
		} else {
			draw_raster_cells(
				cells,
				ImVec4(0.45f, 0.75f, 1.0f, 0.12f),
				ImVec4(0.55f, 0.82f, 1.0f, 0.48f)
			);
		}
	}

	if (e.stroke.active &&
		e.tool == Tool::Select &&
		e.brush.select_mode == SelectMode::ClickMarquee &&
		Distance(e.stroke.start_world, mouse_world) >= 4.0f / e.view_zoom) {
		const F2 constrained{ ImGui::GetIO().KeyShift
			? ConstrainSquareDrag(e.stroke.start_world, mouse_world)
			: mouse_world };
		const F2 a{ WorldToScreen(e, e.stroke.start_world) };
		const F2 b{ WorldToScreen(e, constrained) };
		dl->AddRect(
			{ std::min(a.x, b.x), std::min(a.y, b.y) },
			{ std::max(a.x, b.x), std::max(a.y, b.y) },
			ImGui::GetColorU32(ImVec4(1, 1, 1, 0.8f)),
			0.0f,
			0,
			2.0f
		);
	}
}
static void BeginStroke(EditorState& e, F2 world, const char* label) {
	e.stroke = {};
	e.stroke.active = true;
	e.stroke.start_world = world;
	e.stroke.last_world = world;
	e.stroke.current_world = world;
	e.stroke.points.push_back(world);
	e.stroke.before = CaptureScene(e);
	(void)label;
}

static void EndStroke(EditorState& e, const char* label) {
	if (e.stroke.active && e.stroke.changed && e.stroke.before) {
		PushHistory(e, label, std::move(*e.stroke.before));
	}
	e.stroke = {};
}

static void CancelStroke(EditorState& e) {
	// Line/Rectangle previews do not modify the scene until release, so cancelling
	// simply discards the pending stroke and its snapshot.
	e.stroke = {};
}

static float PencilSampleStep(const EditorState& e) {
	if (const auto* layer = FindLayer(e, e.active_layer_id)) {
		if (layer->kind == LayerKind::Tile) {
			if (const auto* map = FindTilemap(e, layer->tile.tilemap_id)) {
				return std::max(1.0f, std::min(map->cell_size.x, map->cell_size.y) * 0.35f);
			}
		} else if (e.grid.snap) {
			return std::max(1.0f, std::min(e.grid.size.x, e.grid.size.y) * 0.35f);
		}
	}
	return 2.0f;
}

static void PaintPencilSegment(EditorState& e, F2 from, F2 to) {
	const float distance{ Distance(from, to) };
	if (distance <= 0.001f) {
		return;
	}
	const float step{ PencilSampleStep(e) };
	const int samples{ std::max(1, static_cast<int>(std::ceil(distance / step))) };
	for (int i{ 1 }; i <= samples; ++i) {
		const float t{ static_cast<float>(i) / static_cast<float>(samples) };
		PaintAt(e, Lerp(from, to, t));
	}
}

static void HandleViewportInput(EditorState& e, F2 mouse_world) {
	ImGuiIO& io{ ImGui::GetIO() };

	if (e.pending_generator) {
		// Pending generators are live recipe-driven content. Brush generators are
		// deliberately multi-stroke: new Brush strokes keep accumulating until the
		// user presses Enter/checkmark to finish or Escape to discard the generator.
		SyncGeneratorRecipeFromPalette(e, *e.pending_generator);
		const Tool pending_tool{
			e.pending_generator->geometry == GeneratorGeometryKind::BrushStroke ? Tool::Brush :
			e.pending_generator->geometry == GeneratorGeometryKind::Line ? Tool::Line :
			Tool::Rectangle
		};
		const bool pending_brush{ e.pending_generator->geometry == GeneratorGeometryKind::BrushStroke };
		if (e.pending_generator->layer_id != e.active_layer_id || e.tool != pending_tool) {
			CommitPendingGenerator(e);
		} else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
			CancelPendingGenerator(e);
			e.stroke = {};
			return;
		} else if (pending_brush && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
			e.pending_generator->recipe.commit_mode = PaintCommitMode::KeepGenerator;
			CommitPendingGenerator(e);
			e.stroke = {};
			return;
		} else if (!pending_brush && e.canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			CancelPendingGenerator(e);
			return;
		} else if (!pending_brush && e.canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			CommitPendingGenerator(e);
		}
	}

	if (
		e.stroke.active &&
		(e.tool == Tool::Line || e.tool == Tool::Rectangle) &&
		ImGui::IsKeyPressed(ImGuiKey_Escape, false)
	) {
		CancelStroke(e);
		return;
	}
	if (e.move.dragging && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
		if (e.move.before) RestoreScene(e, *e.move.before);
		const MoveSnapMode snap_mode{ e.move.snap_mode };
		e.move = {};
		e.move.snap_mode = snap_mode;
		return;
	}
	if (e.noise_boundary_drag.active) {
		HandleNoiseBoundaryInput(e, mouse_world);
		return;
	}
	if (!e.canvas_hovered) {
		if (e.stroke.active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			EndStroke(e, ToolName(e.tool));
		}
		return;
	}

	if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
		e.view_pan.x += io.MouseDelta.x;
		e.view_pan.y += io.MouseDelta.y;
	}
	if (std::abs(io.MouseWheel) > 0.001f) {
		const F2 before{ mouse_world };
		e.view_zoom = std::clamp(e.view_zoom * std::pow(1.12f, io.MouseWheel), 0.15f, 5.0f);
		const F2 after{ ScreenToWorld(e, { io.MousePos.x, io.MousePos.y }) };
		e.view_pan.x += (after.x - before.x) * e.view_zoom;
		e.view_pan.y += (after.y - before.y) * e.view_zoom;
	}

	const bool shift{ io.KeyShift };
	const bool ctrl{ io.KeyCtrl };

	if (SceneLayer* active = FindLayer(e, e.active_layer_id); active && active->kind == LayerKind::Noise) {
		HandleNoiseBoundaryInput(e, mouse_world);
		return;
	}

	if (e.tool == Tool::Move) {
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			if (!SelectionHitAtWorld(e, mouse_world)) {
				SelectClick(e, mouse_world, shift, ctrl);
			}
			if (HasSelection(e) && SelectionHitAtWorld(e, mouse_world)) {
				e.move.dragging = true;
				e.move.changed = false;
				e.move.start_world = mouse_world;
				e.move.current_world = mouse_world;
				e.move.before = CaptureScene(e);
			}
		}
		if (e.move.dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left) && e.move.before) {
			e.move.current_world = mouse_world;
			const bool default_free{ e.move.snap_mode == MoveSnapMode::Free };
			const bool free_movement{ ctrl ? !default_free : default_free };
			const F2 delta{ mouse_world - e.move.start_world };
			ApplyMoveFromSnapshot(e, *e.move.before, delta, free_movement);
			e.move.changed = Distance(e.move.start_world, mouse_world) > 0.01f;
		}
		if (e.move.dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (e.move.changed && e.move.before) {
				PushHistory(e, "Move Selection", std::move(*e.move.before));
			}
			const MoveSnapMode snap_mode{ e.move.snap_mode };
			e.move = {};
			e.move.snap_mode = snap_mode;
		}
		return;
	}

	if (e.tool == Tool::Select && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		DeselectAll(e);
		e.stroke = {};
		return;
	}

	if (e.tool == Tool::Select && e.brush.select_mode == SelectMode::ClickMarquee) {
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			BeginStroke(e, mouse_world, "Selection");
		}
		if (e.stroke.active) {
			e.stroke.current_world = mouse_world;
		}
		if (e.stroke.active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (Distance(e.stroke.start_world, mouse_world) < 4.0f / e.view_zoom) {
				SelectClick(e, mouse_world, shift, ctrl);
			} else {
				const F2 selection_end{ shift
					? ConstrainSquareDrag(e.stroke.start_world, mouse_world)
					: mouse_world };
				SelectMarquee(e, e.stroke.start_world, selection_end, shift, ctrl);
			}
			e.stroke = {};
		}
		return;
	}

	if (e.tool == Tool::Select && e.brush.select_mode == SelectMode::Brush) {
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			SelectBrush(e, mouse_world, ctrl);
		}
		return;
	}

	if (e.tool == Tool::Eyedropper) {
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			Eyedrop(e, mouse_world);
		}
		return;
	}

	if (e.tool == Tool::Fill) {
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			BeginStroke(e, mouse_world, "Fill");
			FloodFill(e, mouse_world);
			EndStroke(e, "Flood Fill");
		}
		return;
	}

	if (e.tool == Tool::Pencil) {
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			BeginStroke(e, mouse_world, "Pencil");
			PaintAt(e, mouse_world);
		}
		if (e.stroke.active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			PaintPencilSegment(e, e.stroke.last_world, mouse_world);
			e.stroke.last_world = mouse_world;
			e.stroke.current_world = mouse_world;
		}
		if (e.stroke.active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			EndStroke(e, "Pencil Paint");
		}
		return;
	}

	if (e.tool == Tool::Brush || e.tool == Tool::Erase) {
		const bool keep_generator{
			e.tool == Tool::Brush &&
			e.brush.operation == BrushOperation::Paint &&
			e.recipe.commit_mode == PaintCommitMode::KeepGenerator
		};

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			BeginStroke(e, mouse_world, e.tool == Tool::Brush ? "Brush" : "Erase");
			if (keep_generator) {
				if (!e.pending_generator || e.pending_generator->geometry != GeneratorGeometryKind::BrushStroke) {
					e.pending_generator = MakeGeneratorFromCurrentStroke(
						e,
						GeneratorGeometryKind::BrushStroke,
						mouse_world,
						-1
					);
					e.pending_generator->recipe.commit_mode = PaintCommitMode::KeepGenerator;
					e.pending_generator_before = e.stroke.before
						? e.stroke.before
						: std::optional<SceneSnapshot>{ CaptureScene(e) };
				} else {
					SyncGeneratorRecipeFromPalette(e, *e.pending_generator);
					AppendGeneratorBrushSample(*e.pending_generator, mouse_world, true);
				}
			} else if (e.tool == Tool::Brush) {
				PaintAt(e, mouse_world);
			} else {
				EraseAt(e, mouse_world);
			}
			e.stroke.last_world = mouse_world;
		}

		if (e.stroke.active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			e.stroke.current_world = mouse_world;
			const float distance{ Distance(e.stroke.last_world, mouse_world) };
			const RasterGrid stroke_grid{ ActiveRasterGrid(e) };
			const float spacing{
				std::max(1.0f, std::min(stroke_grid.size.x, stroke_grid.size.y) * 0.45f)
			};
			if (distance >= 0.001f) {
				const int samples{ std::max(1, static_cast<int>(std::ceil(distance / spacing))) };
				for (int i{ 1 }; i <= samples; ++i) {
					const F2 sample{
						Lerp(
							e.stroke.last_world,
							mouse_world,
							static_cast<float>(i) / static_cast<float>(samples)
						)
					};
					if (keep_generator && e.pending_generator) {
						AppendGeneratorBrushSample(*e.pending_generator, sample, false);
					} else if (e.tool == Tool::Brush) {
						PaintAt(e, sample);
					} else {
						EraseAt(e, sample);
					}
				}
				e.stroke.last_world = mouse_world;
			}
		}

		if (e.stroke.active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (keep_generator) {
				// The pending generator remains live. Do not push history until Enter /
				// the finish checkmark turns the accumulated strokes into one generator.
				e.stroke = {};
			} else {
				EndStroke(e, e.tool == Tool::Brush ? "Brush Paint" : "Erase");
			}
		}
		return;
	}

	if (e.tool == Tool::Line || e.tool == Tool::Rectangle) {
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			BeginStroke(e, mouse_world, e.tool == Tool::Line ? "Line" : "Rectangle");
		}
		if (e.stroke.active) {
			e.stroke.current_world = mouse_world;
		}
		if (e.stroke.active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			// Paint operations stay as a temporary live procedural shape after release,
			// matching Paint.NET-style shape editing. Paint Palette changes continue
			// updating the result until the next scene action commits it. Replace and
			// Exclusion Mask are destructive operations and therefore apply immediately.
			if (e.brush.operation == BrushOperation::Paint) {
				BeginPendingGeneratorFromCurrentStroke(
					e,
					e.tool == Tool::Line ? GeneratorGeometryKind::Line : GeneratorGeometryKind::Rectangle,
					mouse_world
				);
			} else {
				if (e.tool == Tool::Line) ApplyLine(e, e.stroke.start_world, mouse_world);
				else ApplyArea(e, e.stroke.start_world, mouse_world);
				EndStroke(e, e.tool == Tool::Line ? "Line Paint" : "Rectangle Paint");
			}
		}
	}
}

static void DrawViewport(EditorState& e) {
	ImGui::Begin("Viewport");
	DrawViewportToolbar(e);
	const SceneLayer* viewport_layer{ FindLayer(e, e.active_layer_id) };
	const bool has_contextual_tool_options{
		viewport_layer &&
		viewport_layer->kind != LayerKind::Noise &&
		e.tool != Tool::Fill &&
		e.tool != Tool::Eyedropper
	};

	if (has_contextual_tool_options) {
		// Keep contextual controls dense. Do not insert NewLine() here: after the
		// final toolbar item ImGui's cursor is already positioned on the next row.
		const ImGuiStyle& style{ ImGui::GetStyle() };
		ImGui::PushStyleVar(
			ImGuiStyleVar_ItemSpacing,
			ImVec2{ std::max(3.0f, style.ItemSpacing.x * 0.65f),
					std::max(1.0f, style.ItemSpacing.y * 0.5f) }
		);
		ImGui::PushStyleVar(
			ImGuiStyleVar_FramePadding,
			ImVec2{ style.FramePadding.x, std::max(1.0f, style.FramePadding.y - 1.0f) }
		);
		DrawBrushSettings(e);
		ImGui::PopStyleVar(2);
	}
	ImGui::Separator();

	ImVec2 avail{ ImGui::GetContentRegionAvail() };
	avail.x = std::max(avail.x, 64.0f);
	avail.y = std::max(avail.y, 64.0f);
	const ImVec2 canvas_min{ ImGui::GetCursorScreenPos() };
	const ImVec2 canvas_max{ canvas_min.x + avail.x, canvas_min.y + avail.y };
	e.canvas_screen_min = { canvas_min.x, canvas_min.y };
	e.canvas_screen_max = { canvas_max.x, canvas_max.y };
	ImGui::InvisibleButton("##viewport_canvas", avail, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	e.canvas_hovered = ImGui::IsItemHovered();

	ImDrawList* dl{ ImGui::GetWindowDrawList() };
	dl->PushClipRect(canvas_min, canvas_max, true);
	dl->AddRectFilled(canvas_min, canvas_max, ImGui::GetColorU32(ImVec4(0.08f, 0.09f, 0.11f, 1.0f)));

	// DrawGeneratorsForLayer reads the pending generator by reference, avoiding a
	// per-frame copy of large Brush cell caches.

	for (auto& layer : e.layers) {
		if (layer.kind == LayerKind::Tile) {
			UpdateStreamingForLayer(e, layer);
			DrawGeneratorsForLayer(e, layer, dl);
			DrawTileLayer(e, layer, dl);
		} else if (layer.kind == LayerKind::Entity) {
			DrawGeneratorsForLayer(e, layer, dl);
			DrawEntityLayer(e, layer, dl);
		} else if (layer.kind == LayerKind::Noise) {
			// Legacy compatibility only. New procedural content is a generator inside
			// a normal Tile/Entity layer, so the editor no longer creates Noise layers.
			DrawNoiseLayer(e, layer, dl);
		}
	}


	// Grid lines are intentionally composited after every scene/noise layer so
	// they remain readable regardless of layer content.
	DrawGrid(e, dl);
	DrawActiveNoiseBoundaryHandles(e, dl);

	const ImGuiIO& io{ ImGui::GetIO() };
	const F2 mouse_world{ ScreenToWorld(e, { io.MousePos.x, io.MousePos.y }) };
	DrawToolPreview(e, dl, mouse_world);

	const std::string status{ "World: " + std::to_string(static_cast<int>(mouse_world.x)) + ", " + std::to_string(static_cast<int>(mouse_world.y)) +
		"   Zoom: " + std::to_string(static_cast<int>(e.view_zoom * 100.0f)) + "%" };
	dl->AddText({ canvas_min.x + 8.0f, canvas_max.y - ImGui::GetTextLineHeight() - 6.0f }, ImGui::GetColorU32(ImVec4(1, 1, 1, 0.7f)), status.c_str());
	dl->PopClipRect();

	HandleViewportInput(e, mouse_world);
	ImGui::End();
}

static void DrawSceneHierarchy(EditorState& e) {
	ImGui::Begin("Scene Hierarchy");
	int delete_generator{ -1 };
	int bake_generator{ -1 };
	for (const auto& layer : e.layers) {
		ImGui::PushID(layer.id);
		const bool open{ ImGui::TreeNodeEx(layer.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen) };
		if (ImGui::IsItemClicked()) {
			e.active_layer_id = layer.id;
			e.selected_generator_id = -1;
		}
		if (open) {
			bool has_generators{};
			for (const auto& generator : e.generators) if (generator.layer_id == layer.id) { has_generators = true; break; }
			if (has_generators && ImGui::TreeNodeEx("Generators", ImGuiTreeNodeFlags_DefaultOpen)) {
				for (const auto& generator : e.generators) {
					if (generator.layer_id != layer.id) continue;
					ImGui::PushID(generator.id);
					const bool selected{ e.selected_generator_id == generator.id };
					if (ImGui::Selectable(generator.name.c_str(), selected)) {
						SelectGenerator(e, generator.id);
					}
					if (ImGui::BeginPopupContextItem("GeneratorContext")) {
						const bool infinite{ generator.geometry == GeneratorGeometryKind::Infinite };
						ImGui::BeginDisabled(infinite || layer.locked);
						if (ImGui::MenuItem("Bake Generator")) bake_generator = generator.id;
						ImGui::EndDisabled();
						if (infinite && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Infinite generators cannot be baked globally. Bake a finite authored region instead.");
						if (ImGui::MenuItem("Delete Generator")) delete_generator = generator.id;
						ImGui::EndPopup();
					}
					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			if (layer.kind == LayerKind::Entity) {
				for (const auto& entity : e.entities) {
					if (entity.layer_id != layer.id) continue;
					const bool selected{ e.selected_entities.contains(entity.id) };
					ImGui::PushID(entity.id);
					if (ImGui::Selectable(entity.prefab.c_str(), selected)) {
						e.selected_generator_id = -1;
						if (!ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) e.selected_entities.clear();
						if (ImGui::GetIO().KeyCtrl && selected) e.selected_entities.erase(entity.id);
						else { e.selected_entities.insert(entity.id); e.primary_entity_id = entity.id; }
					}
					ImGui::PopID();
				}
			} else if (layer.kind == LayerKind::Tile) {
				ImGui::TextDisabled("Manual tiles are edited in the viewport; persistent generators are listed above.");
			} else {
				ImGui::TextDisabled("Legacy Noise layer (compatibility only).");
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (bake_generator >= 0) BakeGenerator(e, bake_generator);
	if (delete_generator >= 0) {
		const SceneSnapshot before{ CaptureScene(e) };
		e.generators.erase(std::remove_if(e.generators.begin(), e.generators.end(), [&](const PaintGenerator& generator) { return generator.id == delete_generator; }), e.generators.end());
		if (e.selected_generator_id == delete_generator) e.selected_generator_id = -1;
		PushHistory(e, "Delete Generator", before);
	}
	ImGui::End();
}

static int LayerIndex(const EditorState& e, int id) {
	for (int i{}; i < static_cast<int>(e.layers.size()); ++i) {
		if (e.layers[static_cast<std::size_t>(i)].id == id) return i;
	}
	return -1;
}

static bool CanMergeLayerDown(const EditorState& e, int upper_id) {
	const int index{ LayerIndex(e, upper_id) };
	if (index <= 0) return false;
	const auto& upper{ e.layers[static_cast<std::size_t>(index)] };
	const auto& lower{ e.layers[static_cast<std::size_t>(index - 1)] };
	if (upper.kind != lower.kind) return false;
	if (upper.kind == LayerKind::Tile) {
		return upper.tile.tilemap_id == lower.tile.tilemap_id;
	}
	if (upper.kind == LayerKind::Noise) {
		if (upper.noise.target != lower.noise.target) return false;
		const auto same_axis = [](float a, float b) {
			return std::abs(a - b) <= 0.001f;
		};
		return same_axis(upper.noise.grid_size.x, lower.noise.grid_size.x) &&
			same_axis(upper.noise.grid_size.y, lower.noise.grid_size.y) &&
			same_axis(upper.noise.grid_offset.x, lower.noise.grid_offset.x) &&
			same_axis(upper.noise.grid_offset.y, lower.noise.grid_offset.y);
	}
	return true;
}

static void MergeLayerDown(EditorState& e, int upper_id, LayerMergeMode mode) {
	const int upper_index{ LayerIndex(e, upper_id) };
	if (upper_index <= 0 || !CanMergeLayerDown(e, upper_id)) return;
	const int lower_index{ upper_index - 1 };
	const SceneSnapshot before{ CaptureScene(e) };
	SceneLayer& upper{ e.layers[static_cast<std::size_t>(upper_index)] };
	SceneLayer& lower{ e.layers[static_cast<std::size_t>(lower_index)] };
	const int upper_layer_id{ upper.id };
	const int lower_layer_id{ lower.id };

	if (upper.kind == LayerKind::Tile) {
		const auto* map{ FindTilemap(e, upper.tile.tilemap_id) };
		if (!map) return;
		std::vector<std::pair<I2, TileCell>> upper_tiles;
		ForEachTileAnchor(upper, *map, [&](I2 cell, const TileCell& tile) {
			upper_tiles.push_back({ cell, tile });
		});
		if (mode == LayerMergeMode::ReplaceAll) {
			lower.tile.loaded_chunks.clear();
			lower.tile.backing_chunks.clear();
		}
		for (const auto& [cell, tile] : upper_tiles) {
			const auto* existing{ ReadTileCell(lower, *map, cell) };
			if (mode == LayerMergeMode::Add && existing && existing->tile_id >= 0) {
				continue;
			}
			*WriteTileCell(lower, *map, cell) = tile;
		}
		if (e.selected_tile_layer_id == upper_layer_id) {
			e.selected_tile_layer_id = lower_layer_id;
		}
	} else if (upper.kind == LayerKind::Entity) {
		std::vector<RectF> upper_bounds;
		for (const auto& entity : e.entities) {
			if (entity.layer_id == upper_layer_id) upper_bounds.push_back(EntityBounds(entity));
		}
		if (mode == LayerMergeMode::ReplaceAll) {
			e.entities.erase(
				std::remove_if(e.entities.begin(), e.entities.end(), [&](const Entity& entity) {
					return entity.layer_id == lower_layer_id;
				}),
				e.entities.end()
			);
		} else if (mode == LayerMergeMode::ReplaceConflicts) {
			e.entities.erase(
				std::remove_if(e.entities.begin(), e.entities.end(), [&](const Entity& entity) {
					if (entity.layer_id != lower_layer_id) return false;
					const RectF bounds{ EntityBounds(entity) };
					return std::any_of(upper_bounds.begin(), upper_bounds.end(), [&](const RectF& other) {
						return RectsOverlap(bounds, other);
					});
				}),
				e.entities.end()
			);
		}
		for (auto& entity : e.entities) {
			if (entity.layer_id == upper_layer_id) entity.layer_id = lower_layer_id;
		}
	} else if (upper.kind == LayerKind::Noise) {
		if (mode == LayerMergeMode::ReplaceAll) {
			lower.noise = upper.noise;
		} else if (mode == LayerMergeMode::Add) {
			lower.noise.fields.insert(
				lower.noise.fields.end(),
				upper.noise.fields.begin(),
				upper.noise.fields.end()
			);
			lower.noise.show_noise_preview = lower.noise.show_noise_preview || upper.noise.show_noise_preview;
			lower.noise.show_generated_preview = lower.noise.show_generated_preview || upper.noise.show_generated_preview;
		} else {
			for (const auto& field : upper.noise.fields) {
				auto found = std::find_if(lower.noise.fields.begin(), lower.noise.fields.end(), [&](const NoiseField& candidate) {
					return candidate.name == field.name;
				});
				if (found != lower.noise.fields.end()) *found = field;
				else lower.noise.fields.push_back(field);
			}
		}
	}

	for (auto& generator : e.generators) {
		if (generator.layer_id == upper_id) generator.layer_id = lower_layer_id;
	}
	e.layers.erase(e.layers.begin() + upper_index);
	e.active_layer_id = lower_layer_id;
	PushHistory(e, "Merge Layer Down", before);
}

static bool LayerIconButton(const char* id, bool& value, bool eye_icon) {
	const ImVec2 size{ 18.0f, 18.0f };
	const ImVec2 p0{ ImGui::GetCursorScreenPos() };
	ImGui::InvisibleButton(id, size);
	const bool clicked{ ImGui::IsItemClicked() };
	if (clicked) value = !value;
	ImDrawList* dl{ ImGui::GetWindowDrawList() };
	const ImU32 c{ ImGui::GetColorU32(value ? ImGuiCol_Text : ImGuiCol_TextDisabled) };
	if (eye_icon) {
		const ImVec2 center{ p0.x + 9.0f, p0.y + 9.0f };
		dl->AddLine({ p0.x + 2.0f, p0.y + 9.0f }, { p0.x + 6.0f, p0.y + 5.0f }, c, 1.5f);
		dl->AddLine({ p0.x + 6.0f, p0.y + 5.0f }, { p0.x + 12.0f, p0.y + 5.0f }, c, 1.5f);
		dl->AddLine({ p0.x + 12.0f, p0.y + 5.0f }, { p0.x + 16.0f, p0.y + 9.0f }, c, 1.5f);
		dl->AddLine({ p0.x + 16.0f, p0.y + 9.0f }, { p0.x + 12.0f, p0.y + 13.0f }, c, 1.5f);
		dl->AddLine({ p0.x + 12.0f, p0.y + 13.0f }, { p0.x + 6.0f, p0.y + 13.0f }, c, 1.5f);
		dl->AddLine({ p0.x + 6.0f, p0.y + 13.0f }, { p0.x + 2.0f, p0.y + 9.0f }, c, 1.5f);
		if (value) dl->AddCircleFilled(center, 2.3f, c, 8);
		else dl->AddLine({ p0.x + 3.0f, p0.y + 15.0f }, { p0.x + 15.0f, p0.y + 3.0f }, c, 1.5f);
	} else {
		// Lock icon: closed when true, open when false.
		dl->AddRect({ p0.x + 5.0f, p0.y + 8.0f }, { p0.x + 14.0f, p0.y + 15.0f }, c, 1.0f, 0, 1.5f);
		if (value) {
			dl->AddLine({ p0.x + 7.0f, p0.y + 8.0f }, { p0.x + 7.0f, p0.y + 5.0f }, c, 1.5f);
			dl->AddLine({ p0.x + 7.0f, p0.y + 5.0f }, { p0.x + 12.0f, p0.y + 5.0f }, c, 1.5f);
			dl->AddLine({ p0.x + 12.0f, p0.y + 5.0f }, { p0.x + 12.0f, p0.y + 8.0f }, c, 1.5f);
		} else {
			dl->AddLine({ p0.x + 7.0f, p0.y + 8.0f }, { p0.x + 7.0f, p0.y + 5.0f }, c, 1.5f);
			dl->AddLine({ p0.x + 7.0f, p0.y + 5.0f }, { p0.x + 11.0f, p0.y + 4.0f }, c, 1.5f);
		}
	}
	return clicked;
}

static void DrawLayerKindIcon(LayerKind kind) {
	const ImVec2 size{ 18.0f, 18.0f };
	const ImVec2 p0{ ImGui::GetCursorScreenPos() };
	ImGui::Dummy(size);
	ImDrawList* dl{ ImGui::GetWindowDrawList() };
	const ImU32 c{ ImGui::GetColorU32(ImGuiCol_TextDisabled) };
	if (kind == LayerKind::Tile) {
		for (int y{}; y < 2; ++y) for (int x{}; x < 2; ++x) {
			dl->AddRect({ p0.x + 3.0f + x * 6.0f, p0.y + 3.0f + y * 6.0f }, { p0.x + 8.0f + x * 6.0f, p0.y + 8.0f + y * 6.0f }, c);
		}
	} else if (kind == LayerKind::Entity) {
		dl->AddQuad({ p0.x + 9.0f, p0.y + 2.0f }, { p0.x + 15.0f, p0.y + 6.0f }, { p0.x + 9.0f, p0.y + 10.0f }, { p0.x + 3.0f, p0.y + 6.0f }, c, 1.5f);
		dl->AddLine({ p0.x + 3.0f, p0.y + 6.0f }, { p0.x + 3.0f, p0.y + 12.0f }, c, 1.5f);
		dl->AddLine({ p0.x + 15.0f, p0.y + 6.0f }, { p0.x + 15.0f, p0.y + 12.0f }, c, 1.5f);
		dl->AddLine({ p0.x + 3.0f, p0.y + 12.0f }, { p0.x + 9.0f, p0.y + 16.0f }, c, 1.5f);
		dl->AddLine({ p0.x + 15.0f, p0.y + 12.0f }, { p0.x + 9.0f, p0.y + 16.0f }, c, 1.5f);
		dl->AddLine({ p0.x + 9.0f, p0.y + 10.0f }, { p0.x + 9.0f, p0.y + 16.0f }, c, 1.5f);
	} else {
		for (int i{}; i < 3; ++i) {
			const float yy{ p0.y + 5.0f + i * 4.0f };
			dl->AddLine({ p0.x + 2.0f, yy }, { p0.x + 6.0f, yy - 2.0f }, c, 1.2f);
			dl->AddLine({ p0.x + 6.0f, yy - 2.0f }, { p0.x + 11.0f, yy + 2.0f }, c, 1.2f);
			dl->AddLine({ p0.x + 11.0f, yy + 2.0f }, { p0.x + 16.0f, yy }, c, 1.2f);
		}
	}
	ItemTooltip(kind == LayerKind::Tile ? "Tile layer" : kind == LayerKind::Entity ? "Entity layer" : "Procedural noise layer");
}

static void DrawLayers(EditorState& e) {
	ImGui::Begin("Layers");
	ImGui::TextDisabled("Top renders last. Click a row to make it active. Drag rows to reorder.");
	int move_source_id{ -1 };
	int move_target_id{ -1 };
	int delete_id{ -1 };
	int duplicate_id{ -1 };
	static int merge_id{ -1 };
	static int merge_mode_index{};
	static bool merge_popup_requested{};
	static int editing_layer_id{ -1 };
	static int focus_layer_id{ -1 };
	static std::string original_layer_name;
	static std::optional<SceneSnapshot> layer_rename_before;

	for (int i{ static_cast<int>(e.layers.size()) - 1 }; i >= 0; --i) {
		auto& layer{ e.layers[static_cast<std::size_t>(i)] };
		ImGui::PushID(layer.id);
		LayerIconButton("##visible", layer.visible, true);
		ItemTooltip(layer.visible ? "Layer is visible. Click the eye to hide it." : "Layer is hidden. Click to show it.");
		ImGui::SameLine();
		LayerIconButton("##locked", layer.locked, false);
		ItemTooltip(layer.locked ? "Layer is locked against editing. Click to unlock it." : "Layer is editable. Click to lock it.");
		ImGui::SameLine();
		DrawLayerKindIcon(layer.kind);
		ImGui::SameLine();

		bool begin_rename{};
		bool commit_rename{};
		bool cancel_rename{};
		const bool editing{ editing_layer_id == layer.id };
		if (editing) {
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (focus_layer_id == layer.id) {
				ImGui::SetKeyboardFocusHere();
				focus_layer_id = -1;
			}
			const bool submitted{ ImGui::InputText(
				"##layer_name",
				&layer.name,
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
			) };
			if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
				cancel_rename = true;
			} else if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
				commit_rename = true;
			}
		} else {
			const std::string label{ layer.name + "##row" };
			if (ImGui::Selectable(
					label.c_str(),
					layer.id == e.active_layer_id,
					0,
					{ 0.0f, ImGui::GetFrameHeight() }
				)) {
				e.active_layer_id = layer.id;
			}
			ItemTooltip("Make this the active layer. Right click for layer options.");
		}

		if (!editing && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			const int payload_id{ layer.id };
			ImGui::SetDragDropPayload("SCENE_LAYER_ID", &payload_id, sizeof(payload_id));
			ImGui::Text("Move %s", layer.name.c_str());
			ImGui::EndDragDropSource();
		}
		if (!editing && ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_LAYER_ID")) {
				if (payload->DataSize == sizeof(int)) {
					move_source_id = *static_cast<const int*>(payload->Data);
					move_target_id = layer.id;
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (!editing && ImGui::BeginPopupContextItem("LayerContext")) {
			const bool noise_properties_locked{ layer.kind == LayerKind::Noise && layer.locked };
			ImGui::BeginDisabled(noise_properties_locked);
			if (ImGui::MenuItem("Rename")) {
				begin_rename = true;
			}
			ImGui::EndDisabled();
			if (ImGui::MenuItem("Move Up") && i + 1 < static_cast<int>(e.layers.size())) {
				move_source_id = layer.id;
				move_target_id = e.layers[static_cast<std::size_t>(i + 1)].id;
			}
			if (ImGui::MenuItem("Move Down") && i > 0) {
				move_source_id = layer.id;
				move_target_id = e.layers[static_cast<std::size_t>(i - 1)].id;
			}
			ImGui::Separator();
			ImGui::BeginDisabled(noise_properties_locked);
			ImGui::MenuItem("Selectable", nullptr, &layer.selectable);
			ImGui::EndDisabled();
			if (ImGui::MenuItem("Solo")) {
				for (auto& other : e.layers) {
					other.visible = other.id == layer.id;
				}
			}

			if (layer.kind == LayerKind::Tile) {
				auto* map{ FindTilemap(e, layer.tile.tilemap_id) };
				const bool has_exclusion_mask{ map && !map->exclusion_mask.empty() };
				const bool can_clear_exclusion_mask{ has_exclusion_mask && !layer.locked };

				ImGui::Separator();
				ImGui::BeginDisabled(!can_clear_exclusion_mask);
				if (ImGui::MenuItem("Clear Exclusion Mask")) {
					auto before{ CaptureScene(e) };
					map->exclusion_mask.clear();
					PushHistory(e, "Clear Exclusion Mask", std::move(before));
				}
				ImGui::EndDisabled();

				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
					if (!map) {
						ImGui::SetTooltip("This tile layer has no valid tilemap.");
					} else if (layer.locked) {
						ImGui::SetTooltip("Unlock this tile layer before clearing its exclusion mask.");
					} else if (!has_exclusion_mask) {
						ImGui::SetTooltip("The exclusion mask is already empty.");
					} else {
						ImGui::SetTooltip("Clear every excluded cell from this tilemap. This action is undoable.");
					}
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Duplicate")) {
				duplicate_id = layer.id;
			}
			const bool merge_enabled{ CanMergeLayerDown(e, layer.id) };
			ImGui::BeginDisabled(!merge_enabled);
			if (ImGui::MenuItem("Merge Down...")) {
				merge_id = layer.id;
				merge_mode_index = 0;
				merge_popup_requested = true;
			}
			ImGui::EndDisabled();
			if (!merge_enabled && ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Merge Down requires an immediately lower compatible layer of the same type/grid.");
			}
			if (ImGui::MenuItem("Delete")) {
				delete_id = layer.id;
			}
			ImGui::EndPopup();
		}

		if (begin_rename) {
			editing_layer_id = layer.id;
			focus_layer_id = layer.id;
			original_layer_name = layer.name;
			layer_rename_before = CaptureScene(e);
		}
		if (cancel_rename) {
			layer.name = original_layer_name;
			editing_layer_id = -1;
			focus_layer_id = -1;
			layer_rename_before.reset();
		} else if (commit_rename) {
			if (layer.name.empty()) {
				layer.name = "Layer";
			}
			const std::string base{ layer.name };
			std::string unique{ base };
			int suffix{ 2 };
			while (std::ranges::any_of(e.layers, [&](const SceneLayer& candidate) {
				return candidate.id != layer.id && candidate.name == unique;
			})) {
				unique = base + " " + std::to_string(suffix++);
			}
			layer.name = std::move(unique);
			if (layer_rename_before && layer.name != original_layer_name) {
				PushHistory(e, "Rename Layer", std::move(*layer_rename_before));
			}
			editing_layer_id = -1;
			focus_layer_id = -1;
			layer_rename_before.reset();
		}
		ImGui::PopID();
	}

	if (merge_popup_requested) {
		ImGui::OpenPopup("Merge Layer Down");
		merge_popup_requested = false;
	}

	if (ImGui::BeginPopupModal("Merge Layer Down", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		const int index{ LayerIndex(e, merge_id) };
		if (index > 0 && CanMergeLayerDown(e, merge_id)) {
			const auto& upper{ e.layers[static_cast<std::size_t>(index)] };
			const auto& lower{ e.layers[static_cast<std::size_t>(index - 1)] };
			ImGui::Text("Merge '%s' down into '%s'", upper.name.c_str(), lower.name.c_str());
			const char* modes[]{ "Add / preserve lower conflicts", "Replace conflicts", "Replace lower entirely" };
			ImGui::SetNextItemWidth(240.0f);
			ImGui::Combo("Mode", &merge_mode_index, modes, 3);
			ItemTooltip(
				"Add: keep lower content when both layers occupy the same location. Noise fields are appended.\n"
				"Replace conflicts: upper content wins only where it conflicts; same-named noise fields are replaced.\n"
				"Replace lower entirely: clear the lower layer before merging the upper layer into it."
			);
			if (ImGui::Button("Merge")) {
				MergeLayerDown(e, merge_id, static_cast<LayerMergeMode>(merge_mode_index));
				merge_id = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				merge_id = -1;
				ImGui::CloseCurrentPopup();
			}
		} else {
			ImGui::TextDisabled("The layers are no longer compatible for merging.");
			if (ImGui::Button("Close")) {
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}

	if (move_source_id >= 0 && move_target_id >= 0 && move_source_id != move_target_id) {
		const auto before{ CaptureScene(e) };
		auto src = std::find_if(e.layers.begin(), e.layers.end(), [&](const SceneLayer& l) { return l.id == move_source_id; });
		auto dst = std::find_if(e.layers.begin(), e.layers.end(), [&](const SceneLayer& l) { return l.id == move_target_id; });
		if (src != e.layers.end() && dst != e.layers.end()) {
			const std::size_t dst_index{ static_cast<std::size_t>(std::distance(e.layers.begin(), dst)) };
			SceneLayer moved{ std::move(*src) };
			const std::size_t src_index{ static_cast<std::size_t>(std::distance(e.layers.begin(), src)) };
			e.layers.erase(e.layers.begin() + static_cast<std::ptrdiff_t>(src_index));
			// Move to the target layer's original index. When moving upward (toward a
			// higher index), erasing the source shifts the target down by one, so
			// inserting at dst_index places the moved layer immediately above it.
			const std::size_t adjusted{ std::min(dst_index, e.layers.size()) };
			e.layers.insert(e.layers.begin() + static_cast<std::ptrdiff_t>(adjusted), std::move(moved));
			PushHistory(e, "Reorder Layer", before);
		}
	}

	if (duplicate_id >= 0) {
		if (const SceneLayer* source = FindLayer(e, duplicate_id)) {
			const auto before{ CaptureScene(e) };
			SceneLayer copy{ *source };
			copy.id = e.next_layer_id++;
			copy.name += " Copy";
			if (copy.kind == LayerKind::Entity) {
				std::vector<Entity> copies;
				for (const auto& entity : e.entities) {
					if (entity.layer_id != source->id) continue;
					Entity cloned{ entity };
					cloned.id = e.next_entity_id++;
					cloned.layer_id = copy.id;
					copies.push_back(std::move(cloned));
				}
				e.entities.insert(e.entities.end(), copies.begin(), copies.end());
			}
			std::vector<PaintGenerator> generator_copies;
			for (const auto& generator : e.generators) {
				if (generator.layer_id != source->id) continue;
				PaintGenerator cloned{ generator };
				cloned.id = e.next_generator_id++;
				cloned.layer_id = copy.id;
				cloned.name += " Copy";
				generator_copies.push_back(std::move(cloned));
			}
			e.generators.insert(e.generators.end(), generator_copies.begin(), generator_copies.end());
			e.layers.push_back(std::move(copy));
			e.active_layer_id = e.layers.back().id;
			PushHistory(e, "Duplicate Layer", before);
		}
	}

	if (delete_id >= 0 && e.layers.size() > 1) {
		const auto before{ CaptureScene(e) };
		for (const auto& entity : e.entities) {
			if (entity.layer_id == delete_id) e.selected_entities.erase(entity.id);
		}
		if (!e.selected_entities.contains(e.primary_entity_id)) e.primary_entity_id = -1;
		if (e.selected_tile_layer_id == delete_id) DeselectAll(e);
		e.entities.erase(std::remove_if(e.entities.begin(), e.entities.end(), [&](const Entity& entity) {
			return entity.layer_id == delete_id;
		}), e.entities.end());
		e.generators.erase(std::remove_if(e.generators.begin(), e.generators.end(), [&](const PaintGenerator& generator) {
			return generator.layer_id == delete_id;
		}), e.generators.end());
		if (!FindGenerator(e, e.selected_generator_id)) e.selected_generator_id = -1;
		e.layers.erase(std::remove_if(e.layers.begin(), e.layers.end(), [&](const SceneLayer& layer) {
			return layer.id == delete_id;
		}), e.layers.end());
		if (editing_layer_id == delete_id) {
			editing_layer_id = -1;
			focus_layer_id = -1;
			layer_rename_before.reset();
		}
		if (!FindLayer(e, e.active_layer_id) && !e.layers.empty()) e.active_layer_id = e.layers.back().id;
		PushHistory(e, "Delete Layer", before);
	}

	ImGui::Separator();
	if (ImGui::Button("+ Entity")) {
		const auto before{ CaptureScene(e) };
		SceneLayer layer;
		layer.id = e.next_layer_id++;
		layer.name = "Entity Layer " + std::to_string(layer.id);
		layer.kind = LayerKind::Entity;
		e.layers.push_back(layer);
		e.active_layer_id = layer.id;
		PushHistory(e, "Add Entity Layer", before);
	}
	ItemTooltip("Add an ordinary ECS/prefab entity layer.");
	ImGui::SameLine();
	if (ImGui::Button("+ Tile")) {
		const auto before{ CaptureScene(e) };
		if (e.tilemaps.empty()) {
			Tilemap map; map.id = e.next_tilemap_id++; e.tilemaps.push_back(map);
		}
		SceneLayer layer;
		layer.id = e.next_layer_id++;
		layer.name = "Tile Layer " + std::to_string(layer.id);
		layer.kind = LayerKind::Tile;
		layer.tile.tilemap_id = e.tilemaps.front().id;
		e.layers.push_back(layer);
		e.active_layer_id = layer.id;
		PushHistory(e, "Add Tile Layer", before);
	}
	ItemTooltip("Add a chunk-backed tile layer using the project's tilemap grid.");

	if (SceneLayer* active = FindLayer(e, e.active_layer_id)) {
		if (active->kind != LayerKind::Noise) {
			ImGui::Separator();
			int purpose{ static_cast<int>(active->purpose) };
			const char* purposes[]{ "Visual", "Collision", "Navigation", "Metadata" };
			if (ImGui::Combo("Purpose", &purpose, purposes, 4)) {
				active->purpose = static_cast<LayerPurpose>(purpose);
			}
			ItemTooltip("Optional semantic purpose for this layer. Collision/Navigation/Metadata can be interpreted specially by the engine.");
		}
	}
	ImGui::End();
}


static void DrawPaletteTile(EditorState& e, const TileDefinition& tile, bool selected, bool in_stamp) {
	// Keep the thumbnail shorter than its reserved table row so it can never
	// bleed into the palette tree header above or the next palette row below.
	const float size{ 48.0f };
	const ImVec2 p0{ ImGui::GetCursorScreenPos() };
	ImGui::InvisibleButton(("##tile" + std::to_string(tile.id)).c_str(), { size, size });
	const bool hovered{ ImGui::IsItemHovered() };
	const bool clicked{ ImGui::IsItemClicked() };
	ImDrawList* dl{ ImGui::GetWindowDrawList() };
	if (tile.texture_index >= 0) {
		const auto& tex{ e.textures[static_cast<std::size_t>(tile.texture_index)] };
		dl->AddImage((ImTextureID)(intptr_t)tex.handle, p0, { p0.x + size, p0.y + size }, tile.uv0, tile.uv1);
	}
	const ImU32 outline{ ImGui::GetColorU32(selected ? ImVec4(1.0f, 0.75f, 0.1f, 1.0f) : in_stamp ? ImVec4(0.25f, 0.85f, 1.0f, 1.0f) : ImVec4(1, 1, 1, hovered ? 0.6f : 0.2f)) };
	dl->AddRect(p0, { p0.x + size, p0.y + size }, outline, 2.0f, 0, selected || in_stamp ? 3.0f : 1.0f);
	if (clicked) {
		e.active_tile_id = tile.id;
		if (ImGui::GetIO().KeyCtrl) {
			if (auto it = std::find(e.stamp_tiles.begin(), e.stamp_tiles.end(), tile.id); it != e.stamp_tiles.end()) {
				e.stamp_tiles.erase(it);
			} else {
				e.stamp_tiles.push_back(tile.id);
			}
		} else {
			e.stamp_tiles.clear();
		}
	}
	if (hovered) {
		ImGui::SetTooltip("%s\nCtrl-click: add/remove from stamp", tile.name.c_str());
	}
}

static void DrawImportPopup(EditorState& e) {
	if (e.importer.open_popup) {
		ImGui::OpenPopup("Import Tiles");
		e.importer.open_popup = false;
	}
	if (!ImGui::BeginPopupModal("Import Tiles", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		return;
	}

	char path[1024]{};
	std::snprintf(path, sizeof(path), "%s", e.importer.path.c_str());
	if (ImGui::InputText("Source", path, sizeof(path))) {
		e.importer.path = path;
	}
	ItemTooltip("Image, Tiled .tsx/.tsj, or Tiled tileset .json file to import.");
	ImGui::TextDisabled("Supports images, Tiled .tsx/.tsj, and Tiled tileset .json files.");
	ImGui::TextDisabled("Auto: *_16x16.png is sliced as 16x16; an image without a size postfix is one native-size tile.");

	const char* import_modes[]{ "Auto Detect", "Tileset / Sprite Sheet", "Individual Tile" };
	int mode{ static_cast<int>(e.importer.mode) };
	ImGui::SetNextItemWidth(190.0f);
	if (ImGui::Combo("Import As", &mode, import_modes, 3)) {
		e.importer.mode = static_cast<ImportMode>(mode);
	}
	ItemTooltip(
		"Auto Detect uses Tiled metadata or filename dimensions when available.\n"
		"Tileset / Sprite Sheet slices one image.\n"
		"Individual Tile imports the whole image as one tile."
	);
	const bool import_options_open{ ImGui::BeginCombo("Import Options", "Options") };
	const bool import_options_hovered{ ImGui::IsItemHovered() };
	if (import_options_open) {
		ToggleComboChoice(
			"Read _WxH / -WxH filename postfix",
			e.importer.use_filename_dimensions,
			"In Auto mode, infer sprite-sheet tile dimensions from names such as forest_16x16.png."
		);
		ToggleComboChoice(
			"Create/use palette named after source",
			e.importer.create_palette_from_source,
			"Create or reuse a named palette derived from the imported source filename."
		);
		ImGui::EndCombo();
	}
	if (import_options_hovered) {
		std::string tip{ "Selected options:" };
		bool any{};
		if (e.importer.use_filename_dimensions) {
			tip += "\n- Read _WxH / -WxH filename postfix";
			any = true;
		}
		if (e.importer.create_palette_from_source) {
			tip += "\n- Create/use palette named after source";
			any = true;
		}
		if (!any) tip += "\n- None";
		ImGui::SetTooltip("%s", tip.c_str());
	}

	if (e.importer.mode == ImportMode::Tileset) {
		ImGui::TextDisabled("Used when dimensions cannot be inferred from the filename or Tiled metadata.");
		ImGui::DragInt("Tile Width", &e.importer.tile_width, 1.0f, 1, 4096);
		ItemTooltip("Width of each source tile in pixels.");
		ImGui::DragInt("Tile Height", &e.importer.tile_height, 1.0f, 1, 4096);
		ItemTooltip("Height of each source tile in pixels.");
		ImGui::DragInt("Margin X", &e.importer.margin_x, 1.0f, 0, 4096);
		ItemTooltip("Horizontal outer margin before the first tile.");
		ImGui::DragInt("Margin Y", &e.importer.margin_y, 1.0f, 0, 4096);
		ItemTooltip("Vertical outer margin before the first tile.");
		ImGui::DragInt("Spacing X", &e.importer.spacing_x, 1.0f, 0, 4096);
		ItemTooltip("Horizontal pixel spacing between tiles.");
		ImGui::DragInt("Spacing Y", &e.importer.spacing_y, 1.0f, 0, 4096);
		ItemTooltip("Vertical pixel spacing between tiles.");
	}

	if (!e.palettes.empty()) {
		e.importer.target_palette = std::clamp(e.importer.target_palette, 0, static_cast<int>(e.palettes.size()) - 1);
		if (ImGui::BeginCombo("Target Palette", e.palettes[static_cast<std::size_t>(e.importer.target_palette)].name.c_str())) {
			for (int i{}; i < static_cast<int>(e.palettes.size()); ++i) {
				if (ImGui::Selectable(e.palettes[static_cast<std::size_t>(i)].name.c_str(), i == e.importer.target_palette)) {
					e.importer.target_palette = i;
				}
			}
			ImGui::EndCombo();
		}
	}
	if (ImGui::Button("Import")) {
		const bool success{ ImportTiles(e, e.importer) };
		e.import_status = success
			? "Imported successfully."
			: "Import failed. Check the path, image format, or tileset metadata.";
		if (!e.palettes.empty() && !e.importer.create_palette_from_source) {
			e.active_palette_index = std::clamp(e.importer.target_palette, 0, static_cast<int>(e.palettes.size()) - 1);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Close")) {
		ImGui::CloseCurrentPopup();
	}
	if (!e.import_status.empty()) {
		ImGui::TextWrapped("%s", e.import_status.c_str());
	}
	ImGui::EndPopup();
}
static const char* PaintSourceKindName(PaintSourceKind kind) {
	switch (kind) {
		case PaintSourceKind::Single: return "Single";
		case PaintSourceKind::WeightedSet: return "Weighted Set";
		case PaintSourceKind::Checkerboard: return "Checkerboard";
		case PaintSourceKind::Autotile: return "Autotile / Terrain";
		case PaintSourceKind::Noise: return "Noise";
	}
	return "Source";
}

static const char* PaintCoverageKindName(PaintCoverageKind kind) {
	switch (kind) {
		case PaintCoverageKind::Solid: return "Solid";
		case PaintCoverageKind::RandomDensity: return "Random Density";
		case PaintCoverageKind::RadialFalloff: return "Radial Falloff";
	}
	return "Coverage";
}

static void SyncRecipeSourceSelection(EditorState& e) {
	e.recipe.tile_id = e.active_tile_id;
	e.recipe.prefab_index = e.active_prefab_index;
	e.recipe.weighted_tile_set_id = e.active_tile_weighted_set_id;
	e.recipe.weighted_prefab_set_id = e.active_prefab_weighted_set_id;
}

static void DrawSourceMode(EditorState& e) {
	const SceneLayer* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->kind == LayerKind::Noise) return;
	const bool tile_layer{ layer->kind == LayerKind::Tile };
	ImGui::SetNextItemWidth(150.0f);
	if (ImGui::BeginCombo("Source", PaintSourceKindName(e.recipe.source_kind))) {
		for (PaintSourceKind kind : { PaintSourceKind::Single, PaintSourceKind::WeightedSet, PaintSourceKind::Checkerboard, PaintSourceKind::Autotile, PaintSourceKind::Noise }) {
			if (!tile_layer && kind == PaintSourceKind::Autotile) continue;
			if (ImGui::Selectable(PaintSourceKindName(kind), e.recipe.source_kind == kind)) {
				e.recipe.source_kind = kind;
				SyncRecipeSourceSelection(e);
				if (kind != PaintSourceKind::Single) e.stamp_tiles.clear();
			}
		}
		ImGui::EndCombo();
	}
	ItemTooltip("The source answers what is emitted. Noise is a source because its thresholds choose which tiles/entities are emitted; Coverage separately controls solid, random-density, or radial-falloff placement.");
}

static bool DrawPrefabSourceComboAll(EditorState& e, const char* id, int& prefab_index) {
	const char* preview{ "<none>" };
	if (prefab_index >= 0 && prefab_index < static_cast<int>(e.prefabs.size())) {
		preview = e.prefabs[static_cast<std::size_t>(prefab_index)].name.c_str();
	}

	bool changed{};
	if (!ImGui::BeginCombo(id, preview)) {
		return false;
	}

	static std::string search;
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint(
		"##prefab_source_search",
		"Search prefab name or group...",
		&search
	);
	ItemTooltip("Filter this source picker by prefab name or prefab group.");

	if (ImGui::Selectable("<none>", prefab_index < 0)) {
		prefab_index = -1;
		changed = true;
	}

	std::vector<std::string> groups;
	for (const auto& prefab : e.prefabs) {
		const std::string group{ prefab.group.empty() ? "Ungrouped" : prefab.group };
		if (!ContainsCaseInsensitive(prefab.name, search) &&
			!ContainsCaseInsensitive(group, search)) {
			continue;
		}
		if (std::find(groups.begin(), groups.end(), group) == groups.end()) {
			groups.push_back(group);
		}
	}
	std::ranges::sort(groups);

	for (const auto& group : groups) {
		ImGui::SeparatorText(group.c_str());
		for (int i{}; i < static_cast<int>(e.prefabs.size()); ++i) {
			const auto& prefab{ e.prefabs[static_cast<std::size_t>(i)] };
			const std::string candidate_group{
				prefab.group.empty() ? "Ungrouped" : prefab.group
			};
			if (candidate_group != group) {
				continue;
			}
			if (!ContainsCaseInsensitive(prefab.name, search) &&
				!ContainsCaseInsensitive(candidate_group, search)) {
				continue;
			}
			if (ImGui::Selectable(prefab.name.c_str(), prefab_index == i)) {
				prefab_index = i;
				changed = true;
			}
		}
	}

	if (groups.empty() && !e.prefabs.empty()) {
		ImGui::TextDisabled("No prefab names/groups match the search.");
	} else if (e.prefabs.empty()) {
		ImGui::TextDisabled("No prefabs are registered in this demo.");
	}

	ImGui::EndCombo();
	return changed;
}

static void DrawInlineTileThumbnail(EditorState& e, int tile_id, float size = 0.0f) {
	const float side{ size > 0.0f ? size : ImGui::GetTextLineHeight() };
	const ImVec2 p0{ ImGui::GetCursorScreenPos() };
	ImGui::Dummy({ side, side });
	const ImVec2 p1{ p0.x + side, p0.y + side };
	if (const auto* tile = FindTile(e, tile_id); tile && tile->texture_index >= 0) {
		const auto& texture{ e.textures[static_cast<std::size_t>(tile->texture_index)] };
		ImGui::GetWindowDrawList()->AddImage(
			(ImTextureID)(intptr_t)texture.handle,
			p0,
			p1,
			tile->uv0,
			tile->uv1
		);
	} else {
		ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, ImGui::GetColorU32(ImVec4(0.32f, 0.34f, 0.38f, 1.0f)));
	}
	ImGui::GetWindowDrawList()->AddRect(p0, p1, ImGui::GetColorU32(ImGuiCol_Border));
}

static bool TileMatchesSearch(
	const TileDefinition& tile,
	const TilePalette& palette,
	std::string_view search
) {
	return search.empty() ||
		ContainsCaseInsensitive(tile.name, search) ||
		ContainsCaseInsensitive(palette.name, search);
}

static bool DrawTileSourceComboAll(EditorState& e, const char* id, int& tile_id) {
	const TileDefinition* current{ FindTile(e, tile_id) };
	bool changed{};
	if (!ImGui::BeginCombo(id, current ? current->name.c_str() : "<none>")) {
		return false;
	}

	static std::string search;
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint(
		"##tile_source_search",
		"Search tile name or palette...",
		&search
	);
	ItemTooltip("Filter this source picker by tile name or tile-palette/group name.");

	if (ImGui::Selectable("<none>", tile_id < 0)) {
		tile_id = -1;
		changed = true;
	}

	const float row_height{ ImGui::GetFrameHeight() };
	const float text_height{ ImGui::GetTextLineHeight() };
	const float thumbnail_side{ text_height };
	const float horizontal_padding{ ImGui::GetStyle().FramePadding.x };
	const float item_spacing{ ImGui::GetStyle().ItemInnerSpacing.x };

	for (const auto& palette : e.palettes) {
		bool any{};
		for (const auto& entry : palette.entries) {
			if (const auto* tile = FindTile(e, entry.tile_id);
				tile && TileMatchesSearch(*tile, palette, search)) {
				any = true;
				break;
			}
		}
		if (!any) {
			continue;
		}

		ImGui::SeparatorText(palette.name.c_str());
		for (const auto& entry : palette.entries) {
			const auto* tile{ FindTile(e, entry.tile_id) };
			if (!tile || !TileMatchesSearch(*tile, palette, search)) {
				continue;
			}

			ImGui::PushID(tile->id);
			const bool selected{ tile_id == tile->id };
			if (ImGui::Selectable(
					"##tile_source_row",
					selected,
					0,
					{ 0.0f, row_height }
				)) {
				tile_id = tile->id;
				changed = true;
			}

			const ImVec2 row_min{ ImGui::GetItemRectMin() };
			const ImVec2 row_max{ ImGui::GetItemRectMax() };
			const float thumbnail_y{
				row_min.y + std::max(0.0f, (row_max.y - row_min.y - thumbnail_side) * 0.5f)
			};
			const ImVec2 thumb_min{
				row_min.x + horizontal_padding,
				thumbnail_y
			};
			const ImVec2 thumb_max{
				thumb_min.x + thumbnail_side,
				thumb_min.y + thumbnail_side
			};

			ImDrawList* draw_list{ ImGui::GetWindowDrawList() };
			if (tile->texture_index >= 0) {
				const auto& texture{
					e.textures[static_cast<std::size_t>(tile->texture_index)]
				};
				draw_list->AddImage(
					(ImTextureID)(intptr_t)texture.handle,
					thumb_min,
					thumb_max,
					tile->uv0,
					tile->uv1
				);
			} else {
				draw_list->AddRectFilled(
					thumb_min,
					thumb_max,
					ImGui::GetColorU32(ImVec4(0.32f, 0.34f, 0.38f, 1.0f))
				);
			}
			draw_list->AddRect(
				thumb_min,
				thumb_max,
				ImGui::GetColorU32(ImGuiCol_Border)
			);

			const ImVec2 text_size{ ImGui::CalcTextSize(tile->name.c_str()) };
			const ImVec2 text_position{
				thumb_max.x + item_spacing,
				row_min.y + std::max(0.0f, (row_max.y - row_min.y - text_size.y) * 0.5f)
			};
			draw_list->AddText(
				text_position,
				ImGui::GetColorU32(ImGuiCol_Text),
				tile->name.c_str()
			);
			ItemTooltip((palette.name + " / " + tile->name).c_str());
			ImGui::PopID();
		}
	}

	if (e.palettes.empty()) {
		ImGui::TextDisabled("No tile palettes exist.");
	}
	ImGui::EndCombo();
	return changed;
}


static void DrawPrefabTreeBrowser(
	EditorState& e,
	std::string_view search,
	WeightedPrefabSet* add_to_set = nullptr
) {
	std::vector<std::string> groups;
	for (const auto& prefab : e.prefabs) {
		const std::string group{ prefab.group.empty() ? "Ungrouped" : prefab.group };
		const bool matches{ ContainsCaseInsensitive(prefab.name, search) || ContainsCaseInsensitive(group, search) };
		if (matches && std::find(groups.begin(), groups.end(), group) == groups.end()) groups.push_back(group);
	}
	std::ranges::sort(groups);
	for (const std::string& group : groups) {
		ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth };
		if (!search.empty()) flags |= ImGuiTreeNodeFlags_DefaultOpen;
		const std::string node_id{ group + "##prefab_group" };
		if (!ImGui::TreeNodeEx(node_id.c_str(), flags, "%s", group.c_str())) continue;
		for (int i{}; i < static_cast<int>(e.prefabs.size()); ++i) {
			const auto& prefab{ e.prefabs[static_cast<std::size_t>(i)] };
			const std::string prefab_group{ prefab.group.empty() ? "Ungrouped" : prefab.group };
			if (prefab_group != group) continue;
			if (!ContainsCaseInsensitive(prefab.name, search) && !ContainsCaseInsensitive(group, search)) continue;
			ImGui::PushID(i);
			if (add_to_set) {
				const bool already{ std::ranges::any_of(add_to_set->entries, [&](const WeightedPrefabEntry& entry) {
					return entry.prefab_index == i;
				}) };
				ImGui::BeginDisabled(already);
				if (ImGui::SmallButton("+")) {
					const SceneSnapshot before{ CaptureScene(e) };
					add_to_set->entries.push_back({ i, 1.0f });
					PushHistory(e, "Add Prefab to Weighted Set", before);
				}
				ImGui::EndDisabled();
				ItemTooltip(already ? "This prefab is already in the weighted set." : "Add this prefab to the current weighted set.");
				ImGui::SameLine();
			}
			if (ImGui::Selectable(prefab.name.c_str(), !add_to_set && e.active_prefab_index == i)) {
				if (!add_to_set) e.active_prefab_index = i;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("%.0fx%.0f", prefab.size.x, prefab.size.y);
			ItemTooltip(("Prefab group: " + group + "\nPaint source: " + prefab.name).c_str());
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
	if (groups.empty()) ImGui::TextDisabled(search.empty() ? "No prefab sources." : "No prefab names/groups match the search.");
}

static void DrawPrefabSourcePalette(EditorState& e) {
	static std::string search;
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##prefab_search", "Search prefab name or group...", &search);
	ItemTooltip("Filter prefabs by either prefab name or group name. Groups are collapsible below; ungrouped prefabs share the Ungrouped node.");

	if (e.recipe.source_kind == PaintSourceKind::Single) {
		ImGui::SeparatorText("Prefab Browser");
		const char* selected_prefab{ "<none>" };
		if (e.active_prefab_index >= 0 &&
			e.active_prefab_index < static_cast<int>(e.prefabs.size())) {
			selected_prefab = e.prefabs[static_cast<std::size_t>(e.active_prefab_index)].name.c_str();
		}
		ImGui::Text("Selected: %s", selected_prefab);
		ItemTooltip("Currently selected prefab paint source. It stays visible even when its group is collapsed.");
		DrawPrefabTreeBrowser(e, search);
		return;
	}

	if (e.recipe.source_kind == PaintSourceKind::Checkerboard) {
		ImGui::SeparatorText("Checkerboard Sources");
		DrawPrefabSourceComboAll(e, "Primary", e.recipe.prefab_index);
		DrawPrefabSourceComboAll(e, "Secondary", e.recipe.secondary_prefab_index);
		ImGui::TextDisabled("Alternates sources by raster-cell parity. Coverage still controls which of those cells are emitted.");
		return;
	}

	ImGui::SeparatorText("Named Weighted Prefab Sets");
	const WeightedPrefabSet* active_const{ FindWeightedPrefabSet(e, e.active_prefab_weighted_set_id) };
	ImGui::SetNextItemWidth(190.0f);
	if (ImGui::BeginCombo("Weighted Set", active_const ? active_const->name.c_str() : "<none>")) {
		for (const auto& set : e.weighted_prefab_sets) {
			if (ImGui::Selectable(set.name.c_str(), set.id == e.active_prefab_weighted_set_id)) {
				const SceneSnapshot before{ CaptureScene(e) };
				e.active_prefab_weighted_set_id = set.id;
				PushHistory(e, "Select Weighted Prefab Set", before);
			}
		}
		ImGui::EndCombo();
	}
	ItemTooltip("Choose which reusable weighted prefab brush is currently painted.");
	ImGui::SameLine();
	if (ImGui::Button("Add Weighted Set")) {
		const SceneSnapshot before{ CaptureScene(e) };
		WeightedPrefabSet set;
		set.id = e.next_prefab_weighted_set_id++;
		set.name = UniqueWeightedPrefabSetName(e);
		e.active_prefab_weighted_set_id = set.id;
		e.weighted_prefab_sets.push_back(std::move(set));
		PushHistory(e, "Add Weighted Prefab Set", before);
	}
	ItemTooltip("Create a new empty named custom prefab brush. The default name is numbered uniquely.");
	ImGui::SameLine();
	ImGui::BeginDisabled(!FindWeightedPrefabSet(e, e.active_prefab_weighted_set_id));
	if (ImGui::Button("Delete Set")) {
		const SceneSnapshot before{ CaptureScene(e) };
		const int id{ e.active_prefab_weighted_set_id };
		e.weighted_prefab_sets.erase(std::remove_if(e.weighted_prefab_sets.begin(), e.weighted_prefab_sets.end(), [&](const auto& set) { return set.id == id; }), e.weighted_prefab_sets.end());
		e.active_prefab_weighted_set_id = e.weighted_prefab_sets.empty() ? -1 : e.weighted_prefab_sets.front().id;
		PushHistory(e, "Delete Weighted Prefab Set", before);
	}
	ImGui::EndDisabled();
	ItemTooltip("Delete the currently selected weighted prefab set. Zero weighted sets is allowed.");

	WeightedPrefabSet* active{ FindWeightedPrefabSet(e, e.active_prefab_weighted_set_id) };
	if (!active) {
		ImGui::TextDisabled("No weighted prefab set selected. Create one with Add Weighted Set.");
		return;
	}

	std::string edited_name{ active->name };
	ImGui::SetNextItemWidth(240.0f);
	if (ImGui::InputText("Set Name", &edited_name)) {
		const SceneSnapshot before{ CaptureScene(e) };
		if (edited_name.empty()) edited_name = UniqueWeightedPrefabSetName(e);
		std::string unique{ edited_name };
		int suffix{ 2 };
		while (std::ranges::any_of(e.weighted_prefab_sets, [&](const auto& candidate) { return candidate.id != active->id && candidate.name == unique; })) {
			unique = edited_name + " " + std::to_string(suffix++);
		}
		active->name = std::move(unique);
		PushHistory(e, "Rename Weighted Prefab Set", before);
	}
	ItemTooltip("Unique reusable brush name. Duplicate names are automatically given a numeric suffix.");

	// Let the weighted table grow naturally with all of its rows. The Paint Palette
	// window itself owns scrolling if the complete set no longer fits vertically.
	if (ImGui::BeginTable(
			"##weighted_prefab_members",
			3,
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn("Prefab", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 28.0f);
		ImGui::TableHeadersRow();
		int remove{ -1 };
		for (int i{}; i < static_cast<int>(active->entries.size()); ++i) {
			auto& entry{ active->entries[static_cast<std::size_t>(i)] };
			ImGui::PushID(i);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(PrefabDisplayPath(e, entry.prefab_index).c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);
			float weight{ entry.weight };
			if (ImGui::DragFloat("##weight", &weight, 0.05f, 0.0f, 100.0f, "%.2f")) {
				const SceneSnapshot before{ CaptureScene(e) };
				entry.weight = std::max(0.0f, weight);
				PushHistory(e, "Change Prefab Weight", before);
			}
			ItemTooltip("Relative probability for this prefab within the named weighted set. Zero keeps it in the set but prevents selection.");
			ImGui::TableSetColumnIndex(2);
			if (ImGui::SmallButton("x")) remove = i;
			ItemTooltip("Remove this source from the weighted set; the prefab itself remains available in its group.");
			ImGui::PopID();
		}
		if (remove >= 0) {
			const SceneSnapshot before{ CaptureScene(e) };
			active->entries.erase(active->entries.begin() + remove);
			PushHistory(e, "Remove Prefab from Weighted Set", before);
		}
		ImGui::EndTable();
	}

	if (ImGui::Button("Add Prefab Sources...")) ImGui::OpenPopup("Add Prefab Sources");
	ItemTooltip("Browse every prefab group and add one or more sources to this custom brush.");
	if (ImGui::BeginPopup("Add Prefab Sources")) {
		static std::string add_search;
		ImGui::SetNextItemWidth(340.0f);
		ImGui::InputTextWithHint("##add_prefab_search", "Search prefab name or group...", &add_search);
		if (ImGui::BeginChild("##add_prefab_sources_scroll", { 360.0f, 300.0f }, ImGuiChildFlags_Borders)) {
			DrawPrefabTreeBrowser(e, add_search, active);
		}
		ImGui::EndChild();
		ImGui::EndPopup();
	}
}

static bool DrawTilePaletteTree(
	EditorState& e,
	TilePalette& palette,
	bool allow_select,
	WeightedTileSet* add_to_set = nullptr,
	std::string_view search = {}
) {
	const int palette_id{ palette.id };
	const int palette_index{ static_cast<int>(&palette - e.palettes.data()) };
	const bool group_match{ ContainsCaseInsensitive(palette.name, search) };
	bool any_match{ search.empty() || group_match };
	if (!any_match) {
		for (const auto& entry : palette.entries) {
			if (const auto* tile = FindTile(e, entry.tile_id);
				tile && TileMatchesSearch(*tile, palette, search)) {
				any_match = true;
				break;
			}
		}
	}
	if (!any_match) {
		return false;
	}

	static std::unordered_map<int, bool> open_states;
	static int editing_palette_id{ -1 };
	static int focus_palette_id{ -1 };
	static std::string original_name;
	static std::optional<SceneSnapshot> rename_before;

	bool& stored_open{ open_states.try_emplace(palette_id, true).first->second };
	if (!search.empty()) {
		stored_open = true;
	}

	ImGui::PushID(palette_id);
	bool open{ stored_open };
	bool request_delete{};
	bool request_import{};
	bool begin_edit{};
	bool began_edit_this_frame{};
	bool commit_rename{};
	bool cancel_rename{};
	ImVec2 name_input_min{};
	ImVec2 name_input_max{};
	bool name_input_drawn{};
	bool name_input_hovered{};

	const float row_height{ ImGui::GetFrameHeight() };
	if (ImGui::BeginTable(
			"##palette_tree_header",
			1,
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
		)) {
		ImGui::TableSetupColumn("Palette", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);
		ImGui::TableSetColumnIndex(0);

		const ImVec2 row_min{ ImGui::GetCursorScreenPos() };
		const ImVec2 row_max{
			row_min.x + std::max(1.0f, ImGui::GetContentRegionAvail().x),
			row_min.y + row_height
		};
		const ImVec2 mouse{ ImGui::GetMousePos() };

		ImGui::SetNextItemOpen(stored_open, ImGuiCond_Always);
		const bool editing_before_draw{
			!add_to_set && editing_palette_id == palette_id
		};
		ImGuiTreeNodeFlags flags{
			ImGuiTreeNodeFlags_FramePadding |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_NoTreePushOnOpen |
			ImGuiTreeNodeFlags_AllowOverlap |
			ImGuiTreeNodeFlags_OpenOnArrow
		};
		if (allow_select && e.active_palette_index == palette_index) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}
		open = ImGui::TreeNodeEx(
			"##tile_palette_tree",
			flags,
			"%s",
			editing_before_draw ? "" : palette.name.c_str()
		);
		stored_open = open;

		const bool tree_hovered{ ImGui::IsItemHovered() };
		const ImVec2 tree_min{ ImGui::GetItemRectMin() };
		const ImVec2 tree_max{ ImGui::GetItemRectMax() };
		const float text_start_x{ tree_min.x + row_height };
		const float minimum_name_width{ 48.0f };
		const float visible_name_width{
			std::max(minimum_name_width, ImGui::CalcTextSize(palette.name.c_str()).x)
		};
		const float name_hit_end_x{
			std::min(tree_max.x, text_start_x + visible_name_width)
		};
		const bool name_hit_hovered{
			tree_hovered && mouse.x >= text_start_x && mouse.x <= name_hit_end_x
		};
		const bool row_left_clicked{ ImGui::IsItemClicked(ImGuiMouseButton_Left) };

		if (allow_select && row_left_clicked) {
			e.active_palette_index = palette_index;
		}

		// The arrow keeps ImGui's normal tree interaction. Clicking the unused part
		// of the row to the right of the name also toggles open/closed, while the
		// name itself is reserved for selection and double-click rename.
		if (!editing_before_draw && row_left_clicked && mouse.x > name_hit_end_x) {
			stored_open = !open;
			open = stored_open;
		}

		if (!add_to_set && !editing_before_draw && name_hit_hovered &&
			ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			begin_edit = true;
		}

		if (!add_to_set && ImGui::BeginPopupContextItem("PaletteContext")) {
			if (ImGui::MenuItem("Rename")) {
				begin_edit = true;
			}
			if (ImGui::MenuItem("Import Tiles...")) {
				request_import = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete Palette")) {
				request_delete = true;
			}
			ImGui::EndPopup();
		}

		if (begin_edit) {
			editing_palette_id = palette_id;
			focus_palette_id = palette_id;
			original_name = palette.name;
			rename_before = CaptureScene(e);
			began_edit_this_frame = true;
		}

		if (!add_to_set && editing_palette_id == palette_id) {
			ImGui::SetCursorScreenPos({ text_start_x, tree_min.y });
			ImGui::SetNextItemWidth(std::max(
				minimum_name_width,
				tree_max.x - text_start_x - ImGui::GetStyle().FramePadding.x
			));
			if (focus_palette_id == palette_id) {
				ImGui::SetKeyboardFocusHere();
				focus_palette_id = -1;
			}

			const bool submitted{ ImGui::InputText(
				"##palette_rename",
				&palette.name,
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
			) };
			name_input_min = ImGui::GetItemRectMin();
			name_input_max = ImGui::GetItemRectMax();
			name_input_drawn = true;
			name_input_hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
			if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
				cancel_rename = true;
			} else if (submitted) {
				commit_rename = true;
			}
		}

		if (tree_hovered && !add_to_set && editing_palette_id != palette_id) {
			ImGui::SetTooltip(
				"Double click the palette name to rename.\n"
				"Right click for import, rename, and delete options."
			);
		}
		ImGui::EndTable();
	}

	if (request_import) {
		e.active_palette_index = palette_index;
		e.importer.target_palette = palette_index;
		e.importer.open_popup = true;
	}

	if (request_delete) {
		const SceneSnapshot before{ CaptureScene(e) };
		e.palettes.erase(
			std::remove_if(
				e.palettes.begin(),
				e.palettes.end(),
				[&](const TilePalette& candidate) {
					return candidate.id == palette_id;
				}
			),
			e.palettes.end()
		);
		e.active_palette_index = e.palettes.empty()
			? -1
			: std::clamp(e.active_palette_index, 0, static_cast<int>(e.palettes.size()) - 1);
		if (editing_palette_id == palette_id) {
			editing_palette_id = -1;
			focus_palette_id = -1;
			rename_before.reset();
		}
		PushHistory(e, "Delete Tile Palette", before);
		ImGui::PopID();
		return true;
	}

	if (!add_to_set && editing_palette_id == palette_id) {
		if (cancel_rename) {
			palette.name = original_name;
			editing_palette_id = -1;
			focus_palette_id = -1;
			rename_before.reset();
		} else {
			if (name_input_drawn && !began_edit_this_frame) {
				const bool clicked{
					ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
					ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
					ImGui::IsMouseClicked(ImGuiMouseButton_Right)
				};
				const ImVec2 mouse{ ImGui::GetMousePos() };
				const bool inside_input{
					mouse.x >= name_input_min.x && mouse.x <= name_input_max.x &&
					mouse.y >= name_input_min.y && mouse.y <= name_input_max.y
				};
				if (clicked && !inside_input && !name_input_hovered) {
					commit_rename = true;
				}
			}

			if (commit_rename) {
				if (palette.name.empty()) {
					palette.name = "Palette";
				}
				const std::string base{ palette.name };
				std::string unique{ base };
				int suffix{ 2 };
				while (std::ranges::any_of(
					e.palettes,
					[&](const TilePalette& candidate) {
						return candidate.id != palette_id && candidate.name == unique;
					}
				)) {
					unique = base + " " + std::to_string(suffix++);
				}
				palette.name = std::move(unique);
				if (rename_before && palette.name != original_name) {
					PushHistory(e, "Rename Tile Palette", std::move(*rename_before));
				}
				editing_palette_id = -1;
				focus_palette_id = -1;
				rename_before.reset();
			}
		}
	}

	if (open) {
		ImGui::Indent(row_height * 0.65f);
		bool drew_any{};

		if (add_to_set) {
			for (const auto& palette_entry : palette.entries) {
				const auto* tile{ FindTile(e, palette_entry.tile_id) };
				if (!tile || !TileMatchesSearch(*tile, palette, search)) {
					continue;
				}
				drew_any = true;
				ImGui::PushID(tile->id);
				const bool already{ std::ranges::any_of(
					add_to_set->entries,
					[&](const WeightedTileEntry& entry) {
						return entry.tile_id == tile->id;
					}
				) };
				ImGui::BeginDisabled(already);
				if (ImGui::SmallButton("+")) {
					const SceneSnapshot before{ CaptureScene(e) };
					add_to_set->entries.push_back({ tile->id, 1.0f });
					PushHistory(e, "Add Tile to Weighted Set", before);
				}
				ImGui::EndDisabled();
				ItemTooltip(
					already
						? "This tile is already in the weighted set."
						: "Add this tile to the current weighted set."
				);
				ImGui::SameLine();
				DrawInlineTileThumbnail(e, tile->id, ImGui::GetTextLineHeight());
				ImGui::SameLine();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(tile->name.c_str());
				ImGui::PopID();
			}
		} else if (allow_select) {
			// Use fixed-width cells and fixed cell padding so the gap between 48 px
			// thumbnails stays constant while the Paint Palette window is resized.
			constexpr float thumbnail_size{ 48.0f };
			constexpr float thumbnail_gap{ 6.0f };
			constexpr float cell_width{ thumbnail_size + thumbnail_gap };
			constexpr float row_height_tiles{ thumbnail_size + thumbnail_gap };
			const float available_width{ std::max(cell_width, ImGui::GetContentRegionAvail().x) };
			const int columns{ std::max(
				1,
				static_cast<int>(std::floor((available_width + thumbnail_gap) / cell_width))
			) };
			ImGui::PushStyleVar(
				ImGuiStyleVar_CellPadding,
				ImVec2{ thumbnail_gap * 0.5f, thumbnail_gap * 0.5f }
			);
			if (ImGui::BeginTable(
					"##palette_tile_grid",
					columns,
					ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingFixedFit
				)) {
				for (int c{}; c < columns; ++c) {
					const std::string column_id{
						"##tile_column_" + std::to_string(c)
					};
					ImGui::TableSetupColumn(
						column_id.c_str(),
						ImGuiTableColumnFlags_WidthFixed,
						cell_width
					);
				}
				int column{};
				for (const auto& palette_entry : palette.entries) {
					const auto* tile{ FindTile(e, palette_entry.tile_id) };
					if (!tile || !TileMatchesSearch(*tile, palette, search)) {
						continue;
					}
					drew_any = true;
					if (column == 0) {
						ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height_tiles);
					}
					ImGui::TableSetColumnIndex(column);
					const bool in_stamp{
						std::find(e.stamp_tiles.begin(), e.stamp_tiles.end(), tile->id) !=
						e.stamp_tiles.end()
					};
					DrawPaletteTile(e, *tile, e.active_tile_id == tile->id, in_stamp);
					column = (column + 1) % columns;
				}
				ImGui::EndTable();
			}
			ImGui::PopStyleVar();
		}

		if (!drew_any) {
			ImGui::TextDisabled(
				search.empty()
					? "Empty palette. Right click the palette row to import tiles."
					: "No tiles in this palette match the filter."
			);
		}
		ImGui::Unindent(row_height * 0.65f);
	}

	ImGui::PopID();
	return false;
}


static std::string AutotileSlotLabel(const AutotileRuleSet& rules, int index) {
	if (rules.format == AutotileFormat::DualGrid16) {
		return "Dual corners " + std::to_string(index);
	}
	if (rules.format == AutotileFormat::Blob47) {
		const auto& masks{ ValidBlob47Masks() };
		return "Blob mask " + std::to_string(index < static_cast<int>(masks.size()) ? masks[static_cast<std::size_t>(index)] : index);
	}
	if (rules.format == AutotileFormat::Classic15) return "Classic case " + std::to_string(index + 1);
	return "Neighbor mask " + std::to_string(index);
}

static void EnsureAutotileSlotCount(AutotileRuleSet& rules) {
	rules.tile_ids.resize(static_cast<std::size_t>(RequiredAutotileTileCount(rules.format)), -1);
}

static void DrawAutotileRulesetEditor(EditorState& e) {
	ImGui::SeparatorText("Autotile / Terrain Ruleset");
	const AutotileRuleSet* current{ FindAutotileRuleSet(e, e.recipe.autotile_ruleset_id) };
	ImGui::SetNextItemWidth(190.0f);
	if (ImGui::BeginCombo("Ruleset", current ? current->name.c_str() : "<none>")) {
		for (const auto& rules : e.autotile_rulesets) {
			if (ImGui::Selectable(rules.name.c_str(), rules.id == e.recipe.autotile_ruleset_id)) e.recipe.autotile_ruleset_id = rules.id;
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("+ Ruleset")) {
		AutotileRuleSet rules;
		rules.id = e.next_autotile_ruleset_id++;
		rules.name = "Terrain " + std::to_string(rules.id);
		rules.format = AutotileFormat::DualGrid16;
		EnsureAutotileSlotCount(rules);
		e.recipe.autotile_ruleset_id = rules.id;
		e.autotile_rulesets.push_back(std::move(rules));
	}
	AutotileRuleSet* rules{ FindAutotileRuleSet(e, e.recipe.autotile_ruleset_id) };
	if (!rules) {
		ImGui::TextDisabled("Create a ruleset to paint connected terrain.");
		return;
	}
	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::BeginCombo("Format", AutotileFormatName(rules->format))) {
		for (AutotileFormat format : { AutotileFormat::Classic15, AutotileFormat::Blob47, AutotileFormat::Subset16, AutotileFormat::DualGrid16, AutotileFormat::Wang16 }) {
			if (ImGui::Selectable(AutotileFormatName(format), rules->format == format)) {
				rules->format = format;
				EnsureAutotileSlotCount(*rules);
			}
		}
		ImGui::EndCombo();
	}
	ItemTooltip("Classic 15 uses cardinal connectivity; Blob 47 uses gated 8-neighbor masks; 4-neighbor 16 and Wang use 16 masks; Dual Grid stores logical terrain and renders one of 16 tiles from the four overlapping world cells.");
	ImGui::SameLine();
	if (ImGui::Button("Fill from Active Palette") && !e.palettes.empty()) {
		const auto& palette{ e.palettes[static_cast<std::size_t>(std::clamp(e.active_palette_index, 0, static_cast<int>(e.palettes.size()) - 1))] };
		for (std::size_t i{}; i < rules->tile_ids.size() && i < palette.entries.size(); ++i) rules->tile_ids[i] = palette.entries[i].tile_id;
	}
	ImGui::TextDisabled("Assign %d variants. Missing slots render nothing, making incomplete rulesets easy to spot.", static_cast<int>(rules->tile_ids.size()));
	if (ImGui::BeginTable("##autotile_slots", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Case", ImGuiTableColumnFlags_WidthFixed, 145.0f);
		ImGui::TableSetupColumn("Tile");
		for (int i{}; i < static_cast<int>(rules->tile_ids.size()); ++i) {
			ImGui::PushID(i);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(AutotileSlotLabel(*rules, i).c_str());
			ImGui::TableSetColumnIndex(1);
			DrawTileSourceComboAll(e, "##tile", rules->tile_ids[static_cast<std::size_t>(i)]);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
}

static void DrawTileSourcePalette(EditorState& e) {
	static std::string search;
	if (ImGui::Button("+ Palette")) {
		const SceneSnapshot before{ CaptureScene(e) };
		TilePalette palette;
		palette.id = e.next_palette_id++;
		palette.name = "Palette " + std::to_string(palette.id);
		e.palettes.push_back(std::move(palette));
		e.active_palette_index = static_cast<int>(e.palettes.size()) - 1;
		PushHistory(e, "Add Tile Palette", before);
	}
	ItemTooltip("Create an empty named tile palette/group. It is valid for the project to have zero palettes.");
	ImGui::SameLine();
	if (ImGui::Button("Import...")) {
		e.importer.target_palette = e.palettes.empty() ? 0 : std::clamp(e.active_palette_index, 0, static_cast<int>(e.palettes.size()) - 1);
		e.importer.open_popup = true;
	}
	ItemTooltip("Import an image/tileset. If no palettes exist, importing automatically creates a Default palette unless source-name grouping is enabled.");

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##tile_search", "Search tile name or palette...", &search);
	ItemTooltip("Filter tiles by either tile name or tile-palette/group name. Matching palette names show all tiles in that group.");

	if (e.recipe.source_kind == PaintSourceKind::Single) {
		if (!e.stamp_tiles.empty()) {
			ImGui::SameLine();
			ImGui::Text("Stamp %d", static_cast<int>(e.stamp_tiles.size()));
			ImGui::SameLine();
			ImGui::SetNextItemWidth(70.0f);
			ImGui::DragInt("Width##stamp", &e.stamp_width, 0.2f, 1, 32);
			ItemTooltip("Number of tiles per row in the multi-tile stamp pattern.");
			ImGui::SameLine();
			if (ImGui::SmallButton("Clear Stamp")) e.stamp_tiles.clear();
		}
		ImGui::SeparatorText("Tile Palettes");
		for (int i{}; i < static_cast<int>(e.palettes.size());) {
			if (!DrawTilePaletteTree(e, e.palettes[static_cast<std::size_t>(i)], true, nullptr, search)) ++i;
		}
		if (e.palettes.empty()) ImGui::TextDisabled("No tile palettes. Create one or import a tileset/image.");
		DrawImportPopup(e);
		return;
	}

	if (e.recipe.source_kind == PaintSourceKind::Checkerboard) {
		ImGui::SeparatorText("Checkerboard Sources");
		DrawTileSourceComboAll(e, "Primary", e.recipe.tile_id);
		DrawTileSourceComboAll(e, "Secondary", e.recipe.secondary_tile_id);
		ImGui::TextDisabled("Alternates the two tiles by raster-cell parity.");
		DrawImportPopup(e);
		return;
	}
	if (e.recipe.source_kind == PaintSourceKind::Autotile) {
		DrawAutotileRulesetEditor(e);
		DrawImportPopup(e);
		return;
	}

	ImGui::SeparatorText("Named Weighted Tile Sets");
	const WeightedTileSet* active_const{ FindWeightedTileSet(e, e.active_tile_weighted_set_id) };
	ImGui::SetNextItemWidth(190.0f);
	if (ImGui::BeginCombo("Weighted Set", active_const ? active_const->name.c_str() : "<none>")) {
		for (const auto& set : e.weighted_tile_sets) {
			if (ImGui::Selectable(set.name.c_str(), set.id == e.active_tile_weighted_set_id)) {
				const SceneSnapshot before{ CaptureScene(e) };
				e.active_tile_weighted_set_id = set.id;
				PushHistory(e, "Select Weighted Tile Set", before);
			}
		}
		ImGui::EndCombo();
	}
	ItemTooltip("Choose a reusable weighted tile brush. A set can contain tiles from several different palettes.");
	ImGui::SameLine();
	if (ImGui::Button("Add Weighted Set")) {
		const SceneSnapshot before{ CaptureScene(e) };
		WeightedTileSet set;
		set.id = e.next_tile_weighted_set_id++;
		set.name = UniqueWeightedTileSetName(e);
		e.active_tile_weighted_set_id = set.id;
		e.weighted_tile_sets.push_back(std::move(set));
		PushHistory(e, "Add Weighted Tile Set", before);
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!FindWeightedTileSet(e, e.active_tile_weighted_set_id));
	if (ImGui::Button("Delete Set")) {
		const SceneSnapshot before{ CaptureScene(e) };
		const int id{ e.active_tile_weighted_set_id };
		e.weighted_tile_sets.erase(std::remove_if(e.weighted_tile_sets.begin(), e.weighted_tile_sets.end(), [&](const auto& set) { return set.id == id; }), e.weighted_tile_sets.end());
		e.active_tile_weighted_set_id = e.weighted_tile_sets.empty() ? -1 : e.weighted_tile_sets.front().id;
		PushHistory(e, "Delete Weighted Tile Set", before);
	}
	ImGui::EndDisabled();
	ItemTooltip("Delete the current weighted tile set. Zero weighted sets is allowed.");

	WeightedTileSet* active{ FindWeightedTileSet(e, e.active_tile_weighted_set_id) };
	if (!active) {
		ImGui::TextDisabled("No weighted tile set selected. Create one with Add Weighted Set.");
		DrawImportPopup(e);
		return;
	}
	std::string edited_name{ active->name };
	ImGui::SetNextItemWidth(240.0f);
	if (ImGui::InputText("Set Name", &edited_name)) {
		const SceneSnapshot before{ CaptureScene(e) };
		if (edited_name.empty()) edited_name = UniqueWeightedTileSetName(e);
		std::string unique{ edited_name };
		int suffix{ 2 };
		while (std::ranges::any_of(e.weighted_tile_sets, [&](const auto& candidate) { return candidate.id != active->id && candidate.name == unique; })) unique = edited_name + " " + std::to_string(suffix++);
		active->name = std::move(unique);
		PushHistory(e, "Rename Weighted Tile Set", before);
	}
	// Expand to all weighted members by default. The parent Paint Palette window
	// provides scrolling when the table becomes taller than the docked region.
	if (ImGui::BeginTable(
			"##weighted_tile_members",
			3,
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn("Tile", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 28.0f);
		ImGui::TableHeadersRow();
		int remove{ -1 };
		for (int i{}; i < static_cast<int>(active->entries.size()); ++i) {
			auto& entry{ active->entries[static_cast<std::size_t>(i)] };
			const TileDefinition* tile{ FindTile(e, entry.tile_id) };
			const TilePalette* owner{};
			for (const auto& palette : e.palettes) {
				if (std::ranges::any_of(
						palette.entries,
						[&](const PaletteEntry& pe) { return pe.tile_id == entry.tile_id; }
					)) {
					owner = &palette;
					break;
				}
			}
			if (!search.empty() && tile && owner && !TileMatchesSearch(*tile, *owner, search)) {
				continue;
			}
			ImGui::PushID(i);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
			ImGui::TableSetColumnIndex(0);
			DrawInlineTileThumbnail(e, entry.tile_id, ImGui::GetTextLineHeight());
			ImGui::SameLine();
			ImGui::TextUnformatted(TileDisplayPath(e, entry.tile_id).c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);
			float weight{ entry.weight };
			if (ImGui::DragFloat("##weight", &weight, 0.05f, 0.0f, 100.0f, "%.2f")) {
				const SceneSnapshot before{ CaptureScene(e) };
				entry.weight = std::max(0.0f, weight);
				PushHistory(e, "Change Tile Weight", before);
			}
			ItemTooltip("Relative selection probability for this tile within the weighted set.");
			ImGui::TableSetColumnIndex(2);
			if (ImGui::SmallButton("x")) remove = i;
			ImGui::PopID();
		}
		if (remove >= 0) {
			const SceneSnapshot before{ CaptureScene(e) };
			active->entries.erase(active->entries.begin() + remove);
			PushHistory(e, "Remove Tile from Weighted Set", before);
		}
		ImGui::EndTable();
	}

	if (ImGui::Button("Add Tile Sources...")) ImGui::OpenPopup("Add Tile Sources");
	ItemTooltip("Add sources from any tile palette to this custom weighted brush.");
	if (ImGui::BeginPopup("Add Tile Sources")) {
		static std::string add_search;
		ImGui::SetNextItemWidth(340.0f);
		ImGui::InputTextWithHint("##add_tile_search", "Search tile name or palette...", &add_search);
		ItemTooltip("Filter available sources by tile name or tile-palette/group name.");
		if (ImGui::BeginChild("##add_tile_sources_scroll", { 360.0f, 300.0f }, ImGuiChildFlags_Borders)) {
			for (auto& palette : e.palettes) DrawTilePaletteTree(e, palette, false, active, add_search);
			if (e.palettes.empty()) ImGui::TextDisabled("No tile palettes exist.");
		}
		ImGui::EndChild();
		ImGui::EndPopup();
	}
	DrawImportPopup(e);
}

static NoiseThresholdRegion MakeNoiseThreshold(EditorState& e, const SceneLayer& layer, float lo, float hi, bool enabled = true) {
	NoiseThresholdRegion region;
	region.minimum = std::clamp(lo, 0.0f, 1.0f);
	region.maximum = std::clamp(hi, 0.0f, 1.0f);
	if (region.minimum > region.maximum) std::swap(region.minimum, region.maximum);
	region.enabled = enabled;
	region.tile_id = e.active_tile_id;
	region.prefab_index = e.prefabs.empty() ? -1 : std::clamp(e.active_prefab_index, 0, static_cast<int>(e.prefabs.size()) - 1);
	if (layer.noise.target == NoiseTargetKind::Tile && !FindTile(e, region.tile_id)) region.enabled = false;
	if (layer.noise.target == NoiseTargetKind::Entity && region.prefab_index < 0) region.enabled = false;
	return region;
}

static void NormalizeNoiseThresholds(NoiseField& field) {
	if (field.thresholds.empty()) return;
	std::ranges::sort(field.thresholds, {}, [](const NoiseThresholdRegion& region) { return region.minimum; });
	for (auto& region : field.thresholds) {
		region.minimum = std::clamp(region.minimum, 0.0f, 1.0f);
		region.maximum = std::clamp(region.maximum, 0.0f, 1.0f);
		if (region.minimum > region.maximum) std::swap(region.minimum, region.maximum);
	}
	field.thresholds.front().minimum = 0.0f;
	for (std::size_t i{ 1 }; i < field.thresholds.size(); ++i) {
		const float boundary{ std::clamp(field.thresholds[i - 1].maximum, 0.0f, 1.0f) };
		field.thresholds[i].minimum = boundary;
		field.thresholds[i].maximum = std::max(field.thresholds[i].maximum, boundary);
	}
	field.thresholds.back().maximum = 1.0f;
}

static std::string NoiseRegionSourceLabel(const EditorState& e, const SceneLayer& layer, const NoiseThresholdRegion& region) {
	if (!region.enabled) return "Blank";
	if (layer.noise.target == NoiseTargetKind::Tile) {
		if (const auto* tile = FindTile(e, region.tile_id)) {
			return tile->name;
		}
		return "None";
	}
	if (region.prefab_index >= 0 && region.prefab_index < static_cast<int>(e.prefabs.size())) {
		return e.prefabs[static_cast<std::size_t>(region.prefab_index)].name;
	}
	return "None";
}

static void SplitNoiseRegion(EditorState& e, const SceneLayer& layer, NoiseField& field, float value) {
	value = std::clamp(value, 0.01f, 0.99f);
	if (field.thresholds.empty()) {
		field.thresholds.push_back(MakeNoiseThreshold(e, layer, 0.0f, value, false));
		field.thresholds.push_back(MakeNoiseThreshold(e, layer, value, 1.0f, false));
		return;
	}
	NormalizeNoiseThresholds(field);
	for (std::size_t i{}; i < field.thresholds.size(); ++i) {
		auto& region{ field.thresholds[i] };
		if (value <= region.minimum + 0.002f || value >= region.maximum - 0.002f) continue;
		NoiseThresholdRegion right{ region };
		right.minimum = value;
		region.maximum = value;
		field.thresholds.insert(field.thresholds.begin() + static_cast<std::ptrdiff_t>(i + 1), right);
		return;
	}
}

static void RemoveNoiseBoundary(NoiseField& field, int boundary_index) {
	if (boundary_index < 0 || boundary_index + 1 >= static_cast<int>(field.thresholds.size())) return;
	field.thresholds[static_cast<std::size_t>(boundary_index)].maximum = field.thresholds[static_cast<std::size_t>(boundary_index + 1)].maximum;
	field.thresholds.erase(field.thresholds.begin() + boundary_index + 1);
	NormalizeNoiseThresholds(field);
}

static void AddNoiseThresholdInLargestGap(EditorState& e, SceneLayer& layer, NoiseField& field) {
	if (field.thresholds.empty()) {
		field.thresholds.push_back(MakeNoiseThreshold(e, layer, 0.0f, 1.0f, true));
		return;
	}
	NormalizeNoiseThresholds(field);
	int blank_index{ -1 };
	float blank_width{ -1.0f };
	for (int i{}; i < static_cast<int>(field.thresholds.size()); ++i) {
		const auto& region{ field.thresholds[static_cast<std::size_t>(i)] };
		const float width{ region.maximum - region.minimum };
		if (!region.enabled && width > blank_width) {
			blank_width = width;
			blank_index = i;
		}
	}
	if (blank_index >= 0) {
		auto& region{ field.thresholds[static_cast<std::size_t>(blank_index)] };
		region.enabled = true;
		region.tile_id = e.active_tile_id;
		region.prefab_index = e.prefabs.empty() ? -1 : e.active_prefab_index;
		return;
	}
	int widest{};
	float width{};
	for (int i{}; i < static_cast<int>(field.thresholds.size()); ++i) {
		const auto& region{ field.thresholds[static_cast<std::size_t>(i)] };
		if (region.maximum - region.minimum > width) {
			width = region.maximum - region.minimum;
			widest = i;
		}
	}
	const auto source{ field.thresholds[static_cast<std::size_t>(widest)] };
	const float mid{ (source.minimum + source.maximum) * 0.5f };
	SplitNoiseRegion(e, layer, field, mid);
	if (widest + 1 < static_cast<int>(field.thresholds.size())) {
		auto& created{ field.thresholds[static_cast<std::size_t>(widest + 1)] };
		created.enabled = true;
		created.tile_id = e.active_tile_id;
		created.prefab_index = e.prefabs.empty() ? -1 : e.active_prefab_index;
	}
}

static std::string TruncatedLabel(std::string text, float available_px) {
	if (available_px < 28.0f) return "";
	const float approx_char{ 7.0f };
	const int max_chars{ std::max(3, static_cast<int>(available_px / approx_char)) };
	if (static_cast<int>(text.size()) <= max_chars) return text;
	return text.substr(0, static_cast<std::size_t>(std::max(1, max_chars - 3))) + "...";
}

static void DrawNoiseThresholdGradient(EditorState& e, SceneLayer& layer, NoiseField& field) {
	if (field.thresholds.empty()) field.thresholds.push_back(MakeNoiseThreshold(e, layer, 0.0f, 1.0f, false));
	NormalizeNoiseThresholds(field);
	const float width{ std::max(260.0f, ImGui::GetContentRegionAvail().x) };
	const float height{ 76.0f };
	const ImVec2 p0{ ImGui::GetCursorScreenPos() };
	ImGui::InvisibleButton("##noise_threshold_gradient", { width, height }, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const ImVec2 mouse{ ImGui::GetIO().MousePos };
	ImDrawList* dl{ ImGui::GetWindowDrawList() };
	const float bar_y0{ p0.y + 25.0f };
	const float bar_y1{ p0.y + 47.0f };
	for (int i{}; i < 96; ++i) {
		const float a{ static_cast<float>(i) / 96.0f };
		const float b{ static_cast<float>(i + 1) / 96.0f };
		const float v{ (a + b) * 0.5f };
		dl->AddRectFilled({ p0.x + a * width, bar_y0 }, { p0.x + b * width + 1.0f, bar_y1 }, ImGui::GetColorU32(ImVec4(v, v, v, 1.0f)));
	}
	dl->AddRect({ p0.x, bar_y0 }, { p0.x + width, bar_y1 }, ImGui::GetColorU32(ImGuiCol_Border));

	for (std::size_t i{}; i < field.thresholds.size(); ++i) {
		const auto& region{ field.thresholds[i] };
		const float x0{ p0.x + region.minimum * width };
		const float x1{ p0.x + region.maximum * width };
		if (region.enabled) dl->AddRect({ x0 + 1.0f, bar_y0 + 1.0f }, { x1 - 1.0f, bar_y1 - 1.0f }, ImGui::GetColorU32(ImVec4(0.95f, 0.78f, 0.26f, 0.95f)), 0.0f, 0, 2.0f);
		const std::string source{ TruncatedLabel(NoiseRegionSourceLabel(e, layer, region), std::max(0.0f, x1 - x0 - 4.0f)) };
		if (!source.empty()) {
			const ImVec2 text_size{ ImGui::CalcTextSize(source.c_str()) };
			const float tx{ std::clamp((x0 + x1 - text_size.x) * 0.5f, p0.x, p0.x + width - text_size.x) };
			const float ty{ (i % 2 == 0) ? p0.y + 3.0f : bar_y1 + 6.0f };
			dl->AddText({ tx, ty }, ImGui::GetColorU32(region.enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled), source.c_str());
		}
	}
	for (int i{}; i + 1 < static_cast<int>(field.thresholds.size()); ++i) {
		const float x{ p0.x + field.thresholds[static_cast<std::size_t>(i)].maximum * width };
		dl->AddLine({ x, bar_y0 - 5.0f }, { x, bar_y1 + 5.0f }, ImGui::GetColorU32(ImVec4(1.0f, 0.78f, 0.20f, 1.0f)), 2.0f);
		dl->AddTriangleFilled({ x - 4.0f, bar_y0 - 6.0f }, { x + 4.0f, bar_y0 - 6.0f }, { x, bar_y0 - 1.0f }, ImGui::GetColorU32(ImVec4(1.0f, 0.78f, 0.20f, 1.0f)));
	}

	static ImGuiID dragging_id{};
	static int dragging_boundary{ -1 };
	static std::optional<SceneSnapshot> drag_before;
	const ImGuiID widget_id{ ImGui::GetID("##noise_threshold_gradient") };
	auto nearest_boundary = [&]() {
		int nearest{ -1 };
		float best{ 9.0f };
		for (int i{}; i + 1 < static_cast<int>(field.thresholds.size()); ++i) {
			const float x{ p0.x + field.thresholds[static_cast<std::size_t>(i)].maximum * width };
			const float d{ std::abs(mouse.x - x) };
			if (d < best) { best = d; nearest = i; }
		}
		return nearest;
	};

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		const int nearest{ nearest_boundary() };
		if (nearest >= 0) {
			dragging_id = widget_id;
			dragging_boundary = nearest;
			drag_before = CaptureScene(e);
		} else {
			const SceneSnapshot before{ CaptureScene(e) };
			const float value{ std::clamp((mouse.x - p0.x) / width, 0.01f, 0.99f) };
			SplitNoiseRegion(e, layer, field, value);
			PushHistory(e, "Add Noise Stop", before);
		}
	}
	if (dragging_id == widget_id && dragging_boundary >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		const float value{ std::clamp((mouse.x - p0.x) / width, 0.0f, 1.0f) };
		const float lo{ field.thresholds[static_cast<std::size_t>(dragging_boundary)].minimum + 0.005f };
		const float hi{ field.thresholds[static_cast<std::size_t>(dragging_boundary + 1)].maximum - 0.005f };
		const float boundary{ std::clamp(value, lo, hi) };
		field.thresholds[static_cast<std::size_t>(dragging_boundary)].maximum = boundary;
		field.thresholds[static_cast<std::size_t>(dragging_boundary + 1)].minimum = boundary;
	}
	if (dragging_id == widget_id && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		if (drag_before) PushHistory(e, "Move Noise Stop", std::move(*drag_before));
		drag_before.reset();
		dragging_id = 0;
		dragging_boundary = -1;
	}
	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		const int nearest{ nearest_boundary() };
		if (nearest >= 0) {
			const SceneSnapshot before{ CaptureScene(e) };
			RemoveNoiseBoundary(field, nearest);
			PushHistory(e, "Remove Noise Stop", before);
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Left-click empty gradient space: add a stop / split the range.\n"
			"Left-drag a divider: move that stop.\n"
			"Right-click a divider: remove it and merge the neighboring ranges.\n"
			"Source labels alternate above/below so narrow adjacent ranges stay readable."
		);
	}
}

static bool SameGrid(const Tilemap& map, const RasterGrid& grid) {
	return std::abs(map.cell_size.x - grid.size.x) < 0.001f &&
		std::abs(map.cell_size.y - grid.size.y) < 0.001f &&
		std::abs(map.origin.x - grid.offset.x) < 0.001f &&
		std::abs(map.origin.y - grid.offset.y) < 0.001f;
}

static int ResolveNoiseBakeTilemap(EditorState& e, const SceneLayer& layer) {
	const RasterGrid grid{ NoiseRasterGrid(e, layer) };
	for (const auto& map : e.tilemaps) {
		if (SameGrid(map, grid)) return map.id;
	}

	Tilemap map;
	if (const auto* source = FindTilemap(e, layer.noise.tilemap_id)) {
		map = *source;
	}
	map.id = e.next_tilemap_id++;
	map.name = layer.name + " Grid";
	map.cell_size = grid.size;
	map.origin = grid.offset;
	e.tilemaps.push_back(std::move(map));
	return e.tilemaps.back().id;
}

static void ConvertBoundedNoiseLayer(EditorState& e, int layer_id) {
	SceneLayer* layer{ FindLayer(e, layer_id) };
	if (!layer || layer->kind != LayerKind::Noise || !layer->noise.bounded || layer->locked) return;

	const SceneSnapshot before{ CaptureScene(e) };
	const NoiseLayerData source{ layer->noise };
	const RasterGrid grid{ NoiseRasterGrid(e, *layer) };
	const auto [first, last]{ NoiseBoundedCellRange(e, *layer) };

	if (source.target == NoiseTargetKind::Tile) {
		const int tilemap_id{ ResolveNoiseBakeTilemap(e, *layer) };
		layer = FindLayer(e, layer_id);
		if (!layer) return;
		layer->kind = LayerKind::Tile;
		layer->purpose = LayerPurpose::Visual;
		layer->tile = {};
		layer->tile.tilemap_id = tilemap_id;
		layer->noise = {};

		const Tilemap* map{ FindTilemap(e, tilemap_id) };
		if (map) {
			for (int y{ first.y }; y <= last.y; ++y) {
				for (int x{ first.x }; x <= last.x; ++x) {
					const I2 cell{ x, y };
					const RectF rect{ RasterCellRect(grid, cell) };
					const F2 sample{ RectCenter(rect) };
					int tile_id{ -1 };
					const NoiseThresholdRegion* chosen_region{};
					for (const auto& field : source.fields) {
						if (!field.enabled) continue;
						if (const auto* region = FindNoiseThreshold(
								field,
								FractalNoiseValue(sample, field)
							);
							region && region->tile_id >= 0 && FindTile(e, region->tile_id)) {
							tile_id = region->tile_id;
							chosen_region = region;
						}
					}
					if (tile_id >= 0 && chosen_region) {
						SetTile(*layer, *map, cell, tile_id);
						if (auto* baked_cell = WriteTileCell(*layer, *map, cell)) {
							baked_cell->offset = NoiseTileOffset(
								e,
								grid,
								*map,
								tile_id,
								chosen_region->origin
							);
						}
					}
				}
			}
		}
	} else {
		layer->kind = LayerKind::Entity;
		layer->purpose = LayerPurpose::Visual;
		layer->tile = {};
		layer->noise = {};

		for (int y{ first.y }; y <= last.y; ++y) {
			for (int x{ first.x }; x <= last.x; ++x) {
				const I2 cell{ x, y };
				const RectF rect{ RasterCellRect(grid, cell) };
				const F2 sample{ RectCenter(rect) };
				for (const auto& field : source.fields) {
					if (!field.enabled) continue;
					const auto* region{ FindNoiseThreshold(field, FractalNoiseValue(sample, field)) };
					if (!region || region->prefab_index < 0 ||
						region->prefab_index >= static_cast<int>(e.prefabs.size())) continue;
					const auto& prefab{ e.prefabs[static_cast<std::size_t>(region->prefab_index)] };
					Entity entity;
					entity.id = e.next_entity_id++;
					entity.layer_id = layer_id;
					entity.prefab = prefab.name;
					entity.size = prefab.size;
					entity.origin = region->origin;
					entity.position = NoiseEntityPlacement(grid, cell, region->origin);
					e.entities.push_back(std::move(entity));
				}
			}
		}
	}

	DeselectAll(e);
	PushHistory(e, source.target == NoiseTargetKind::Tile
		? "Convert Noise to Tile Layer"
		: "Convert Noise to Entity Layer", before);
}

static void DrawNoiseLayerPalette(EditorState& e, SceneLayer& layer) {
	int target{ static_cast<int>(layer.noise.target) };
	const char* target_names[]{ "Entities", "Tiles" };
	ImGui::SetNextItemWidth(105.0f);
	if (ImGui::Combo("Generate", &target, target_names, 2)) {
		layer.noise.target = static_cast<NoiseTargetKind>(target);
	}
	ItemTooltip("Choose whether threshold ranges preview prefab entities or tiles.");

	if (layer.noise.target == NoiseTargetKind::Tile && !e.tilemaps.empty()) {
		if (!FindTilemap(e, layer.noise.tilemap_id)) {
			layer.noise.tilemap_id = e.tilemaps.front().id;
		}
		ImGui::SameLine();
		const auto* map{ FindTilemap(e, layer.noise.tilemap_id) };
		ImGui::SetNextItemWidth(130.0f);
		if (ImGui::BeginCombo("Tilemap", map ? map->name.c_str() : "<none>")) {
			for (const auto& candidate : e.tilemaps) {
				if (ImGui::Selectable(candidate.name.c_str(), candidate.id == layer.noise.tilemap_id)) {
					layer.noise.tilemap_id = candidate.id;
				}
			}
			ImGui::EndCombo();
		}
		ItemTooltip("Optional tilemap/chunk template used when this bounded noise layer is baked to an ordinary tile layer. The live procedural grid size is controlled from the viewport toolbar.");
	}

	ImGui::SameLine();
	ImGui::Checkbox("Noise", &layer.noise.show_noise_preview);
	ItemTooltip("Toggle the grayscale procedural noise preview in the viewport.");
	ImGui::SameLine();
	ImGui::Checkbox("Spawn", &layer.noise.show_generated_preview);
	ItemTooltip("Toggle the generated entity/tile preview from threshold ranges.");
	if (layer.noise.show_noise_preview) {
		ImGui::SameLine();
		ImGui::SetNextItemWidth(75.0f);
		ImGui::SliderFloat("Alpha", &layer.noise.noise_preview_alpha, 0.05f, 1.0f, "%.2f");
		ItemTooltip("Opacity of the grayscale field when Noise preview is enabled.");
	}

	const bool was_bounded{ layer.noise.bounded };
	ImGui::Checkbox("Bounded", &layer.noise.bounded);
	ItemTooltip("Limit this noise layer to a finite grid-aligned world-space rectangle. Bounded layers can be baked into ordinary editable Entity/Tile layers.");
	if (!was_bounded && layer.noise.bounded) {
		SetNoiseBoundsToCurrentViewport(e, layer);
	}
	if (layer.noise.bounded) {
		const auto [boundary_origin, boundary_size]{
			NoiseBoundaryCellRect(e, layer)
		};
		int origin_cells[2]{ boundary_origin.x, boundary_origin.y };
		int size_cells[2]{ boundary_size.x, boundary_size.y };

		ImGui::SameLine();
		ImGui::SetNextItemWidth(135.0f);
		if (ImGui::DragInt2(
				"Origin Cell##noise_bounds",
				origin_cells,
				0.10f,
				-100000,
				100000
			)) {
			SetNoiseBoundaryFromCellRect(
				e,
				layer,
				{ origin_cells[0], origin_cells[1] },
				boundary_size
			);
		}
		ItemTooltip(
			"Top-left boundary in noise-grid cell coordinates. One unit equals one full noise-grid cell."
		);

		ImGui::SameLine();
		ImGui::SetNextItemWidth(125.0f);
		if (ImGui::DragInt2(
				"Size Cells##noise_bounds",
				size_cells,
				0.10f,
				1,
				100000
			)) {
			SetNoiseBoundaryFromCellRect(
				e,
				layer,
				{ origin_cells[0], origin_cells[1] },
				{ std::max(1, size_cells[0]), std::max(1, size_cells[1]) }
			);
		}
		ItemTooltip(
			"Width and height of the bounded noise area measured in whole noise-grid cells."
		);

		ImGui::SameLine();
		if (ImGui::SmallButton("Viewport Bounds")) {
			SetNoiseBoundsToCurrentViewport(e, layer);
		}
		ItemTooltip(
			"Reset the finite noise boundary to the currently visible viewport, expanded outward to whole noise-grid cells."
		);

		ImGui::SameLine();
		if (ImGui::SmallButton("Frame Bounds")) {
			FrameNoiseBoundsInViewport(e, layer);
		}
		ItemTooltip(
			"Pan and zoom the viewport so the complete bounded noise region is visible."
		);
	}

	static int editing_noise_layer_id{ -1 };
	static int editing_noise_field_index{ -1 };
	static int focus_noise_field_index{ -1 };
	static std::string original_noise_field_name;
	static std::optional<SceneSnapshot> noise_field_rename_before;

	int remove_field{ -1 };
	for (int field_index{}; field_index < static_cast<int>(layer.noise.fields.size()); ++field_index) {
		auto& field{ layer.noise.fields[static_cast<std::size_t>(field_index)] };
		ImGui::PushID(field_index);

		bool begin_edit{};
		bool began_edit_this_frame{};
		bool commit_rename{};
		bool cancel_rename{};
		ImVec2 name_input_min{};
		ImVec2 name_input_max{};
		bool name_input_drawn{};
		bool name_input_hovered{};

		const bool editing_before_draw{
			editing_noise_layer_id == layer.id &&
			editing_noise_field_index == field_index
		};

		if (!field.enabled) {
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.29f, 0.29f, 0.29f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.33f, 0.33f, 0.33f, 1.0f));
		}

		const bool open{ ImGui::TreeNodeEx(
			"##noise_field",
			ImGuiTreeNodeFlags_DefaultOpen |
				ImGuiTreeNodeFlags_FramePadding |
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_OpenOnArrow,
			"%s",
			editing_before_draw ? "" : field.name.c_str()
		) };

		if (!field.enabled) {
			ImGui::PopStyleColor(3);
		}

		const bool tree_hovered{ ImGui::IsItemHovered() };
		const ImVec2 tree_min{ ImGui::GetItemRectMin() };
		const ImVec2 tree_max{ ImGui::GetItemRectMax() };
		const ImVec2 mouse{ ImGui::GetMousePos() };
		const float row_height{ ImGui::GetFrameHeight() };
		const float text_start_x{ tree_min.x + row_height };
		const float minimum_name_width{ 48.0f };
		const float visible_name_width{
			std::max(minimum_name_width, ImGui::CalcTextSize(field.name.c_str()).x)
		};
		const float name_hit_end_x{
			std::min(tree_max.x, text_start_x + visible_name_width)
		};
		const bool name_hit_hovered{
			tree_hovered && mouse.x >= text_start_x && mouse.x <= name_hit_end_x
		};

		// Match the tile-palette rename interaction exactly: only a double click on
		// the visible name begins inline editing. The disclosure arrow retains its
		// normal tree-node behavior and does not start a rename.
		if (!editing_before_draw && name_hit_hovered &&
			ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			begin_edit = true;
		}

		if (ImGui::BeginPopupContextItem("NoiseFieldContext")) {
			if (ImGui::MenuItem("Rename")) {
				begin_edit = true;
			}
			if (ImGui::MenuItem(field.enabled ? "Disable" : "Enable")) {
				const SceneSnapshot before{ CaptureScene(e) };
				field.enabled = !field.enabled;
				PushHistory(
					e,
					field.enabled ? "Enable Noise Field" : "Disable Noise Field",
					before
				);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete Noise Field")) {
				remove_field = field_index;
			}
			ImGui::EndPopup();
		}

		if (begin_edit) {
			editing_noise_layer_id = layer.id;
			editing_noise_field_index = field_index;
			focus_noise_field_index = field_index;
			original_noise_field_name = field.name;
			noise_field_rename_before = CaptureScene(e);
			began_edit_this_frame = true;
		}

		if (editing_noise_layer_id == layer.id &&
			editing_noise_field_index == field_index) {
			ImGui::SetCursorScreenPos({ text_start_x, tree_min.y });
			ImGui::SetNextItemWidth(std::max(
				minimum_name_width,
				tree_max.x - text_start_x - ImGui::GetStyle().FramePadding.x
			));
			if (focus_noise_field_index == field_index) {
				ImGui::SetKeyboardFocusHere();
				focus_noise_field_index = -1;
			}

			const bool submitted{ ImGui::InputText(
				"##noise_field_rename",
				&field.name,
				ImGuiInputTextFlags_EnterReturnsTrue |
					ImGuiInputTextFlags_AutoSelectAll
			) };
			name_input_min = ImGui::GetItemRectMin();
			name_input_max = ImGui::GetItemRectMax();
			name_input_drawn = true;
			name_input_hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();

			if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
				cancel_rename = true;
			} else if (submitted) {
				commit_rename = true;
			}
		}

		if (tree_hovered &&
			!(editing_noise_layer_id == layer.id &&
			  editing_noise_field_index == field_index)) {
			ImGui::SetTooltip(
				field.enabled
					? "Double click the field name to rename. Right click to disable, rename, or remove it."
					: "Disabled field. Double click the name to rename; right click to enable, rename, or remove it."
			);
		}

		if (editing_noise_layer_id == layer.id &&
			editing_noise_field_index == field_index) {
			if (cancel_rename) {
				field.name = original_noise_field_name;
				editing_noise_layer_id = -1;
				editing_noise_field_index = -1;
				focus_noise_field_index = -1;
				noise_field_rename_before.reset();
			} else {
				if (name_input_drawn && !began_edit_this_frame) {
					const bool clicked{
						ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
						ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
						ImGui::IsMouseClicked(ImGuiMouseButton_Right)
					};
					const ImVec2 click_mouse{ ImGui::GetMousePos() };
					const bool inside_input{
						click_mouse.x >= name_input_min.x && click_mouse.x <= name_input_max.x &&
						click_mouse.y >= name_input_min.y && click_mouse.y <= name_input_max.y
					};
					if (clicked && !inside_input && !name_input_hovered) {
						commit_rename = true;
					}
				}

				if (commit_rename) {
					if (field.name.empty()) {
						field.name = std::string{ NoiseTypeName(field.type) } + " Field";
					}
					if (noise_field_rename_before &&
						field.name != original_noise_field_name) {
						PushHistory(
							e,
							"Rename Noise Field",
							std::move(*noise_field_rename_before)
						);
					}
					editing_noise_layer_id = -1;
					editing_noise_field_index = -1;
					focus_noise_field_index = -1;
					noise_field_rename_before.reset();
				}
			}
		}

		if (open) {
			int type{ static_cast<int>(field.type) };
			const char* types[]{ "Perlin", "Simplex", "Value" };
			ImGui::SetNextItemWidth(95.0f);
			if (ImGui::Combo("Type", &type, types, 3)) {
				field.type = static_cast<NoiseType>(type);
			}
			ItemTooltip("Base noise algorithm. Perlin is gradient noise, Simplex uses simplex cells, and Value interpolates random lattice values.");

			ImGui::SetNextItemWidth(82.0f);
			ImGui::DragInt("Seed", &field.seed, 1.0f);
			ItemTooltip("Seed controlling this field's deterministic pattern.");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.0f);
			ImGui::DragFloat("Frequency", &field.frequency, 0.00025f, 0.0001f, 2.0f, "%.4f");
			field.frequency = std::max(0.0001f, field.frequency);
			ItemTooltip("Base spatial frequency. Lower values create larger coherent features.");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(72.0f);
			ImGui::SliderInt("Octaves", &field.octaves, 1, 10);
			ItemTooltip("Number of fractal layers. Each octave adds finer detail.");

			ImGui::SetNextItemWidth(86.0f);
			ImGui::DragFloat("Lacunarity", &field.lacunarity, 0.01f, 1.0f, 4.0f, "%.2f");
			field.lacunarity = std::max(1.0f, field.lacunarity);
			ItemTooltip("Frequency multiplier from one octave to the next; 2.0 is common.");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(86.0f);
			ImGui::SliderFloat("Persistence", &field.persistence, 0.0f, 1.0f, "%.2f");
			ItemTooltip("Amplitude multiplier between octaves. Higher persistence retains more high-frequency detail.");
			ImGui::SameLine();
			float offset[2]{ field.offset.x, field.offset.y };
			ImGui::SetNextItemWidth(130.0f);
			if (ImGui::DragFloat2("Offset", offset, 1.0f)) {
				field.offset = { offset[0], offset[1] };
			}
			ItemTooltip("Translate this noise field through world space without changing the seed.");

			DrawNoiseThresholdGradient(e, layer, field);
			ImGui::SeparatorText("Threshold Regions");
			if (ImGui::BeginTable(
					"##thresholds",
					5,
					ImGuiTableFlags_RowBg |
					ImGuiTableFlags_BordersInnerV |
					ImGuiTableFlags_SizingStretchProp
				)) {
				ImGui::TableSetupColumn("Spawn", ImGuiTableColumnFlags_WidthFixed, 48.0f);
				ImGui::TableSetupColumn("Noise Range", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn("Spawn Source", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Origin", ImGuiTableColumnFlags_WidthFixed, 105.0f);
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 28.0f);
				ImGui::TableHeadersRow();
				int remove_region{ -1 };
				for (int i{}; i < static_cast<int>(field.thresholds.size()); ++i) {
					auto& region{ field.thresholds[static_cast<std::size_t>(i)] };
					ImGui::PushID(i);
					ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
					ImGui::TableSetColumnIndex(0);
					ImGui::Checkbox("##enabled", &region.enabled);
					ItemTooltip("Enabled ranges generate their configured source. A source of <none> still produces no content.");
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%.3f - %.3f", region.minimum, region.maximum);
					ItemTooltip("Range boundaries are edited by dragging the stops on the grayscale gradient above.");
					ImGui::TableSetColumnIndex(2);
					ImGui::BeginDisabled(!region.enabled);
					ImGui::SetNextItemWidth(-1.0f);
					if (layer.noise.target == NoiseTargetKind::Tile) {
						DrawTileSourceComboAll(e, "##source", region.tile_id);
					} else {
						DrawPrefabSourceComboAll(e, "##source", region.prefab_index);
					}
					ImGui::EndDisabled();
					ItemTooltip(layer.noise.target == NoiseTargetKind::Tile
						? "Choose any tile from any palette for this noise band."
						: "Choose any prefab from any prefab group for this noise band.");
					ImGui::TableSetColumnIndex(3);
					int origin{ static_cast<int>(region.origin) };
					const char* origins[]{
						"Top Left", "Top", "Top Right",
						"Left", "Center", "Right",
						"Bottom Left", "Bottom", "Bottom Right"
					};
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::Combo("##origin", &origin, origins, 9)) {
						region.origin = static_cast<EntityOrigin>(origin);
					}
					ItemTooltip(
						layer.noise.target == NoiseTargetKind::Entity
							? "Origin point of the spawned prefab inside the noise grid cell."
							: "Align this tile inside the noise grid cell. This matters when the tile is smaller or larger than the grid cell."
					);
					ImGui::TableSetColumnIndex(4);
					if (ImGui::SmallButton("x")) {
						remove_region = i;
					}
					ItemTooltip("Remove this range by merging it with a neighbor. The remaining neighbor's source is preserved.");
					ImGui::PopID();
				}
				if (remove_region >= 0 && field.thresholds.size() > 1) {
					const SceneSnapshot before{ CaptureScene(e) };
					const int boundary{
						remove_region == static_cast<int>(field.thresholds.size()) - 1
							? remove_region - 1
							: remove_region
					};
					RemoveNoiseBoundary(field, std::max(0, boundary));
					PushHistory(e, "Remove Noise Range", before);
				}
				ImGui::EndTable();
			}
			if (ImGui::Button("+ Add Range")) {
				const SceneSnapshot before{ CaptureScene(e) };
				AddNoiseThresholdInLargestGap(e, layer, field);
				PushHistory(e, "Add Noise Range", before);
			}
			ItemTooltip("Enable the largest blank range, or split the widest range when no blank band exists.");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	if (remove_field >= 0) {
		const SceneSnapshot before{ CaptureScene(e) };
		layer.noise.fields.erase(layer.noise.fields.begin() + remove_field);
		if (editing_noise_layer_id == layer.id) {
			editing_noise_layer_id = -1;
			editing_noise_field_index = -1;
			focus_noise_field_index = -1;
			noise_field_rename_before.reset();
		}
		PushHistory(e, "Remove Noise Field", before);
	}

	if (ImGui::Button("+ Add Noise Field")) {
		ImGui::OpenPopup("Add Noise Field Type");
	}
	ItemTooltip("Add a new independent noise field. Choose the algorithm from the popup; zero fields is also valid.");
	if (ImGui::BeginPopup("Add Noise Field Type")) {
		for (NoiseType type : { NoiseType::Perlin, NoiseType::Simplex, NoiseType::Value }) {
			if (ImGui::Selectable(NoiseTypeName(type))) {
				const SceneSnapshot before{ CaptureScene(e) };
				NoiseField field;
				field.type = type;
				field.name = std::string{ NoiseTypeName(type) } + " " + std::to_string(layer.noise.fields.size() + 1);
				field.seed = 1337 + static_cast<int>(layer.noise.fields.size()) * 997;
				NoiseThresholdRegion initial_region;
				initial_region.minimum = 0.0f;
				initial_region.maximum = 1.0f;
				initial_region.enabled = true;
				initial_region.tile_id = -1;
				initial_region.prefab_index = -1;
				field.thresholds.push_back(initial_region);
				layer.noise.fields.push_back(std::move(field));
				PushHistory(e, "Add Noise Field", before);
			}
		}
		ImGui::EndPopup();
	}
	if (layer.noise.fields.empty()) {
		ImGui::TextDisabled("This noise layer currently contains no noise fields.");
	}
}


static std::vector<int> RecipeTileIds(const EditorState& e, const PaintRecipe& recipe) {
	std::vector<int> result;
	auto add = [&](int id) { if (id >= 0 && std::find(result.begin(), result.end(), id) == result.end()) result.push_back(id); };
	switch (recipe.source_kind) {
		case PaintSourceKind::Single: add(recipe.tile_id); break;
		case PaintSourceKind::Checkerboard: add(recipe.tile_id); add(recipe.secondary_tile_id); break;
		case PaintSourceKind::WeightedSet:
			if (const auto* set = FindWeightedTileSet(e, recipe.weighted_tile_set_id)) for (const auto& entry : set->entries) add(entry.tile_id);
			break;
		case PaintSourceKind::Autotile:
			if (const auto* rules = FindAutotileRuleSet(e, recipe.autotile_ruleset_id)) for (int id : rules->tile_ids) add(id);
			break;
		case PaintSourceKind::Noise:
			for (const auto& region : recipe.noise.thresholds) {
				if (!region.enabled) continue;
				if (region.source_kind == PaintSourceKind::Single) add(region.tile_id);
				else if (region.source_kind == PaintSourceKind::WeightedSet) {
					if (const auto* set = FindWeightedTileSet(e, region.weighted_tile_set_id)) for (const auto& entry : set->entries) add(entry.tile_id);
				}
			}
			break;
	}
	return result;
}

static std::vector<int> RecipePrefabIndices(const EditorState& e, const PaintRecipe& recipe) {
	std::vector<int> result;
	auto add = [&](int id) { if (id >= 0 && id < static_cast<int>(e.prefabs.size()) && std::find(result.begin(), result.end(), id) == result.end()) result.push_back(id); };
	switch (recipe.source_kind) {
		case PaintSourceKind::Single: add(recipe.prefab_index); break;
		case PaintSourceKind::Checkerboard: add(recipe.prefab_index); add(recipe.secondary_prefab_index); break;
		case PaintSourceKind::WeightedSet:
			if (const auto* set = FindWeightedPrefabSet(e, recipe.weighted_prefab_set_id)) for (const auto& entry : set->entries) add(entry.prefab_index);
			break;
		case PaintSourceKind::Autotile: break;
		case PaintSourceKind::Noise:
			for (const auto& region : recipe.noise.thresholds) {
				if (!region.enabled) continue;
				if (region.source_kind == PaintSourceKind::Single) add(region.prefab_index);
				else if (region.source_kind == PaintSourceKind::WeightedSet) {
					if (const auto* set = FindWeightedPrefabSet(e, region.weighted_prefab_set_id)) for (const auto& entry : set->entries) add(entry.prefab_index);
				}
			}
			break;
	}
	return result;
}

static bool TilePaintModeApplicable(const EditorState& e, const SceneLayer& layer) {
	if (layer.kind != LayerKind::Tile || e.recipe.source_kind == PaintSourceKind::Autotile) return false;
	const auto* map{ FindTilemap(e, layer.tile.tilemap_id) };
	if (!map) return false;
	for (int id : RecipeTileIds(e, e.recipe)) {
		if (const auto* tile = FindTile(e, id); tile &&
			(static_cast<float>(tile->pixel_w) > map->cell_size.x || static_cast<float>(tile->pixel_h) > map->cell_size.y)) return true;
	}
	return false;
}

static bool TileOriginApplicable(const EditorState& e, const SceneLayer& layer) {
	if (layer.kind != LayerKind::Tile || e.recipe.source_kind == PaintSourceKind::Autotile) return false;
	const auto* map{ FindTilemap(e, layer.tile.tilemap_id) };
	if (!map) return false;
	for (int id : RecipeTileIds(e, e.recipe)) {
		if (const auto* tile = FindTile(e, id); tile &&
			(tile->pixel_w != static_cast<int>(std::lround(map->cell_size.x)) || tile->pixel_h != static_cast<int>(std::lround(map->cell_size.y)))) return true;
	}
	return false;
}

static bool EntityOriginApplicable(const EditorState& e) {
	for (int index : RecipePrefabIndices(e, e.recipe)) {
		const auto& prefab{ e.prefabs[static_cast<std::size_t>(index)] };
		if (std::abs(prefab.size.x - e.grid.size.x) > 0.01f || std::abs(prefab.size.y - e.grid.size.y) > 0.01f) return true;
	}
	return false;
}

static void DrawOriginCombo(const char* label, EntityOrigin& origin) {
	const char* origins[]{ "Top Left", "Top", "Top Right", "Left", "Center", "Right", "Bottom Left", "Bottom", "Bottom Right" };
	int value{ static_cast<int>(origin) };
	ImGui::SetNextItemWidth(125.0f);
	if (ImGui::Combo(label, &value, origins, 9)) origin = static_cast<EntityOrigin>(value);
}

static void EnsureRecipeNoiseThreshold(EditorState& e, const SceneLayer& layer) {
	if (!e.recipe.noise.thresholds.empty()) return;
	NoiseThresholdRegion region;
	region.minimum = 0.0f;
	region.maximum = 1.0f;
	region.enabled = true;
	region.source_kind = PaintSourceKind::Single;
	if (layer.kind == LayerKind::Tile) region.tile_id = e.recipe.tile_id;
	else region.prefab_index = e.recipe.prefab_index;
	e.recipe.noise.thresholds.push_back(region);
}

static std::string RecipeNoiseRegionSourceLabel(
	const EditorState& e,
	const SceneLayer& layer,
	const NoiseThresholdRegion& region
) {
	if (!region.enabled) return "None";
	if (region.source_kind == PaintSourceKind::WeightedSet) {
		if (layer.kind == LayerKind::Tile) {
			if (const auto* set = FindWeightedTileSet(e, region.weighted_tile_set_id)) return set->name;
		} else {
			if (const auto* set = FindWeightedPrefabSet(e, region.weighted_prefab_set_id)) return set->name;
		}
		return "None";
	}
	if (layer.kind == LayerKind::Tile) {
		if (const auto* tile = FindTile(e, region.tile_id)) return tile->name;
		return "None";
	}
	if (region.prefab_index >= 0 && region.prefab_index < static_cast<int>(e.prefabs.size())) {
		return e.prefabs[static_cast<std::size_t>(region.prefab_index)].name;
	}
	return "None";
}

static void SplitRecipeNoiseRegion(NoiseField& field, float value) {
	value = std::clamp(value, 0.01f, 0.99f);
	NormalizeNoiseThresholds(field);
	for (std::size_t i{}; i < field.thresholds.size(); ++i) {
		auto& region{ field.thresholds[i] };
		if (value <= region.minimum + 0.002f || value >= region.maximum - 0.002f) continue;
		NoiseThresholdRegion right{ region };
		right.minimum = value;
		region.maximum = value;
		field.thresholds.insert(field.thresholds.begin() + static_cast<std::ptrdiff_t>(i + 1), right);
		return;
	}
}

static void DrawRecipeNoiseThresholdGradient(EditorState& e, SceneLayer& layer) {
	auto& field{ e.recipe.noise };
	EnsureRecipeNoiseThreshold(e, layer);
	NormalizeNoiseThresholds(field);

	const float width{ std::max(260.0f, ImGui::GetContentRegionAvail().x) };
	const float height{ 76.0f };
	const ImVec2 p0{ ImGui::GetCursorScreenPos() };
	ImGui::InvisibleButton(
		"##recipe_noise_threshold_gradient",
		{ width, height },
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
	);
	const ImVec2 mouse{ ImGui::GetIO().MousePos };
	ImDrawList* dl{ ImGui::GetWindowDrawList() };
	const float bar_y0{ p0.y + 25.0f };
	const float bar_y1{ p0.y + 47.0f };

	for (int i{}; i < 96; ++i) {
		const float a{ static_cast<float>(i) / 96.0f };
		const float b{ static_cast<float>(i + 1) / 96.0f };
		const float v{ (a + b) * 0.5f };
		dl->AddRectFilled(
			{ p0.x + a * width, bar_y0 },
			{ p0.x + b * width + 1.0f, bar_y1 },
			ImGui::GetColorU32(ImVec4(v, v, v, 1.0f))
		);
	}
	dl->AddRect({ p0.x, bar_y0 }, { p0.x + width, bar_y1 }, ImGui::GetColorU32(ImGuiCol_Border));

	for (std::size_t i{}; i < field.thresholds.size(); ++i) {
		const auto& region{ field.thresholds[i] };
		const float x0{ p0.x + region.minimum * width };
		const float x1{ p0.x + region.maximum * width };
		if (region.enabled) {
			dl->AddRect(
				{ x0 + 1.0f, bar_y0 + 1.0f },
				{ x1 - 1.0f, bar_y1 - 1.0f },
				ImGui::GetColorU32(ImVec4(0.95f, 0.78f, 0.26f, 0.95f)),
				0.0f,
				0,
				2.0f
			);
		}
		const std::string source{
			TruncatedLabel(
				RecipeNoiseRegionSourceLabel(e, layer, region),
				std::max(0.0f, x1 - x0 - 4.0f)
			)
		};
		if (!source.empty()) {
			const ImVec2 text_size{ ImGui::CalcTextSize(source.c_str()) };
			const float tx{ std::clamp((x0 + x1 - text_size.x) * 0.5f, p0.x, p0.x + width - text_size.x) };
			const float ty{ (i % 2 == 0) ? p0.y + 3.0f : bar_y1 + 6.0f };
			dl->AddText(
				{ tx, ty },
				ImGui::GetColorU32(region.enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled),
				source.c_str()
			);
		}
	}

	for (int i{}; i + 1 < static_cast<int>(field.thresholds.size()); ++i) {
		const float x{ p0.x + field.thresholds[static_cast<std::size_t>(i)].maximum * width };
		dl->AddLine(
			{ x, bar_y0 - 5.0f },
			{ x, bar_y1 + 5.0f },
			ImGui::GetColorU32(ImVec4(1.0f, 0.78f, 0.20f, 1.0f)),
			2.0f
		);
		dl->AddTriangleFilled(
			{ x - 4.0f, bar_y0 - 6.0f },
			{ x + 4.0f, bar_y0 - 6.0f },
			{ x, bar_y0 - 1.0f },
			ImGui::GetColorU32(ImVec4(1.0f, 0.78f, 0.20f, 1.0f))
		);
	}

	static ImGuiID dragging_id{};
	static int dragging_boundary{ -1 };
	const ImGuiID widget_id{ ImGui::GetID("##recipe_noise_threshold_gradient") };
	auto nearest_boundary = [&]() {
		int nearest{ -1 };
		float best{ 9.0f };
		for (int i{}; i + 1 < static_cast<int>(field.thresholds.size()); ++i) {
			const float x{ p0.x + field.thresholds[static_cast<std::size_t>(i)].maximum * width };
			const float d{ std::abs(mouse.x - x) };
			if (d < best) {
				best = d;
				nearest = i;
			}
		}
		return nearest;
	};

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		const int nearest{ nearest_boundary() };
		if (nearest >= 0) {
			dragging_id = widget_id;
			dragging_boundary = nearest;
		} else {
			SplitRecipeNoiseRegion(
				field,
				std::clamp((mouse.x - p0.x) / width, 0.01f, 0.99f)
			);
		}
	}
	if (
		dragging_id == widget_id &&
		dragging_boundary >= 0 &&
		ImGui::IsMouseDown(ImGuiMouseButton_Left)
	) {
		const float value{ std::clamp((mouse.x - p0.x) / width, 0.0f, 1.0f) };
		const float lo{ field.thresholds[static_cast<std::size_t>(dragging_boundary)].minimum + 0.005f };
		const float hi{ field.thresholds[static_cast<std::size_t>(dragging_boundary + 1)].maximum - 0.005f };
		const float boundary{ std::clamp(value, lo, hi) };
		field.thresholds[static_cast<std::size_t>(dragging_boundary)].maximum = boundary;
		field.thresholds[static_cast<std::size_t>(dragging_boundary + 1)].minimum = boundary;
	}
	if (dragging_id == widget_id && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		dragging_id = 0;
		dragging_boundary = -1;
	}
	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		const int nearest{ nearest_boundary() };
		if (nearest >= 0) RemoveNoiseBoundary(field, nearest);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Left-click empty gradient space: split a range.\n"
			"Left-drag a divider: move the threshold.\n"
			"Right-click a divider: remove it and merge its neighboring ranges."
		);
	}
}

static bool NoiseThresholdOriginApplicable(
	const EditorState& e,
	const SceneLayer& layer,
	const NoiseThresholdRegion& region
) {
	if (!region.enabled) return false;
	const RasterGrid grid{ ActiveRasterGrid(e) };
	if (region.source_kind == PaintSourceKind::Single) {
		if (layer.kind == LayerKind::Tile) {
			if (const auto* tile = FindTile(e, region.tile_id)) {
				return std::abs(static_cast<float>(tile->pixel_w) - grid.size.x) > 0.01f ||
					std::abs(static_cast<float>(tile->pixel_h) - grid.size.y) > 0.01f;
			}
		} else if (region.prefab_index >= 0 && region.prefab_index < static_cast<int>(e.prefabs.size())) {
			const auto& prefab{ e.prefabs[static_cast<std::size_t>(region.prefab_index)] };
			return std::abs(prefab.size.x - grid.size.x) > 0.01f ||
				std::abs(prefab.size.y - grid.size.y) > 0.01f;
		}
	}
	// A weighted set may contain heterogeneous sizes, so origin remains meaningful.
	return region.source_kind == PaintSourceKind::WeightedSet;
}

static void DrawRecipeNoiseSettings(EditorState& e, SceneLayer& layer) {
	EnsureRecipeNoiseThreshold(e, layer);
	ImGui::SeparatorText("Noise Source");

	const char* types[]{ "Perlin", "Simplex", "Value" };
	int type{ static_cast<int>(e.recipe.noise.type) };
	ImGui::SetNextItemWidth(110.0f);
	if (ImGui::Combo("Type", &type, types, 3)) e.recipe.noise.type = static_cast<NoiseType>(type);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	ImGui::DragInt("Seed", &e.recipe.noise.seed);
	ImGui::SetNextItemWidth(110.0f);
	ImGui::DragFloat("Frequency", &e.recipe.noise.frequency, 0.001f, 0.0001f, 1.0f, "%.4f");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	ImGui::DragInt("Octaves", &e.recipe.noise.octaves, 0.2f, 1, 12);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	ImGui::DragFloat("Lacunarity", &e.recipe.noise.lacunarity, 0.02f, 1.0f, 8.0f, "%.2f");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	ImGui::SliderFloat("Persistence", &e.recipe.noise.persistence, 0.0f, 1.0f, "%.2f");

	ImGui::Checkbox("Noise Preview", &e.recipe.show_noise_preview);
	ItemTooltip("Overlay the raw grayscale noise only inside the painted generator geometry.");
	if (e.recipe.show_noise_preview) {
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat("Opacity", &e.recipe.noise_preview_alpha, 0.0f, 1.0f, "%.2f");
	}
	ImGui::SameLine();
	ImGui::Checkbox(layer.kind == LayerKind::Tile ? "Tile Preview" : "Entity Preview", &e.recipe.show_generated_preview);
	ItemTooltip("Preview the threshold-resolved tiles/entities on top of the raw noise overlay.");

	ImGui::SeparatorText("Noise Thresholds");
	DrawRecipeNoiseThresholdGradient(e, layer);

	int remove_region{ -1 };
	for (int i{}; i < static_cast<int>(e.recipe.noise.thresholds.size()); ++i) {
		auto& region{ e.recipe.noise.thresholds[static_cast<std::size_t>(i)] };
		ImGui::PushID(i);
		ImGui::Text("%.2f - %.2f", region.minimum, region.maximum);
		ImGui::SameLine();
		bool none{ !region.enabled };
		const char* kinds[]{ "None", "Single", "Weighted Set" };
		int kind{ none ? 0 : (region.source_kind == PaintSourceKind::WeightedSet ? 2 : 1) };
		ImGui::SetNextItemWidth(112.0f);
		if (ImGui::Combo("##noise_source_kind", &kind, kinds, 3)) {
			region.enabled = kind != 0;
			if (kind == 1) region.source_kind = PaintSourceKind::Single;
			if (kind == 2) region.source_kind = PaintSourceKind::WeightedSet;
		}
		if (region.enabled) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(170.0f);
			if (layer.kind == LayerKind::Tile) {
				if (region.source_kind == PaintSourceKind::Single) {
					DrawTileSourceComboAll(e, "##noise_source", region.tile_id);
				} else {
					const auto* set{ FindWeightedTileSet(e, region.weighted_tile_set_id) };
					if (ImGui::BeginCombo("##noise_source", set ? set->name.c_str() : "<none>")) {
						if (ImGui::Selectable("<none>", region.weighted_tile_set_id < 0)) region.weighted_tile_set_id = -1;
						for (const auto& candidate : e.weighted_tile_sets) {
							if (ImGui::Selectable(candidate.name.c_str(), candidate.id == region.weighted_tile_set_id)) region.weighted_tile_set_id = candidate.id;
						}
						ImGui::EndCombo();
					}
				}
			} else {
				if (region.source_kind == PaintSourceKind::Single) {
					DrawPrefabSourceComboAll(e, "##noise_source", region.prefab_index);
				} else {
					const auto* set{ FindWeightedPrefabSet(e, region.weighted_prefab_set_id) };
					if (ImGui::BeginCombo("##noise_source", set ? set->name.c_str() : "<none>")) {
						if (ImGui::Selectable("<none>", region.weighted_prefab_set_id < 0)) region.weighted_prefab_set_id = -1;
						for (const auto& candidate : e.weighted_prefab_sets) {
							if (ImGui::Selectable(candidate.name.c_str(), candidate.id == region.weighted_prefab_set_id)) region.weighted_prefab_set_id = candidate.id;
						}
						ImGui::EndCombo();
					}
				}
			}
			if (NoiseThresholdOriginApplicable(e, layer, region)) {
				ImGui::SameLine();
				DrawOriginCombo("Origin##noise", region.origin);
			}
		}
		if (e.recipe.noise.thresholds.size() > 1) {
			ImGui::SameLine();
			if (ImGui::SmallButton("x")) remove_region = i;
		}
		ImGui::PopID();
	}
	if (remove_region >= 0 && e.recipe.noise.thresholds.size() > 1) {
		const int boundary{ std::max(0, std::min(remove_region, static_cast<int>(e.recipe.noise.thresholds.size()) - 2)) };
		RemoveNoiseBoundary(e.recipe.noise, boundary);
	}
	ImGui::TextDisabled("Use the gradient dividers to define ranges. A range set to None emits nothing.");
}

static void DrawPaintPalette(EditorState& e) {
	ImGui::Begin("Paint Palette");
	SceneLayer* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->kind == LayerKind::Noise) {
		ImGui::TextDisabled(
			layer
				? "Legacy Noise layers are compatibility data. New procedural content uses recipes/generators inside Tile or Entity layers."
				: "Select a Tile or Entity layer to choose a paint recipe."
		);
		ImGui::End();
		return;
	}

	int delete_generator{ -1 };
	int bake_generator{ -1 };
	PaintGenerator* selected_generator{ FindGenerator(e, e.selected_generator_id) };
	if (selected_generator && selected_generator->layer_id != layer->id) {
		selected_generator = nullptr;
	}

	if (selected_generator) {
		ImGui::SeparatorText("Selected Generator");
		ImGui::InputText("Name", &selected_generator->name);
		ImGui::SameLine();
		ImGui::Checkbox("Visible", &selected_generator->visible);

		const char* geometry_name{
			selected_generator->geometry == GeneratorGeometryKind::Infinite ? "Infinite" :
			selected_generator->geometry == GeneratorGeometryKind::Rectangle ? "Rectangle" :
			selected_generator->geometry == GeneratorGeometryKind::Line ? "Line" : "Brush Stroke"
		};
		ImGui::Text("Geometry: %s", geometry_name);
		if (selected_generator->geometry == GeneratorGeometryKind::Line || selected_generator->geometry == GeneratorGeometryKind::Rectangle) {
			float start_pos[2]{ selected_generator->start.x, selected_generator->start.y };
			float end_pos[2]{ selected_generator->end.x, selected_generator->end.y };
			if (ImGui::DragFloat2("Start##generator", start_pos, 1.0f)) selected_generator->start = { start_pos[0], start_pos[1] };
			if (ImGui::DragFloat2("End##generator", end_pos, 1.0f)) selected_generator->end = { end_pos[0], end_pos[1] };
		}
		if (selected_generator->geometry == GeneratorGeometryKind::BrushStroke) {
			int diameter{ selected_generator->brush_diameter_tiles };
			ImGui::SetNextItemWidth(90.0f);
			if (ImGui::DragInt(layer->kind == LayerKind::Tile ? "Diameter (tiles)##generator" : "Diameter (entities)##generator", &diameter, 0.15f, 1, 128)) {
				selected_generator->brush_diameter_tiles = std::clamp(diameter, 1, 128);
				RebuildGeneratorBrushCache(*selected_generator);
			}
		}
		if (selected_generator->geometry == GeneratorGeometryKind::Line) {
			ImGui::SetNextItemWidth(90.0f);
			ImGui::DragInt(layer->kind == LayerKind::Tile ? "Thickness (tiles)##generator" : "Thickness (entities)##generator", &selected_generator->line_thickness, 0.15f, 1, 32);
			selected_generator->line_thickness = std::max(1, selected_generator->line_thickness);
		}
		if (selected_generator->geometry == GeneratorGeometryKind::Rectangle) {
			const char* area_modes[]{ "Fill", "Outline", "Corners", "Random Fill" };
			int area_mode{ static_cast<int>(selected_generator->area_mode) };
			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::Combo("Mode##generator_rectangle", &area_mode, area_modes, 4)) selected_generator->area_mode = static_cast<AreaMode>(area_mode);
			if (selected_generator->area_mode == AreaMode::Outline || selected_generator->area_mode == AreaMode::Corners) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(90.0f);
				ImGui::DragInt(layer->kind == LayerKind::Tile ? "Thickness (tiles)##generator" : "Thickness (entities)##generator", &selected_generator->area_thickness, 0.15f, 1, 32);
				selected_generator->area_thickness = std::max(1, selected_generator->area_thickness);
			}
		}
		ImGui::TextDisabled(
			"The Paint Recipe below is this generator's stored recipe. Choosing a tile, prefab, weighted set, noise threshold, or other recipe option updates the selected generator directly."
		);

		const int suppressed{
			static_cast<int>(std::ranges::count_if(
				selected_generator->overrides,
				[](const auto& item) { return item.suppressed; }
			))
		};
		ImGui::Text("Suppressed generated cells: %d", suppressed);
		ImGui::SameLine();
		ImGui::BeginDisabled(selected_generator->overrides.empty());
		if (ImGui::SmallButton("Clear Overrides")) selected_generator->overrides.clear();
		ImGui::EndDisabled();
		ItemTooltip("Restore manually erased generated output. Future regeneration will again be allowed to emit at these cells.");

		ImGui::BeginDisabled(selected_generator->geometry == GeneratorGeometryKind::Infinite || layer->locked);
		if (ImGui::Button("Bake Generator")) bake_generator = selected_generator->id;
		ImGui::EndDisabled();
		if (selected_generator->geometry == GeneratorGeometryKind::Infinite) {
			ItemTooltip("Infinite generators cannot be globally baked because they have no finite output extent.");
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete Generator")) delete_generator = selected_generator->id;
	}

	// Keep browser selections and the active recipe source synchronized. Secondary
	// checkerboard/noise-threshold sources remain explicit and are not overwritten.
	if (e.recipe.source_kind == PaintSourceKind::Single) {
		if (layer->kind == LayerKind::Tile) e.recipe.tile_id = e.active_tile_id;
		else e.recipe.prefab_index = e.active_prefab_index;
	} else if (e.recipe.source_kind == PaintSourceKind::WeightedSet) {
		if (layer->kind == LayerKind::Tile) e.recipe.weighted_tile_set_id = e.active_tile_weighted_set_id;
		else e.recipe.weighted_prefab_set_id = e.active_prefab_weighted_set_id;
	}

	if (e.pending_generator && e.pending_generator->layer_id == layer->id) {
		ImGui::SeparatorText(
			e.pending_generator->geometry == GeneratorGeometryKind::BrushStroke
				? "Live Brush Generator"
				: "Live Shape"
		);
		if (e.pending_generator->geometry == GeneratorGeometryKind::BrushStroke) {
			ImGui::TextDisabled("Consecutive Brush strokes are being combined into one generator.");
			if (ImGui::Button("✓ Finish Generator")) {
				e.recipe.commit_mode = PaintCommitMode::KeepGenerator;
				e.pending_generator->recipe.commit_mode = PaintCommitMode::KeepGenerator;
				CommitPendingGenerator(e);
			}
			ItemTooltip("Finish/lock in this generator (Enter). The next Brush stroke will start a new generator instead of combining with this one.");
			ImGui::SameLine();
			if (ImGui::Button("Cancel Generator")) CancelPendingGenerator(e);
			ItemTooltip("Discard this live multi-stroke generator (Esc).");
		} else {
			ImGui::TextDisabled("Recipe changes below update this live Line/Rectangle until it is committed.");
			if (ImGui::Button("Apply / Bake")) {
				e.recipe.commit_mode = PaintCommitMode::BakeOnCommit;
				SyncGeneratorRecipeFromPalette(e, *e.pending_generator);
				CommitPendingGenerator(e);
			}
			ImGui::SameLine();
			if (ImGui::Button("Keep as Generator")) {
				e.recipe.commit_mode = PaintCommitMode::KeepGenerator;
				SyncGeneratorRecipeFromPalette(e, *e.pending_generator);
				CommitPendingGenerator(e);
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel Live Shape")) CancelPendingGenerator(e);
		}
	}

	ImGui::SeparatorText("Paint Recipe");
	DrawSourceMode(e);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(145.0f);
	if (ImGui::BeginCombo("Coverage", PaintCoverageKindName(e.recipe.coverage))) {
		for (PaintCoverageKind coverage : {
			PaintCoverageKind::Solid,
			PaintCoverageKind::RandomDensity,
			PaintCoverageKind::RadialFalloff
		}) {
			if (ImGui::Selectable(PaintCoverageKindName(coverage), e.recipe.coverage == coverage)) {
				e.recipe.coverage = coverage;
			}
		}
		ImGui::EndCombo();
	}
	ItemTooltip("Coverage controls which eligible positions emit the selected source. Noise is configured as a source because its thresholds resolve different emitted tiles/entities.");

	const bool finite_generator_tool{
		e.tool == Tool::Brush || e.tool == Tool::Line || e.tool == Tool::Rectangle
	};
	if (finite_generator_tool && e.brush.operation == BrushOperation::Paint && !selected_generator && !PendingBrushGeneratorActive(e)) {
		ImGui::SameLine();
		ImGui::SetNextItemWidth(145.0f);
		const char* modes[]{ "Bake on Commit", "Keep Generator" };
		int mode{ static_cast<int>(e.recipe.commit_mode) };
		if (ImGui::Combo("Result", &mode, modes, 2)) {
			e.recipe.commit_mode = static_cast<PaintCommitMode>(mode);
		}
		ItemTooltip("Bake produces ordinary content. Keep Generator preserves the geometry + recipe. Brush Keep Generator combines strokes until Enter/checkmark finishes the current generator.");
	}

	if (e.recipe.coverage == PaintCoverageKind::RandomDensity) {
		ImGui::SetNextItemWidth(160.0f);
		ImGui::SliderFloat("Density", &e.recipe.density, 0.0f, 1.0f, "%.2f");
	} else if (e.recipe.coverage == PaintCoverageKind::RadialFalloff) {
		ImGui::SetNextItemWidth(150.0f);
		ImGui::SliderFloat("Center Density", &e.recipe.density, 0.0f, 1.0f, "%.2f");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat("Inner", &e.recipe.radial_inner, 0.0f, 0.95f, "%.2f");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat("Outer", &e.recipe.radial_outer, 0.05f, 1.0f, "%.2f");
		e.recipe.radial_outer = std::max(e.recipe.radial_outer, e.recipe.radial_inner + 0.01f);
	}

	if (e.recipe.source_kind == PaintSourceKind::Noise) {
		DrawRecipeNoiseSettings(e, *layer);
	} else {
		ImGui::SeparatorText("Source Library");
		if (layer->kind == LayerKind::Tile) DrawTileSourcePalette(e);
		else DrawPrefabSourcePalette(e);

		// Browser clicks become the primary source immediately.
		if (e.recipe.source_kind == PaintSourceKind::Single) {
			if (layer->kind == LayerKind::Tile) e.recipe.tile_id = e.active_tile_id;
			else e.recipe.prefab_index = e.active_prefab_index;
		} else if (e.recipe.source_kind == PaintSourceKind::WeightedSet) {
			if (layer->kind == LayerKind::Tile) e.recipe.weighted_tile_set_id = e.active_tile_weighted_set_id;
			else e.recipe.weighted_prefab_set_id = e.active_prefab_weighted_set_id;
		}
	}

	const bool tile_mode_applicable{ TilePaintModeApplicable(e, *layer) };
	const bool tile_origin_applicable{ TileOriginApplicable(e, *layer) };
	const bool entity_origin_applicable{ layer->kind == LayerKind::Entity && EntityOriginApplicable(e) };
	const bool mask_applicable{ layer->kind == LayerKind::Tile && [&] {
		const auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
		return map && !map->exclusion_mask.empty();
	}() };
	const bool entity_random_applicable{
		layer->kind == LayerKind::Entity && e.recipe.source_kind != PaintSourceKind::Autotile
	};
	const bool any_placement{
		tile_mode_applicable ||
		tile_origin_applicable ||
		entity_origin_applicable ||
		mask_applicable ||
		entity_random_applicable
	};
	if (any_placement) {
		ImGui::SeparatorText("Placement");
		if (tile_mode_applicable) {
			const char* modes[]{ "Grid", "Tile" };
			int mode{ static_cast<int>(e.recipe.tile_paint_mode) };
			ImGui::SetNextItemWidth(115.0f);
			if (ImGui::Combo("Placement", &mode, modes, 2)) {
				e.recipe.tile_paint_mode = static_cast<TilePaintMode>(mode);
			}
			ItemTooltip("Only shown when at least one possible emitted tile is larger than the grid. Tile mode advances in complete native-tile footprints; Grid mode anchors every grid cell.");
		} else if (layer->kind == LayerKind::Tile) {
			e.recipe.tile_paint_mode = TilePaintMode::Tile;
		}
		if (tile_origin_applicable) {
			if (tile_mode_applicable) ImGui::SameLine();
			DrawOriginCombo("Tile Origin", e.recipe.tile_origin);
		}
		if (entity_origin_applicable) DrawOriginCombo("Entity Origin", e.recipe.entity_origin);
		if (mask_applicable) ImGui::Checkbox("Avoid Exclusion Mask", &e.recipe.avoid_exclusion_mask);
		if (entity_random_applicable) {
			ImGui::Checkbox("Random Rotation", &e.recipe.random_rotation);
			if (e.recipe.random_rotation) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(180.0f);
				ImGui::DragFloatRange2("Rotation Range", &e.recipe.rotation_min, &e.recipe.rotation_max, 0.5f, -3600.0f, 3600.0f, "%.0f", "%.0f");
			}
			ImGui::Checkbox("Random Scale", &e.recipe.random_scale);
			if (e.recipe.random_scale) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(180.0f);
				ImGui::DragFloatRange2("Scale Range", &e.recipe.scale_min, &e.recipe.scale_max, 0.01f, 0.01f, 8.0f, "%.2f", "%.2f");
			}
			if (e.recipe.coverage != PaintCoverageKind::Solid) {
				ImGui::SetNextItemWidth(140.0f);
				ImGui::DragFloat("Minimum Spacing", &e.recipe.min_spacing, 0.5f, 0.0f, 1024.0f, "%.0f");
			} else {
				e.recipe.min_spacing = 0.0f;
			}
		}
	}

	if (!selected_generator) {
		ImGui::SeparatorText("Procedural");
		ImGui::BeginDisabled(layer->locked);
		if (ImGui::Button("Create Infinite Generator")) CreateInfiniteGenerator(e);
		ImGui::EndDisabled();
		ItemTooltip("Create a persistent unbounded generator using the current recipe. Infinite generators evaluate visible cells on demand and cannot be globally baked.");
	}

	// A selected persistent generator uses the Paint Palette as its live stored
	// recipe editor. Pending generators do the same while they are being authored.
	if (selected_generator) {
		SyncGeneratorRecipeFromPalette(e, *selected_generator);
	}
	if (e.pending_generator && e.pending_generator->layer_id == layer->id) {
		SyncGeneratorRecipeFromPalette(e, *e.pending_generator);
	}

	ImGui::End();

	if (bake_generator >= 0) BakeGenerator(e, bake_generator);
	if (delete_generator >= 0) DeleteGenerator(e, delete_generator);
}

static void DrawInspector(EditorState& e) {
	ImGui::Begin("Inspector");
	if (e.selected_generator_id >= 0) {
		ImGui::TextDisabled("Generator settings are edited in Paint Palette.");
		ImGui::Separator();
	}
	if (SceneLayer* layer = FindLayer(e, e.active_layer_id)) {
		ImGui::Text("Layer: %s", layer->name.c_str());
		ImGui::Text("Kind: %s", LayerKindName(layer->kind));
		ImGui::Text("Purpose: %s", PurposeName(layer->purpose));
		if (layer->kind == LayerKind::Tile) {
			ImGui::SeparatorText("Tilemap");
			if (!e.tilemaps.empty()) {
				const Tilemap* current_map{ FindTilemap(e, layer->tile.tilemap_id) };
				const char* current_name{ current_map ? current_map->name.c_str() : "<missing>" };
				if (ImGui::BeginCombo("Tilemap", current_name)) {
					for (const auto& candidate : e.tilemaps) {
						if (ImGui::Selectable(candidate.name.c_str(), candidate.id == layer->tile.tilemap_id)) {
							layer->tile.tilemap_id = candidate.id;
						}
					}
					ImGui::EndCombo();
				}
			}
			if (ImGui::Button("New Tilemap")) {
				Tilemap created;
				created.id = e.next_tilemap_id++;
				created.name = "Tilemap " + std::to_string(created.id);
				e.tilemaps.push_back(created);
				layer->tile.tilemap_id = created.id;
			}
			if (Tilemap* map = FindTilemap(e, layer->tile.tilemap_id)) {
				char map_name[128]{};
				std::snprintf(map_name, sizeof(map_name), "%s", map->name.c_str());
				if (ImGui::InputText("Map Name", map_name, sizeof(map_name))) map->name = map_name;
				float origin[2]{ map->origin.x, map->origin.y };
				if (ImGui::DragFloat2("Origin", origin, 1.0f)) map->origin = { origin[0], origin[1] };
				const bool has_chunk_data{ !layer->tile.loaded_chunks.empty() || !layer->tile.backing_chunks.empty() };
				ImGui::BeginDisabled(has_chunk_data);
				float cell[2]{ map->cell_size.x, map->cell_size.y };
				if (ImGui::DragFloat2("Cell Size", cell, 1.0f, 1.0f, 512.0f, "%.0f")) map->cell_size = { std::max(1.0f, cell[0]), std::max(1.0f, cell[1]) };
				int chunk[2]{ map->chunk_size.x, map->chunk_size.y };
				if (ImGui::DragInt2("Chunk Size", chunk, 1.0f, 1, 256)) map->chunk_size = { std::max(1, chunk[0]), std::max(1, chunk[1]) };
				ImGui::EndDisabled();
				if (has_chunk_data) ImGui::TextDisabled("Cell/chunk size is locked after chunk data exists in this demo.");
				ImGui::Checkbox("Streaming", &map->streaming.enabled);
				ImGui::DragInt("Preload Margin", &map->streaming.preload_margin, 1.0f, 0, 16);
				ImGui::DragInt("Keep Alive Margin", &map->streaming.keep_alive_margin, 1.0f, 0, 32);
				map->streaming.keep_alive_margin = std::max(map->streaming.keep_alive_margin, map->streaming.preload_margin);
				ImGui::DragInt("Max Loaded Chunks", &map->streaming.max_loaded_chunks, 1.0f, 1, 4096);
				if (ImGui::BeginCombo("##stream_debug", "Debug View")) {
					ToggleComboChoice(
						"Chunk boundaries",
						map->streaming.show_chunk_boundaries,
						"Draw tilemap chunk boundaries in the viewport."
					);
					ToggleComboChoice(
						"Streaming state / exclusion overlay",
						map->streaming.show_streaming_state,
						"Show loaded chunk labels and the exclusion-mask overlay."
					);
					ImGui::EndCombo();
				}
				{
					std::string enabled;
					if (map->streaming.show_chunk_boundaries) enabled += "\n- Chunk boundaries";
					if (map->streaming.show_streaming_state) enabled += "\n- Streaming state / exclusion overlay";
					if (enabled.empty()) enabled = "\n- None";
					ItemTooltip(("Debug overlays currently enabled:" + enabled).c_str());
				}
				int dirty_chunks{};
				for (const auto& [_, chunk_data] : layer->tile.loaded_chunks) if (chunk_data.dirty) ++dirty_chunks;
				for (const auto& [_, chunk_data] : layer->tile.backing_chunks) if (chunk_data.dirty) ++dirty_chunks;
				ImGui::Text("Loaded chunks: %d", static_cast<int>(layer->tile.loaded_chunks.size()));
				ImGui::Text("Backing chunks: %d", static_cast<int>(layer->tile.backing_chunks.size()));
				ImGui::Text("Dirty chunks: %d", dirty_chunks);
				ImGui::Text("Exclusion cells: %d", static_cast<int>(map->exclusion_mask.size()));
				if (layer->purpose == LayerPurpose::Collision) {
					ImGui::Text("Merged collision runs: %d", CountMergedCollisionRects(*layer, *map));
				}
			}
		}
	}

	if (e.primary_entity_id >= 0) {
		for (auto& entity : e.entities) {
			if (entity.id != e.primary_entity_id) continue;
			ImGui::SeparatorText("Primary Selection");
			ImGui::Text("%s #%d", entity.prefab.c_str(), entity.id);
			float pos[2]{ entity.position.x, entity.position.y };
			if (ImGui::DragFloat2("Position", pos, 1.0f)) entity.position = { pos[0], pos[1] };
			float size[2]{ entity.size.x, entity.size.y };
			if (ImGui::DragFloat2("Size", size, 1.0f, 1.0f, 2048.0f, "%.0f")) entity.size = { std::max(1.0f, size[0]), std::max(1.0f, size[1]) };
			int origin{ static_cast<int>(entity.origin) };
			const char* origins[]{ "Top Left", "Top", "Top Right", "Left", "Center", "Right", "Bottom Left", "Bottom", "Bottom Right" };
			if (ImGui::Combo("Origin", &origin, origins, 9)) entity.origin = static_cast<EntityOrigin>(origin);
			ImGui::DragFloat("Rotation", &entity.rotation, 1.0f);
			ImGui::DragFloat("Scale", &entity.scale, 0.01f, 0.01f, 100.0f);
			ImGui::DragFloat("Depth", &entity.depth, 0.05f, -1000.0f, 1000.0f);
			break;
		}
	}
	ImGui::End();
}

static void DrawHistory(EditorState& e) {
	ImGui::Begin("Undo History");
	if (ImGui::Button("Undo")) Undo(e);
	ImGui::SameLine();
	if (ImGui::Button("Redo")) Redo(e);
	ImGui::Separator();
	for (int i{}; i < static_cast<int>(e.history.size()); ++i) {
		ImGui::Text("%s%s", i < e.history_cursor ? "* " : "  ", e.history[static_cast<std::size_t>(i)].label.c_str());
	}
	ImGui::End();
}

static void DrawMainMenu(EditorState& e) {
	if (!ImGui::BeginMainMenuBar()) {
		return;
	}
	if (ImGui::BeginMenu("Edit")) {
		if (ImGui::MenuItem("Undo", "Ctrl+Z", false, e.history_cursor > 0)) Undo(e);
		if (ImGui::MenuItem("Redo", "Ctrl+Y", false, e.history_cursor < static_cast<int>(e.history.size()))) Redo(e);
		ImGui::Separator();
		if (ImGui::MenuItem("Cut", "Ctrl+X", false, HasSelection(e))) CutSelection(e);
		if (ImGui::MenuItem("Copy", "Ctrl+C", false, HasSelection(e))) CopySelection(e);
		if (ImGui::MenuItem("Paste", "Ctrl+V", false, e.clipboard.valid)) PasteClipboard(e);
		ImGui::Separator();
		if (ImGui::MenuItem("Deselect All", "Ctrl+D", false, HasSelection(e))) DeselectAll(e);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("View")) {
		ImGui::MenuItem("Show Grid", nullptr, &e.grid.visible);
		ImGui::MenuItem("Snap to Grid", nullptr, &e.grid.snap);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Help")) {
		ImGui::TextDisabled("Viewport controls:");
		ImGui::BulletText("Middle-drag: pan");
		ImGui::BulletText("Mouse wheel: zoom");
		ImGui::BulletText("Selection: Ctrl toggles items; Shift adds and constrains marquee selection to a square");
		ImGui::BulletText("Move: drag selection; arrows move; Shift = 10x arrow step; Ctrl temporarily inverts Grid/Free movement");
		ImGui::BulletText("Ctrl+C / Ctrl+X / Ctrl+V: copy, cut, paste; pasted content switches to Move");
		ImGui::BulletText("Delete: erase selected entities/tiles (undoable)");
		ImGui::BulletText("Pencil: click-drag for continuous drawing");
		ImGui::BulletText("Line/Rectangle: Escape cancels the active drag");
		ImGui::BulletText("Eyedropper: hold left mouse and move to continuously pick");
		ImGui::BulletText("Ctrl-click palette tile: add/remove stamp tile");
		ImGui::BulletText("S/M/P/B/L/R/F/E/K: Select/Move/Pencil/Brush/Line/Rectangle/Fill/Erase/Pick");
		ImGui::BulletText("Drag image/TSX/TSJ/JSON onto window: import tiles");
		ImGui::EndMenu();
	}
	ImGui::EndMainMenuBar();
}

static void DrawDefaultDockspace(EditorState& e) {
#ifdef IMGUI_HAS_DOCK
	const ImGuiViewport* viewport{ ImGui::GetMainViewport() };
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	constexpr ImGuiWindowFlags host_flags{
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground
	};
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
	ImGui::Begin("##ProtegonDemoDockHost", nullptr, host_flags);
	ImGui::PopStyleVar(3);

	const ImGuiID dockspace_id{ ImGui::GetID("ProtegonDemoDockSpace") };
	if (!e.dock_layout_initialized) {
		e.dock_layout_initialized = true;
		ImGui::DockBuilderRemoveNode(dockspace_id);
		ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodePos(dockspace_id, viewport->WorkPos);
		ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

		ImGuiID center{ dockspace_id };
		ImGuiID left{};
		ImGuiID right{};
		ImGuiID bottom{};
		left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.182f, nullptr, &center);
		right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.22f, nullptr, &center);
		bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.26f, nullptr, &center);

		ImGuiID left_top{ left };
		ImGuiID left_bottom{};
		left_bottom = ImGui::DockBuilderSplitNode(left_top, ImGuiDir_Down, 0.34f, nullptr, &left_top);

		ImGuiID right_top{ right };
		ImGuiID right_bottom{};
		right_bottom = ImGui::DockBuilderSplitNode(right_top, ImGuiDir_Down, 0.27f, nullptr, &right_top);

		ImGui::DockBuilderDockWindow("Viewport", center);
		ImGui::DockBuilderDockWindow("Scene Hierarchy", left_top);
		ImGui::DockBuilderDockWindow("Layers", left_bottom);
		ImGui::DockBuilderDockWindow("Paint Palette", bottom);
		ImGui::DockBuilderDockWindow("Inspector", right_top);
		ImGui::DockBuilderDockWindow("Undo History", right_bottom);
		ImGui::DockBuilderFinish(dockspace_id);
	}

	ImGui::DockSpace(dockspace_id, { 0.0f, 0.0f }, ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();
#else
	(void)e;
#endif
}

static void DrawEditor(EditorState& e) {
	SyncViewportToolToActiveLayer(e);
	ImGuiIO& io{ ImGui::GetIO() };
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) Undo(e);
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) Redo(e);
	if (!io.WantTextInput) {
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) CopySelection(e);
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X, false)) CutSelection(e);
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) PasteClipboard(e);
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) DeselectAll(e);
		if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) DeleteSelection(e);

		// Arrow keys belong to Move only. Tool buttons are also NoNav, so arrows
		// cannot accidentally move ImGui keyboard focus through the paint toolbar.
		if (e.tool == Tool::Move) {
			I2 move_direction{};
			if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) --move_direction.x;
			if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) ++move_direction.x;
			if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) --move_direction.y;
			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) ++move_direction.y;
			if (move_direction != I2{}) MoveSelectionByKeyboard(e, move_direction, io.KeyCtrl, io.KeyShift);
		}
	}
	if (!io.WantTextInput && e.canvas_hovered && !ActiveLayerIsNoise(e) && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper) {
		for (const auto& binding : e.tool_bindings) {
			if (binding.key != ImGuiKey_None && ImGui::IsKeyPressed(binding.key, false)) {
				e.tool = binding.tool;
				e.last_non_noise_tool = binding.tool;
				break;
			}
		}
	}
	DrawMainMenu(e);
	DrawDefaultDockspace(e);
	DrawViewport(e);
	DrawSceneHierarchy(e);
	DrawLayers(e);
	SyncViewportToolToActiveLayer(e);
	DrawPaintPalette(e);
	DrawInspector(e);
	DrawHistory(e);
}

} // namespace demo

int main(int, char**) {
	if (!glfwInit()) {
		std::fprintf(stderr, "Failed to initialize GLFW.\n");
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	GLFWwindow* window{ glfwCreateWindow(1600, 960, "Entity + Tile Paint Editor Demo", nullptr, nullptr) };
	if (!window) {
		std::fprintf(stderr, "Failed to create GLFW window.\n");
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

    #ifndef __EMSCRIPTEN__
    int status{ gladLoadGL(glfwGetProcAddress) };

    if (!status) {
        std::fprintf(
            stderr,
            "Failed to load OpenGL functions.\n"
        );

        glfwDestroyWindow(window);
        glfwTerminate();

        return 1;
    }
    #endif

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io{ ImGui::GetIO() };
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef IMGUI_HAS_DOCK
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330 core");

	demo::EditorState editor;
	demo::AddDefaultScene(editor);
	demo::g_editor = &editor;
	glfwSetDropCallback(window, demo::GLFWDropCallback);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		if (editor.runtime.playing && !editor.runtime.paused) {
			editor.runtime.time += io.DeltaTime * editor.runtime.speed;
		}

		demo::DrawEditor(editor);

		ImGui::Render();
		int fb_w{}, fb_h{};
		glfwGetFramebufferSize(window, &fb_w, &fb_h);
		glViewport(0, 0, fb_w, fb_h);
		glClearColor(0.05f, 0.055f, 0.065f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}

	for (const auto& texture : editor.textures) {
		if (texture.handle != 0) {
			glDeleteTextures(1, &texture.handle);
		}
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
