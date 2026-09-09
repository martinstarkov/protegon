#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
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

enum class Tool {
	Select,
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
	Modify,
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
		case Tool::Select: return "Select";
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
		case Tool::Select:
			return "Select (Q)\nClick/marquee entities or tiles, or switch to the raster selection brush.";
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
			return "Eyedropper (I)\nHold left mouse and move over tiles/entities to continuously pick the source.";
	}
	return "Tool";
}

static const char* LayerKindName(LayerKind kind) {
	return kind == LayerKind::Entity ? "Entity" : "Tile";
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
	std::string name{ "Default" };
	std::vector<PaletteEntry> entries;
};

struct TileCell {
	int tile_id{ -1 };
	bool terrain{};
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

struct SceneLayer {
	int id{};
	std::string name;
	LayerKind kind{ LayerKind::Entity };
	LayerPurpose purpose{ LayerPurpose::Visual };
	bool visible{ true };
	bool locked{};
	bool selectable{ true };
	TileLayerData tile;
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
	float weight{ 1.0f };
	F2 size{ 32.0f, 32.0f };
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

	float modify_rotation_jitter{ 15.0f };
	float modify_scale_min{ 0.9f };
	float modify_scale_max{ 1.1f };
	float modify_position_jitter{ 4.0f };
};

struct GridSettings {
	bool visible{ true };
	bool snap{ true };
	F2 size{ 32.0f, 32.0f };
	F2 offset{};
	int major_every{ 8 };
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
	int active_layer_id{};
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

struct EditorState {
	std::vector<TextureAsset> textures;
	std::vector<TileDefinition> tiles;
	std::vector<TilePalette> palettes;
	std::vector<PrefabBrushEntry> prefabs;
	std::vector<SceneLayer> layers;
	std::vector<Tilemap> tilemaps;
	std::vector<Entity> entities;

	Tool tool{ Tool::Select };
	BrushSettings brush;
	GridSettings grid;
	RuntimeState runtime;
	ImportSettings importer;
	std::string import_status;
	StrokeState stroke;

