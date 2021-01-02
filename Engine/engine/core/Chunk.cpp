#include "Chunk.h"

#include "utils/RNG.h"

#include "ecs/Components.h"

namespace engine {

Chunk::~Chunk() {
	Unload();
}

void Chunk::Init(AABB chunk_info, V2_int tile_size, ecs::Manager* manager) {
	info = chunk_info;
	this->tile_size = tile_size;
	tile_count.x = static_cast<int>(info.size.x) / tile_size.x;
	tile_count.y = static_cast<int>(info.size.y) / tile_size.y;
	this->manager = manager;
	assert(this->manager != nullptr && "Cannot initialize chunk with null manager");
	auto count = tile_count.x * tile_count.y;
	// Generate new empty grid.
	grid.resize(static_cast<std::size_t>(count), ecs::null);
}

ecs::Entity Chunk::GetEntity(V2_int relative_coordinate) const {
	return grid[GetIndex(relative_coordinate)];
}

ecs::Entity& Chunk::GetEntity(V2_int relative_coordinate) {
	return grid[GetIndex(relative_coordinate)];
}

const AABB& Chunk::GetInfo() const {
	return info;
}

void Chunk::Unload() {
	auto count = tile_count.x * tile_count.y;
	for (auto i = 0; i < count; ++i) {
		grid[i].Destroy();
	}
}

bool Chunk::MatchesPotentialChunk(const AABB& potential_chunk, const V2_int& tile_size, const ecs::Manager* manager) const {
	return potential_chunk == info && tile_size == this->tile_size && manager == this->manager;
}

std::size_t Chunk::GetIndex(V2_int relative_coordinate) const {
	assert(relative_coordinate.x < tile_count.x && "X coordinate out of range of chunk grid");
	assert(relative_coordinate.y < tile_count.y && "Y coordinate out of range of chunk grid");
	auto index = relative_coordinate.x + relative_coordinate.y * static_cast<int>(tile_count.x);
	assert(index < grid.size() && "Index out of range of chunk grid");
	return index;
}

} // namespace engine