#pragma once

#include <engine/Include.h>

#include "factory/Factories.h"

class BoxChunk : public engine::Chunk {
public:
	virtual void Generate(int seed) override final {
		engine::RNG rng;
		V2_int tile = { 0, 0 };
		for (tile.x = 0; tile.x < tile_count.x; ++tile.x) {
			for (tile.y = 0; tile.y < tile_count.y; ++tile.y) {
				auto absolute_tile_position = static_cast<V2_int>(info.position) / tile_size + tile;
				auto& entity = GetEntity(tile);
				auto tile_seed = (absolute_tile_position.x & 0xFFFF) << 16 | (absolute_tile_position.y & 0xFFFF);
				rng.SetSeed(tile_seed * seed);
				bool draw_box = (rng.RandomInt(1, 2) == 1);
				if (draw_box) {
					auto absolute_position = absolute_tile_position * tile_size;// - static_cast<V2_int>(camera->offset);
					V2_int size = tile_size;
					if (rng.RandomInt(0, 20) == 1) {
						entity = CreateBox(entity, absolute_position, size, "./resources/textures/box.png");
					} else if (rng.RandomInt(0, 20) == 1) {
						entity = CreateBox(entity, absolute_position, size, "./resources/textures/box2.png");
					} else if (rng.RandomInt(0, 20) == 1) {
						entity = CreateBox(entity, absolute_position, size, "./resources/textures/box3.png");
					} else if (rng.RandomInt(0, 20) == 1) {
						entity = CreateBox(entity, absolute_position, size, "./resources/textures/box4.png");
					} else {
						entity = CreateBox(entity, absolute_position, size, "./resources/textures/tree.png");
					}
					//size.x = rng.RandomInt(tile_size.x, engine::math::RoundCast<int>(tile_size.x * 1.5));
					//size.y = rng.RandomInt(tile_size.y, engine::math::RoundCast<int>(tile_size.y * 1.5));
					//auto color = colors[rng.RandomInt(0, 8)];
					//LOG("Tile count: " << tile_count << ", chunk pos: " << info.position << ", abs tile pos: " << absolute_tile_position);
				}
			}
		}
	}
};