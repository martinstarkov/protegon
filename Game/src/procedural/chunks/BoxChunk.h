#pragma once

#include <engine/Include.h>

#include "factory/Factories.h"

class BoxChunk : public engine::Chunk {
public:
	virtual void Generate(int seed, int octave, double bias) override final {
		float scale = 400.f;
		float offset_x = 5.9f;
		float offset_y = 5.1f;
		float offset_z = 0.05f;
		float lacunarity = 1.99f;
		float persistance = 0.5f;

		/*SimplexNoise simplex(float frequency = 1.0f,
							 float amplitude = 1.0f,
							 float lacunarity = 2.0f,
							 float persistence = 0.5f);*/
		const SimplexNoise simplex(0.1f / scale, 0.5f, lacunarity, persistance);
		for (auto x = 0; x < info.size.x; ++x) {
			for (auto y = 0; y < info.size.y; ++y) {
				auto tile = V2_int{ x, y };
				int index = x + y * info.size.x;
				auto& entity = GetEntity(tile);
				V2_double tile_position = tile * tile_size;
				V2_double absolute_tile_position = tile_position + info.position;
				auto tile_seed = ((int)absolute_tile_position.x & 0xFFFF) << 16 | ((int)absolute_tile_position.y & 0xFFFF);
				//auto noise = rng.RandomDouble(0.0, 1.0);
				auto noise = (simplex.fractal(octave, absolute_tile_position.x, absolute_tile_position.y) + 1.0f) / 2.0f;
				int pixel = (int)(noise * 3.0f);
				entity = CreateBox(entity, absolute_tile_position, tile_size, "./resources/textures/tree.png");
				auto& color = entity.GetComponent<RenderComponent>().color;
				switch (pixel) {
					case 0: { color = engine::GOLD; break; }
					case 1: { color = engine::ORANGE; break; }
					case 2: { color = engine::RED; break; }
					case 3: { color = engine::DARK_RED; break; }
					default: { LOG("Noise: " << noise << ", Pixel: " << pixel); assert(!"Noise value out of range"); break; }
				}
			}
		}














		/*auto seeder = info.position / info.size;
		auto grid_seed = ((int)seeder.x & 0xFFFF) << 16 | ((int)seeder.y & 0xFFFF);

		const siv::PerlinNoise perlin(grid_seed * seed);
		const double fx = info.size.x / bias;
		const double fy = info.size.y / bias;

		for (std::int32_t y = 0; y < info.size.y; ++y) {
			for (std::int32_t x = 0; x < info.size.x; ++x) {
				auto tile = V2_int{ x, y };
				auto& entity = GetEntity(tile);
				V2_double tile_position = tile * tile_size;
				V2_double absolute_tile_position = tile_position + info.position;
				entity = CreateBox(entity, absolute_tile_position, tile_size, "./resources/textures/tree.png");
				auto& color = entity.GetComponent<RenderComponent>().color;


				auto noise = perlin.accumulatedOctaveNoise2D_0_1(x / fx, y / fy, octave);
				int pixel = (int)(noise * 3.0);

				switch (pixel) {
					case 0: { color = engine::GOLD; break; }
					case 1: { color = engine::ORANGE; break; }
					case 2: { color = engine::RED; break; }
					case 3: { color = engine::DARK_RED; break; }
					default: { LOG("Noise: " << noise << ", Pixel: " << pixel); assert(!"Noise value out of range"); break; }
				}
			}
		}*/





		//engine::RNG rng;
		//engine::PerlinNoise perlin(info, tile_size);
		//perlin.PerlinNoise2D(perlin.nOutputWidth, perlin.nOutputHeight, perlin.fNoiseSeed2D, octave, (float)bias, perlin.fPerlinNoise2D);
		//for (auto x = 0; x < info.size.x; ++x) {
		//	for (auto y = 0; y < info.size.y; ++y) {
		//		auto tile = V2_int{ x, y };
		//		int index = x + y * info.size.x;
		//		auto& entity = GetEntity(tile);
		//		V2_double tile_position = tile * tile_size;
		//		V2_double absolute_tile_position = tile_position + info.position;
		//		auto tile_seed = ((int)absolute_tile_position.x & 0xFFFF) << 16 | ((int)absolute_tile_position.y & 0xFFFF);
		//		rng.SetSeed(tile_seed);
		//		//auto noise = rng.RandomDouble(0.0, 1.0);
		//		auto noise = perlin.fPerlinNoise2D[index];
		//		int pixel = (int)(noise * 3.0);
		//		entity = CreateBox(entity, absolute_tile_position, tile_size, "./resources/textures/tree.png");
		//		auto& color = entity.GetComponent<RenderComponent>().color;
		//		switch (pixel) {
		//			case 0: { color = engine::GOLD; break; }
		//			case 1: { color = engine::ORANGE; break; }
		//			case 2: { color = engine::RED; break; }
		//			case 3: { color = engine::DARK_RED; break; }
		//			default: { LOG("Noise: " << noise << ", Pixel: " << pixel); assert(!"Noise value out of range"); break; }
		//		}
		//	}
		//}










		//engine::RNG rng;
		//auto chunk_tile = static_cast<V2_int>(info.position) / tile_size;
		////perlin = engine::PerlinNoise(tile_count, rng, chunk_tile, seed);
		////perlin.Generate2D(octave, bias);
		//auto tile_seed = (chunk_tile.x & 0xFFFF) << 16 | (chunk_tile.y & 0xFFFF);
		//const siv::PerlinNoise perlin(tile_seed * seed);
		//V2_int tile = { 0, 0 };
		//const double fx = tile_count.x / bias;
		//const double fy = tile_count.y / bias;
		//for (tile.x = 0; tile.x < tile_count.x; ++tile.x) {
		//	for (tile.y = 0; tile.y < tile_count.y; ++tile.y) {
		//		auto absolute_tile_position = chunk_tile + tile;
		//		auto absolute_position = absolute_tile_position * tile_size;
		//		auto& entity = GetEntity(tile);
		//		auto tile_seed = (absolute_tile_position.x & 0xFFFF) << 16 | (absolute_tile_position.y & 0xFFFF);
		//		//rng.SetSeed(tile_seed * seed);
		//		//double noise = perlin.GetNoise2D(tile);
		//		double noise = perlin.accumulatedOctaveNoise2D_0_1(tile.x / fx, tile.y / fy, octave);
		//		int pixel_bw = (int)(noise * 3);
		//		switch (pixel_bw) {
		//			case 0: { entity = CreateBox(entity, absolute_position, tile_size, "./resources/textures/tree.png"); break; }
		//			case 1: { entity = CreateBox(entity, absolute_position, tile_size, "./resources/textures/box.png"); break; }
		//			case 2: { entity = CreateBox(entity, absolute_position, tile_size, "./resources/textures/box2.png"); break; }
		//			default: { entity = CreateBox(entity, absolute_position, tile_size, "./resources/textures/box3.png"); break; }
		//		}
		//	}
		//}
	}
};