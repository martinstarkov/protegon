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
	std::vector<ecs::Entity> entities;
};

class ChunkManager {
public:
	void Update(const Camera& camera) {
		auto cam_rect{ camera.GetVertices() };

		V2_float padding{ 0, 0 };

		V2_int min{ (cam_rect[0] - padding) / (tile_size * chunk_size) - V2_int{ 1 } };
		V2_int max{ (cam_rect[2] + padding) / (tile_size * chunk_size) + V2_int{ 1 } };

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

	std::unordered_map<V2_float, Chunk> chunks;
	std::vector<std::pair<FractalNoise, std::function<void(float)>>> noise_layers;
	V2_float tile_size{ 64, 64 };
	V2_float chunk_size{ 8, 8 };
};

} // namespace ptgn