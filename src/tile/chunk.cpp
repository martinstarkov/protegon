#include "tile/chunk.h"

#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "scene/camera.h"

namespace ptgn {

Chunk::Chunk(const std::vector<Entity>& entities) : entities{ entities } {}

Chunk::Chunk(Chunk&& other) noexcept : entities{ std::exchange(other.entities, {}) } {}

Chunk& Chunk::operator=(Chunk&& other) noexcept {
	if (this != &other) {
		entities = std::exchange(other.entities, {});
	}
	return *this;
}

Chunk::~Chunk() {
	for (auto& entity : entities) {
		entity.Destroy();
	}
}

Entity NoiseLayer::GetEntity(const V2_float& tile_coordinate, const V2_float& tile_size) const {
	if (callback == nullptr) {
		return {};
	}
	float noise_value{ noise.Get(tile_coordinate.x, tile_coordinate.y) };
	V2_float coordinate{ tile_coordinate * tile_size };
	return std::invoke(callback, coordinate, noise_value);
}

ChunkManager::ChunkManager(ChunkManager&& other) noexcept :
	chunks{ std::exchange(other.chunks, {}) },
	noise_layers{ std::exchange(other.noise_layers, {}) },
	tile_size{ std::exchange(other.tile_size, {}) },
	chunk_size{ std::exchange(other.chunk_size, {}) } {}

ChunkManager& ChunkManager::operator=(ChunkManager&& other) noexcept {
	if (this != &other) {
		chunks		 = std::exchange(other.chunks, {});
		noise_layers = std::exchange(other.noise_layers, {});
		tile_size	 = std::exchange(other.tile_size, {});
		chunk_size	 = std::exchange(other.chunk_size, {});
	}
	return *this;
}

ChunkManager::~ChunkManager() {
	/*
	FileStreamWriter chunk_file{ "resources/chunks.bin" };
	FileStreamWriter coordinate_file{ "resources/coordinates.bin" };
	for (const auto& [coordinate, chunk] : chunks) {
		// TODO: Figure out when and how a chunk should be serialized.
	}
	*/
}

void ChunkManager::Update(const Camera& camera) {
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
				// Newly visible chunk.
				auto entities{ GenerateEntities(chunk_coordinate) };
				chunks.emplace(chunk_coordinate, entities);
			}
		}
	}

	std::vector<V2_int> chunks_to_unload;

	for (const auto& [coordinate, chunk] : chunks) {
		if (visible_chunks.find(coordinate) == visible_chunks.end()) {
			// Chunk no longer visible.
			chunks_to_unload.emplace_back(coordinate);
		}
	}

	for (const auto& coordinate : chunks_to_unload) {
		// PTGN_LOG("Unloading chunk: ", coordinate);
		chunks.erase(coordinate);
	}

	// Debug draw chunk borders.
	/*for (const auto& [coordinate, chunk] : chunks) {
		DrawDebugRect(
			coordinate * chunk_size * tile_size, chunk_size * tile_size, color::Red,
			Origin::TopLeft, 2.0f
		);
	}*/
}

[[nodiscard]] std::vector<Entity> ChunkManager::GenerateEntities(
	const V2_int& chunk_coordinate
) const {
	std::vector<Entity> entities;
	for (const auto& layer : noise_layers) {
		for (int i{ 0 }; i < chunk_size.x; i++) {
			for (int j{ 0 }; j < chunk_size.y; j++) {
				auto tile_coordinate{ chunk_coordinate * chunk_size + V2_int{ i, j } };
				auto entity{ layer.GetEntity(tile_coordinate, tile_size) };
				if (!entity) {
					continue;
				}
				entities.emplace_back(entity);
			}
		}
	}
	return entities;
}

} // namespace ptgn