	int active_layer_id{};
	int active_palette_index{};
	int active_tile_id{ -1 };
	int replace_source_tile_id{ -1 };
	int active_prefab_index{};
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
};

static EditorState* g_editor{};

static SceneSnapshot CaptureScene(const EditorState& e) {
	return {
		.layers = e.layers,
		.entities = e.entities,
		.tilemaps = e.tilemaps,
		.active_layer_id = e.active_layer_id,
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
	e.active_layer_id = s.active_layer_id;
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
}

static void EraseTile(SceneLayer& layer, const Tilemap& map, I2 cell) {
	auto* dst{ WriteTileCell(layer, map, cell) };
	dst->tile_id = -1;
	dst->terrain = false;
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

static RectF TileAnchorRect(const EditorState& e, const Tilemap& map, I2 cell, int tile_id) {
	const F2 origin{ CellToWorld(map, cell) };
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
		const RectF bounds{ TileAnchorRect(e, map, cell, tile.tile_id) };
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

	if (const auto* layer = FindLayer(e, e.active_layer_id); layer && layer->kind == LayerKind::Tile) {
		if (const auto* map = FindTilemap(e, layer->tile.tilemap_id)) {
			return std::max(1.0f, std::min(map->cell_size.x, map->cell_size.y));
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
	if (const auto* layer = FindLayer(e, e.active_layer_id); layer && layer->kind == LayerKind::Tile) {
		if (const auto* map = FindTilemap(e, layer->tile.tilemap_id)) {
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

static std::vector<I2> RasterLineCells(
	const EditorState& e,
	F2 a,
	F2 b
) {
	const RasterGrid grid{ ActiveRasterGrid(e) };
	I2 start{ WorldToRasterCell(grid, a) };
	const I2 end{ WorldToRasterCell(grid, b) };

	std::vector<I2> cells;
	const int dx{ std::abs(end.x - start.x) };
	const int sx{ start.x < end.x ? 1 : -1 };
	const int dy{ -std::abs(end.y - start.y) };
	const int sy{ start.y < end.y ? 1 : -1 };
	int error{ dx + dy };

	for (;;) {
		cells.push_back(start);
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

	return cells;
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

	std::vector<I2> cells;
	for (int y{ min_y }; y <= max_y; ++y) {
		for (int x{ min_x }; x <= max_x; ++x) {
			const bool edge{
				x == min_x || x == max_x ||
				y == min_y || y == max_y
			};
			const bool corner{
				(x == min_x || x == max_x) &&
				(y == min_y || y == max_y)
			};

			if (e.brush.area_mode == AreaMode::Outline && !edge) {
				continue;
			}
			if (e.brush.area_mode == AreaMode::Corners && !corner) {
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
		if (RectsOverlap(candidate_rect, TileAnchorRect(e, map, cell, existing.tile_id))) {
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

static bool IsExcluded(const Tilemap& map, I2 cell) {
	return map.exclusion_mask.contains(cell);
}

static float RandomRange(EditorState& e, float a, float b) {
	std::uniform_real_distribution<float> d{ a, b };
	return d(e.rng);
}

static int WeightedTile(EditorState& e) {
	if (e.palettes.empty()) {
		return e.active_tile_id;
	}
	const auto& palette{ e.palettes[static_cast<std::size_t>(std::clamp(e.active_palette_index, 0, static_cast<int>(e.palettes.size()) - 1))] };
	if (palette.entries.empty()) {
		return e.active_tile_id;
	}
	float sum{};
	for (const auto& item : palette.entries) {
		sum += std::max(0.0f, item.weight);
	}
	if (sum <= 0.0f) {
		return palette.entries.front().tile_id;
	}
	float r{ RandomRange(e, 0.0f, sum) };
	for (const auto& item : palette.entries) {
		r -= std::max(0.0f, item.weight);
		if (r <= 0.0f) {
			return item.tile_id;
		}
	}
	return palette.entries.back().tile_id;
}

static int ChooseTile(EditorState& e, F2 world) {
	if (e.brush.source_mode == BrushSourceMode::Single || e.palettes.empty()) {
		return e.active_tile_id;
	}
	const auto& palette{ e.palettes[static_cast<std::size_t>(std::clamp(e.active_palette_index, 0, static_cast<int>(e.palettes.size()) - 1))] };
	if (palette.entries.empty()) {
		return e.active_tile_id;
	}
	if (e.brush.distribution == BrushDistribution::Noise) {
		const float n{ Perlin2(world.x * e.brush.noise_scale, world.y * e.brush.noise_scale, static_cast<std::uint32_t>(e.brush.noise_seed)) };
		const int index{ std::clamp(static_cast<int>(n * static_cast<float>(palette.entries.size())), 0, static_cast<int>(palette.entries.size()) - 1) };
		return palette.entries[static_cast<std::size_t>(index)].tile_id;
	}
	return WeightedTile(e);
}

static int WeightedPrefab(EditorState& e) {
	if (e.prefabs.empty()) {
		return -1;
	}
	if (e.brush.source_mode == BrushSourceMode::Single) {
		return std::clamp(e.active_prefab_index, 0, static_cast<int>(e.prefabs.size()) - 1);
	}
	float sum{};
	for (const auto& item : e.prefabs) {
		sum += std::max(0.0f, item.weight);
	}
	if (sum <= 0.0f) {
		return 0;
	}
	float r{ RandomRange(e, 0.0f, sum) };
	for (int i{}; i < static_cast<int>(e.prefabs.size()); ++i) {
		r -= std::max(0.0f, e.prefabs[static_cast<std::size_t>(i)].weight);
		if (r <= 0.0f) {
			return i;
		}
	}
	return static_cast<int>(e.prefabs.size()) - 1;
}

static int ChoosePrefab(EditorState& e, F2 world) {
	if (e.prefabs.empty()) {
		return -1;
	}
	if (e.brush.source_mode == BrushSourceMode::Single) {
		return std::clamp(e.active_prefab_index, 0, static_cast<int>(e.prefabs.size()) - 1);
	}
	if (e.brush.distribution == BrushDistribution::Noise) {
		const float n{ Perlin2(world.x * e.brush.noise_scale, world.y * e.brush.noise_scale, static_cast<std::uint32_t>(e.brush.noise_seed)) };
		return std::clamp(static_cast<int>(n * static_cast<float>(e.prefabs.size())), 0, static_cast<int>(e.prefabs.size()) - 1);
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

	// Pencil is a one-cell/one-position tool. Brush behavior below is rasterized.
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
			const auto saved_operation{ e.brush.operation };
			e.brush.operation = BrushOperation::Paint;
			PlaceTile(e, *layer, *map, cell);
			e.brush.operation = saved_operation;
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

			// Modify is entity-only. The UI does not expose it for Tile layers.
			if (e.brush.operation == BrushOperation::Modify) {
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

	if (e.brush.operation == BrushOperation::Modify) {
		for (auto& entity : e.entities) {
			if (entity.layer_id != layer->id ||
				e.stroke.touched_entities.contains(entity.id) ||
				!RectIntersectsRasterCells(e, world, EntityBounds(entity))) {
				continue;
			}
			e.stroke.touched_entities.insert(entity.id);
			entity.rotation += RandomRange(
				e,
				-e.brush.modify_rotation_jitter,
				e.brush.modify_rotation_jitter
			);
			entity.scale *= RandomRange(
				e,
				e.brush.modify_scale_min,
				e.brush.modify_scale_max
			);
			entity.position.x += RandomRange(
				e,
				-e.brush.modify_position_jitter,
				e.brush.modify_position_jitter
			);
			entity.position.y += RandomRange(
				e,
				-e.brush.modify_position_jitter,
				e.brush.modify_position_jitter
			);
			e.stroke.changed = true;
		}
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
				if (RectsOverlap(brush_rect, TileAnchorRect(e, *map, anchor, tile.tile_id))) {
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
						TileAnchorRect(e, *map, cell, tile.tile_id)
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
					TileAnchorRect(e, *map, anchor, tile.tile_id)
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
		const auto saved_operation{ e.brush.operation };
		e.brush.operation = BrushOperation::Paint;
		for (const I2 cell : cells) {
			PlaceTile(e, *layer, *map, cell);
		}
		e.brush.operation = saved_operation;
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

		const auto saved_operation{ e.brush.operation };
		e.brush.operation = BrushOperation::Paint;
		for (const I2 cell : cells) {
			if (
				e.brush.area_mode == AreaMode::RandomFill &&
				!PassesAreaRandomFill(e, cell)
			) {
				continue;
			}
			PlaceTile(e, *layer, *map, cell);
		}
		e.brush.operation = saved_operation;
		return;
	}

	for (const I2 cell : cells) {
		if (
			e.brush.area_mode == AreaMode::RandomFill &&
			!PassesAreaRandomFill(e, cell)
		) {
			continue;
		}
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

static F2 VisibleWorldMin(const EditorState& e) {
	return ScreenToWorld(e, e.canvas_screen_min);
}

static F2 VisibleWorldMax(const EditorState& e) {
	return ScreenToWorld(e, e.canvas_screen_max);
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
		e.palettes.push_back({ "Default", {} });
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
	e.palettes.push_back({ name, {} });
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
	e.prefabs = {
		{ "Tree", 5.0f, { 32.0f, 48.0f } },
		{ "Bush", 3.0f, { 32.0f, 24.0f } },
		{ "Rock", 2.0f, { 24.0f, 24.0f } },
		{ "Crate", 1.0f, { 32.0f, 32.0f } },
		{ "Lamp", 1.0f, { 16.0f, 40.0f } },
	};

	e.palettes.push_back({ "Basic Tiles", {} });
	e.palettes.push_back({ "Terrain", {} });
	auto& basic{ e.palettes.front() };
	const int grass{ AddGeneratedTile(e, basic, "Grass", 16, 16, { 72, 132, 70, 255 }, { 99, 160, 80, 255 }, 1) };
	AddGeneratedTile(e, basic, "Dirt", 16, 16, { 126, 88, 58, 255 }, { 151, 108, 72, 255 }, 0);
	AddGeneratedTile(e, basic, "Water", 16, 16, { 55, 104, 171, 255 }, { 78, 139, 205, 255 }, 2);
	AddGeneratedTile(e, basic, "Stone", 16, 16, { 105, 110, 116, 255 }, { 135, 141, 146, 255 }, 3);
	AddGeneratedTile(e, basic, "Sand", 16, 16, { 202, 177, 112, 255 }, { 228, 204, 139, 255 }, 4);
	AddGeneratedTile(e, basic, "Large Stone 24x24", 24, 24, { 115, 118, 124, 255 }, { 151, 154, 160, 255 }, 3);
	AddGeneratedTile(e, basic, "Brick 32x32", 32, 32, { 145, 68, 55, 255 }, { 91, 49, 45, 255 }, 5);
	e.active_tile_id = grass;
	e.active_palette_index = 0;

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

	// Start on a tile layer so the generated palette can be tested immediately.
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
		case Tool::Select:
			dl->AddTriangleFilled(P(2, 1), P(2, 14), P(7, 10), color);
			dl->AddLine(P(6, 9), P(11, 14), color, 2.0f);
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

static void DrawViewportToolbar(EditorState& e) {
	ToolButton(e, Tool::Select, "##tool_select"); ImGui::SameLine();
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

	const SceneLayer* toolbar_layer{ FindLayer(e, e.active_layer_id) };
	const bool tile_layer_active{
		toolbar_layer && toolbar_layer->kind == LayerKind::Tile
	};

	ImGui::Checkbox("Grid", &e.grid.visible);
	ItemTooltip(tile_layer_active
		? "Show or hide the active tilemap grid."
		: "Show or hide the editor placement grid.");

	// Tile layers inherently use their tilemap cell size/origin. Snapping and
	// arbitrary editor-grid controls are therefore hidden instead of disabled.
	if (!tile_layer_active) {
		ImGui::SameLine();
		ImGui::Checkbox("Snap", &e.grid.snap);
		ItemTooltip("Snap entity placement to the editor grid.");

		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.0f);
		float grid[2]{ e.grid.size.x, e.grid.size.y };
		if (ImGui::DragFloat2(
				"##grid_size",
				grid,
				1.0f,
				1.0f,
				512.0f,
				"%.0f"
			)) {
			e.grid.size = {
				std::max(1.0f, grid[0]),
				std::max(1.0f, grid[1]),
			};
		}
		ItemTooltip("Editor grid cell width and height.");

		ImGui::SameLine();
		if (ImGui::Button("Grid...")) {
			ImGui::OpenPopup("Grid Settings");
		}
		ItemTooltip("Open advanced grid offset and major-line settings.");

		if (ImGui::BeginPopup("Grid Settings")) {
			float offset[2]{ e.grid.offset.x, e.grid.offset.y };
			if (ImGui::DragFloat2("Offset", offset, 1.0f)) {
				e.grid.offset = { offset[0], offset[1] };
			}
			ItemTooltip("World-space origin of the editor grid.");

			ImGui::DragInt(
				"Major Line Every",
				&e.grid.major_every,
				1.0f,
				1,
				64
			);
			ItemTooltip("Draw a stronger grid line every N cells.");
			ImGui::EndPopup();
		}
	}

	// This is anchored to the right edge, so it stays put when tile-layer
	// controls disappear from the middle of the toolbar.
	const float runtime_width{ 430.0f };
	const float right{ ImGui::GetWindowContentRegionMax().x };
	const float runtime_x{ right - runtime_width };
	if (ImGui::GetCursorPosX() < runtime_x) {
		ImGui::SameLine(runtime_x);
	} else {
		ImGui::SameLine();
	}
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

static std::string PaintSourcePreview(const EditorState& e, bool tile_layer) {
	if (tile_layer) {
		if (e.brush.source_mode == BrushSourceMode::WeightedSet) {
			const std::string palette_name{ e.palettes.empty()
				? "<empty>"
				: e.palettes[static_cast<std::size_t>(std::clamp(e.active_palette_index, 0, static_cast<int>(e.palettes.size()) - 1))].name };
			return "TILE SET: " + palette_name;
		}
		const auto* tile{ FindTile(e, e.active_tile_id) };
		return std::string{ "TILE: " } + (tile ? tile->name : "<none>");
	}

	if (e.brush.source_mode == BrushSourceMode::WeightedSet) {
		return "PREFAB SET: weighted";
	}
	if (e.prefabs.empty()) {
		return "PREFAB: <none>";
	}
	const int index{ std::clamp(e.active_prefab_index, 0, static_cast<int>(e.prefabs.size()) - 1) };
	return "PREFAB: " + e.prefabs[static_cast<std::size_t>(index)].name;
}

static void DrawPaintSourcePopup(EditorState& e, bool tile_layer) {
	if (!ImGui::BeginPopup("Paint Source Settings")) {
		return;
	}

	const char* source_modes[]{ "Single", "Weighted Set" };
	int source_mode{ static_cast<int>(e.brush.source_mode) };
	ImGui::SetNextItemWidth(150.0f);
	if (ImGui::Combo("Source Mode", &source_mode, source_modes, 2)) {
		e.brush.source_mode = static_cast<BrushSourceMode>(source_mode);
	}
	ItemTooltip(
		"Single always paints the chosen source.\n"
		"Weighted Set randomly chooses from the listed sources using their weights."
	);

	ImGui::Separator();

	if (tile_layer) {
		if (!e.palettes.empty()) {
			e.active_palette_index = std::clamp(
				e.active_palette_index,
				0,
				static_cast<int>(e.palettes.size()) - 1
			);

			ImGui::SetNextItemWidth(200.0f);
			if (ImGui::BeginCombo(
					"Palette",
					e.palettes[static_cast<std::size_t>(e.active_palette_index)]
						.name.c_str()
				)) {
				for (int i{}; i < static_cast<int>(e.palettes.size()); ++i) {
					if (ImGui::Selectable(
							e.palettes[static_cast<std::size_t>(i)].name.c_str(),
							i == e.active_palette_index
						)) {
						e.active_palette_index = i;
					}
				}
				ImGui::EndCombo();
			}
			ItemTooltip("Choose the named tile palette used by this paint source.");

			auto& palette{
				e.palettes[static_cast<std::size_t>(e.active_palette_index)]
			};

			for (auto& entry : palette.entries) {
				const auto* tile{ FindTile(e, entry.tile_id) };
				if (!tile) {
					continue;
				}

				ImGui::PushID(entry.tile_id);
				if (ImGui::Selectable(
						tile->name.c_str(),
						e.active_tile_id == entry.tile_id,
						ImGuiSelectableFlags_DontClosePopups
					)) {
					e.active_tile_id = entry.tile_id;
					e.stamp_tiles.clear();
				}
				ItemTooltip("Make this tile the primary paint source.");

				if (e.brush.source_mode == BrushSourceMode::WeightedSet) {
					ImGui::SameLine(180.0f);
					ImGui::SetNextItemWidth(72.0f);
					ImGui::DragFloat(
						"##weight",
						&entry.weight,
						0.1f,
						0.0f,
						100.0f,
						"%.1f"
					);
					ItemTooltip(
						"Relative probability of choosing this tile when painting a weighted set."
					);
				}
				ImGui::PopID();
			}
		}

		if (e.tool == Tool::Brush && e.brush.operation == BrushOperation::Replace &&
			e.active_tile_id >= 0) {
			ImGui::SeparatorText("Replace Source");
			const auto* current{ FindTile(e, e.replace_source_tile_id) };
			const char* preview{
				current ? current->name.c_str() : "<pick source>"
			};

			ImGui::SetNextItemWidth(200.0f);
			if (ImGui::BeginCombo("Replace", preview)) {
				if (!e.palettes.empty()) {
					for (const auto& entry :
						 e.palettes[static_cast<std::size_t>(
							 e.active_palette_index
						 )].entries) {
						if (const auto* tile = FindTile(e, entry.tile_id)) {
							if (ImGui::Selectable(
									tile->name.c_str(),
									entry.tile_id == e.replace_source_tile_id
								)) {
								e.replace_source_tile_id = entry.tile_id;
							}
						}
					}
				}
				ImGui::EndCombo();
			}
			ItemTooltip(
				"Only tiles matching this source are replaced by the active paint source."
			);
		}
	} else {
		for (int i{}; i < static_cast<int>(e.prefabs.size()); ++i) {
			auto& item{ e.prefabs[static_cast<std::size_t>(i)] };
			ImGui::PushID(i);

			if (ImGui::Selectable(
					item.name.c_str(),
					e.active_prefab_index == i,
					ImGuiSelectableFlags_DontClosePopups
				)) {
				e.active_prefab_index = i;
			}
			ItemTooltip("Make this prefab the primary entity paint source.");

			if (e.brush.source_mode == BrushSourceMode::WeightedSet) {
				ImGui::SameLine(160.0f);
				ImGui::SetNextItemWidth(72.0f);
				ImGui::DragFloat(
					"##weight",
					&item.weight,
					0.1f,
					0.0f,
					100.0f,
					"%.1f"
				);
				ItemTooltip(
					"Relative probability of choosing this prefab when painting a weighted set."
				);
			}
			ImGui::PopID();
		}

		if (ImGui::Button("Add Prefab Source")) {
			e.prefabs.push_back({
				"Prefab " + std::to_string(e.prefabs.size() + 1),
				1.0f,
				{ 32.0f, 32.0f },
			});
		}
		ItemTooltip("Add another demo prefab source to the weighted/source list.");

		if (e.tool == Tool::Brush && e.brush.operation == BrushOperation::Replace &&
			!e.prefabs.empty()) {
			ImGui::SeparatorText("Replace Source");
			e.replace_source_prefab_index = std::clamp(
				e.replace_source_prefab_index,
				0,
				static_cast<int>(e.prefabs.size()) - 1
			);

			ImGui::SetNextItemWidth(200.0f);
			if (ImGui::BeginCombo(
					"Replace",
					e.prefabs[static_cast<std::size_t>(
						e.replace_source_prefab_index
					)].name.c_str()
				)) {
				for (int i{}; i < static_cast<int>(e.prefabs.size()); ++i) {
					if (ImGui::Selectable(
							e.prefabs[static_cast<std::size_t>(i)].name.c_str(),
							i == e.replace_source_prefab_index
						)) {
						e.replace_source_prefab_index = i;
					}
				}
				ImGui::EndCombo();
			}
			ItemTooltip(
				"Only entities using this prefab are replaced by the active prefab source."
			);
		}
	}

	ImGui::EndPopup();
}

static void DrawBrushSettings(EditorState& e) {
	const SceneLayer* active_layer{ FindLayer(e, e.active_layer_id) };
	const bool tile_layer{
		active_layer && active_layer->kind == LayerKind::Tile
	};

	auto draw_brush_size = [&]() {
		float diameter{
			SnapBrushDiameter(e, e.brush.radius * 2.0f)
		};

		if (ImGui::SmallButton("-##brush_size")) {
			AdjustBrushDiameter(e, -1);
			diameter = e.brush.radius * 2.0f;
		}
		ItemTooltip(
			e.brush.size_snap == BrushSizeSnapMode::Free
				? "Decrease brush diameter to the previous common brush size."
				: "Decrease brush diameter by one selected grid/tile-size multiple."
		);

		ImGui::SameLine();
		ImGui::SetNextItemWidth(76.0f);
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
			e.brush.radius =
				SnapBrushDiameter(e, diameter) * 0.5f;
		}
		ItemTooltip(
			"Brush diameter. Drag slowly to resize.\n"
			"When Grid or Tile snapping is selected, the value changes in exact multiples."
		);

		ImGui::SameLine();
		if (ImGui::SmallButton("+##brush_size")) {
			AdjustBrushDiameter(e, 1);
		}
		ItemTooltip(
			e.brush.size_snap == BrushSizeSnapMode::Free
				? "Increase brush diameter to the next common brush size."
				: "Increase brush diameter by one selected grid/tile-size multiple."
		);

		ImGui::SameLine();
		const char* snap_names[]{ "Free", "Grid", "Tile" };
		int snap_mode{ static_cast<int>(e.brush.size_snap) };
		ImGui::SetNextItemWidth(72.0f);
		if (ImGui::Combo(
				"##size_snap",
				&snap_mode,
				snap_names,
				3
			)) {
			e.brush.size_snap =
				static_cast<BrushSizeSnapMode>(snap_mode);
			e.brush.radius =
				SnapBrushDiameter(e, e.brush.radius * 2.0f) * 0.5f;
		}
		ItemTooltip(
			"Free: arbitrary/common brush sizes.\n"
			"Grid: diameter is a multiple of the active grid cell size.\n"
			"Tile: diameter is a multiple of the active source tile size."
		);

		ImGui::SameLine();
		const char* shape_names[]{ "Circle", "Square" };
		int shape{ static_cast<int>(e.brush.shape) };
		ImGui::SetNextItemWidth(72.0f);
		if (ImGui::Combo(
				"##brush_shape",
				&shape,
				shape_names,
				2
			)) {
			e.brush.shape = static_cast<BrushShape>(shape);
		}
		ItemTooltip(
			"Raster brush footprint shape. Both shapes are resolved to whole grid cells."
		);
	};

	if (e.tool == Tool::Select) {
		const char* modes[]{ "Click + Marquee", "Selection Brush" };
		int mode{ static_cast<int>(e.brush.select_mode) };
		ImGui::SetNextItemWidth(145.0f);
		if (ImGui::Combo("Select", &mode, modes, 2)) {
			e.brush.select_mode = static_cast<SelectMode>(mode);
		}
		ItemTooltip(
			"Click + Marquee selects individual items or rectangular regions.\n"
			"Selection Brush paints selection over rasterized grid cells; hold Ctrl to remove."
		);

		if (e.brush.select_mode == SelectMode::Brush) {
			ImGui::SameLine();
			draw_brush_size();
		}

		ImGui::SameLine();
		ImGui::TextDisabled("Ctrl+D deselects all | Delete removes selection");
		return;
	}

	if (!active_layer) {
		ImGui::TextDisabled("Select a layer in the Layers window to paint.");
		return;
	}

	const std::string source_preview{
		PaintSourcePreview(e, tile_layer)
	};
	if (ImGui::Button(source_preview.c_str())) {
		ImGui::OpenPopup("Paint Source Settings");
	}
	ItemTooltip(
		tile_layer
			? "Current tile paint source. Click to choose a tile/palette or configure weighted sources."
			: "Current prefab paint source. Click to choose a prefab or configure weighted sources."
	);
	DrawPaintSourcePopup(e, tile_layer);

	if (!tile_layer) {
		ImGui::SameLine();
		int origin{ static_cast<int>(e.brush.entity_origin) };
		const char* origins[]{
			"Top Left", "Top", "Top Right",
			"Left", "Center", "Right",
			"Bottom Left", "Bottom", "Bottom Right"
		};
		ImGui::SetNextItemWidth(105.0f);
		if (ImGui::Combo(
				"##entity_origin",
				&origin,
				origins,
				9
			)) {
			e.brush.entity_origin =
				static_cast<EntityOrigin>(origin);
		}
		ItemTooltip(
			"Entity origin placed on the grid anchor.\n"
			"Top Left keeps a same-size entity fully inside its grid cell."
		);
	}

	if (e.tool == Tool::Eyedropper) {
		ImGui::SameLine();
		ImGui::TextDisabled(
			"Hold left mouse and move across tiles/entities to continuously pick sources."
		);
		return;
	}

	if (e.tool == Tool::Fill) {
		ImGui::SameLine();
		ImGui::TextDisabled(
			tile_layer
				? "Fill replaces a connected matching region. Empty fills are bounded by the visible viewport."
				: "Fill is only available on Tile layers."
		);
		return;
	}

	// Brush operations are only shown for tools that actually support them.
	if (e.tool == Tool::Brush) {
		ImGui::SameLine();
		if (tile_layer) {
			const char* operations[]{ "Paint", "Replace", "Exclusion Mask" };
			int choice{
				e.brush.operation == BrushOperation::Replace ? 1 :
				e.brush.operation == BrushOperation::ExclusionMask ? 2 : 0
			};
			ImGui::SetNextItemWidth(112.0f);
			if (ImGui::Combo("##operation", &choice, operations, 3)) {
				e.brush.operation =
					choice == 1 ? BrushOperation::Replace :
					choice == 2 ? BrushOperation::ExclusionMask :
					BrushOperation::Paint;
			}
			ItemTooltip(
				"Paint: place the active tile.\n"
				"Replace: only replace the configured source tile.\n"
				"Exclusion Mask: paint cells where normal painting is blocked."
			);
		} else {
			const char* operations[]{ "Paint", "Replace", "Modify" };
			int choice{
				e.brush.operation == BrushOperation::Replace ? 1 :
				e.brush.operation == BrushOperation::Modify ? 2 : 0
			};
			ImGui::SetNextItemWidth(100.0f);
			if (ImGui::Combo("##operation", &choice, operations, 3)) {
				e.brush.operation =
					choice == 1 ? BrushOperation::Replace :
					choice == 2 ? BrushOperation::Modify :
					BrushOperation::Paint;
			}
			ItemTooltip(
				"Paint: create prefab entities.\n"
				"Replace: replace matching prefab entities under the brush.\n"
				"Modify: jitter existing entities under the brush without creating new ones."
			);
		}
	} else if (e.tool == Tool::Erase && tile_layer) {
		ImGui::SameLine();
		const char* erase_modes[]{ "Erase Tiles", "Erase Mask" };
		int choice{
			e.brush.operation == BrushOperation::ExclusionMask ? 1 : 0
		};
		ImGui::SetNextItemWidth(104.0f);
		if (ImGui::Combo("##erase_mode", &choice, erase_modes, 2)) {
			e.brush.operation =
				choice == 1
					? BrushOperation::ExclusionMask
					: BrushOperation::Paint;
		}
		ItemTooltip(
			"Erase Tiles removes tile anchors touched by the raster footprint.\n"
			"Erase Mask removes painted exclusion-mask cells instead."
		);
	}

	if (e.tool == Tool::Brush || e.tool == Tool::Erase) {
		ImGui::SameLine();
		draw_brush_size();
	}

	if (e.tool == Tool::Brush || e.tool == Tool::Line) {
		ImGui::SameLine();
		ImGui::SetNextItemWidth(72.0f);
		ImGui::DragFloat(
			"Spacing",
			&e.brush.spacing,
			0.5f,
			1.0f,
			512.0f,
			"%.0f"
		);
		ItemTooltip(
			"Sampling/placement spacing used by free placement and line painting."
		);
	}

	if (tile_layer &&
		(e.tool == Tool::Pencil ||
		 e.tool == Tool::Brush ||
		 e.tool == Tool::Line ||
		 e.tool == Tool::Area)) {
		ImGui::SameLine();
		const char* tile_modes[]{ "Grid Paint", "Tile Paint" };
		int tile_mode{ static_cast<int>(e.brush.tile_paint_mode) };
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::Combo(
				"##tile_placement",
				&tile_mode,
				tile_modes,
				2
			)) {
			e.brush.tile_paint_mode =
				static_cast<TilePaintMode>(tile_mode);
		}
		ItemTooltip(
			"Grid Paint: each raster grid cell can be an anchor, so large native tiles may overlap.\n"
			"Tile Paint: anchor spacing expands to the tile's whole-cell footprint to avoid overlap."
		);
	}

	if (e.tool == Tool::Brush) {
		ImGui::SameLine();
		const char* placements[]{ "Continuous", "Scatter", "Density" };
		int placement{ static_cast<int>(e.brush.placement) };
		ImGui::SetNextItemWidth(96.0f);
		if (ImGui::Combo(
				"##placement",
				&placement,
				placements,
				3
			)) {
			e.brush.placement =
				static_cast<BrushPlacementMode>(placement);
		}
		ItemTooltip(
			"Continuous affects every raster cell.\n"
			"Scatter randomly affects approximately Count cells.\n"
			"Density randomly affects the selected fraction of raster cells."
		);

		ImGui::SameLine();
		const char* distributions[]{ "Uniform", "Random", "Noise" };
		int distribution{ static_cast<int>(e.brush.distribution) };
		ImGui::SetNextItemWidth(82.0f);
		if (ImGui::Combo(
				"##distribution",
				&distribution,
				distributions,
				3
			)) {
			e.brush.distribution =
				static_cast<BrushDistribution>(distribution);
		}
		ItemTooltip(
			"Uniform uses the configured source normally.\n"
			"Random applies density as a random probability.\n"
			"Noise uses the noise field to choose/filter sources."
		);

		if (e.brush.placement == BrushPlacementMode::Scatter) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(62.0f);
			ImGui::DragInt(
				"Count",
				&e.brush.scatter_count,
				1.0f,
				1,
				100
			);
			ItemTooltip("Approximate number of raster cells affected by each brush footprint.");
		} else if (
			e.brush.placement == BrushPlacementMode::Density ||
			e.brush.distribution == BrushDistribution::Random
		) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(72.0f);
			ImGui::SliderFloat(
				"Density",
				&e.brush.density,
				0.01f,
				1.0f,
				"%.2f"
			);
			ItemTooltip("Probability/fraction of candidate cells that are affected.");
		}
	}

	if (e.tool == Tool::Area) {
		ImGui::SameLine();
		const char* areas[]{ "Fill", "Outline", "Corners", "Random Fill" };
		int area{ static_cast<int>(e.brush.area_mode) };
		ImGui::SetNextItemWidth(96.0f);
		if (ImGui::Combo("##area_mode", &area, areas, 4)) {
			e.brush.area_mode = static_cast<AreaMode>(area);
		}
		ItemTooltip(
			"Fill paints every raster cell in the rectangle.\n"
			"Outline paints only the perimeter.\n"
			"Corners paints four corner cells.\n"
			"Random Fill uses Density to sparsely fill the area."
		);
		if (e.brush.area_mode == AreaMode::RandomFill) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(72.0f);
			ImGui::SliderFloat(
				"Density##area",
				&e.brush.density,
				0.01f,
				1.0f,
				"%.2f"
			);
			ItemTooltip("Probability that each raster cell in Random Fill is painted.");
		}
	}

	if (e.brush.operation == BrushOperation::Modify &&
		!tile_layer &&
		e.tool == Tool::Brush) {
		ImGui::SameLine();
		if (ImGui::Button("Modify...")) {
			ImGui::OpenPopup("Modify Brush Settings");
		}
		ItemTooltip("Configure the random transform changes applied to existing entities.");
		if (ImGui::BeginPopup("Modify Brush Settings")) {
			ImGui::DragFloat(
				"Rotation Jitter",
				&e.brush.modify_rotation_jitter,
				0.25f,
				0.0f,
				360.0f,
				"%.1f deg"
			);
			ItemTooltip("Maximum random rotation added/subtracted from each affected entity.");

			ImGui::DragFloatRange2(
				"Scale Multiplier",
				&e.brush.modify_scale_min,
				&e.brush.modify_scale_max,
				0.01f,
				0.01f,
				4.0f,
				"%.2f",
				"%.2f"
			);
			ItemTooltip("Random scale multiplier range applied to each affected entity.");

			ImGui::DragFloat(
				"Position Jitter",
				&e.brush.modify_position_jitter,
				0.25f,
				0.0f,
				512.0f,
				"%.1f"
			);
			ItemTooltip("Maximum random X/Y world-space offset applied to each affected entity.");
			ImGui::EndPopup();
		}
	}

	// Less-common toggles live in one compact multi-choice combo.
	ImGui::SameLine();
	ImGui::SetNextItemWidth(94.0f);
	const bool options_open{
		ImGui::BeginCombo("##paint_options", "Options")
	};
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
			"Only affect candidate cells whose noise value passes the configured threshold."
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
			if (!enabled) {
				return;
			}
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
		if (!any) {
			tooltip += "\n- None";
		}
		ImGui::SetTooltip("%s", tooltip.c_str());
	}

	if (e.brush.noise_mask ||
		e.brush.distribution == BrushDistribution::Noise) {
		ImGui::SameLine();
		if (ImGui::Button("Noise...")) {
			ImGui::OpenPopup("Noise Settings");
		}
		ItemTooltip("Configure the procedural noise field used by Noise distribution/masking.");

		if (ImGui::BeginPopup("Noise Settings")) {
			ImGui::DragFloat(
				"Scale",
				&e.brush.noise_scale,
				0.001f,
				0.001f,
				1.0f,
				"%.3f"
			);
			ItemTooltip("Noise frequency. Smaller values produce larger coherent regions.");

			ImGui::SliderFloat(
				"Threshold",
				&e.brush.noise_threshold,
				0.0f,
				1.0f
			);
			ItemTooltip("Minimum noise value required when Noise Mask is enabled.");

			ImGui::DragInt("Seed", &e.brush.noise_seed);
			ItemTooltip("Seed controlling the deterministic noise pattern.");
			ImGui::EndPopup();
		}
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
	if (const SceneLayer* active = FindLayer(e, e.active_layer_id); active && active->kind == LayerKind::Tile) {
		if (const Tilemap* map = FindTilemap(e, active->tile.tilemap_id)) {
			grid_size = map->cell_size;
			grid_offset = map->origin;
		}
	}
	const float sx{ std::max(1.0f, grid_size.x) };
	const float sy{ std::max(1.0f, grid_size.y) };
	const int x0{ static_cast<int>(std::floor((std::min(w0.x, w1.x) - grid_offset.x) / sx)) - 1 };
	const int x1{ static_cast<int>(std::ceil((std::max(w0.x, w1.x) - grid_offset.x) / sx)) + 1 };
	const int y0{ static_cast<int>(std::floor((std::min(w0.y, w1.y) - grid_offset.y) / sy)) - 1 };
	const int y1{ static_cast<int>(std::ceil((std::max(w0.y, w1.y) - grid_offset.y) / sy)) + 1 };
	const ImU32 minor{ ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.07f)) };
	const ImU32 major{ ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.15f)) };
	for (int x{ x0 }; x <= x1; ++x) {
		const float wx{ grid_offset.x + static_cast<float>(x) * sx };
		const auto p0{ WorldToScreen(e, { wx, w0.y }) };
		const auto p1{ WorldToScreen(e, { wx, w1.y }) };
		dl->AddLine({ p0.x, p0.y }, { p1.x, p1.y }, x % std::max(1, e.grid.major_every) == 0 ? major : minor);
	}
	for (int y{ y0 }; y <= y1; ++y) {
		const float wy{ grid_offset.y + static_cast<float>(y) * sy };
		const auto p0{ WorldToScreen(e, { w0.x, wy }) };
		const auto p1{ WorldToScreen(e, { w1.x, wy }) };
		dl->AddLine({ p0.x, p0.y }, { p1.x, p1.y }, y % std::max(1, e.grid.major_every) == 0 ? major : minor);
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
		const F2 world{ CellToWorld(*map, anchor.cell) };
		const F2 draw_size{ TileWorldSize(e, *map, cell.tile_id) };
		const F2 p0{ WorldToScreen(e, world) };
		const F2 p1{ WorldToScreen(e, world + draw_size) };
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

static void DrawToolPreview(EditorState& e, ImDrawList* dl, F2 mouse_world) {
	const auto* layer{ FindLayer(e, e.active_layer_id) };
	const RasterGrid raster{ ActiveRasterGrid(e) };
	const BrushOperation preview_operation{
		(e.tool == Tool::Brush || e.tool == Tool::Erase)
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
		e.brush.select_mode == SelectMode::ClickMarquee) {
		const F2 a{ WorldToScreen(e, e.stroke.start_world) };
		const F2 b{ WorldToScreen(e, mouse_world) };
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
				SelectMarquee(e, e.stroke.start_world, mouse_world, shift, ctrl);
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
		}
		if (e.stroke.active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			e.stroke.current_world = mouse_world;
			if (e.tool == Tool::Brush) {
				PaintAt(e, mouse_world);
			} else {
				EraseAt(e, mouse_world);
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
	DrawBrushSettings(e);
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
	DrawGrid(e, dl);

	for (auto& layer : e.layers) {
		if (layer.kind == LayerKind::Tile) {
			UpdateStreamingForLayer(e, layer);
			DrawTileLayer(e, layer, dl);
		} else {
			DrawEntityLayer(e, layer, dl);
		}
	}

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
		const bool open{ ImGui::TreeNodeEx(layer.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | (layer.kind == LayerKind::Tile ? ImGuiTreeNodeFlags_Leaf : 0)) };
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
			} else {
				ImGui::TextDisabled("Tiles are edited in the viewport/palette, not listed individually.");
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	ImGui::End();
}

static void DrawLayers(EditorState& e) {
	ImGui::Begin("Layers");
	ImGui::TextDisabled("Top renders last. Click a row to make it active. Drag rows to reorder.");
	int move_source_id{ -1 };
	int move_target_id{ -1 };
	int delete_id{ -1 };
	int duplicate_id{ -1 };

	for (int i{ static_cast<int>(e.layers.size()) - 1 }; i >= 0; --i) {
		auto& layer{ e.layers[static_cast<std::size_t>(i)] };
		ImGui::PushID(layer.id);
		ImGui::Checkbox("##visible", &layer.visible);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible");
		ImGui::SameLine();
		ImGui::Checkbox("##locked", &layer.locked);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Locked");
		ImGui::SameLine();
		const std::string label{ std::string(layer.kind == LayerKind::Tile ? "# " : "◆ ") + layer.name + "##row" };
		if (ImGui::Selectable(label.c_str(), layer.id == e.active_layer_id, 0, { 0.0f, 0.0f })) {
			e.active_layer_id = layer.id;
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			const int payload_id{ layer.id };
			ImGui::SetDragDropPayload("SCENE_LAYER_ID", &payload_id, sizeof(payload_id));
			ImGui::Text("Move %s", layer.name.c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_LAYER_ID")) {
				if (payload->DataSize == sizeof(int)) {
					move_source_id = *static_cast<const int*>(payload->Data);
					move_target_id = layer.id;
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Move Up") && i + 1 < static_cast<int>(e.layers.size())) {
				move_source_id = layer.id;
				move_target_id = e.layers[static_cast<std::size_t>(i + 1)].id;
			}
			if (ImGui::MenuItem("Move Down") && i > 0) {
				move_source_id = layer.id;
				move_target_id = e.layers[static_cast<std::size_t>(i - 1)].id;
			}
			ImGui::Separator();
			ImGui::MenuItem("Visible", nullptr, &layer.visible);
			ImGui::MenuItem("Locked", nullptr, &layer.locked);
			ImGui::MenuItem("Selectable", nullptr, &layer.selectable);
			if (ImGui::MenuItem("Solo")) {
				for (auto& other : e.layers) other.visible = other.id == layer.id;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Duplicate")) duplicate_id = layer.id;
			if (ImGui::MenuItem("Delete")) delete_id = layer.id;
			ImGui::EndPopup();
		}
		ImGui::PopID();
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
			const std::size_t adjusted{ src_index < dst_index ? dst_index - 1 : dst_index };
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
		e.entities.erase(std::remove_if(e.entities.begin(), e.entities.end(), [&](const Entity& entity) {
			return entity.layer_id == delete_id;
		}), e.entities.end());
		e.layers.erase(std::remove_if(e.layers.begin(), e.layers.end(), [&](const SceneLayer& layer) {
			return layer.id == delete_id;
		}), e.layers.end());
		if (!FindLayer(e, e.active_layer_id) && !e.layers.empty()) {
			e.active_layer_id = e.layers.back().id;
		}
		PushHistory(e, "Delete Layer", before);
	}

	ImGui::Separator();
	if (ImGui::Button("+ Entity Layer")) {
		const auto before{ CaptureScene(e) };
		SceneLayer layer;
		layer.id = e.next_layer_id++;
		layer.name = "Entity Layer " + std::to_string(layer.id);
		layer.kind = LayerKind::Entity;
		e.layers.push_back(layer);
		e.active_layer_id = layer.id;
		PushHistory(e, "Add Entity Layer", before);
	}
	ImGui::SameLine();
	if (ImGui::Button("+ Tile Layer")) {
		const auto before{ CaptureScene(e) };
		if (e.tilemaps.empty()) {
			Tilemap map;
			map.id = e.next_tilemap_id++;
			e.tilemaps.push_back(map);
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
	if (SceneLayer* active = FindLayer(e, e.active_layer_id)) {
		ImGui::Separator();
		char name[128]{};
		std::snprintf(name, sizeof(name), "%s", active->name.c_str());
		if (ImGui::InputText("Name", name, sizeof(name))) {
			active->name = name;
		}
		int purpose{ static_cast<int>(active->purpose) };
		const char* purposes[]{ "Visual", "Collision", "Navigation", "Metadata" };
		if (ImGui::Combo("Purpose", &purpose, purposes, 4)) {
			active->purpose = static_cast<LayerPurpose>(purpose);
		}
		ImGui::Checkbox("Selectable", &active->selectable);
	}
	ImGui::End();
}

static void DrawPaletteTile(EditorState& e, const TileDefinition& tile, bool selected, bool in_stamp) {
	const float size{ 56.0f };
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
static void DrawTilePalette(EditorState& e) {
	ImGui::Begin("Tile Palette");
	if (const auto* layer = FindLayer(e, e.active_layer_id)) {
		ImGui::TextDisabled(layer->kind == LayerKind::Tile
			? "Active target: TILE layer '%s'"
			: "Active target: PREFAB/ENTITY layer '%s' (tile selection is retained)", layer->name.c_str());
	}
	if (e.palettes.empty()) {
		e.palettes.push_back({ "Default", {} });
	}
	e.active_palette_index = std::clamp(e.active_palette_index, 0, static_cast<int>(e.palettes.size()) - 1);

	if (ImGui::BeginCombo("Palette", e.palettes[static_cast<std::size_t>(e.active_palette_index)].name.c_str())) {
		for (int i{}; i < static_cast<int>(e.palettes.size()); ++i) {
			const bool selected{ i == e.active_palette_index };
			if (ImGui::Selectable(e.palettes[static_cast<std::size_t>(i)].name.c_str(), selected)) {
				e.active_palette_index = i;
				e.stamp_tiles.clear();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("New Palette")) {
		e.palettes.push_back({ "Palette " + std::to_string(e.palettes.size() + 1), {} });
		e.active_palette_index = static_cast<int>(e.palettes.size()) - 1;
	}
	ImGui::SameLine();
	if (ImGui::Button("Duplicate")) {
		TilePalette copy{ e.palettes[static_cast<std::size_t>(e.active_palette_index)] };
		copy.name += " Copy";
		e.palettes.push_back(std::move(copy));
		e.active_palette_index = static_cast<int>(e.palettes.size()) - 1;
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(e.palettes.size() <= 1);
	if (ImGui::Button("Delete")) {
		e.palettes.erase(e.palettes.begin() + e.active_palette_index);
		e.active_palette_index = std::clamp(e.active_palette_index, 0, static_cast<int>(e.palettes.size()) - 1);
		e.stamp_tiles.clear();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Import Tiles...")) {
		e.importer.target_palette = e.active_palette_index;
		e.importer.open_popup = true;
	}

	auto& palette{ e.palettes[static_cast<std::size_t>(e.active_palette_index)] };
	char palette_name[128]{};
	std::snprintf(palette_name, sizeof(palette_name), "%s", palette.name.c_str());
	if (ImGui::InputText("Palette Name", palette_name, sizeof(palette_name))) {
		palette.name = palette_name;
	}

	if (!e.stamp_tiles.empty()) {
		ImGui::Text("Stamp: %d tile(s)", static_cast<int>(e.stamp_tiles.size()));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.0f);
		ImGui::DragInt("Width", &e.stamp_width, 1.0f, 1, 32);
		ImGui::SameLine();
		if (ImGui::Button("Clear Stamp")) {
			e.stamp_tiles.clear();
		}
	}

	ImGui::Separator();
	const float cell{ 64.0f };
	const int columns{ std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell)) };
	int column{};
	for (auto& entry : palette.entries) {
		const auto* tile{ FindTile(e, entry.tile_id) };
		if (!tile) continue;
		ImGui::PushID(entry.tile_id);
		const bool in_stamp{ std::find(e.stamp_tiles.begin(), e.stamp_tiles.end(), entry.tile_id) != e.stamp_tiles.end() };
		DrawPaletteTile(e, *tile, e.active_tile_id == entry.tile_id, in_stamp);
		ImGui::PopID();
		++column;
		if (column % columns != 0) {
			ImGui::SameLine();
		}
	}
	if (palette.entries.empty()) {
		ImGui::TextDisabled("No tiles in this palette. Drag images/TSX/JSON here or use Import Tiles...");
	}
	DrawImportPopup(e);
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
				int debug_options{ static_cast<int>(map->streaming.show_chunk_boundaries) + static_cast<int>(map->streaming.show_streaming_state) };
				const std::string debug_preview{ debug_options == 0 ? "Debug View" : "Debug View (" + std::to_string(debug_options) + ")" };
				if (ImGui::BeginCombo("##stream_debug", debug_preview.c_str())) {
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
		if (ImGui::MenuItem("Deselect All", "Ctrl+D", false, !e.selected_entities.empty() || !e.selected_tile_cells.empty())) DeselectAll(e);
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
		ImGui::BulletText("Ctrl/Shift: selection modifiers; Ctrl+D: deselect all");
		ImGui::BulletText("Delete: erase selected entities/tiles (undoable)");
		ImGui::BulletText("Pencil: click-drag for continuous drawing");
		ImGui::BulletText("Line/Area: Escape cancels the active drag");
		ImGui::BulletText("Eyedropper: hold left mouse and move to continuously pick");
		ImGui::BulletText("Ctrl-click palette tile: add/remove stamp tile");
		ImGui::BulletText("Q/P/B/L/A/F/E/I: Select/Pencil/Brush/Line/Area/Fill/Erase/Pick");
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
		ImGui::DockBuilderDockWindow("Tile Palette", bottom);
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
	ImGuiIO& io{ ImGui::GetIO() };
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) Undo(e);
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) Redo(e);
	if (io.KeyCtrl && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_D, false)) DeselectAll(e);
	if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) DeleteSelection(e);
	if (!io.WantTextInput && e.canvas_hovered) {
		if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) e.tool = Tool::Select;
		if (ImGui::IsKeyPressed(ImGuiKey_P, false)) e.tool = Tool::Pencil;
		if (ImGui::IsKeyPressed(ImGuiKey_B, false)) e.tool = Tool::Brush;
		if (ImGui::IsKeyPressed(ImGuiKey_L, false)) e.tool = Tool::Line;
		if (ImGui::IsKeyPressed(ImGuiKey_A, false)) e.tool = Tool::Area;
		if (ImGui::IsKeyPressed(ImGuiKey_F, false)) e.tool = Tool::Fill;
		if (ImGui::IsKeyPressed(ImGuiKey_E, false)) e.tool = Tool::Erase;
		if (ImGui::IsKeyPressed(ImGuiKey_I, false)) e.tool = Tool::Eyedropper;
	}

	DrawMainMenu(e);
	DrawDefaultDockspace(e);
	DrawViewport(e);
	DrawSceneHierarchy(e);
	DrawLayers(e);
	DrawTilePalette(e);
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
