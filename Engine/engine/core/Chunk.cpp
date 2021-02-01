#include "Chunk.h"

#include "math/RNG.h"

#include "ecs/Components.h"
#include "core/Scene.h"

#include "ecs/systems/RenderSystem.h"
#include "ecs/systems/HitboxRenderSystem.h"
#include <SDL.h>

namespace engine {

Chunk::~Chunk() {
	manager.DestroyEntities();
	SDL_DestroyTexture(chunk);
}

void Chunk::Init(AABB chunk_info, V2_int tile_size, Scene* scene) {
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

void Chunk::Render() {
	auto position = scene->WorldToScreen(info.position);
	auto size = scene->Scale(info.size * tile_size);
	SDL_Rect dest_rect{ position.x, position.y, size.x, size.y };
	SDL_RenderCopy(Engine::GetRenderer(), chunk, NULL, &dest_rect);
}

namespace internal {

// Convert an SDL surface coordinate to a 4 byte integer value containg the RGBA32 color of the pixel.
static std::uint32_t* GetSurfacePixelColor(int pitch, void* pixels, V2_int position) {
	// Source: http://sdl.beuc.net/sdl.wiki/Pixel_Access
	auto row = position.y * pitch;
	auto column = position.x * sizeof(std::uint32_t);
	auto index = static_cast<std::size_t>(row) + static_cast<std::size_t>(column);
	auto pixel_address = static_cast<std::uint8_t*>(pixels) + index;
	return (std::uint32_t*)pixel_address;
}

} // namespace internal

void Chunk::Update() {
	void* pixels;
	int pitch;
	if (SDL_LockTexture(chunk, NULL, &pixels, &pitch) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't lock texture: %s\n", SDL_GetError());
		assert("Couldn't lock texture for chunk rendering");
	}
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
					auto pixel = internal::GetSurfacePixelColor(pitch, pixels, pos);
					*pixel = (0xFF000000 | (color.r << 16) | (color.g << 8) | color.b);
				}
			}
		}
	}
	SDL_UnlockTexture(chunk);
}

} // namespace engine