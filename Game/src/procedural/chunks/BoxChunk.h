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

		for (unsigned l = 0; l < numLayers; ++l) {
			maxPossibleNoiseVal += amplitude;
			amplitude *= amplitudeMult;
		}

		//LOG("maxPossibleNoiseVal: " << maxPossibleNoiseVal);

		engine::ValueNoise noise(256, seed);

		unsigned imageWidth = (unsigned)info.size.x;
		unsigned imageHeight = (unsigned)info.size.y;

		auto overall = info.position / static_cast<V2_double>(tile_size) / info.size;

		float* noiseMap = new float[imageWidth * imageHeight]{ 0 };

		// FRACTAL NOISE
		float frequency = 0.05f;//0.02f;
		float frequencyMult = (float)bias;//1.8;
		float maxNoiseVal = 0;
		for (unsigned j = 0; j < imageHeight; ++j) {
			for (unsigned i = 0; i < imageWidth; ++i) {
				engine::Vec2f pNoise = engine::Vec2f((float)overall.x * imageWidth + i, (float)overall.y * imageHeight + j) * frequency;
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
				auto raw_noise = noiseMap[j * imageWidth + i];
				auto noise = static_cast<unsigned char>(engine::math::Clamp(raw_noise * 255, 0.0f, 255.0f));
				//auto noise = noiseMap[j * imageWidth + i];
				//engine::TextureManager::DrawPoint(, engine::Color(a, 0, 0, 255));
				auto tile = V2_int{ (int)i, (int)j };
				V2_double tile_position = tile * tile_size;
				V2_double absolute_tile_position = tile_position + info.position;
				auto& entity = GetEntity(tile);
				CreateBox(entity, absolute_tile_position, tile_size, "./resources/textures/tree.png", engine::RED);
				auto& color = entity.AddComponent<RenderComponent>().color;
				//color.r = noise;
				int pixel = (int)engine::math::Clamp(raw_noise * 4.0f, 0.0f, 3.0f);
				switch (pixel) {
					case 0: { 
						color = engine::GOLD;
						break; 
					}
					case 1: { 
						color = engine::ORANGE;
						break; 
					}
					case 2: {
						color = engine::RED;
						entity.AddComponent<CollisionComponent>(absolute_tile_position, tile_size);
						break; 
					}
					case 3: {
						color = engine::PURPLE;
						entity.AddComponent<CollisionComponent>(absolute_tile_position, tile_size);
						break; 
					}
					default: {
						LOG("Noise: " << noise << ", Pixel: " << pixel);
						assert(!"Noise value out of range"); 
						break; 
					}
				}
				entity.GetComponent<RenderComponent>().original_color = color;
			}
		}

		delete[] noiseMap;
	}
};