#include "Chunk.h"

#include "math/RNG.h"
#include "math/Vector2.h"
#include "renderer/AABB.h"

#include "ecs/Components.h"
#include "core/Scene.h"

#include "ecs/systems/RenderSystem.h"
#include "ecs/systems/HitboxRenderSystem.h"

namespace engine {

Chunk::~Chunk() {
	manager.~Manager();
	chunk.Destroy();
}

void Chunk::Init(const AABB& chunk_info, const V2_int& tile_size, Scene* scene) {
	this->scene = scene;
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
	manager.Refresh();
	manager.AddSystem<TileRenderSystem>();
}

const ecs::Entity& Chunk::GetEntity(const V2_int& relative_coordinate) const {
	return grid[GetIndex(relative_coordinate)];
}

ecs::Entity& Chunk::GetEntity(const V2_int& relative_coordinate) {
	return grid[GetIndex(relative_coordinate)];
}

const AABB& Chunk::GetInfo() const {
	return info;
}

void Chunk::Unload() {
	manager.Clear();
}

std::size_t Chunk::GetIndex(const V2_int& relative_coordinate) const {
	assert(relative_coordinate.x < info.size.x && "X coordinate out of range of chunk grid");
	assert(relative_coordinate.y < info.size.y && "Y coordinate out of range of chunk grid");
	auto index = relative_coordinate.x + relative_coordinate.y * static_cast<int>(info.size.x);
	assert(index < grid.size() && "Index out of range of chunk grid");
	return index;
}

void Chunk::Render() {
	auto position = scene->WorldToScreen(info.position);
	auto size = scene->Scale(info.size * tile_size);
	AABB rect{ position, size };
	TextureManager::RenderTexture(Engine::GetRenderer(), chunk, NULL, &rect);
}

} // namespace engine