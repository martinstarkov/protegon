#pragma once

#include <engine/Include.h>

#include "factory/Factories.h"

enum class TileType {
	NONE,
	T_IRON,
	T_SILVER
};

struct Tile {
	TileType type;
	float relative_probability;
};

inline TileType SelectTile(float probability, const std::vector<Tile>& tiles) {
	float cumulative{ 0.0 };
	for (auto& tile : tiles) {
		cumulative += tile.relative_probability;
		if (probability <= cumulative) {
			return tile.type;
		}
	}
	return TileType::NONE;
}

class BoxChunk : public engine::Chunk {
public:
	virtual void InitBackground(const engine::ValueNoise<float>& noise) override final {
		engine::Timer timer;
		timer.Start();
		background_texture_ = engine::Texture{ engine::Engine::GetRenderer(), info_.size, engine::PixelFormat::ARGB8888, engine::TextureAccess::STREAMING };

		void* pixels{ nullptr };
		int pitch{ 0 };

		bool locked{ background_texture_.Lock(&pixels, &pitch) };
		assert(locked && "Could not lock chunk background texture when initializing");

		auto relative_position{ info_.position / tile_size_ };

		std::size_t octaves{ 5 }; // Number of layers of noise.
		float frequency{ 0.03f }; // Initial frequency of layers.
		float lacunarity{ 2.0f }; // Layer multiplier on frequency.
		float persistence{ 0.8f }; // Multiplier on amplitude.

		auto noise_map{ std::move(noise.GenerateNoiseMap(relative_position, octaves, frequency, lacunarity, persistence)) };

		for (auto y{ 0 }; y < info_.size.y; ++y) {
			int index{ y * static_cast<int>(info_.size.x) };
			for (auto x{ 0 }; x < info_.size.x; ++x) {
				V2_int tile{ x, y };
				auto tile_position{ tile * tile_size_ };
				auto raw_noise{ noise_map[index + x] };
				int pixel_noise{ engine::math::Clamp<int>(raw_noise * 3.0f, 0.0f, 3.0f) };
				engine::Color color{ engine::BLACK };
				switch (pixel_noise) {
					case 0:
						color = { 193,68,14, 255 };
						break;
					case 1:
						color = { 231,125,17, 255 };
						break;
					case 2:
						color = { 253,166,0, 255 };
						break;
					default:
						assert(!"Noise for background generation out of range");
				}
				auto& pixel{ engine::TextureManager::GetTexturePixel(pixels, pitch, tile) };
				pixel = (0xFF000000 | (color.r << 16) | (color.g << 8) | color.b);
				/*for (auto row{ 0 }; row < tile_size_.y; ++row) {
					for (auto col{ 0 }; col < tile_size_.x; ++col) {
						auto pos{ tile_position + V2_int{ col, row } };
						auto& pixel{ engine::TextureManager::GetTexturePixel(pixels, pitch, pos) };
						pixel = (0xFF000000 | (color.r << 16) | (color.g << 8) | color.b);
					}
				}*/
			}
		}

		background_texture_.Unlock();

		LOG("InitBoxChunkBackground: " << timer.ElapsedSeconds());
	}
	virtual void Generate(const engine::ValueNoise<float>& noise) override final {

		auto relative_position{ info_.position / tile_size_ };

		std::size_t octaves{ 5 }; // Number of layers of noise.
		float frequency{ 0.05f }; // Initial frequency of layers.
		float lacunarity{ 2.0f }; // Layer multiplier on frequency.
		float persistence{ 0.4f }; // Multiplier on amplitude.

		auto noise_map{ std::move(noise.GenerateNoiseMap(relative_position, octaves, frequency, lacunarity, persistence)) };

		std::vector<Tile> tiles{ { TileType::T_SILVER, 0.1f }, { TileType::T_IRON, 0.15f } };

		for (std::size_t y{ 0 }; y < info_.size.y; ++y) {
			std::size_t index{ y * static_cast<std::size_t>(info_.size.x) };
			for (std::size_t x{ 0 }; x < info_.size.x; ++x) {
				// Noise in the range [0 -> 1].
				auto p{ noise_map[index + x] };
				// Convert noise to RGB range.
				V2_int tile{ x, y };
				auto& entity{ GetEntity(tile) };
				V2_double absolute_position{ (tile + relative_position) * tile_size_ };
				TileType type{ SelectTile(p, tiles) };
				switch (type) {
					case TileType::NONE:
						break;
					case TileType::T_IRON: {
						CreateBox(entity, absolute_position, tile_size_, "./resources/textures/tree.png", {});
						auto& color{ entity.AddComponent<RenderComponent>().color };
						color = { 69,24,4, 255 };
						entity.AddComponent<CollisionComponent>(absolute_position, tile_size_);
						entity.GetComponent<RenderComponent>().original_color = color;
						break;
					}
					case TileType::T_SILVER: {
						CreateBox(entity, absolute_position, tile_size_, "./resources/textures/tree.png", {});
						auto& color{ entity.AddComponent<RenderComponent>().color };
						color = { 240,231,231, 255 };
						entity.AddComponent<CollisionComponent>(absolute_position, tile_size_);
						entity.GetComponent<RenderComponent>().original_color = color;
						break;
					}
					default:
						assert(!"Probability out of range");
						break;
				}
			}
		}
	}
};