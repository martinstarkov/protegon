#pragma once

#include <engine/Include.h>

#include "factory/Factories.h"

class BoxChunk : public engine::Chunk {
public:
	virtual void Generate(engine::ValueNoise<float>& noise2) override final {

		//engine::ValueNoise<float> noise{ static_cast<V2_int>(info.size) };
		auto relative_position{ info.position / tile_size };

		std::size_t octaves{ 5 }; // Number of layers of noise.
		float frequency{ 0.05f }; // Initial frequency of layers.
		float lacunarity{ 2.0f }; // Layer multiplier on frequency.
		float persistence{ 0.4f }; // Multiplier on amplitude.

		auto noise_map{ std::move(noise2.GenerateNoiseMap(relative_position, octaves, frequency, lacunarity, persistence)) };

		for (std::size_t y{ 0 }; y < info.size.y; ++y) {
			std::size_t index{ y * static_cast<std::size_t>(info.size.x) };
			for (std::size_t x{ 0 }; x < info.size.x; ++x) {
				// Noise in the range [0 -> 1].
				auto raw_noise{ noise_map[index + x] };
				// Convert noise to RGB range.
				V2_int tile{ x, y };
				auto& entity{ GetEntity(tile) };
				V2_double absolute_position{ (tile + relative_position) * tile_size };
				int pixel_noise{ engine::math::Clamp<int>(raw_noise * 4.0f, 0.0f, 3.0f) };
				switch (pixel_noise) {
					case 0:
					case 1:
						break;
					case 2: {
						CreateBox(entity, absolute_position, tile_size, "./resources/textures/tree.png", engine::RED);
						auto& color{ entity.AddComponent<RenderComponent>().color };
						color = engine::RED;
						entity.AddComponent<CollisionComponent>(absolute_position, tile_size);
						entity.GetComponent<RenderComponent>().original_color = color;
						break; 
					}
					case 3: {
						CreateBox(entity, absolute_position, tile_size, "./resources/textures/tree.png", engine::RED);
						auto& color{ entity.AddComponent<RenderComponent>().color };
						color = engine::PURPLE;
						entity.AddComponent<CollisionComponent>(absolute_position, tile_size);
						entity.GetComponent<RenderComponent>().original_color = color;
						break; 
					}
					default: {
						assert(!"Pixel noise value out of range"); 
						break; 
					}
				}
			}
		}
	}
};