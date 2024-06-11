#pragma once

#include "protegon/debug.h"
#include "ecs/ecs.h"
#include "grid.h"
#include "vector2.h"
#include "polygon.h"
#include "texture.h"
#include "resources.h"
#include "file.h"
#include "hash.h"

namespace ptgn {

class EntityLayer {
public:
	EntityLayer() = default;
private:
	ecs::Manager manager;
};

namespace impl {

class Tile {
public:
	Tile() = default;
	Tile(const Rectangle<int>& rect) : rect{ rect } {}
	Tile(std::size_t texture_key, const Rectangle<int>& source) : source{ source } {
		PTGN_CHECK(texture::Has(texture_key));
		texture = texture::Get(texture_key);
	}
	void Draw() const {
		if (texture.IsValid()) {
			texture.Draw(rect, source);
		}
	}
private:
	Rectangle<int> rect;
	Rectangle<int> source;
	Texture texture;
};

} // namespace impl

class TileLayer : public Grid<impl::Tile> {
public:
	TileLayer(const char* tileset_path, const V2_int& tile_size, const V2_int& grid_size, const V2_float& scale) : Grid<impl::Tile>{ grid_size }, texture_key{ Hash(tileset_path) }, tile_size{ tile_size }, scale{ scale }, scaled_tile_size{ scale * tile_size } {
		PTGN_CHECK(FileExists(tileset_path));
		texture::Load(texture_key, tileset_path);
		for (int i = 0; i < size.x; i++) {
			for (int j = 0; j < size.y; j++) {
				V2_int tile{ i, j };
				Rectangle<int> rect{ tile * grid_size * scaled_tile_size, scaled_tile_size };
				Set(tile, impl::Tile{ rect });
			}
		}
	}
	void Draw() {
		ForEachElement([](const impl::Tile& tile) {
			tile.Draw();
		});
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