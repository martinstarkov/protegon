#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/world/entity_layer.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;

/// @brief One authored tile placement owned by a Tilemap entity.
///
/// The tile is intentionally self-contained at the engine layer: it references a texture and UVs
/// rather than an editor palette index. Editor-side tile assets can later resolve down to this
/// representation without making runtime scenes depend on palette/group UI state.
struct TilemapTile {
	V2_int coordinate{};
	TextureKey texture{};
	std::array<V2_float, 4> texture_coordinates{
		V2_float{ 0.0f, 0.0f },
		V2_float{ 1.0f, 0.0f },
		V2_float{ 1.0f, 1.0f },
		V2_float{ 0.0f, 1.0f },
	};
	/// Native visual size in world/pixel units. A zero/invalid size falls back to the Tilemap cell size.
	V2_int pixel_size{ 32, 32 };
	Origin origin{ Origin::TopLeft };
	V2_float offset{};
	Color tint{ color::White };
	/// Stable editor-authored terrain ruleset id when this is a derived autotile display cell.
	std::optional<std::uint64_t> terrain_ruleset_id{};

	PTGN_REFLECT(
		TilemapTile,
		coordinate,
		texture,
		texture_coordinates,
		pixel_size,
		origin,
		offset,
		tint,
		terrain_ruleset_id
	)
};

/// Logical terrain ownership is stored separately from the derived display tile.
/// This is required by Dual Grid, whose display cells are offset by half a cell.
struct TilemapTerrainCell {
	V2_int coordinate{};
	std::uint64_t ruleset_id{};

	PTGN_REFLECT(TilemapTerrainCell, coordinate, ruleset_id)
};

struct TilemapStreamingSettings {
	bool enabled{ true };
	int preload_radius{ 1 };
	int keep_alive_radius{ 2 };
	int max_loaded_chunks{ 128 };

	PTGN_REFLECT(
		TilemapStreamingSettings,
		enabled,
		preload_radius,
		keep_alive_radius,
		max_loaded_chunks
	)
};

namespace impl {

/// @brief Persistent component that turns an ECS entity into a Tilemap entity.
///
/// A Tilemap owns exactly one authored set of tiles. It must belong to a SceneLayerKind::Tile
/// layer. Tilemaps are deliberately entities so they can be selected, named, serialized, copied,
/// and targeted by generators like any other scene object.
struct TilemapData {
	V2_float cell_size{ 32.0f, 32.0f };
	V2_int chunk_size{ 16, 16 };
	TilemapStreamingSettings streaming{};
	std::vector<TilemapTile> tiles{};
	std::vector<TilemapTerrainCell> terrain{};
	std::vector<V2_int> exclusion_mask{};

	PTGN_REFLECT(
		TilemapData,
		cell_size,
		chunk_size,
		streaming,
		tiles,
		terrain,
		exclusion_mask
	)
};

} // namespace impl

class Tilemap : public Entity {
public:
	Tilemap() = default;
	explicit Tilemap(Entity entity);

	[[nodiscard]] const impl::TilemapData& GetData() const;
	[[nodiscard]] impl::TilemapData& GetData();

	Tilemap& SetCellSize(V2_float cell_size);
	Tilemap& SetChunkSize(V2_int chunk_size);
	Tilemap& SetStreaming(TilemapStreamingSettings settings);

	[[nodiscard]] V2_int WorldToCell(V2_float world) const;
	[[nodiscard]] V2_float CellToWorld(V2_int coordinate) const;

	[[nodiscard]] std::optional<std::size_t> FindTileIndex(V2_int coordinate) const;
	[[nodiscard]] const TilemapTile* FindTile(V2_int coordinate) const;
	[[nodiscard]] TilemapTile* FindTile(V2_int coordinate);

	/// @brief Adds or replaces the tile anchored at tile.coordinate.
	Tilemap& SetTile(TilemapTile tile);
	bool EraseTile(V2_int coordinate);
	void ClearTiles();

	[[nodiscard]] std::optional<std::uint64_t> GetTerrainRuleset(V2_int coordinate) const;
	bool SetTerrainRuleset(V2_int coordinate, std::optional<std::uint64_t> ruleset_id);
	void ClearTerrain();

	[[nodiscard]] bool IsExcluded(V2_int coordinate) const;
	bool SetExcluded(V2_int coordinate, bool excluded = true);
	void ClearExclusionMask();
};

[[nodiscard]] bool IsTilemap(Entity entity);

/// @brief Creates a Tilemap entity directly in a Tile layer.
/// @return A null Tilemap if layer is missing or is not a Tile layer.
[[nodiscard]] Tilemap CreateTilemap(
	Scene& scene,
	SceneLayerId layer,
	Tag tag = Tag{ "Tilemap" }
);

} // namespace ptgn
