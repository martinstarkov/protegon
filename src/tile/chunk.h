#pragma once

#include <functional>
#include <unordered_map>
#include <utility>

#include "ecs/ecs.h"
#include "math/noise.h"
#include "math/vector2.h"
#include "renderer/renderer.h"
#include "scene/camera.h"

namespace ptgn {

class Chunk {
public:
	// TODO: Add serialization.
	std::vector<ecs::Entity> entities;
};

class ChunkManager {
public:
	void Update(const Camera& camera) {
		auto cam_rect{ camera.GetVertices() };

		V2_float padding{ 0, 0 };

		V2_int min{ (cam_rect[0] - padding) / (tile_size * chunk_size) - V2_int{ 1 } };
		V2_int max{ (cam_rect[2] + padding) / (tile_size * chunk_size) + V2_int{ 1 } };

		// TODO: Remove and add chunks based on new movement by storing previous min and max and
		// comparing with current.

		for (int i{ min.x }; i < max.x; i++) {
			for (int j{ min.y }; j < max.y; j++) {
				V2_int p{ i, j };
				DrawDebugRect(
					p * chunk_size * tile_size, chunk_size * tile_size, color::Red, Origin::TopLeft,
					2.0f
				);
			}
		}
	}

	// TODO: When serializing, store the chunks in binary format such that:
	// 0th byte -> 32th byte is filled with zeroes. These zeroes will later be written as the
	// location of the chunk serialization map (which related chunk coordinates to their stream
	// position. The chunk serialization map is added after serializing each chunk.
	// std::unordered_map<V2_float, std::size_t> stream_positions;
	// std::size_t stream_map_position{ 0 }; // TODO: Set this to be where stream_positions was
	// written in the binary file.

	std::unordered_map<V2_float, Chunk> chunks;
	// TODO: Allow for different noise layers for generating chunk entities. Loop through the noise
	// layers when populating a chunk with entities.
	std::vector<std::pair<FractalNoise, std::function<void(float)>>> noise_layers;
	V2_float tile_size{ 64, 64 };
	V2_float chunk_size{ 8, 8 };
};

} // namespace ptgn