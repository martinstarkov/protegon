#include "Chunk.h"

#include "ecs/systems/HitboxRenderSystem.h"

namespace engine {

Chunk::~Chunk() {
	manager_.~Manager();
}

void Chunk::Init(const AABB& chunk_info, const V2_int& tile_size) {
	info_ = chunk_info;
	this->tile_size_ = tile_size;
	auto count = info_.size.x * info_.size.y;
	// Generate new empty grid.
	if (grid_.size() != count) {
		manager_.Reserve((std::size_t)count);
		grid_.resize((std::size_t)count, ecs::null);
		for (size_t i = 0; i < count; i++) {
			grid_[i] = manager_.CreateEntity();
		}
	}
	manager_.Refresh();
	manager_.AddSystem<TileRenderSystem>();
}

const ecs::Entity& Chunk::GetEntity(const V2_int& relative_coordinate) const {
	return grid_[GetIndex(relative_coordinate)];
}

ecs::Entity& Chunk::GetEntity(const V2_int& relative_coordinate) {
	return grid_[GetIndex(relative_coordinate)];
}

const AABB& Chunk::GetInfo() const {
	return info_;
}

void Chunk::Unload() {
	manager_.Clear();
}

std::size_t Chunk::GetIndex(const V2_int& relative_coordinate) const {
	assert(relative_coordinate.x < info_.size.x && "X coordinate out of range of chunk grid");
	assert(relative_coordinate.y < info_.size.y && "Y coordinate out of range of chunk grid");
	auto index = relative_coordinate.x + relative_coordinate.y * static_cast<int>(info_.size.x);
	assert(index < grid_.size() && "Index out of range of chunk grid");
	return index;
}

} // namespace engine