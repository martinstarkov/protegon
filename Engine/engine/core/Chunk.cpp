#include "Chunk.h"

#include "utils/RNG.h"

#include "ecs/Components.h"

#include "ecs/systems/RenderSystem.h"
#include "ecs/systems/HitboxRenderSystem.h"

namespace engine {

void Chunk::Init(AABB chunk_info, V2_int tile_size, Scene* scene) {
	manager.AddSystem<RenderSystem>(scene);
	manager.AddSystem<HitboxRenderSystem>(scene);
	info = chunk_info;
	this->tile_size = tile_size;
	auto count = info.size.x * info.size.y;
	// Generate new empty grid.
	if (grid.size() != count) {
		grid.resize((std::size_t)count, ecs::null);
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
	assert(relative_coordinate.x < info.size.x && "X coordinate out of range of chunk grid");
	assert(relative_coordinate.y < info.size.y && "Y coordinate out of range of chunk grid");
	auto index = relative_coordinate.x + relative_coordinate.y * static_cast<int>(info.size.x);
	assert(index < grid.size() && "Index out of range of chunk grid");
	return index;
}

} // namespace engine