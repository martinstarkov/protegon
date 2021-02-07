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
	manager.Clear();
	chunk.Destroy();
}

void Chunk::Init(const AABB& chunk_info, const V2_int& tile_size, Scene* scene) {
	this->scene = scene;
	info = chunk_info;
	this->tile_size = tile_size;
	auto count = info.size.x * info.size.y;
	//chunk.texture = SDL_CreateTexture(Engine::GetRenderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, static_cast<int>(info.size.x) * tile_size.x, static_cast<int>(info.size.y) * tile_size.y);
	// Generate new empty grid.
	if (grid.size() != count) {
		grid.resize((std::size_t)count, ecs::null);
		for (size_t i = 0; i < count; i++) {
			grid[i] = manager.CreateEntity();
		}
	}
	manager.AddSystem<TileRenderSystem>(scene);
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
	manager.DestroyEntities();
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

void Chunk::Update() {
	void* pixels;
	int pitch;
	auto locked = chunk.Lock(&pixels, &pitch, NULL);
	assert(locked && "Couldn't lock texture for chunk rendering");
	for (auto i = 0; i < static_cast<int>(info.size.x); ++i) {
		for (auto j = 0; j < static_cast<int>(info.size.y); ++j) {
			auto tile = V2_int{ i, j };
			auto tile_position = tile * tile_size;
			auto& entity = GetEntity(tile);
			auto color = TextureManager::GetDefaultRendererColor();
			if (entity.HasComponent<RenderComponent>()) {
				color = entity.GetComponent<RenderComponent>().color;
			}
			for (auto row = 0; row < tile_size.y; ++row) {
				for (auto col = 0; col < tile_size.x; ++col) {
					auto pos = tile_position + V2_int{ col, row };
					auto pixel = TextureManager::GetTexturePixel(pixels, pitch, pos);
					pixel = (0xFF000000 | (color.r << 16) | (color.g << 8) | color.b);
				}
			}
		}
	}
	chunk.Unlock();
}

} // namespace engine