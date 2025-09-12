#include "tile/chunk.h"

#include <algorithm>
#include <array>
#include <functional>
#include <list>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "debug/debug_system.h"
#include "math/noise.h"
#include "math/vector2.h"
#include "nlohmann/json.hpp"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "serialization/fwd.h"

namespace ptgn {

Chunk::Chunk(const std::vector<Entity>& chunk_entities) : entities{ chunk_entities } {}

Chunk::Chunk(const json& j, Manager& manager) {
	Deserialize(j, manager);
}

Chunk::Chunk(Chunk&& other) noexcept : entities{ std::exchange(other.entities, {}) } {}

Chunk& Chunk::operator=(Chunk&& other) noexcept {
	if (this != &other) {
		entities = std::exchange(other.entities, {});
	}
	return *this;
}

bool Chunk::HasChanged() const {
	return has_changed_;
}

void Chunk::FlagAsChanged(bool changed) {
	has_changed_ = changed;
}

json Chunk::Serialize() const {
	json j{};

	for (const auto& entity : entities) {
		j["entities"].emplace_back(entity.Serialize());
	}

	return j;
}

void Chunk::Deserialize(const json& j, Manager& manager) {
	PTGN_ASSERT(entities.empty());
	const auto& json_entities{ j["entities"] };
	entities.reserve(json_entities.size());
	for (const auto& entity : json_entities) {
		PTGN_ASSERT(entity != json{});
		auto e{ manager.CreateEntity() };
		manager.Refresh();
		e.Deserialize(entity);
		entities.emplace_back(e);
	}
}

Chunk::~Chunk() {
	for (auto& entity : entities) {
		entity.Destroy();
	}
}

Entity NoiseLayer::GetEntity(const V2_int& tile_coordinate, const V2_int& tile_size) const {
	if (!callback) {
		return {};
	}
	float noise_value{
		noise.Get(static_cast<float>(tile_coordinate.x), static_cast<float>(tile_coordinate.y))
	};
	auto coordinate{ tile_coordinate * tile_size };
	return callback(coordinate, noise_value);
}

ChunkManager::ChunkManager(ChunkManager&& other) noexcept :
	chunks{ std::exchange(other.chunks, {}) },
	tile_size{ std::exchange(other.tile_size, {}) },
	chunk_size{ std::exchange(other.chunk_size, {}) },
	noise_layers_{ std::exchange(other.noise_layers_, {}) } {}

ChunkManager& ChunkManager::operator=(ChunkManager&& other) noexcept {
	if (this != &other) {
		chunks		  = std::exchange(other.chunks, {});
		tile_size	  = std::exchange(other.tile_size, {});
		chunk_size	  = std::exchange(other.chunk_size, {});
		noise_layers_ = std::exchange(other.noise_layers_, {});
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

void ChunkManager::GetBounds(
	V2_int& out_min, V2_int& out_max, const Camera& camera, const V2_int& chunk_padding
) const {
	auto cam_rect{ camera.GetWorldVertices() };

	auto chunk_pixel_size{ tile_size * chunk_size };

	// TODO: Consider accounting for rotation?
	V2_int min{ cam_rect[0] / chunk_pixel_size - V2_int{ 1 } - chunk_padding };
	V2_int max{ cam_rect[2] / chunk_pixel_size + V2_int{ 1 } + chunk_padding };

	out_min.x = std::min(min.x, max.x);
	out_min.y = std::min(min.y, max.y);
	out_max.x = std::max(min.x, max.x);
	out_max.y = std::max(min.y, max.y);

	PTGN_ASSERT(out_min.x <= out_max.x, "Invalid camera rectangle chunk extents");
	PTGN_ASSERT(out_min.y <= out_max.y, "Invalid camera rectangle chunk extents");
}

void ChunkManager::Update(Manager& manager, const Camera& camera) {
	V2_int chunk_padding{ 1, 1 };

	V2_int min;
	V2_int max;

	GetBounds(min, max, camera, chunk_padding);

	if (min == previous_min_ && max == previous_max_) {
		return;
	}

	previous_min_ = min;
	previous_max_ = max;

	std::unordered_set<V2_int> visible_chunks;

	for (int i{ min.x }; i < max.x; i++) {
		for (int j{ min.y }; j < max.y; j++) {
			V2_int coordinate{ i, j };
			visible_chunks.emplace(coordinate);

			if (auto it{ chunks.find(coordinate) }; it != chunks.end()) {
				continue;
			}

			if (auto cache_it{ chunk_cache.find(coordinate) }; cache_it != chunk_cache.end()) {
				chunks.try_emplace(coordinate, cache_it->second, manager);
				continue;
			}

			// PTGN_LOG("Loading chunk for the first time: ", coordinate);
			// Newly visible chunk.
			auto entities{ GenerateEntities(coordinate) };
			[[maybe_unused]] auto [new_it, _] = chunks.try_emplace(coordinate, entities);
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
		auto it{ chunks.find(coordinate) };
		PTGN_ASSERT(it != chunks.end());
		if (const auto& chunk{ it->second }; chunk.HasChanged()) {
			chunk_cache[coordinate] = chunk.Serialize();
		}
		chunks.erase(coordinate);
	}

	chunks.rehash(0);

	/*for (auto [coord, json_cache] : chunk_cache) {
		PTGN_LOG(json_cache.dump(4));
	}*/

	manager.Refresh();

	// DrawDebugChunkBorders();
}

void ChunkManager::AddNoiseLayer(const NoiseLayer& noise_layer) {
	noise_layers_.emplace_back(noise_layer);
}

void ChunkManager::DrawDebugChunkBorders() const {
	for (const auto& [coordinate, chunk] : chunks) {
		game.debug.DrawShape(
			{ coordinate * chunk_size * tile_size }, Rect{ chunk_size * tile_size }, color::Red,
			2.0f, Origin::TopLeft
		);
	}
}

[[nodiscard]] std::vector<Entity> ChunkManager::GenerateEntities(const V2_int& chunk_coordinate
) const {
	std::vector<Entity> entities;
	for (const auto& layer : noise_layers_) {
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