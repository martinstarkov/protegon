#include "runtime/world/tilemap.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <utility>

#include "core/assert.h"
#include "core/math/transform.h"
#include "runtime/scene/scene.h"

namespace ptgn {

Tilemap::Tilemap(Entity entity) : Entity{ entity } {
	PTGN_ASSERT(!entity || entity.Has<::ptgn::impl::TilemapData>(), "Entity is not a Tilemap");
}

const ::ptgn::impl::TilemapData& Tilemap::GetData() const {
	PTGN_ASSERT(*this, "Cannot access a null Tilemap");
	return Get<::ptgn::impl::TilemapData>();
}

::ptgn::impl::TilemapData& Tilemap::GetData() {
	return const_cast<::ptgn::impl::TilemapData&>(std::as_const(*this).GetData());
}

Tilemap& Tilemap::SetCellSize(V2_float cell_size) {
	PTGN_ASSERT(cell_size.IsPositive(), "Tilemap cell size must be positive");
	GetData().cell_size = cell_size;
	return *this;
}

Tilemap& Tilemap::SetChunkSize(V2_int chunk_size) {
	PTGN_ASSERT(chunk_size.IsPositive(), "Tilemap chunk size must be positive");
	GetData().chunk_size = chunk_size;
	return *this;
}

Tilemap& Tilemap::SetStreaming(TilemapStreamingSettings settings) {
	settings.preload_radius = std::max(0, settings.preload_radius);
	settings.keep_alive_radius = std::max(settings.preload_radius, settings.keep_alive_radius);
	settings.max_loaded_chunks = std::max(1, settings.max_loaded_chunks);
	GetData().streaming = settings;
	return *this;
}

V2_int Tilemap::WorldToCell(V2_float world) const {
	const auto& data{ GetData() };
	const V2_float origin{ GetWorldPosition(*this) };
	return {
		static_cast<int>(std::floor((world.x - origin.x) / data.cell_size.x)),
		static_cast<int>(std::floor((world.y - origin.y) / data.cell_size.y)),
	};
}

V2_float Tilemap::CellToWorld(V2_int coordinate) const {
	const auto& data{ GetData() };
	return GetWorldPosition(*this) + V2_float{ static_cast<float>(coordinate.x), static_cast<float>(coordinate.y) } * data.cell_size;
}

std::optional<std::size_t> Tilemap::FindTileIndex(V2_int coordinate) const {
	const auto& tiles{ GetData().tiles };
	const auto it{ std::ranges::find(tiles, coordinate, &TilemapTile::coordinate) };
	if (it == tiles.end()) {
		return std::nullopt;
	}
	return static_cast<std::size_t>(std::distance(tiles.begin(), it));
}

const TilemapTile* Tilemap::FindTile(V2_int coordinate) const {
	const auto index{ FindTileIndex(coordinate) };
	return index ? &GetData().tiles[*index] : nullptr;
}

TilemapTile* Tilemap::FindTile(V2_int coordinate) {
	return const_cast<TilemapTile*>(std::as_const(*this).FindTile(coordinate));
}

Tilemap& Tilemap::SetTile(TilemapTile tile) {
	if (auto* current{ FindTile(tile.coordinate) }) {
		*current = std::move(tile);
	} else {
		GetData().tiles.emplace_back(std::move(tile));
	}
	return *this;
}

bool Tilemap::EraseTile(V2_int coordinate) {
	auto& tiles{ GetData().tiles };
	return std::erase_if(tiles, [coordinate](const TilemapTile& tile) {
		return tile.coordinate == coordinate;
	}) > 0;
}

void Tilemap::ClearTiles() {
	GetData().tiles.clear();
}

bool Tilemap::IsExcluded(V2_int coordinate) const {
	return std::ranges::contains(GetData().exclusion_mask, coordinate);
}

bool Tilemap::SetExcluded(V2_int coordinate, bool excluded) {
	auto& mask{ GetData().exclusion_mask };
	const auto it{ std::ranges::find(mask, coordinate) };
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
	return entity && entity.Has<::ptgn::impl::TilemapData>();
}

Tilemap CreateTilemap(Scene& scene, SceneLayerId layer, Tag tag) {
	const SceneLayer* destination{ scene.GetLayers().Find(layer) };
	if (!destination || destination->kind != SceneLayerKind::Tile) {
		return {};
	}

	Entity entity{ scene.CreateEntity(std::move(tag)) };
	entity.TryAdd<Transform>();
	entity.Add<::ptgn::impl::TilemapData>();

	if (!scene.GetLayers().Assign(entity, layer, false)) {
		entity.Destroy();
		return {};
	}

	return Tilemap{ entity };
}

} // namespace ptgn
