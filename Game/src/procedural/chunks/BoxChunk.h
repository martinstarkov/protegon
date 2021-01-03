#pragma once

#include <engine/Include.h>

#include "factory/Factories.h"

class BoxChunk : public engine::Chunk {
public:
	virtual void Generate(int seed, int octave, double bias) override final {
		engine::RNG rng;
		V2_int tile = { 0, 0 };
		auto position_seed = ((int)info.position.x & 0xFFFF) << 16 | ((int)info.position.y & 0xFFFF);
		rng.SetSeed(position_seed * seed);
		auto perlin = engine::PerlinNoise(tile_count, rng);
		//LOG("Tile count: " << tile_count);
		perlin.Generate2D(octave, bias);
		/*LOG("Perlin noise: ");
		for (auto i = 0; i < perlin.noise2D.size(); ++i) {
			if (i % perlin.size.x == 1) {
				LOG("");
			} else {
				LOG_(perlin.noise2D[i]);
			}
		}
		LOG("");*/
		for (tile.x = 0; tile.x < tile_count.x; ++tile.x) {
			for (tile.y = 0; tile.y < tile_count.y; ++tile.y) {
				auto absolute_tile_position = static_cast<V2_int>(info.position) / tile_size + tile;
				auto absolute_position = absolute_tile_position * tile_size;
				auto& entity = GetEntity(tile);
				auto tile_seed = (absolute_tile_position.x & 0xFFFF) << 16 | (absolute_tile_position.y & 0xFFFF);
				//rng.SetSeed(tile_seed * seed);
				double noise = perlin.GetNoise2D(tile);
				int pixel_bw = (int)(noise * 16.0);
				switch (pixel_bw) {
					case 0:
					case 1:
					case 2: 
					case 3: 
					case 4: { entity = CreateBox(entity, absolute_position, tile_size, "./resources/textures/tree.png"); break; }

					case 5: 
					case 6: 
					case 7: 
					case 8: { entity = CreateBox(entity, absolute_position, tile_size, "./resources/textures/box.png"); break; }

					case 9:  
					case 10: 
					case 11: 
					case 12: { entity = CreateBox(entity, absolute_position, tile_size, "./resources/textures/box2.png"); break; }

					case 13:
					case 14:
					case 15:
					case 16: 
					default: { entity = CreateBox(entity, absolute_position, tile_size, "./resources/textures/box3.png"); break; }
				}
				//bool draw_box = (rng.RandomInt(1, 2) == 1);
				//if (draw_box) {
				//	auto absolute_position = absolute_tile_position * tile_size;// - static_cast<V2_int>(camera->offset);
				//	V2_int size = tile_size;
				//	if (rng.RandomInt(0, 20) == 1) {
				//		entity = CreateBox(entity, absolute_position, size, "./resources/textures/box.png");
				//	} else if (rng.RandomInt(0, 20) == 1) {
				//		entity = CreateBox(entity, absolute_position, size, "./resources/textures/box2.png");
				//	} else if (rng.RandomInt(0, 20) == 1) {
				//		entity = CreateBox(entity, absolute_position, size, "./resources/textures/box3.png");
				//	} else if (rng.RandomInt(0, 20) == 1) {
				//		entity = CreateBox(entity, absolute_position, size, "./resources/textures/box4.png");
				//	} else {
				//		entity = CreateBox(entity, absolute_position, size, "./resources/textures/tree.png");
				//	}
				//	//size.x = rng.RandomInt(tile_size.x, engine::math::RoundCast<int>(tile_size.x * 1.5));
				//	//size.y = rng.RandomInt(tile_size.y, engine::math::RoundCast<int>(tile_size.y * 1.5));
				//	//auto color = colors[rng.RandomInt(0, 8)];
				//	//LOG("Tile count: " << tile_count << ", chunk pos: " << info.position << ", abs tile pos: " << absolute_tile_position);
				//}
			}
		}
	}
};