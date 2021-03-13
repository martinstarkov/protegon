#pragma once

#include <engine/Include.h>

#include "factory/Factories.h"

class BoxChunk : public engine::Chunk {
public:
	virtual void Generate(int seed, int octave, double bias) override final {
		engine::ValueNoise noise(256, seed);

		auto position{ (info.position / static_cast<V2_double>(tile_size)) / info.size };

		auto noiseMap{ std::move(noise.GenerateNoiseMap(position, info.size, octave, static_cast<float>(bias))) };

		for (unsigned j{ 0 }; j < info.size.y; ++j) {
			auto x{ j * (int)info.size.x };
			for (unsigned i{ 0 }; i < info.size.x; ++i) {
				// generate a float in the range [0:1]
				auto raw_noise = noiseMap[x + i];
				auto noise{ engine::math::Clamp<std::uint8_t>(raw_noise * 255.0f, 0.0f, 255.0f) };
				V2_int tile{ (int)i, (int)j };
				V2_double tile_position{ tile * tile_size };
				V2_double absolute_tile_position{ tile_position + info.position };
				auto& entity{ GetEntity(tile) };
				int pixel{ engine::math::Clamp<int>(raw_noise * 4.0f, 0.0f, 3.0f) };
				switch (pixel) {
					case 0:
					case 1:
						break;
					case 2: {
						CreateBox(entity, absolute_tile_position, tile_size, "./resources/textures/tree.png", engine::RED);
						auto& color{ entity.AddComponent<RenderComponent>().color };
						color = engine::RED;
						entity.AddComponent<CollisionComponent>(absolute_tile_position, tile_size);
						entity.GetComponent<RenderComponent>().original_color = color;
						break; 
					}
					case 3: {
						CreateBox(entity, absolute_tile_position, tile_size, "./resources/textures/tree.png", engine::RED);
						auto& color{ entity.AddComponent<RenderComponent>().color };
						color = engine::PURPLE;
						entity.AddComponent<CollisionComponent>(absolute_tile_position, tile_size);
						entity.GetComponent<RenderComponent>().original_color = color;
						break; 
					}
					default: {
						LOG("Noise: " << noise << ", Pixel: " << pixel);
						assert(!"Noise value out of range"); 
						break; 
					}
				}
			}
		}
	}
};