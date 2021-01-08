#pragma once

#include <engine/Include.h>

#include "factory/Factories.h"

class BoxChunk : public engine::Chunk {
public:
	virtual void Generate(int seed, int octave, double bias) override final {


		auto tiles = 2;

		float amplitude = 1;
		float maxPossibleNoiseVal = 0;
		float amplitudeMult = 0.5f;//0.35;
		unsigned numLayers = octave;//5;

		for (unsigned l = 0; l < octave; ++l) {
			maxPossibleNoiseVal += amplitude;
			amplitude *= amplitudeMult;
		}

		//LOG("maxPossibleNoiseVal: " << maxPossibleNoiseVal);

		engine::ValueNoise noise(256, seed);

		unsigned imageWidth = 16;
		unsigned imageHeight = 16;

		auto overall = info.position / static_cast<V2_double>(tile_size) / info.size;

		float* noiseMap = new float[imageWidth * imageHeight]{ 0 };

		// FRACTAL NOISE
		float frequency = 0.05f;//0.02f;
		float frequencyMult = bias;//1.8;
		float maxNoiseVal = 0;
		for (unsigned j = 0; j < imageHeight; ++j) {
			for (unsigned i = 0; i < imageWidth; ++i) {
				engine::Vec2f pNoise = engine::Vec2f(overall.x * imageWidth + i, overall.y * imageHeight + j) * frequency;
				amplitude = 1;
				for (unsigned l = 0; l < numLayers; ++l) {
					//LOG("pNoise: " << pNoise.x << "," << pNoise.y);
					auto fractal_noise = noise.eval(pNoise);
					auto value = fractal_noise * amplitude;
					if (std::isnan(value) && l > 10) value = 0;
					bool assertion = value >= 0;
					if (!assertion) {
						LOG("fractal_noise: " << value << ", pNoise: (" << pNoise.x << "," << pNoise.y << "), octave: " << l << ", amplitude: " << amplitude);
					}
					assert(assertion && "fractal_noise must be above or equal to 0");
					noiseMap[j * imageWidth + i] += value;
					pNoise *= frequencyMult;
					amplitude *= amplitudeMult;
				}
				if (noiseMap[j * imageWidth + i] > maxNoiseVal) maxNoiseVal = noiseMap[j * imageWidth + i];
			}
		}
		//for (unsigned i = 0; i < imageWidth * imageHeight; ++i) noiseMap[i] /= maxNoiseVal;
		for (unsigned i = 0; i < imageWidth * imageHeight; ++i) {
			assert(noiseMap[i] >= 0 && "Noise must be above or equal to 0");
			noiseMap[i] = noiseMap[i] / maxPossibleNoiseVal;
			assert(noiseMap[i] >= 0 && "Noise divided by something which made it negative");
		}

		for (unsigned j = 0; j < imageHeight; ++j) {
			for (unsigned i = 0; i < imageWidth; ++i) {
				// generate a float in the range [0:1]
				//auto noise = static_cast<unsigned char>(engine::math::Clamp(noiseMap[j * imageWidth + i] * 255, 0.0f, 255.0f));
				auto noise = noiseMap[j * imageWidth + i];
				int pixel = engine::math::Clamp(noise * 3.0f, 0.0f, 3.0f);
				//engine::TextureManager::DrawPoint(, engine::Color(a, 0, 0, 255));
				auto tile = V2_int{ (int)i, (int)j };
				auto index = i + j * info.size.x;
				V2_double tile_position = tile * tile_size;
				V2_double absolute_tile_position = tile_position + info.position;
				auto& entity = GetEntity(tile);
				entity = CreateBox(entity, absolute_tile_position, tile_size, "./resources/textures/tree.png");
				auto& color = entity.GetComponent<RenderComponent>().color;
				//color.r = noise;
				switch (pixel) {
					case 0: { color = engine::GOLD; break; }
					case 1: { color = engine::ORANGE; break; }
					case 2: { color = engine::RED; break; }
					case 3: { color = engine::DARK_RED; break; }
					default: { LOG("Noise: " << noise << ", Pixel: " << pixel); assert(!"Noise value out of range"); break; }
				}
			}
		}

		delete[] noiseMap;








		////WORKING VERSION
		//float scale = 400.f;
		//float offset_x = 5.9f;
		//float offset_y = 5.1f;
		//float offset_z = 0.05f;
		//float lacunarity = 1.99f;
		//float persistance = 0.5f;

		////SimplexNoise simplex(float frequency = 1.0f,
		////					 float amplitude = 1.0f,
		////					 float lacunarity = 2.0f,
		////					 float persistence = 0.5f);
		//auto seed_cord = info.position / static_cast<V2_double>(tile_size);
		//auto chunk_seed = ((int)seed_cord.x & 0xFFFF) << 16 | ((int)seed_cord.y & 0xFFFF);


		//engine::CustomNoise simplex(0.1f / scale, 0.5f, lacunarity, persistance, 1);

		//for (auto x = 0; x < info.size.x; ++x) {
		//	for (auto y = 0; y < info.size.y; ++y) {
		//		auto tile = V2_int{ x, y };
		//		int index = x + y * info.size.x;
		//		V2_double tile_position = tile * tile_size;
		//		V2_double absolute_tile_position = tile_position + info.position;
		//		auto tile_seed = ((int)absolute_tile_position.x & 0xFFFF) << 16 | ((int)absolute_tile_position.y & 0xFFFF);
		//		//auto noise = rng.RandomDouble(0.0, 1.0);
		//		auto noise = simplex.fractal(octave, absolute_tile_position.x, absolute_tile_position.y);
		//		int pixel = (int)(noise * 3.0f);
		//		auto& entity = GetEntity(tile);
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