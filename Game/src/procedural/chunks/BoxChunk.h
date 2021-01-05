#pragma once

#include <engine/Include.h>

#include "factory/Factories.h"

class BoxChunk : public engine::Chunk {
public:
	virtual void Generate(int seed, int octave, double bias) override final {
		engine::RNG rng;
		auto chunk_tile = static_cast<V2_int>(info.position) / tile_size;
		//perlin = engine::PerlinNoise(tile_count, rng, chunk_tile, seed);
		//perlin.Generate2D(octave, bias);
		auto tile_seed = (chunk_tile.x & 0xFFFF) << 16 | (chunk_tile.y & 0xFFFF);
		const siv::PerlinNoise perlin(tile_seed * seed);
		V2_int tile = { 0, 0 };
		const double fx = tile_count.x / bias;
		const double fy = tile_count.y / bias;
		for (tile.x = 0; tile.x < tile_count.x; ++tile.x) {
			for (tile.y = 0; tile.y < tile_count.y; ++tile.y) {
				auto absolute_tile_position = chunk_tile + tile;
				auto absolute_position = absolute_tile_position * tile_size;
				auto& entity = GetEntity(tile);
				auto tile_seed = (absolute_tile_position.x & 0xFFFF) << 16 | (absolute_tile_position.y & 0xFFFF);
				//rng.SetSeed(tile_seed * seed);
				//double noise = perlin.GetNoise2D(tile);
				double noise = perlin.accumulatedOctaveNoise2D_0_1(tile.x / fx, tile.y / fy, octave);
				int pixel_bw = (int)(noise * 3);
				switch (pixel_bw) {
					case 0: { entity = CreateBox(entity, absolute_position, tile_size, "./resources/textures/tree.png"); break; }
					case 1: { entity = CreateBox(entity, absolute_position, tile_size, "./resources/textures/box.png"); break; }
					case 2: { entity = CreateBox(entity, absolute_position, tile_size, "./resources/textures/box2.png"); break; }
					default: { entity = CreateBox(entity, absolute_position, tile_size, "./resources/textures/box3.png"); break; }
				}
			}
		}
	}
};