#pragma once

#include <functional>
#include <unordered_map>
#include <utility>

#include "ecs/ecs.h"
#include "math/collision/overlap.h"
#include "math/noise.h"
#include "math/vector2.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "tile/grid.h"

namespace ptgn {

class Chunk {
public:
	Chunk(const std::vector<ecs::Entity>& entities) : entities{ entities } {}

	~Chunk() {
		for (auto& entity : entities) {
			entity.Destroy();
		}
	}

	// TODO: Add serialization.
	std::vector<ecs::Entity> entities;
};

class ChunkManager {
public:
	void Update(const Camera& camera) {
		auto cam_rect{ camera.GetVertices() };

		V2_float chunk_pixel_size{ tile_size * chunk_size };

		// Number of additional chunks on each side that are loaded past the camera view rectangle.
		V2_int chunk_padding{ 1, 1 };

		// TODO: Consider accounting for rotation?
		V2_int min{ cam_rect[0] / chunk_pixel_size - V2_int{ 1 } - chunk_padding };
		V2_int max{ cam_rect[2] / chunk_pixel_size + V2_int{ 1 } + chunk_padding };

		min.x = std::min(min.x, max.x);
		min.y = std::min(min.y, max.y);
		max.x = std::max(min.x, max.x);
		max.y = std::max(min.y, max.y);

		PTGN_ASSERT(min.x <= max.x, "Invalid camera rectangle chunk extents");
		PTGN_ASSERT(min.y <= max.y, "Invalid camera rectangle chunk extents");

		std::unordered_set<V2_int> visible_chunks;

		for (int i{ min.x }; i < max.x; i++) {
			for (int j{ min.y }; j < max.y; j++) {
				V2_int chunk_coordinate{ i, j };
				visible_chunks.emplace(chunk_coordinate);

				if (auto it{ chunks.find(chunk_coordinate) }; it == chunks.end()) {
					// PTGN_LOG("Loading chunk: ", chunk_coordinate);
					auto entities{ GetEntities(chunk_coordinate) };
					chunks.emplace(chunk_coordinate, entities);
				}
			}
		}

		std::vector<V2_int> chunks_to_unload;

		for (const auto& [coordinate, chunk] : chunks) {
			// Chunk no longer visible.
			if (visible_chunks.find(coordinate) == visible_chunks.end()) {
				chunks_to_unload.emplace_back(coordinate);
			}
		}

		for (const auto& coordinate : chunks_to_unload) {
			// PTGN_LOG("Unloading chunk: ", coordinate);
			chunks.erase(coordinate);
		}

		for (const auto& [coordinate, chunk] : chunks) {
			DrawDebugRect(
				coordinate * chunk_size * tile_size, chunk_size * tile_size, color::Red,
				Origin::TopLeft, 2.0f
			);
		}
	}

	// TODO: When serializing, store the chunks in binary format such that:
	// 0th byte -> 32th byte is filled with zeroes. These zeroes will later be written as the
	// location of the chunk serialization map (which related chunk coordinates to their stream
	// position. The chunk serialization map is added after serializing each chunk.
	// std::unordered_map<V2_float, std::size_t> stream_positions;
	// std::size_t stream_map_position{ 0 }; // TODO: Set this to be where stream_positions was
	// written in the binary file.

	std::unordered_map<V2_int, Chunk> chunks;
	// TODO: Allow for different noise layers for generating chunk entities. Loop through the noise
	// layers when populating a chunk with entities.

	// std::function<Entity(coordinate, noise value)>
	std::vector<std::pair<FractalNoise, std::function<ecs::Entity(V2_float, float)>>> noise_layers;
	V2_float tile_size{ 64, 64 };
	V2_float chunk_size{ 8, 8 };

private:
	[[nodiscard]] std::vector<ecs::Entity> GetEntities(const V2_int& chunk_coordinate) const {
		std::vector<ecs::Entity> entities;
		for (auto [noise, func] : noise_layers) {
			for (int i{ 0 }; i < chunk_size.x; i++) {
				for (int j{ 0 }; j < chunk_size.y; j++) {
					V2_int local_tile_coordinate{ i, j };
					V2_float tile_coordinate{ chunk_coordinate * chunk_size +
											  local_tile_coordinate };
					float noise_value{ noise.Get(tile_coordinate.x, tile_coordinate.y) };
					if (func == nullptr) {
						continue;
					}
					V2_float coordinate{ tile_coordinate * tile_size };
					auto entity{ std::invoke(func, coordinate, noise_value) };
					if (entity == ecs::Entity{}) {
						continue;
					}
					entities.emplace_back(entity);
				}
			}
		}
		return entities;
	}
};

} // namespace ptgn