#include "runtime/world/tilemap.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <utility>

#include "core/assert.h"
#include "core/math/transform.h"
#include "runtime/ecs/entity_creation_context.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

Tilemap::Tilemap(Entity entity) : Entity{ entity } {
	PTGN_ASSERT(!entity || entity.Has<impl::TilemapData>(), "Entity is not a Tilemap");
}

const impl::TilemapData& Tilemap::GetData() const {
	PTGN_ASSERT(*this, "Cannot access a null Tilemap");
	return Get<impl::TilemapData>();
}

impl::TilemapData& Tilemap::GetData() {
	return const_cast<impl::TilemapData&>(std::as_const(*this).GetData());
}

Tilemap& Tilemap::SetCellSize(V2_float cell_size) {
	PTGN_ASSERT(cell_size.IsPositive(), "Tilemap cell size must be positive");
	GetData().cell_size = cell_size;
	return *this;
}

V2_int Tilemap::WorldToCell(V2_float world) const {
	const auto& data{ GetData() };
	V2_float origin{ GetWorldPosition(*this) };
	return {
		static_cast<int>(std::floor((world.x - origin.x) / data.cell_size.x)),
		static_cast<int>(std::floor((world.y - origin.y) / data.cell_size.y)),
	};
}

V2_float Tilemap::CellToWorld(V2_int coordinate) const {
	const auto& data{ GetData() };
	return GetWorldPosition(*this) +
		   V2_float{ static_cast<float>(coordinate.x), static_cast<float>(coordinate.y) } *
			   data.cell_size;
}

std::optional<std::size_t> Tilemap::FindTileIndex(V2_int coordinate) const {
	const auto& data{ GetData() };
	const auto& tiles{ data.tiles };
	if (HasScene() && GetScene().IsRuntime()) {
		if (!data.runtime_tile_index_valid) {
			data.runtime_tile_index.clear();
			data.runtime_tile_index.reserve(tiles.size());
			for (std::size_t i{ 0 }; i < tiles.size(); ++i) {
				data.runtime_tile_index.insert_or_assign(tiles[i].coordinate, i);
			}
			data.runtime_tile_index_valid = true;
		}
		auto it{ data.runtime_tile_index.find(coordinate) };
		return it == data.runtime_tile_index.end() ? std::nullopt
												   : std::optional<std::size_t>{ it->second };
	}

	auto it{ std::ranges::find(tiles, coordinate, &TilemapTile::coordinate) };
	return it == tiles.end() ? std::nullopt
							 : std::optional<std::size_t>{
								   static_cast<std::size_t>(std::distance(tiles.begin(), it))
							   };
}

const TilemapTile* Tilemap::FindTile(V2_int coordinate) const {
	auto index{ FindTileIndex(coordinate) };
	return index ? &GetData().tiles[*index] : nullptr;
}

TilemapTile* Tilemap::FindTile(V2_int coordinate) {
	return const_cast<TilemapTile*>(std::as_const(*this).FindTile(coordinate));
}

Tilemap& Tilemap::SetTile(TilemapTile tile) {
	if (HasScene() && GetScene().IsRuntime()) {
		GetScene().ctx().chunks.BeforeTileChanged(*this, tile.coordinate);
	}
	V2_int cell{ tile.coordinate };
	if (auto* current{ FindTile(cell) }) {
		*current = std::move(tile);
	} else {
		auto& data{ GetData() };
		data.tiles.emplace_back(std::move(tile));
		if (data.runtime_tile_index_valid) {
			data.runtime_tile_index[cell] = data.tiles.size() - 1;
		}
	}
	if (HasScene() && GetScene().IsRuntime()) {
		GetScene().ctx().chunks.AfterTileChanged(*this, cell);
	}
	return *this;
}

bool Tilemap::EraseTile(V2_int coordinate) {
	bool runtime{ HasScene() && GetScene().IsRuntime() };
	if (runtime) {
		GetScene().ctx().chunks.BeforeTileChanged(*this, coordinate);
	}

	auto index{ FindTileIndex(coordinate) };
	if (!index) {
		return false;
	}

	auto& data{ GetData() };
	if (runtime) {
		// In chunked worlds removal must not shift every remaining tile's index.
		std::size_t last{ data.tiles.size() - 1 };
		if (*index != last) {
			data.tiles[*index]									   = std::move(data.tiles[last]);
			data.runtime_tile_index[data.tiles[*index].coordinate] = *index;
		}
		data.tiles.pop_back();
		data.runtime_tile_index.erase(coordinate);
		GetScene().ctx().chunks.AfterTileChanged(*this, coordinate);
	} else {
		// Preserve authored tile order inside the editor.
		data.tiles.erase(data.tiles.begin() + static_cast<std::ptrdiff_t>(*index));
		data.runtime_tile_index_valid = false;
	}
	return true;
}

void Tilemap::ClearTiles() {
	std::vector<V2_int> cells;
	cells.reserve(GetData().tiles.size());
	for (const auto& tile : GetData().tiles) {
		cells.push_back(tile.coordinate);
	}
	for (V2_int cell : cells) {
		EraseTile(cell);
	}
}

std::optional<std::uint64_t> Tilemap::GetTerrainRuleset(V2_int coordinate) const {
	const auto& terrain{ GetData().terrain };
	auto it{ std::ranges::find(terrain, coordinate, &TilemapTerrainCell::coordinate) };
	return it == terrain.end() ? std::nullopt : std::optional<std::uint64_t>{ it->ruleset_id };
}

bool Tilemap::SetTerrainRuleset(V2_int coordinate, std::optional<std::uint64_t> ruleset_id) {
	auto& terrain{ GetData().terrain };
	auto it{ std::ranges::find(terrain, coordinate, &TilemapTerrainCell::coordinate) };
	if (!ruleset_id.has_value()) {
		if (it == terrain.end()) {
			return false;
		}
		terrain.erase(it);
		return true;
	}
	if (it != terrain.end()) {
		if (it->ruleset_id == *ruleset_id) {
			return false;
		}
		it->ruleset_id = *ruleset_id;
		return true;
	}
	terrain.push_back(TilemapTerrainCell{ .coordinate = coordinate, .ruleset_id = *ruleset_id });
	return true;
}

void Tilemap::ClearTerrain() {
	GetData().terrain.clear();
}

bool Tilemap::IsExcluded(V2_int coordinate) const {
	return std::ranges::contains(GetData().exclusion_mask, coordinate);
}

bool Tilemap::SetExcluded(V2_int coordinate, bool excluded) {
	auto& mask{ GetData().exclusion_mask };
	auto it{ std::ranges::find(mask, coordinate) };
	if (excluded) {
		if (it != mask.end()) {
			return false;
		}
		mask.emplace_back(coordinate);
		return true;
	}
	if (it == mask.end()) {
		return false;
	}
	mask.erase(it);
	return true;
}

void Tilemap::ClearExclusionMask() {
	GetData().exclusion_mask.clear();
}

bool IsTilemap(Entity entity) {
	return entity && entity.Has<impl::TilemapData>();
}

Tilemap CreateTilemap(EntityCreationContext context, Tag tag) {
	Entity entity{ context.CreateEntity(std::move(tag)) };
	entity.TryAdd<Transform>();
	entity.Add<impl::TilemapData>();
	return Tilemap{ entity };
}

} // namespace ptgn
