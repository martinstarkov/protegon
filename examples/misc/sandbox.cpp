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
	Area,
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

enum class BrushSourceMode {
	Single,
	WeightedSet,
};

enum class BrushPlacementMode {
	Continuous,
	Scatter,
	Density,
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

enum class BrushDistribution {
	Uniform,
	Random,
	Noise,
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
		case Tool::Area: return "Area";
		case Tool::Fill: return "Fill";
		case Tool::Erase: return "Erase";
		case Tool::Eyedropper: return "Eyedropper";
	}
	return "Unknown";
}


static const char* ToolTooltip(Tool tool) {
	switch (tool) {
		case Tool::None:
			return "No viewport paint tool is active on procedural noise layers.";
		case Tool::Select:
			return "Select (S)\nClick/marquee entities or tiles, or switch to the raster selection brush.";
		case Tool::Move:
			return "Move (M)\nDrag selected entities/tiles together. Grid snapping is the default; hold Ctrl to temporarily use free movement.";
		case Tool::Pencil:
			return "Pencil (P)\nContinuously draw one tile/entity at a time while dragging.";
		case Tool::Brush:
			return "Brush (B)\nPaint a rasterized circle or square footprint over the active layer.";
		case Tool::Line:
			return "Line (L)\nDrag a grid-rasterized line; release to apply or press Escape to cancel.";
		case Tool::Area:
			return "Area (A)\nDrag a grid-rasterized rectangular region; release to apply or press Escape to cancel.";
		case Tool::Fill:
			return "Fill (F)\nFlood-fill a connected tile region. Empty regions are bounded by the visible viewport.";
		case Tool::Erase:
			return "Eraser (E)\nErase entities/tiles touched by the rasterized brush footprint.";
		case Tool::Eyedropper:
			return "Eyedropper (K)\nHold left mouse and move over tiles/entities to continuously pick the source.";
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
	std::unordered_map<I2, TileChunk, I2Hash> loaded_chunks;
	std::unordered_map<I2, TileChunk, I2Hash> backing_chunks;
	std::unordered_set<I2, I2Hash> preload_chunks;
	std::unordered_set<I2, I2Hash> keep_alive_chunks;
};

struct NoiseThresholdRegion {
	float minimum{};
	float maximum{ 1.0f };
	int tile_id{ -1 };
	int prefab_index{ -1 };
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
	BrushSourceMode source_mode{ BrushSourceMode::Single };
	BrushPlacementMode placement{ BrushPlacementMode::Continuous };
	BrushDistribution distribution{ BrushDistribution::Uniform };
	BrushOperation operation{ BrushOperation::Paint };
	AreaMode area_mode{ AreaMode::Fill };
	SelectMode select_mode{ SelectMode::ClickMarquee };
	BrushSizeSnapMode size_snap{ BrushSizeSnapMode::Free };
	BrushShape shape{ BrushShape::Circle };
	TilePaintMode tile_paint_mode{ TilePaintMode::Grid };
	EntityOrigin entity_origin{ EntityOrigin::TopLeft };

	float radius{ 48.0f };
	float spacing{ 32.0f };
	float density{ 0.25f };
	float min_spacing{ 24.0f };
	int scatter_count{ 6 };
	bool replace_occupied_anchor{ true };
	bool allow_visual_overlap{ false };

	bool random_rotation{};
	float rotation_min{};
	float rotation_max{ 360.0f };
	bool random_scale{};
	float scale_min{ 0.8f };
	float scale_max{ 1.2f };
	bool line_align_rotation{};

	bool noise_mask{};
	float noise_scale{ 0.025f };
	float noise_threshold{ 0.45f };
	int noise_seed{ 1337 };

	bool avoid_exclusion_mask{ true };
	bool autotile{};
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
	int active_layer_id{};
	int active_palette_index{};
	int active_tile_weighted_set_id{ -1 };
	int active_prefab_weighted_set_id{ -1 };
	std::unordered_set<int> selected_entities;
	int primary_entity_id{ -1 };
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

struct EditorState {
	std::vector<TextureAsset> textures;
	std::vector<TileDefinition> tiles;
	std::vector<TilePalette> palettes;
	std::vector<WeightedTileSet> weighted_tile_sets;
	std::vector<PrefabBrushEntry> prefabs;
	std::vector<WeightedPrefabSet> weighted_prefab_sets;
	std::vector<SceneLayer> layers;
	std::vector<Tilemap> tilemaps;
	std::vector<Entity> entities;

	Tool tool{ Tool::Select };
	Tool last_non_noise_tool{ Tool::Select };
	BrushSettings brush;
	GridSettings grid;
	RuntimeState runtime;
	ImportSettings importer;
	std::string import_status;
	StrokeState stroke;
	MoveState move;
	NoiseBoundaryDragState noise_boundary_drag;
	SelectionClipboard clipboard;

	int active_layer_id{};
	int active_palette_index{ -1 };
	int active_tile_id{ -1 };
	int active_tile_weighted_set_id{ -1 };
	int replace_source_tile_id{ -1 };
	int active_prefab_index{};
	int active_prefab_weighted_set_id{ -1 };
	int replace_source_prefab_index{};

	std::vector<int> stamp_tiles;
	int stamp_width{ 1 };

	std::unordered_set<int> selected_entities;
	int primary_entity_id{ -1 };
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
		.active_layer_id = e.active_layer_id,
		.active_palette_index = e.active_palette_index,
		.active_tile_weighted_set_id = e.active_tile_weighted_set_id,
		.active_prefab_weighted_set_id = e.active_prefab_weighted_set_id,
		.selected_entities = e.selected_entities,
		.primary_entity_id = e.primary_entity_id,
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
	e.active_layer_id = s.active_layer_id;
	e.active_palette_index = s.active_palette_index;
	e.active_tile_weighted_set_id = s.active_tile_weighted_set_id;
	e.active_prefab_weighted_set_id = s.active_prefab_weighted_set_id;
	e.selected_entities = s.selected_entities;
	e.primary_entity_id = s.primary_entity_id;
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

static void SetTile(SceneLayer& layer, const Tilemap& map, I2 cell, int tile_id, bool terrain = false) {
	auto* dst{ WriteTileCell(layer, map, cell) };
	dst->tile_id = tile_id;
	dst->terrain = terrain;
	dst->offset = {};
}

static void EraseTile(SceneLayer& layer, const Tilemap& map, I2 cell) {
	auto* dst{ WriteTileCell(layer, map, cell) };
	dst->tile_id = -1;
	dst->terrain = false;
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
	F2 local_offset = {}
) {
	const F2 origin{ CellToWorld(map, cell) + local_offset };
	const F2 size{ TileWorldSize(e, map, tile_id) };
	return { origin, origin + size };
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
		const RectF bounds{ TileAnchorRect(e, map, cell, tile.tile_id, tile.offset) };
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

static float BrushSnapUnit(const EditorState& e) {
	if (e.brush.size_snap == BrushSizeSnapMode::Free) {
		return 1.0f;
	}

	if (e.brush.size_snap == BrushSizeSnapMode::Tile) {
		if (const auto* tile = FindTile(e, e.active_tile_id)) {
			return std::max(1.0f, static_cast<float>(std::max(tile->pixel_w, tile->pixel_h)));
		}
	}

	if (const auto* layer = FindLayer(e, e.active_layer_id)) {
		if (layer->kind == LayerKind::Tile) {
			if (const auto* map = FindTilemap(e, layer->tile.tilemap_id)) {
				return std::max(1.0f, std::min(map->cell_size.x, map->cell_size.y));
			}
		} else if (layer->kind == LayerKind::Noise && layer->noise.target == NoiseTargetKind::Tile) {
			if (const auto* map = FindTilemap(e, layer->noise.tilemap_id)) {
				return std::max(1.0f, std::min(map->cell_size.x, map->cell_size.y));
			}
		}
	}

	return std::max(1.0f, std::min(e.grid.size.x, e.grid.size.y));
}

static float SnapBrushDiameter(const EditorState& e, float diameter) {
	diameter = std::max(1.0f, diameter);
	if (e.brush.size_snap == BrushSizeSnapMode::Free) {
		return diameter;
	}
	const float unit{ BrushSnapUnit(e) };
	return std::max(unit, std::round(diameter / unit) * unit);
}

static float EffectiveBrushRadius(const EditorState& e) {
	return SnapBrushDiameter(e, e.brush.radius * 2.0f) * 0.5f;
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
	const float diameter{ SnapBrushDiameter(e, e.brush.radius * 2.0f) };

	const int count_x{
		std::max(1, static_cast<int>(std::ceil(diameter / grid.size.x)))
	};
	const int count_y{
		std::max(1, static_cast<int>(std::ceil(diameter / grid.size.y)))
	};

	const int start_x{ center.x - (count_x - 1) / 2 };
	const int start_y{ center.y - (count_y - 1) / 2 };

	std::vector<I2> cells;
	cells.reserve(static_cast<std::size_t>(count_x * count_y));

	for (int y{}; y < count_y; ++y) {
		for (int x{}; x < count_x; ++x) {
			if (e.brush.shape == BrushShape::Circle) {
				const float center_x{ (static_cast<float>(count_x) - 1.0f) * 0.5f };
				const float center_y{ (static_cast<float>(count_y) - 1.0f) * 0.5f };
				const float radius_x{
					std::max(0.75f, center_x + 0.25f)
				};
				const float radius_y{
					std::max(0.75f, center_y + 0.25f)
				};
				const float nx{ (static_cast<float>(x) - center_x) / radius_x };
				const float ny{ (static_cast<float>(y) - center_y) / radius_y };

				if (nx * nx + ny * ny > 1.0f) {
					continue;
				}
			}

			cells.push_back({ start_x + x, start_y + y });
		}
	}

	// Tiny circle brushes should still affect the hovered cell.
	if (cells.empty()) {
		cells.push_back(center);
	}

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
		if (start == end) {
			break;
		}
		const int twice_error{ 2 * error };
		if (twice_error >= dy) {
			error += dy;
			start.x += sx;
		}
		if (twice_error <= dx) {
			error += dx;
			start.y += sy;
		}
	}

	const int spacing{ std::max(1, e.brush.line_spacing_cells) };
	if (spacing > 1 && base.size() > 2) {
		std::vector<I2> spaced;
		for (std::size_t i{}; i < base.size(); i += static_cast<std::size_t>(spacing)) spaced.push_back(base[i]);
		if (spaced.empty() || spaced.back() != base.back()) spaced.push_back(base.back());
		base = std::move(spaced);
	}
	return ExpandRasterCells(base, e.brush.line_thickness);
}

static std::vector<I2> RasterAreaCells(
	const EditorState& e,
	F2 a,
	F2 b
) {
	const RasterGrid grid{ ActiveRasterGrid(e) };
	const I2 ca{ WorldToRasterCell(grid, a) };
	const I2 cb{ WorldToRasterCell(grid, b) };

	const int min_x{ std::min(ca.x, cb.x) };
	const int max_x{ std::max(ca.x, cb.x) };
	const int min_y{ std::min(ca.y, cb.y) };
	const int max_y{ std::max(ca.y, cb.y) };
	const int thickness{ std::max(1, e.brush.area_thickness) };

	std::vector<I2> cells;
	for (int y{ min_y }; y <= max_y; ++y) {
		for (int x{ min_x }; x <= max_x; ++x) {
			const int left{ x - min_x };
			const int right{ max_x - x };
			const int top{ y - min_y };
			const int bottom{ max_y - y };
			const bool thick_edge{
				left < thickness || right < thickness ||
				top < thickness || bottom < thickness
			};
			const bool thick_corner{
				(left < thickness || right < thickness) &&
				(top < thickness || bottom < thickness)
			};

			if (e.brush.area_mode == AreaMode::Outline && !thick_edge) {
				continue;
			}
			if (e.brush.area_mode == AreaMode::Corners && !thick_corner) {
				continue;
			}
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
			static_cast<std::uint32_t>(e.brush.noise_seed) ^ 0xA511E9B3u
		)
	};
	const float value{
		static_cast<float>(hash & 0x00FFFFFFu) /
		static_cast<float>(0x01000000u)
	};
	return value <= e.brush.density;
}


static float PreviousFreeBrushDiameter(float current) {
	static constexpr std::array<float, 24> kSizes{
		1.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f,
		12.0f, 14.0f, 16.0f, 18.0f, 20.0f, 24.0f,
		28.0f, 32.0f, 36.0f, 48.0f, 64.0f, 72.0f,
		96.0f, 128.0f, 192.0f, 256.0f, 384.0f, 512.0f,
	};

	for (auto it = kSizes.rbegin(); it != kSizes.rend(); ++it) {
		if (*it < current - 0.01f) {
			return *it;
		}
	}
	return kSizes.front();
}

static float NextFreeBrushDiameter(float current) {
	static constexpr std::array<float, 24> kSizes{
		1.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f,
		12.0f, 14.0f, 16.0f, 18.0f, 20.0f, 24.0f,
		28.0f, 32.0f, 36.0f, 48.0f, 64.0f, 72.0f,
		96.0f, 128.0f, 192.0f, 256.0f, 384.0f, 512.0f,
	};

	for (float size : kSizes) {
		if (size > current + 0.01f) {
			return size;
		}
	}
	return std::min(2048.0f, current + 128.0f);
}

static void AdjustBrushDiameter(EditorState& e, int direction) {
	float diameter{ SnapBrushDiameter(e, e.brush.radius * 2.0f) };

	if (e.brush.size_snap == BrushSizeSnapMode::Free) {
		diameter = direction < 0
			? PreviousFreeBrushDiameter(diameter)
			: NextFreeBrushDiameter(diameter);
	} else {
		const float unit{ BrushSnapUnit(e) };
		diameter = std::max(unit, diameter + static_cast<float>(direction) * unit);
	}

	e.brush.radius = SnapBrushDiameter(e, diameter) * 0.5f;
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
	const RectF candidate_rect{ TileAnchorRect(e, map, candidate, tile_id) };
	bool overlap{};
	ForEachTileAnchor(layer, map, [&](I2 cell, const TileCell& existing) {
		if (overlap || (ignore_same_anchor && cell == candidate)) {
			return;
		}
		if (RectsOverlap(candidate_rect, TileAnchorRect(e, map, cell, existing.tile_id, existing.offset))) {
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
	if (anchor_occupied && !e.brush.replace_occupied_anchor) {
		return false;
	}

	if (e.brush.tile_paint_mode == TilePaintMode::Grid) {
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

	if (!e.brush.allow_visual_overlap && TileFootprintOverlapsExisting(e, layer, map, cell, tile_id, anchor_occupied && e.brush.replace_occupied_anchor)) {
		return false;
	}
	return true;
}

static bool HasTerrain(const SceneLayer& layer, const Tilemap& map, I2 cell) {
	if (const auto* c = ReadTileCell(layer, map, cell)) {
		return c->terrain;
	}
	return false;
}

static int ActivePaletteTerrainTile(const EditorState& e, int mask) {
	if (e.palettes.empty()) {
		return -1;
	}
	const auto& p{ e.palettes[static_cast<std::size_t>(std::clamp(e.active_palette_index, 0, static_cast<int>(e.palettes.size()) - 1))] };
	if (p.entries.empty()) {
		return -1;
	}
	return p.entries[static_cast<std::size_t>(mask % static_cast<int>(p.entries.size()))].tile_id;
}

static void RecomputeAutotile(EditorState& e, SceneLayer& layer, const Tilemap& map, I2 cell) {
	static constexpr std::array<I2, 5> offsets{ I2{0, 0}, I2{0, -1}, I2{1, 0}, I2{0, 1}, I2{-1, 0} };
	for (const I2 o : offsets) {
		const I2 c{ cell.x + o.x, cell.y + o.y };
		if (!HasTerrain(layer, map, c)) {
			continue;
		}
		int mask{};
		if (HasTerrain(layer, map, { c.x, c.y - 1 })) mask |= 1;
		if (HasTerrain(layer, map, { c.x + 1, c.y })) mask |= 2;
		if (HasTerrain(layer, map, { c.x, c.y + 1 })) mask |= 4;
		if (HasTerrain(layer, map, { c.x - 1, c.y })) mask |= 8;
		if (const int tile{ ActivePaletteTerrainTile(e, mask) }; tile >= 0) {
			SetTile(layer, map, c, tile, true);
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
						const RectF r{ TileAnchorRect(e, *map, cell, tile->tile_id, tile->offset) };
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

static bool IsExcluded(const Tilemap& map, I2 cell) {
	return map.exclusion_mask.contains(cell);
}

static float RandomRange(EditorState& e, float a, float b) {
	std::uniform_real_distribution<float> d{ a, b };
	return d(e.rng);
}

static int WeightedTile(EditorState& e) {
	const auto* set{ FindWeightedTileSet(e, e.active_tile_weighted_set_id) };
	if (!set || set->entries.empty()) {
		return e.active_tile_id;
	}
	float sum{};
	for (const auto& item : set->entries) {
		if (FindTile(e, item.tile_id)) sum += std::max(0.0f, item.weight);
	}
	if (sum <= 0.0f) return set->entries.front().tile_id;
	float r{ RandomRange(e, 0.0f, sum) };
	for (const auto& item : set->entries) {
		if (!FindTile(e, item.tile_id)) continue;
		r -= std::max(0.0f, item.weight);
		if (r <= 0.0f) return item.tile_id;
	}
	return set->entries.back().tile_id;
}

static int ChooseTile(EditorState& e, F2 world) {
	if (e.brush.source_mode == BrushSourceMode::Single) return e.active_tile_id;
	const auto* set{ FindWeightedTileSet(e, e.active_tile_weighted_set_id) };
	if (!set || set->entries.empty()) return e.active_tile_id;
	if (e.brush.distribution == BrushDistribution::Noise) {
		const float n{ Perlin2(world.x * e.brush.noise_scale, world.y * e.brush.noise_scale, static_cast<std::uint32_t>(e.brush.noise_seed)) };
		const int index{ std::clamp(static_cast<int>(n * static_cast<float>(set->entries.size())), 0, static_cast<int>(set->entries.size()) - 1) };
		return set->entries[static_cast<std::size_t>(index)].tile_id;
	}
	return WeightedTile(e);
}

static int WeightedPrefab(EditorState& e) {
	const auto* set{ FindWeightedPrefabSet(e, e.active_prefab_weighted_set_id) };
	if (!set || set->entries.empty()) return std::clamp(e.active_prefab_index, 0, std::max(0, static_cast<int>(e.prefabs.size()) - 1));
	float sum{};
	for (const auto& item : set->entries) {
		if (item.prefab_index >= 0 && item.prefab_index < static_cast<int>(e.prefabs.size())) sum += std::max(0.0f, item.weight);
	}
	if (sum <= 0.0f) return set->entries.front().prefab_index;
	float r{ RandomRange(e, 0.0f, sum) };
	for (const auto& item : set->entries) {
		if (item.prefab_index < 0 || item.prefab_index >= static_cast<int>(e.prefabs.size())) continue;
		r -= std::max(0.0f, item.weight);
		if (r <= 0.0f) return item.prefab_index;
	}
	return set->entries.back().prefab_index;
}

static int ChoosePrefab(EditorState& e, F2 world) {
	if (e.prefabs.empty()) return -1;
	if (e.brush.source_mode == BrushSourceMode::Single) {
		return std::clamp(e.active_prefab_index, 0, static_cast<int>(e.prefabs.size()) - 1);
	}
	const auto* set{ FindWeightedPrefabSet(e, e.active_prefab_weighted_set_id) };
	if (!set || set->entries.empty()) return std::clamp(e.active_prefab_index, 0, static_cast<int>(e.prefabs.size()) - 1);
	if (e.brush.distribution == BrushDistribution::Noise) {
		const float n{ Perlin2(world.x * e.brush.noise_scale, world.y * e.brush.noise_scale, static_cast<std::uint32_t>(e.brush.noise_seed)) };
		const int index{ std::clamp(static_cast<int>(n * static_cast<float>(set->entries.size())), 0, static_cast<int>(set->entries.size()) - 1) };
		return set->entries[static_cast<std::size_t>(index)].prefab_index;
	}
	return WeightedPrefab(e);
}

static bool PassesDistribution(EditorState& e, F2 world) {
	if (e.brush.distribution == BrushDistribution::Random && RandomRange(e, 0.0f, 1.0f) > e.brush.density) {
		return false;
	}
	if (!e.brush.noise_mask && e.brush.distribution != BrushDistribution::Noise) {
		return true;
	}
	const float n{ Perlin2(world.x * e.brush.noise_scale, world.y * e.brush.noise_scale, static_cast<std::uint32_t>(e.brush.noise_seed)) };
	return n >= e.brush.noise_threshold;
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
	if (layer.locked || layer.kind != LayerKind::Entity) {
		return;
	}
	if (!PassesDistribution(e, world)) {
		return;
	}

	if (e.grid.snap) {
		world = SnapEntityPlacementToGrid(e, world, e.brush.entity_origin);
	}

	if (e.brush.placement != BrushPlacementMode::Continuous && TooCloseToEntity(e, layer.id, world, e.brush.min_spacing)) {
		return;
	}

	const int prefab_index{ ChoosePrefab(e, world) };
	if (prefab_index < 0) {
		return;
	}

	const auto& prefab{ e.prefabs[static_cast<std::size_t>(prefab_index)] };
	Entity entity;
	entity.id = e.next_entity_id++;
	entity.layer_id = layer.id;
	entity.prefab = prefab.name;
	entity.position = world;
	entity.size = prefab.size;
	entity.origin = e.brush.entity_origin;
	if (e.brush.random_rotation) {
		entity.rotation = RandomRange(e, e.brush.rotation_min, e.brush.rotation_max);
	}
	if (e.brush.random_scale) {
		entity.scale = RandomRange(e, e.brush.scale_min, e.brush.scale_max);
	}
	e.entities.push_back(std::move(entity));
	e.stroke.changed = true;
}

static void PlaceTile(EditorState& e, SceneLayer& layer, Tilemap& map, I2 cell) {
	if (layer.locked || layer.kind != LayerKind::Tile) {
		return;
	}

	if (e.brush.avoid_exclusion_mask && IsExcluded(map, cell) && e.brush.operation != BrushOperation::ExclusionMask) {
		return;
	}

	const F2 world{ CellToWorld(map, cell) };
	if (!PassesDistribution(e, world)) {
		return;
	}

	if (e.brush.operation == BrushOperation::ExclusionMask) {
		map.exclusion_mask.insert(cell);
		e.stroke.changed = true;
		return;
	}

	if (e.brush.operation == BrushOperation::Paint && !e.stamp_tiles.empty() && e.brush.source_mode == BrushSourceMode::Single && !e.brush.autotile) {
		const int width{ std::max(1, e.stamp_width) };
		for (int i{}; i < static_cast<int>(e.stamp_tiles.size()); ++i) {
			const I2 target{ cell.x + i % width, cell.y + i / width };
			const int stamp_tile{ e.stamp_tiles[static_cast<std::size_t>(i)] };
			if (e.brush.avoid_exclusion_mask && IsExcluded(map, target)) {
				continue;
			}
			const auto* old{ ReadTileCell(layer, map, target) };
			if (old && old->tile_id >= 0 && !e.brush.replace_occupied_anchor) {
				continue;
			}
			if (!e.brush.allow_visual_overlap && e.brush.tile_paint_mode == TilePaintMode::Tile &&
				TileFootprintOverlapsExisting(e, layer, map, target, stamp_tile, old && old->tile_id >= 0 && e.brush.replace_occupied_anchor)) {
				continue;
			}
			SetTile(layer, map, target, stamp_tile);
			e.stroke.changed = true;
		}
		return;
	}

	const int tile_id{ ChooseTile(e, world) };
	if (tile_id < 0) {
		return;
	}

	if (e.brush.operation == BrushOperation::Replace) {
		const auto* old{ ReadTileCell(layer, map, cell) };
		if (!old || old->tile_id != e.replace_source_tile_id) {
			return;
		}
	} else if (!CanPlaceTileAnchor(e, layer, map, cell, tile_id)) {
		return;
	}

	if (e.brush.autotile) {
		SetTile(layer, map, cell, tile_id, true);
		RecomputeAutotile(e, layer, map, cell);
	} else {
		SetTile(layer, map, cell, tile_id);
	}
	e.stroke.changed = true;
}
static void PaintAt(EditorState& e, F2 world) {
	auto* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->locked) {
		return;
	}

	// Pencil is a one-cell/one-position tool. Unlike the original demo, it keeps
	// the selected Paint/Replace/Exclusion operation while dragging.
	if (e.tool == Tool::Pencil) {
		if (layer->kind == LayerKind::Tile) {
			auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
			if (!map) {
				return;
			}
			const I2 cell{ WorldToCell(*map, world) };
			if (!e.stroke.touched_cells.insert(cell).second) {
				return;
			}
			PlaceTile(e, *layer, *map, cell);
		} else if (e.brush.operation == BrushOperation::Replace) {
			if (e.prefabs.empty()) {
				return;
			}

			const I2 cell{ EntityGridCell(e, world) };
			if (e.grid.snap && !e.stroke.touched_cells.insert(cell).second) {
				return;
			}

			const int from_index{ std::clamp(
				e.replace_source_prefab_index, 0, static_cast<int>(e.prefabs.size()) - 1
			) };
			const int to_index{ ChoosePrefab(e, world) };
			if (to_index < 0) {
				return;
			}
			const auto& from{ e.prefabs[static_cast<std::size_t>(from_index)] };
			const auto& to{ e.prefabs[static_cast<std::size_t>(to_index)] };
			const RasterGrid raster{ ActiveRasterGrid(e) };
			const RectF target{ e.grid.snap
				? RasterCellRect(raster, cell)
				: RectF{ world - F2{ 0.5f, 0.5f }, world + F2{ 0.5f, 0.5f } } };

			for (auto& entity : e.entities) {
				if (entity.layer_id != layer->id || entity.prefab != from.name ||
					e.stroke.touched_entities.contains(entity.id) ||
					!RectsOverlap(EntityBounds(entity), target)) {
					continue;
				}
				e.stroke.touched_entities.insert(entity.id);
				entity.prefab = to.name;
				entity.size = to.size;
				entity.origin = e.brush.entity_origin;
				e.stroke.changed = true;
			}
		} else {
			if (e.grid.snap) {
				const I2 cell{ EntityGridCell(e, world) };
				if (!e.stroke.touched_cells.insert(cell).second) {
					return;
				}
			}
			PlaceEntity(e, *layer, world);
		}
		return;
	}

	const RasterGrid raster{ ActiveRasterGrid(e) };
	const std::vector<I2> raster_cells{ RasterBrushCells(e, world) };

	if (layer->kind == LayerKind::Tile) {
		auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
		if (!map) {
			return;
		}

		for (const I2 cell : raster_cells) {
			if (!e.stroke.touched_cells.insert(cell).second) {
				continue;
			}

			if (e.brush.placement == BrushPlacementMode::Scatter &&
				RandomRange(e, 0.0f, 1.0f) >
					std::min(1.0f, static_cast<float>(e.brush.scatter_count) /
						static_cast<float>(std::max<std::size_t>(1, raster_cells.size())))) {
				continue;
			}
			if (e.brush.placement == BrushPlacementMode::Density &&
				RandomRange(e, 0.0f, 1.0f) > e.brush.density) {
				continue;
			}

			if (e.brush.operation == BrushOperation::ExclusionMask) {
				if (map->exclusion_mask.insert(cell).second) {
					e.stroke.changed = true;
				}
				continue;
			}


			PlaceTile(e, *layer, *map, cell);
		}
		return;
	}

	// Exclusion masks are tilemap data and therefore do not apply to Entity layers.
	if (e.brush.operation == BrushOperation::ExclusionMask) {
		return;
	}


	if (e.brush.operation == BrushOperation::Replace) {
		if (e.prefabs.empty()) {
			return;
		}

		const int from_index{
			std::clamp(
				e.replace_source_prefab_index,
				0,
				static_cast<int>(e.prefabs.size()) - 1
			)
		};
		const int to_index{
			std::clamp(
				e.active_prefab_index,
				0,
				static_cast<int>(e.prefabs.size()) - 1
			)
		};
		const auto& from{ e.prefabs[static_cast<std::size_t>(from_index)] };
		const auto& to{ e.prefabs[static_cast<std::size_t>(to_index)] };

		for (auto& entity : e.entities) {
			if (entity.layer_id != layer->id ||
				entity.prefab != from.name ||
				e.stroke.touched_entities.contains(entity.id) ||
				!RectIntersectsRasterCells(e, world, EntityBounds(entity))) {
				continue;
			}
			e.stroke.touched_entities.insert(entity.id);
			entity.prefab = to.name;
			entity.size = to.size;
			entity.origin = e.brush.entity_origin;
			e.stroke.changed = true;
		}
		return;
	}

	for (const I2 cell : raster_cells) {
		if (!e.stroke.touched_cells.insert(cell).second) {
			continue;
		}

		if (e.brush.placement == BrushPlacementMode::Scatter &&
			RandomRange(e, 0.0f, 1.0f) >
				std::min(1.0f, static_cast<float>(e.brush.scatter_count) /
					static_cast<float>(std::max<std::size_t>(1, raster_cells.size())))) {
			continue;
		}
		if (e.brush.placement == BrushPlacementMode::Density &&
			RandomRange(e, 0.0f, 1.0f) > e.brush.density) {
			continue;
		}

		const RectF cell_rect{ RasterCellRect(raster, cell) };
		const F2 origin_fraction{ EntityOriginFraction(e.brush.entity_origin) };
		const F2 placement{
			cell_rect.min.x + (cell_rect.max.x - cell_rect.min.x) * origin_fraction.x,
			cell_rect.min.y + (cell_rect.max.y - cell_rect.min.y) * origin_fraction.y,
		};
		PlaceEntity(e, *layer, placement);
	}
}
static void EraseAt(EditorState& e, F2 world) {
	auto* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer || layer->locked) {
		return;
	}

	const std::vector<I2> raster_cells{ RasterBrushCells(e, world) };
	const RasterGrid raster{ ActiveRasterGrid(e) };

	if (layer->kind == LayerKind::Tile) {
		auto* map{ FindTilemap(e, layer->tile.tilemap_id) };
		if (!map) {
			return;
		}

		if (e.brush.operation == BrushOperation::ExclusionMask) {
			for (const I2 cell : raster_cells) {
				if (map->exclusion_mask.erase(cell) > 0) {
					e.stroke.changed = true;
				}
			}
			return;
		}

		std::unordered_set<I2, I2Hash> anchors_to_erase;
		for (const I2 brush_cell : raster_cells) {
			const RectF brush_rect{ RasterCellRect(raster, brush_cell) };
			ForEachTileAnchor(*layer, *map, [&](I2 anchor, const TileCell& tile) {
				if (RectsOverlap(brush_rect, TileAnchorRect(e, *map, anchor, tile.tile_id, tile.offset))) {
					if (!e.brush.eraser_current_source_only ||
						tile.tile_id == e.active_tile_id) {
						anchors_to_erase.insert(anchor);
					}
				}
			});
		}

		for (const I2 anchor : anchors_to_erase) {
			const auto* old{ ReadTileCell(*layer, *map, anchor) };
			const bool was_terrain{ old && old->terrain };
			EraseTile(*layer, *map, anchor);
			e.selected_tile_cells.erase(anchor);
			if (was_terrain) {
				RecomputeAutotile(e, *layer, *map, anchor);
			}
			e.stroke.changed = true;
		}
		return;
	}

	const std::string current_prefab{
		e.prefabs.empty()
			? std::string{}
			: e.prefabs[static_cast<std::size_t>(
				std::clamp(
					e.active_prefab_index,
					0,
					static_cast<int>(e.prefabs.size()) - 1
				)
			)].name
	};

	const auto old_size{ e.entities.size() };
	e.entities.erase(
		std::remove_if(
			e.entities.begin(),
			e.entities.end(),
			[&](const Entity& entity) {
				if (entity.layer_id != layer->id ||
					!RectIntersectsRasterCells(e, world, EntityBounds(entity))) {
					return false;
				}
				if (e.brush.eraser_current_source_only &&
					entity.prefab != current_prefab) {
					return false;
				}
				e.selected_entities.erase(entity.id);
				if (e.primary_entity_id == entity.id) {
					e.primary_entity_id = -1;
				}
				return true;
			}
		),
		e.entities.end()
	);

	if (e.entities.size() != old_size) {
		e.stroke.changed = true;
	}
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
		e.brush.source_mode == BrushSourceMode::Single
			? e.active_tile_id
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

		if (!e.brush.avoid_exclusion_mask || !IsExcluded(*map, cell)) {
			const int tile_id{ ChooseTile(e, CellToWorld(*map, cell)) };
			if (tile_id >= 0) {
				// Flood fill intentionally replaces the connected source region.
				// It does not depend on the normal "replace occupied anchors" toggle.
				if (e.brush.autotile) {
					SetTile(*layer, *map, cell, tile_id, true);
				} else {
					SetTile(*layer, *map, cell, tile_id);
				}
				e.stroke.changed = true;
			}
		}

		stack.push_back({ cell.x + 1, cell.y });
		stack.push_back({ cell.x - 1, cell.y });
		stack.push_back({ cell.x, cell.y + 1 });
		stack.push_back({ cell.x, cell.y - 1 });
	}

	if (e.brush.autotile && e.stroke.changed) {
		for (const I2 cell : visited) {
			RecomputeAutotile(e, *layer, *map, cell);
		}
	}
}

static void Eyedrop(EditorState& e, F2 world) {
	auto* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer) {
		return;
	}

	if (layer->kind == LayerKind::Tile) {
		if (const auto* map = FindTilemap(e, layer->tile.tilemap_id)) {
			if (const auto anchor = FindVisibleTileAnchorAtWorld(e, *layer, *map, world)) {
				if (const auto* cell = ReadTileCell(*layer, *map, *anchor);
					cell && cell->tile_id >= 0) {
					e.active_tile_id = cell->tile_id;
					e.stamp_tiles.clear();
				}
			}
		}
		return;
	}

	float best{ std::numeric_limits<float>::max() };
	const Entity* found{};

	for (const auto& entity : e.entities) {
		if (entity.layer_id != layer->id) {
			continue;
		}

		const RectF bounds{ EntityBounds(entity) };
		const float d{ Distance(RectCenter(bounds), world) };
		const bool inside{
			world.x >= bounds.min.x && world.x <= bounds.max.x &&
			world.y >= bounds.min.y && world.y <= bounds.max.y
		};

		if (d < best && inside) {
			best = d;
			found = &entity;
		}
	}

	if (!found) {
		return;
	}

	for (int i{}; i < static_cast<int>(e.prefabs.size()); ++i) {
		if (e.prefabs[static_cast<std::size_t>(i)].name == found->prefab) {
			e.active_prefab_index = i;
			break;
		}
	}
}

static bool PointInEntity(const Entity& entity, F2 p) {
	const RectF bounds{ EntityBounds(entity) };
	return p.x >= bounds.min.x && p.x <= bounds.max.x &&
		p.y >= bounds.min.y && p.y <= bounds.max.y;
}

static void SelectClick(EditorState& e, F2 world, bool add, bool toggle) {
	const auto* active_layer{ FindLayer(e, e.active_layer_id) };
	if (active_layer && active_layer->kind == LayerKind::Noise) {
		return;
	}

	if (active_layer && active_layer->kind == LayerKind::Tile) {
		const auto* map{ FindTilemap(e, active_layer->tile.tilemap_id) };
		if (!map || active_layer->locked || !active_layer->selectable) {
			return;
		}

		if (!add && !toggle) {
			e.selected_tile_cells.clear();
		}
		e.selected_entities.clear();
		e.primary_entity_id = -1;
		e.selected_tile_layer_id = active_layer->id;

		const auto anchor{
			FindVisibleTileAnchorAtWorld(e, *active_layer, *map, world)
		};
		if (!anchor) {
			if (!add && !toggle) {
				e.selected_tile_layer_id = -1;
			}
			return;
		}

		if (toggle && e.selected_tile_cells.contains(*anchor)) {
			e.selected_tile_cells.erase(*anchor);
		} else {
			e.selected_tile_cells.insert(*anchor);
		}

		if (e.selected_tile_cells.empty()) {
			e.selected_tile_layer_id = -1;
		}
		return;
	}

	int found{ -1 };
	for (
		auto layer_it = e.layers.rbegin();
		layer_it != e.layers.rend() && found < 0;
		++layer_it
	) {
		if (
			!layer_it->visible ||
			layer_it->locked ||
			!layer_it->selectable ||
			layer_it->kind != LayerKind::Entity
		) {
			continue;
		}

		for (
			auto entity_it = e.entities.rbegin();
			entity_it != e.entities.rend();
			++entity_it
		) {
			if (
				entity_it->layer_id == layer_it->id &&
				PointInEntity(*entity_it, world)
			) {
				found = entity_it->id;
				break;
			}
		}
	}

	if (!add && !toggle) {
		e.selected_entities.clear();
	}
	e.selected_tile_cells.clear();
	e.selected_tile_layer_id = -1;

	if (found >= 0) {
		if (toggle && e.selected_entities.contains(found)) {
			e.selected_entities.erase(found);
			if (e.primary_entity_id == found) {
				e.primary_entity_id =
					e.selected_entities.empty()
						? -1
						: *e.selected_entities.begin();
			}
		} else {
			e.selected_entities.insert(found);
			e.primary_entity_id = found;
		}
	} else if (!add && !toggle) {
		e.primary_entity_id = -1;
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
						TileAnchorRect(e, *map, cell, tile.tile_id, tile.offset)
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
					TileAnchorRect(e, *map, anchor, tile.tile_id, tile.offset)
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
	if (layer.kind != LayerKind::Entity || e.prefabs.empty() || cells.empty()) {
		return;
	}

	const int from_index{ std::clamp(
		e.replace_source_prefab_index,
		0,
		static_cast<int>(e.prefabs.size()) - 1
	) };
	const auto& from{ e.prefabs[static_cast<std::size_t>(from_index)] };

	for (auto& entity : e.entities) {
		if (entity.layer_id != layer.id || entity.prefab != from.name) {
			continue;
		}

		const RectF bounds{ EntityBounds(entity) };
		bool overlaps{};
		for (const I2 cell : cells) {
			if (RectsOverlap(bounds, RasterCellRect(raster, cell))) {
				overlaps = true;
				break;
			}
		}
		if (!overlaps) {
			continue;
		}

		const int to_index{ ChoosePrefab(e, entity.position) };
		if (to_index < 0) {
			continue;
		}
		const auto& to{ e.prefabs[static_cast<std::size_t>(to_index)] };
		entity.prefab = to.name;
		entity.size = to.size;
		entity.origin = e.brush.entity_origin;
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
		const F2 origin_fraction{ EntityOriginFraction(e.brush.entity_origin) };
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
		const F2 origin_fraction{ EntityOriginFraction(e.brush.entity_origin) };
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

	SceneLayer noise_demo;
	noise_demo.id = e.next_layer_id++;
	noise_demo.name = "Noise Demo";
	noise_demo.kind = LayerKind::Noise;
	noise_demo.visible = true;
	noise_demo.noise.target = NoiseTargetKind::Tile;
	noise_demo.noise.tilemap_id = map.id;
	noise_demo.noise.grid_size = map.cell_size;
	noise_demo.noise.grid_offset = map.origin;
	noise_demo.noise.show_noise_preview = true;
	noise_demo.noise.show_generated_preview = true;
	noise_demo.noise.noise_preview_alpha = 0.62f;
	noise_demo.noise.bounded = true;
	noise_demo.noise.bounds_min = { -320.0f, -240.0f };
	noise_demo.noise.bounds_max = { 320.0f, 240.0f };
	NoiseField demo_field;
	demo_field.type = NoiseType::Perlin;
	demo_field.name = "Terrain Perlin";
	demo_field.frequency = 0.015f;
	demo_field.octaves = 4;
	demo_field.thresholds = {
		NoiseThresholdRegion{ .minimum = 0.0f, .maximum = 0.38f, .tile_id = water, .enabled = true },
		NoiseThresholdRegion{ .minimum = 0.38f, .maximum = 0.55f, .tile_id = dirt, .enabled = true },
		NoiseThresholdRegion{ .minimum = 0.55f, .maximum = 0.72f, .tile_id = grass, .enabled = true },
		NoiseThresholdRegion{ .minimum = 0.72f, .maximum = 1.0f, .tile_id = -1, .enabled = false },
	};
	noise_demo.noise.fields.push_back(std::move(demo_field));
	e.layers.push_back(noise_demo);

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
	e.brush.entity_origin = EntityOrigin::TopLeft;
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
		case Tool::Area:
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

static bool ToolButton(EditorState& e, Tool tool, const char* id) {
	const ImVec2 size{ 26.0f, 26.0f };
	const ImVec2 p0{ ImGui::GetCursorScreenPos() };
	ImGui::InvisibleButton(id, size);
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
		ImGui::SetTooltip("%s", ToolTooltip(tool));
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
		ToolButton(e, Tool::Area, "##tool_area"); ImGui::SameLine();
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
	const bool tile_layer{
		active_layer && active_layer->kind == LayerKind::Tile
	};
	const bool noise_layer{
		active_layer && active_layer->kind == LayerKind::Noise
	};

	// This function is the contextual toolbar directly below the paint-tool icons.
	// Never rely on a caller's previous SameLine() state: every tool begins its own row.
	bool row_has_item{};
	auto next_item = [&]() {
		if (row_has_item) {
			ImGui::SameLine();
		}
		row_has_item = true;
	};
	auto new_row = [&]() {
		// ImGui already advances the cursor below the current item. Resetting the
		// SameLine group is enough to begin a compact new toolbar row.
		row_has_item = false;
	};

	auto draw_entity_origin = [&]() {
		next_item();
		int origin{ static_cast<int>(e.brush.entity_origin) };
		const char* origins[]{
			"Top Left", "Top", "Top Right",
			"Left", "Center", "Right",
			"Bottom Left", "Bottom", "Bottom Right"
		};
		ImGui::SetNextItemWidth(112.0f);
		if (ImGui::Combo("Origin##entity_origin", &origin, origins, 9)) {
			e.brush.entity_origin = static_cast<EntityOrigin>(origin);
		}
		ItemTooltip(
			"Entity placement origin. The selected point is aligned to the grid anchor.\n"
			"Top Left keeps a same-size entity fully inside its grid cell."
		);
	};

	auto draw_tile_paint_mode = [&]() {
		next_item();
		const char* tile_modes[]{ "Grid Paint", "Tile Paint" };
		int tile_mode{ static_cast<int>(e.brush.tile_paint_mode) };
		ImGui::SetNextItemWidth(118.0f);
		if (ImGui::Combo("Placement##tile_placement", &tile_mode, tile_modes, 2)) {
			e.brush.tile_paint_mode = static_cast<TilePaintMode>(tile_mode);
		}
		ItemTooltip(
			"Grid Paint: every raster grid cell may be a tile anchor, even when native-size tiles overlap.\n"
			"Tile Paint: anchor spacing expands to the tile's whole-cell footprint to avoid overlap."
		);
	};

	auto draw_operation = [&]() {
		next_item();
		if (tile_layer) {
			const char* operations[]{ "Paint", "Replace", "Exclusion Mask" };
			int choice{
				e.brush.operation == BrushOperation::Replace ? 1 :
				e.brush.operation == BrushOperation::ExclusionMask ? 2 : 0
			};
			ImGui::SetNextItemWidth(124.0f);
			if (ImGui::Combo("Operation##paint_operation", &choice, operations, 3)) {
				e.brush.operation =
					choice == 1 ? BrushOperation::Replace :
					choice == 2 ? BrushOperation::ExclusionMask :
					BrushOperation::Paint;
			}
			ItemTooltip(
				"Paint: place the active tile/source.\n"
				"Replace: only replace anchors containing the configured source tile.\n"
				"Exclusion Mask: paint no-paint mask cells instead of tiles."
			);
		} else {
			const char* operations[]{ "Paint", "Replace" };
			int choice{ e.brush.operation == BrushOperation::Replace ? 1 : 0 };
			ImGui::SetNextItemWidth(112.0f);
			if (ImGui::Combo("Operation##paint_operation", &choice, operations, 2)) {
				e.brush.operation = choice == 1 ? BrushOperation::Replace : BrushOperation::Paint;
			}
			ItemTooltip(
				"Paint: create prefab entities.\n"
				"Replace: replace matching prefab entities on the rasterized tool cells."
			);
		}
	};

	auto draw_placement_mode = [&]() {
		next_item();
		const char* placements[]{ "Continuous", "Scatter", "Density" };
		int placement{ static_cast<int>(e.brush.placement) };
		ImGui::SetNextItemWidth(118.0f);
		if (ImGui::Combo("Placement##brush_placement", &placement, placements, 3)) {
			e.brush.placement = static_cast<BrushPlacementMode>(placement);
		}
		ItemTooltip(
			"Continuous: affect every raster candidate in the brush footprint.\n"
			"Scatter: randomly choose approximately Count candidates.\n"
			"Density: randomly affect the configured fraction of candidates."
		);
	};

	auto draw_distribution = [&]() {
		next_item();
		const char* distributions[]{ "Uniform", "Random", "Noise" };
		int distribution{ static_cast<int>(e.brush.distribution) };
		ImGui::SetNextItemWidth(108.0f);
		if (ImGui::Combo("Distribution##brush_distribution", &distribution, distributions, 3)) {
			e.brush.distribution = static_cast<BrushDistribution>(distribution);
		}
		ItemTooltip(
			"Uniform: use the configured source normally.\n"
			"Random: apply Density as a random probability.\n"
			"Noise: use the procedural noise field to choose/filter candidates."
		);
	};

	auto draw_brush_size = [&]() {
		next_item();
		float diameter{ SnapBrushDiameter(e, e.brush.radius * 2.0f) };

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Diameter");
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

		if (ImGui::SmallButton("-##brush_size")) {
			AdjustBrushDiameter(e, -1);
			diameter = e.brush.radius * 2.0f;
		}
		ItemTooltip(
			e.brush.size_snap == BrushSizeSnapMode::Free
				? "Decrease diameter to the previous common brush size."
				: "Decrease diameter by one selected grid/tile-size multiple."
		);

		// Keep the decrement button, slider, and increment button immediately
		// adjacent so they read as one compound diameter control.
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::SetNextItemWidth(72.0f);
		const float drag_speed{
			e.brush.size_snap == BrushSizeSnapMode::Free
				? 0.25f
				: std::max(0.05f, BrushSnapUnit(e) * 0.05f)
		};
		if (ImGui::DragFloat(
				"##brush_diameter",
				&diameter,
				drag_speed,
				1.0f,
				2048.0f,
				"%.0f"
			)) {
			e.brush.radius = SnapBrushDiameter(e, diameter) * 0.5f;
		}
		ItemTooltip(
			"Raster brush diameter. Drag slowly to resize.\n"
			"Grid/Tile snapping quantizes the value to exact multiples."
		);

		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		if (ImGui::SmallButton("+##brush_size")) {
			AdjustBrushDiameter(e, 1);
		}
		ItemTooltip(
			e.brush.size_snap == BrushSizeSnapMode::Free
				? "Increase diameter to the next common brush size."
				: "Increase diameter by one selected grid/tile-size multiple."
		);
	};

	auto draw_size_snap = [&]() {
		next_item();
		const char* snap_names[]{ "Free", "Grid", "Tile" };
		int snap_mode{ static_cast<int>(e.brush.size_snap) };
		ImGui::SetNextItemWidth(105.0f);
		if (ImGui::Combo("Size Snap##size_snap", &snap_mode, snap_names, 3)) {
			e.brush.size_snap = static_cast<BrushSizeSnapMode>(snap_mode);
			e.brush.radius = SnapBrushDiameter(e, e.brush.radius * 2.0f) * 0.5f;
		}
		ItemTooltip(
			"Free: arbitrary/common brush sizes.\n"
			"Grid: diameter is a multiple of the active grid size.\n"
			"Tile: diameter is a multiple of the active source tile size."
		);
	};

	auto draw_brush_shape = [&]() {
		next_item();
		const char* shape_names[]{ "Circle", "Square" };
		int shape{ static_cast<int>(e.brush.shape) };
		ImGui::SetNextItemWidth(94.0f);
		if (ImGui::Combo("Shape##brush_shape", &shape, shape_names, 2)) {
			e.brush.shape = static_cast<BrushShape>(shape);
		}
		ItemTooltip("Rasterized brush footprint shape. Both choices resolve to complete grid cells.");
	};

	auto draw_options = [&]() {
		next_item();
		ImGui::SetNextItemWidth(94.0f);
		const bool options_open{ ImGui::BeginCombo("Options##paint_options", "Options") };
		const bool options_hovered{ ImGui::IsItemHovered() };

		if (options_open) {
			if (tile_layer) {
				ImGui::TextDisabled("Tile placement");
				ToggleComboChoice(
					"Replace occupied anchors",
					e.brush.replace_occupied_anchor,
					"Allow normal Paint to overwrite a tile already anchored in the same cell."
				);
				ToggleComboChoice(
					"Allow visual overlap",
					e.brush.allow_visual_overlap,
					"Allow Tile Paint placements whose native-size rectangles overlap existing tiles.",
					e.brush.tile_paint_mode == TilePaintMode::Tile
				);
				ToggleComboChoice(
					"Avoid exclusion mask",
					e.brush.avoid_exclusion_mask,
					"Prevent ordinary painting in cells marked by the Exclusion Mask operation."
				);
				ToggleComboChoice(
					"Autotile / terrain",
					e.brush.autotile,
					"Mark painted cells as terrain and recompute neighboring autotile variants."
				);
			} else {
				ImGui::TextDisabled("Entity placement");
				ToggleComboChoice(
					"Random rotation",
					e.brush.random_rotation,
					"Randomize the rotation of newly painted prefab entities."
				);
				ToggleComboChoice(
					"Random scale",
					e.brush.random_scale,
					"Randomize the scale of newly painted prefab entities."
				);
				if (e.tool == Tool::Line) {
					ToggleComboChoice(
						"Align rotation to line",
						e.brush.line_align_rotation,
						"Rotate newly painted line entities to match the line direction."
					);
				}
			}

			ImGui::Separator();
			ToggleComboChoice(
				"Noise mask",
				e.brush.noise_mask,
				"Only affect candidates whose noise value passes the configured threshold."
			);
			if (e.tool == Tool::Erase) {
				ToggleComboChoice(
					"Current source only",
					e.brush.eraser_current_source_only,
					tile_layer
						? "Erase only tiles matching the currently active tile."
						: "Erase only entities matching the currently active prefab."
				);
			}
			ImGui::EndCombo();
		}

		if (options_hovered) {
			std::string tooltip{ "Selected options:" };
			bool any{};
			auto append = [&](bool enabled, const char* name) {
				if (!enabled) return;
				tooltip += "\n- ";
				tooltip += name;
				any = true;
			};
			append(tile_layer && e.brush.replace_occupied_anchor, "Replace occupied anchors");
			append(tile_layer && e.brush.allow_visual_overlap, "Allow visual overlap");
			append(tile_layer && e.brush.avoid_exclusion_mask, "Avoid exclusion mask");
			append(tile_layer && e.brush.autotile, "Autotile / terrain");
			append(!tile_layer && e.brush.random_rotation, "Random rotation");
			append(!tile_layer && e.brush.random_scale, "Random scale");
			append(!tile_layer && e.tool == Tool::Line && e.brush.line_align_rotation, "Align rotation to line");
			append(e.brush.noise_mask, "Noise mask");
			append(e.tool == Tool::Erase && e.brush.eraser_current_source_only, "Current source only");
			if (!any) tooltip += "\n- None";
			ImGui::SetTooltip("%s", tooltip.c_str());
		}
	};

	auto draw_random_popup = [&]() {
		if (tile_layer ||
			(!e.brush.random_rotation && !e.brush.random_scale) ||
			!(e.tool == Tool::Pencil || e.tool == Tool::Brush ||
			  e.tool == Tool::Line || e.tool == Tool::Area)) {
			return;
		}
		next_item();
		if (ImGui::Button("Random...")) {
			ImGui::OpenPopup("Random Transform Settings");
		}
		ItemTooltip("Configure random rotation/scale ranges used by painted entities.");
		if (ImGui::BeginPopup("Random Transform Settings")) {
			if (e.brush.random_rotation) {
				ImGui::DragFloatRange2(
					"Rotation",
					&e.brush.rotation_min,
					&e.brush.rotation_max,
					0.25f,
					-3600.0f,
					3600.0f,
					"%.1f deg",
					"%.1f deg"
				);
				ItemTooltip("Random rotation range applied independently to each new entity.");
			}
			if (e.brush.random_scale) {
				ImGui::DragFloatRange2(
					"Scale",
					&e.brush.scale_min,
					&e.brush.scale_max,
					0.01f,
					0.01f,
					8.0f,
					"%.2f",
					"%.2f"
				);
				ItemTooltip("Random scale multiplier range applied independently to each new entity.");
			}
			ImGui::EndPopup();
		}
	};

	auto draw_noise_popup = [&]() {
		if (!e.brush.noise_mask && e.brush.distribution != BrushDistribution::Noise) {
			return;
		}
		next_item();
		if (ImGui::Button("Noise...")) {
			ImGui::OpenPopup("Noise Settings");
		}
		ItemTooltip("Configure the procedural noise used by Noise distribution/masking.");
		if (ImGui::BeginPopup("Noise Settings")) {
			ImGui::DragFloat("Scale", &e.brush.noise_scale, 0.001f, 0.001f, 1.0f, "%.3f");
			ItemTooltip("Noise frequency. Smaller values produce larger coherent regions.");
			ImGui::SliderFloat("Threshold", &e.brush.noise_threshold, 0.0f, 1.0f);
			ItemTooltip("Minimum noise value required when Noise Mask is enabled.");
			ImGui::DragInt("Seed", &e.brush.noise_seed);
			ItemTooltip("Seed controlling the deterministic noise pattern.");
			ImGui::EndPopup();
		}
	};

	if (!active_layer) {
		ImGui::TextDisabled("Select a layer in the Layers window to edit.");
		return;
	}
	if (noise_layer) {
		return;
	}

	// Select tool: SelectMode is the primary contextual mode.
	if (e.tool == Tool::Select) {
		next_item();
		const char* modes[]{ "Click + Marquee", "Selection Brush" };
		int mode{ static_cast<int>(e.brush.select_mode) };
		ImGui::SetNextItemWidth(152.0f);
		if (ImGui::Combo("Mode##select_mode", &mode, modes, 2)) {
			e.brush.select_mode = static_cast<SelectMode>(mode);
		}
		ItemTooltip(
			"Click + Marquee: select individual items or rectangular regions.\n"
			"Selection Brush: paint selection over rasterized grid cells; hold Ctrl to remove."
		);

		if (e.brush.select_mode == SelectMode::Brush) {
			draw_brush_size();
			draw_size_snap();
			draw_brush_shape();
		}

		next_item();
		ImGui::BeginDisabled(!HasSelection(e));
		if (ImGui::SmallButton("Deselect")) {
			DeselectAll(e);
		}
		ImGui::EndDisabled();
		ItemTooltip("Clear the current entity/tile selection (Ctrl+D).");
		return;
	}

	// Move tool: its snap mode is the only primary tool mode.
	if (e.tool == Tool::Move) {
		next_item();
		const char* modes[]{ "Grid", "Free" };
		int mode{ static_cast<int>(e.move.snap_mode) };
		ImGui::SetNextItemWidth(104.0f);
		if (ImGui::Combo("Move##move_mode", &mode, modes, 2)) {
			e.move.snap_mode = static_cast<MoveSnapMode>(mode);
		}
		ItemTooltip(
			"Grid: selected items move in whole grid/tile-cell steps.\n"
			"Free: selected items move in world-space units. Hold Ctrl to temporarily invert this mode."
		);
		return;
	}

	// Fill and Eyedropper have no contextual toolbar controls. Their behavior is
	// described by the tool-button tooltip, so they consume no extra viewport height.
	if (e.tool == Tool::Eyedropper || e.tool == Tool::Fill) {
		return;
	}

	// -------------------- Primary contextual mode row --------------------
	// Keep enum-like controls here so the currently selected tool's behavior is
	// immediately visible directly below the icon toolbar.
	if (e.tool == Tool::Pencil) {
		draw_operation();
		if (tile_layer) draw_tile_paint_mode();
		else draw_entity_origin();
	} else if (e.tool == Tool::Brush) {
		draw_operation();
		draw_placement_mode();
		draw_distribution();
		if (tile_layer) draw_tile_paint_mode();
		else draw_entity_origin();
	} else if (e.tool == Tool::Line) {
		draw_operation();
		if (tile_layer) draw_tile_paint_mode();
		else draw_entity_origin();
	} else if (e.tool == Tool::Area) {
		draw_operation();
		if (tile_layer) draw_tile_paint_mode();
		else draw_entity_origin();

		next_item();
		const char* areas[]{ "Fill", "Outline", "Corners", "Random Fill" };
		int area{ static_cast<int>(e.brush.area_mode) };
		ImGui::SetNextItemWidth(116.0f);
		if (ImGui::Combo("Area##area_mode", &area, areas, 4)) {
			e.brush.area_mode = static_cast<AreaMode>(area);
		}
		ItemTooltip(
			"Fill: paint every raster cell in the rectangle.\n"
			"Outline: paint only the perimeter.\n"
			"Corners: paint corner blocks.\n"
			"Random Fill: sparsely fill according to Density."
		);
	} else if (e.tool == Tool::Erase) {
		if (tile_layer) {
			next_item();
			const char* erase_modes[]{ "Erase Tiles", "Erase Mask" };
			int choice{ e.brush.operation == BrushOperation::ExclusionMask ? 1 : 0 };
			ImGui::SetNextItemWidth(116.0f);
			if (ImGui::Combo("Mode##erase_mode", &choice, erase_modes, 2)) {
				e.brush.operation = choice == 1 ? BrushOperation::ExclusionMask : BrushOperation::Paint;
			}
			ItemTooltip(
				"Erase Tiles: remove tile anchors touched by the raster footprint.\n"
				"Erase Mask: remove exclusion-mask cells instead."
			);
		}
	}

	// -------------------- Secondary controls --------------------
	// Brush has enough controls to justify a second compact row. Simpler tools
	// keep their numeric/options controls on the same row as their primary modes.
	if (e.tool == Tool::Brush) {
		new_row();
	}

	if (e.tool == Tool::Brush || e.tool == Tool::Erase) {
		draw_brush_size();
		draw_size_snap();
		draw_brush_shape();
	}

	const bool pencil_spacing{ !tile_layer && !e.grid.snap && e.tool == Tool::Pencil };
	const bool brush_spacing{ e.tool == Tool::Brush || e.tool == Tool::Erase };
	if (pencil_spacing || brush_spacing) {
		next_item();
		ImGui::SetNextItemWidth(86.0f);
		ImGui::DragFloat("Spacing##stroke_spacing", &e.brush.spacing, 0.25f, 1.0f, 512.0f, "%.0f");
		e.brush.spacing = std::max(1.0f, e.brush.spacing);
		ItemTooltip(pencil_spacing
			? "Distance between samples while dragging an unsnapped Pencil. Smaller spacing creates denser free-form placements."
			: "Maximum distance between successive Brush/Eraser footprint samples while dragging. Smaller values make a smoother continuous stroke; larger values separate stamps.");
	}

	if (e.tool == Tool::Line) {
		next_item();
		ImGui::SetNextItemWidth(76.0f);
		ImGui::DragInt("Thickness##line", &e.brush.line_thickness, 0.15f, 1, 32);
		e.brush.line_thickness = std::max(1, e.brush.line_thickness);
		ItemTooltip("Raster line thickness measured in grid cells.");

		next_item();
		ImGui::SetNextItemWidth(76.0f);
		ImGui::DragInt("Spacing##line", &e.brush.line_spacing_cells, 0.1f, 1, 32);
		e.brush.line_spacing_cells = std::max(1, e.brush.line_spacing_cells);
		ItemTooltip("Raster-cell interval along the line. 1 keeps every line cell; 2 keeps every second cell, and so on.");
	}

	if (e.tool == Tool::Brush) {
		if (e.brush.placement == BrushPlacementMode::Scatter) {
			next_item();
			ImGui::SetNextItemWidth(72.0f);
			ImGui::DragInt("Count##scatter_count", &e.brush.scatter_count, 1.0f, 1, 100);
			ItemTooltip("Approximate number of raster candidates affected by each brush footprint.");
		} else if (
			e.brush.placement == BrushPlacementMode::Density ||
			e.brush.distribution == BrushDistribution::Random
		) {
			next_item();
			ImGui::SetNextItemWidth(82.0f);
			ImGui::SliderFloat("Density##brush_density", &e.brush.density, 0.01f, 1.0f, "%.2f");
			ItemTooltip("Probability/fraction of candidate cells affected by the brush.");
		}

		if (!tile_layer && e.brush.placement != BrushPlacementMode::Continuous) {
			next_item();
			ImGui::SetNextItemWidth(88.0f);
			ImGui::DragFloat("Min Spacing##entity_min_spacing", &e.brush.min_spacing, 0.25f, 0.0f, 1024.0f, "%.0f");
			e.brush.min_spacing = std::max(0.0f, e.brush.min_spacing);
			ItemTooltip("Minimum world-space separation allowed between newly scattered/density-painted entities.");
		}
	}

	if (e.tool == Tool::Area) {
		if (e.brush.area_mode == AreaMode::Outline || e.brush.area_mode == AreaMode::Corners) {
			next_item();
			ImGui::SetNextItemWidth(82.0f);
			ImGui::DragInt("Thickness##area", &e.brush.area_thickness, 0.15f, 1, 32);
			e.brush.area_thickness = std::max(1, e.brush.area_thickness);
			ItemTooltip(
				e.brush.area_mode == AreaMode::Outline
					? "Outline thickness measured inward from the rectangle edge, in grid cells."
					: "Corner block width/height measured in grid cells."
			);
		}
		if (e.brush.area_mode == AreaMode::RandomFill) {
			next_item();
			ImGui::SetNextItemWidth(82.0f);
			ImGui::SliderFloat("Density##area", &e.brush.density, 0.01f, 1.0f, "%.2f");
			ItemTooltip("Probability that each raster cell in Random Fill is painted.");
		}
	}


	if (e.tool == Tool::Pencil || e.tool == Tool::Brush ||
		e.tool == Tool::Line || e.tool == Tool::Area || e.tool == Tool::Erase) {
		draw_options();
	}
	draw_random_popup();
	draw_noise_popup();
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
		const F2 world{ CellToWorld(*map, anchor.cell) + cell.offset };
		const F2 draw_size{ TileWorldSize(e, *map, cell.tile_id) };
		const auto [p0_px, p1_px]{ PixelCoveredScreenRect(e, world, world + draw_size) };
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
		e.tool == Tool::Line || e.tool == Tool::Area ||
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
			return old && old->tile_id == e.replace_source_tile_id;
		}
		if (e.active_tile_id < 0) {
			return false;
		}

		if (!e.stroke.active) {
			const auto* old{ ReadTileCell(tile_layer, map, cell) };
			const bool occupied{ old && old->tile_id >= 0 };
			if (occupied && !e.brush.replace_occupied_anchor) {
				return false;
			}

			if (e.brush.tile_paint_mode == TilePaintMode::Tile) {
				const I2 footprint{
					TileFootprintCells(e, map, e.active_tile_id)
				};
				const I2 origin{ WorldToCell(map, mouse_world) };
				if (
					FloorMod(cell.x - origin.x, footprint.x) != 0 ||
					FloorMod(cell.y - origin.y, footprint.y) != 0
				) {
					return false;
				}

				if (
					!e.brush.allow_visual_overlap &&
					TileFootprintOverlapsExisting(
						e,
						tile_layer,
						map,
						cell,
						e.active_tile_id,
						occupied && e.brush.replace_occupied_anchor
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
			e.active_tile_id
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

			const int tile_id{
				preview_operation == BrushOperation::Replace
					? e.active_tile_id
					: e.active_tile_id
			};
			const auto* tile{ FindTile(e, tile_id) };
			const F2 w0{ CellToWorld(map, cell) };
			const F2 size{ TileWorldSize(e, map, tile_id) };
			const F2 p0{ WorldToScreen(e, w0) };
			const F2 p1{ WorldToScreen(e, w0 + size) };

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

	if (e.stroke.active && (e.tool == Tool::Line || e.tool == Tool::Area)) {
		std::vector<I2> cells{
			e.tool == Tool::Line
				? RasterLineCells(e, e.stroke.start_world, mouse_world)
				: RasterAreaCells(e, e.stroke.start_world, mouse_world)
		};

		if (
			e.tool == Tool::Area &&
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
	// Line/Area previews do not modify the scene until release, so cancelling
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
	return std::max(1.0f, e.brush.spacing * 0.5f);
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

	if (
		e.stroke.active &&
		(e.tool == Tool::Line || e.tool == Tool::Area) &&
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
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			BeginStroke(e, mouse_world, e.tool == Tool::Brush ? "Brush" : "Erase");
			if (e.tool == Tool::Brush) PaintAt(e, mouse_world);
			else EraseAt(e, mouse_world);
			e.stroke.last_world = mouse_world;
		}
		if (e.stroke.active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			e.stroke.current_world = mouse_world;
			const float distance{ Distance(e.stroke.last_world, mouse_world) };
			const float spacing{ std::max(1.0f, e.brush.spacing) };
			if (distance >= spacing) {
				const int samples{ std::max(1, static_cast<int>(std::ceil(distance / spacing))) };
				for (int i{ 1 }; i <= samples; ++i) {
					const F2 sample{ Lerp(e.stroke.last_world, mouse_world, static_cast<float>(i) / static_cast<float>(samples)) };
					if (e.tool == Tool::Brush) PaintAt(e, sample);
					else EraseAt(e, sample);
				}
				e.stroke.last_world = mouse_world;
			}
		}
		if (e.stroke.active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			EndStroke(e, e.tool == Tool::Brush ? "Brush Paint" : "Erase");
		}
		return;
	}

	if (e.tool == Tool::Line || e.tool == Tool::Area) {
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			BeginStroke(e, mouse_world, e.tool == Tool::Line ? "Line" : "Area");
		}
		if (e.stroke.active) {
			e.stroke.current_world = mouse_world;
		}
		if (e.stroke.active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (e.tool == Tool::Line) {
				ApplyLine(e, e.stroke.start_world, mouse_world);
			} else {
				ApplyArea(e, e.stroke.start_world, mouse_world);
			}
			EndStroke(e, e.tool == Tool::Line ? "Line Paint" : "Area Paint");
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

	for (auto& layer : e.layers) {
		if (layer.kind == LayerKind::Tile) {
			UpdateStreamingForLayer(e, layer);
			DrawTileLayer(e, layer, dl);
		} else if (layer.kind == LayerKind::Entity) {
			DrawEntityLayer(e, layer, dl);
		} else if (layer.kind == LayerKind::Noise) {
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
	for (const auto& layer : e.layers) {
		ImGui::PushID(layer.id);
		const bool leaf{ layer.kind != LayerKind::Entity };
		const bool open{ ImGui::TreeNodeEx(layer.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | (leaf ? ImGuiTreeNodeFlags_Leaf : 0)) };
		if (ImGui::IsItemClicked()) {
			e.active_layer_id = layer.id;
		}
		if (open) {
			if (layer.kind == LayerKind::Entity) {
				for (const auto& entity : e.entities) {
					if (entity.layer_id != layer.id) continue;
					const bool selected{ e.selected_entities.contains(entity.id) };
					ImGui::PushID(entity.id);
					if (ImGui::Selectable(entity.prefab.c_str(), selected)) {
						if (!ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) {
							e.selected_entities.clear();
						}
						if (ImGui::GetIO().KeyCtrl && selected) {
							e.selected_entities.erase(entity.id);
						} else {
							e.selected_entities.insert(entity.id);
							e.primary_entity_id = entity.id;
						}
					}
					ImGui::PopID();
				}
			} else if (layer.kind == LayerKind::Tile) {
				ImGui::TextDisabled("Tiles are edited in the viewport/Paint Palette, not listed individually.");
			} else {
				ImGui::TextDisabled("Procedural noise fields and thresholds are edited in the Paint Palette.");
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
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
	ImGui::SameLine();
	if (ImGui::Button("+ Noise")) {
		const auto before{ CaptureScene(e) };
		if (e.tilemaps.empty()) {
			Tilemap map; map.id = e.next_tilemap_id++; e.tilemaps.push_back(map);
		}
		SceneLayer layer;
		layer.id = e.next_layer_id++;
		layer.name = "Noise Layer " + std::to_string(layer.id);
		layer.kind = LayerKind::Noise;
		layer.noise.target = NoiseTargetKind::Tile;
		layer.noise.tilemap_id = e.tilemaps.front().id;
		layer.noise.grid_size = e.tilemaps.front().cell_size;
		layer.noise.grid_offset = e.tilemaps.front().origin;
		SetNoiseBoundsToCurrentViewport(e, layer);
		NoiseField field;
		field.type = NoiseType::Perlin;
		field.name = "Perlin 1";
		NoiseThresholdRegion region;
		region.minimum = 0.0f;
		region.maximum = 1.0f;
		region.tile_id = -1;
		region.prefab_index = -1;
		region.enabled = true;
		field.thresholds.push_back(region);
		layer.noise.fields.push_back(std::move(field));
		e.layers.push_back(layer);
		e.active_layer_id = layer.id;
		PushHistory(e, "Add Noise Layer", before);
	}
	ItemTooltip("Add a procedural noise layer. Its Paint Palette can contain Perlin, Simplex, and Value fields with independent thresholds and finite boundaries.");

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
static void DrawSourceMode(EditorState& e) {
	const char* modes[]{ "Single", "Weighted Set" };
	int mode{ static_cast<int>(e.brush.source_mode) };
	ImGui::SetNextItemWidth(130.0f);
	if (ImGui::Combo("Source Mode", &mode, modes, 2)) {
		e.brush.source_mode = static_cast<BrushSourceMode>(mode);
	}
	ItemTooltip(
		"Single paints one exact source selected from the browser below.\n"
		"Weighted Set uses a named reusable custom brush whose members can come from any group/palette."
	);
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
	DrawSourceMode(e);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##prefab_search", "Search prefab name or group...", &search);
	ItemTooltip("Filter prefabs by either prefab name or group name. Groups are collapsible below; ungrouped prefabs share the Ungrouped node.");

	if (e.brush.source_mode == BrushSourceMode::Single) {
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


static void DrawTileSourcePalette(EditorState& e) {
	static std::string search;
	DrawSourceMode(e);
	ImGui::SameLine();
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

	if (e.brush.source_mode == BrushSourceMode::Single) {
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


static void DrawPaintPalette(EditorState& e) {
	ImGui::Begin("Paint Palette");
	SceneLayer* layer{ FindLayer(e, e.active_layer_id) };
	if (!layer) {
		ImGui::TextDisabled("Select a layer to choose paint sources.");
		ImGui::End();
		return;
	}

	if (layer->kind == LayerKind::Noise) {
		const char* convert_label{ layer->noise.target == NoiseTargetKind::Tile
			? "Convert to Tile Layer"
			: "Convert to Entity Layer" };
		const float button_width{ ImGui::CalcTextSize(convert_label).x + ImGui::GetStyle().FramePadding.x * 2.0f };
		const float right_x{ ImGui::GetWindowContentRegionMax().x };
		ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), right_x - button_width));
		ImGui::BeginDisabled(!layer->noise.bounded || layer->locked);
		const bool convert{ ImGui::Button(convert_label) };
		ImGui::EndDisabled();
		if (layer->locked) {
			ItemTooltip("Unlock this noise layer before converting it. Conversion bakes the current procedural result into editable scene content.");
		} else if (!layer->noise.bounded) {
			ItemTooltip("Only bounded noise can be converted because an unbounded procedural field has no finite amount of content to bake.");
		} else {
			ItemTooltip(layer->noise.target == NoiseTargetKind::Tile
				? "Bake the bounded procedural result into this same layer as ordinary editable tiles. Undo restores the procedural noise layer."
				: "Bake the bounded procedural result into this same layer as ordinary editable prefab entities. Undo restores the procedural noise layer.");
		}
		if (convert) {
			const int id{ layer->id };
			ConvertBoundedNoiseLayer(e, id);
			layer = FindLayer(e, id);
			if (!layer) {
				ImGui::End();
				return;
			}
		}
	}

	if (layer->kind == LayerKind::Tile) {
		ImGui::TextDisabled("Tile layer: choose tiles, named palettes, stamps, or a weighted set.");
		DrawTileSourcePalette(e);
	} else if (layer->kind == LayerKind::Entity) {
		ImGui::TextDisabled("Entity layer: choose prefab sources or configure a weighted prefab set.");
		DrawPrefabSourcePalette(e);
	} else {
		ImGui::BeginDisabled(layer->locked);
		DrawNoiseLayerPalette(e, *layer);
		ImGui::EndDisabled();
	}
	ImGui::End();
}


static void DrawInspector(EditorState& e) {
	ImGui::Begin("Inspector");
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
		ImGui::BulletText("Line/Area: Escape cancels the active drag");
		ImGui::BulletText("Eyedropper: hold left mouse and move to continuously pick");
		ImGui::BulletText("Ctrl-click palette tile: add/remove stamp tile");
		ImGui::BulletText("S/M/P/B/L/A/F/E/K: Select/Move/Pencil/Brush/Line/Area/Fill/Erase/Pick");
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

		I2 move_direction{};
		if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) --move_direction.x;
		if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) ++move_direction.x;
		if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) --move_direction.y;
		if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) ++move_direction.y;
		if (move_direction != I2{}) {
			MoveSelectionByKeyboard(e, move_direction, io.KeyCtrl, io.KeyShift);
		}
	}
	if (!io.WantTextInput && e.canvas_hovered && !ActiveLayerIsNoise(e)) {
		if (ImGui::IsKeyPressed(ImGuiKey_S, false)) e.tool = Tool::Select;
		if (ImGui::IsKeyPressed(ImGuiKey_M, false)) e.tool = Tool::Move;
		if (ImGui::IsKeyPressed(ImGuiKey_P, false)) e.tool = Tool::Pencil;
		if (ImGui::IsKeyPressed(ImGuiKey_B, false)) e.tool = Tool::Brush;
		if (ImGui::IsKeyPressed(ImGuiKey_L, false)) e.tool = Tool::Line;
		if (ImGui::IsKeyPressed(ImGuiKey_A, false)) e.tool = Tool::Area;
		if (ImGui::IsKeyPressed(ImGuiKey_F, false)) e.tool = Tool::Fill;
		if (ImGui::IsKeyPressed(ImGuiKey_E, false)) e.tool = Tool::Erase;
		if (ImGui::IsKeyPressed(ImGuiKey_K, false)) e.tool = Tool::Eyedropper;
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
