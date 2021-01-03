#include "Chunk.h"

#include "utils/RNG.h"

#include "ecs/Components.h"

#include "ecs/systems/RenderSystem.h"

namespace engine {

void Chunk::Init(AABB chunk_info, V2_int tile_size, Scene* scene) {
	manager.AddSystem<RenderSystem>(scene);
	info = chunk_info;
	this->tile_size = tile_size;
	tile_count.x = static_cast<int>(info.size.x) / tile_size.x;
	tile_count.y = static_cast<int>(info.size.y) / tile_size.y;
	auto count = tile_count.x * tile_count.y;
	// Generate new empty grid.
	if (grid.size() != count) {
		grid.resize(count, ecs::null);
		for (size_t i = 0; i < count; i++) {
			grid[i] = manager.CreateEntity();
		}
	}
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
	manager.DestroyEntities();
}

std::size_t Chunk::GetIndex(V2_int relative_coordinate) const {
	assert(relative_coordinate.x < tile_count.x && "X coordinate out of range of chunk grid");
	assert(relative_coordinate.y < tile_count.y && "Y coordinate out of range of chunk grid");
	auto index = relative_coordinate.x + relative_coordinate.y * static_cast<int>(tile_count.x);
	assert(index < grid.size() && "Index out of range of chunk grid");
	return index;
}

} // namespace engine