#pragma once

// TODO: Come back to this.
/*
#include "core/app/game.h"
#include "core/app/manager.h"
#include "math/geometry_utils.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "renderer/materials/texture.h"
#include "world/tile/grid.h"
#include "debug/runtime/assert.h"
#include "core/util/file.h"

namespace ptgn {

class EntityLayer {
public:
	EntityLayer() = default;

private:
	Manager manager;
};

class TileLayer : public Grid<impl::Tile> {
public:
	// TOOD: Change to take path.
	TileLayer(
		const path& tileset_path, const V2_int& tile_size, const V2_int& grid_size,
		const V2_float& scale
	) :
		Grid<impl::Tile>{ grid_size },
		texture_key{ Hash(tileset_path) },
		tile_size{ tile_size },
		scale{ scale },
		scaled_tile_size{ scale * tile_size } {
		PTGN_ASSERT(FileExists(tileset_path));
		game.texture.Load(texture_key, tileset_path);
		for (int i = 0; i < size.x; i++) {
			for (int j = 0; j < size.y; j++) {
				V2_int tile{ i, j };
				Rect rect{ tile * grid_size * scaled_tile_size, scaled_tile_size };
				Set(tile, impl::Tile{ rect });
			}
		}
	}

	void Draw() {
		ForEachElement([](const impl::Tile& tile) { tile.Draw(); });
	}

private:
	std::size_t texture_key;
	V2_int scaled_tile_size;
	V2_int tile_size;
	V2_float scale;
};

template <typename T = int>
class GridLayer : public Grid<T> {
public:
private:
};

} // namespace ptgn

*/