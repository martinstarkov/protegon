#include "TextureManager.h"

#include <cassert> // assert

#include <SDL_image.h>

#include "renderer/Texture.h"
#include "utils/Hasher.h"

#include "core/Engine.h"

namespace engine {

std::unordered_map<std::size_t, Texture> TextureManager::texture_map_;

void TextureManager::Load(const char* texture_key, const char* texture_path) {
	assert(texture_path != "" && "Cannot load empty texture path");
	assert(texture_key != "" && "Cannot load invalid texture key");
	auto key = Hasher::HashCString(texture_key);
	auto it = texture_map_.find(key);
	// Only add texture if it doesn't already exists in map.
	// TODO: Add better checks for texture not already existing (SDL_Texture pointer comparison).
	if (it == std::end(texture_map_)) { 
		SDL_Surface* temp_surface = IMG_Load(texture_path);
		if (!temp_surface) {
			printf("IMG_Load: %s\n", IMG_GetError());
			assert(!"Failed to load image into surface");
		}
		SDL_Texture* texture = SDL_CreateTextureFromSurface(Engine::GetRenderer(), temp_surface);
		SDL_FreeSurface(temp_surface);
		assert(texture != nullptr && "Failed to create texture from surface");
		texture_map_.emplace(key, texture);
	}
}

Color TextureManager::GetDefaultRendererColor() {
	return DEFAULT_RENDERER_COLOR;
}

Texture TextureManager::GetTexture(const char* texture_key) {
	auto key = Hasher::HashCString(texture_key);
	auto it = texture_map_.find(key);
	assert(it != std::end(texture_map_) && "Key does not exist in texture map");
	return it->second;
}

void TextureManager::SetDrawColor(Color color) {
	SDL_SetRenderDrawColor(Engine::GetRenderer(), color.r, color.g, color.b, color.a);
}

void TextureManager::SetDrawColor(Renderer renderer, Color color) {
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void TextureManager::DrawPoint(V2_int point, Color color) {
	SetDrawColor(color);
	SDL_RenderDrawPoint(Engine::GetRenderer(), point.x, point.y);
	SetDrawColor(DEFAULT_RENDERER_COLOR);
}

void TextureManager::DrawPoint(Renderer renderer, V2_int point, Color color) {
	SetDrawColor(renderer, color);
	SDL_RenderDrawPoint(renderer, point.x, point.y);
	SetDrawColor(renderer, DEFAULT_RENDERER_COLOR);
}

void TextureManager::DrawLine(V2_int origin, V2_int destination, Color color) {
	SetDrawColor(color);
	SDL_RenderDrawLine(Engine::GetRenderer(), origin.x, origin.y, destination.x, destination.y);
	SetDrawColor(DEFAULT_RENDERER_COLOR);
}

void TextureManager::DrawLine(Renderer renderer, V2_int origin, V2_int destination, Color color) {
	SetDrawColor(renderer, color);
	SDL_RenderDrawLine(renderer, origin.x, origin.y, destination.x, destination.y);
	SetDrawColor(renderer, DEFAULT_RENDERER_COLOR);
}

void TextureManager::DrawSolidRectangle(V2_int position, V2_int size, Color color) {
	SetDrawColor(color);
	SDL_Rect rect{ position.x, position.y, size.x, size.y };
	SDL_RenderFillRect(Engine::GetRenderer(), &rect);
	SetDrawColor(DEFAULT_RENDERER_COLOR);
}

void TextureManager::DrawRectangle(V2_int position, V2_int size, Color color) {
	SetDrawColor(color);
	SDL_Rect rect{ position.x, position.y, size.x, size.y };
	SDL_RenderDrawRect(Engine::GetRenderer(), &rect);
	SetDrawColor(DEFAULT_RENDERER_COLOR);
}

void TextureManager::DrawRectangle(V2_int position, V2_int size, double rotation, V2_double* center_of_rotation, Color color) {
	SetDrawColor(color);
	SDL_Rect dest_rect{ position.x, position.y, size.x, size.y };
	SDL_Texture* texture = SDL_CreateTexture(Engine::GetRenderer(), SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, size.x, size.y);
	if (center_of_rotation) {
		SDL_Point center{ static_cast<int>(center_of_rotation->x), static_cast<int>(center_of_rotation->y) };
		SDL_RenderCopyEx(Engine::GetRenderer(), texture, NULL, &dest_rect, rotation, &center, SDL_FLIP_NONE);
	} else {
		SDL_RenderCopyEx(Engine::GetRenderer(), texture, NULL, &dest_rect, rotation, NULL, SDL_FLIP_NONE);
	}
}

void TextureManager::DrawRectangle(const char* texture_key, V2_int src_position, V2_int src_size, V2_int dest_position, V2_int dest_size, Flip flip, V2_double* center_of_rotation, double angle) {
	SDL_Rect src_rect{ src_position.x, src_position.y, src_size.x, src_size.y };
	SDL_Rect dest_rect{ dest_position.x, dest_position.y, dest_size.x, dest_size.y };
	if (center_of_rotation) {
		SDL_Point center{ static_cast<int>(center_of_rotation->x), static_cast<int>(center_of_rotation->y) };
		SDL_RenderCopyEx(Engine::GetRenderer(), GetTexture(texture_key), &src_rect, &dest_rect, angle, &center, static_cast<SDL_RendererFlip>(flip));
	} else {
		SDL_RenderCopyEx(Engine::GetRenderer(), GetTexture(texture_key), &src_rect, &dest_rect, angle, NULL, static_cast<SDL_RendererFlip>(flip));
	}
}

void TextureManager::DrawCircle(V2_int center, int radius, Color color) {
	V2_int position{ radius, 0 };
    // Printing the initial point on the axes  
    // after translation 
	DrawPoint(center + position, color);
    // When radius is zero only a single 
    // point will be printed 
    if (radius > 0) {
		DrawPoint({ position.x + center.x, -position.y + center.y }, color);
		DrawPoint({ position.y + center.x, position.x + center.y }, color);
		DrawPoint({ -position.y + center.x, position.x + center.y }, color);
    }

    // Initialising the value of P 
    int P = 1 - radius;
    while (position.x > position.y) {
		position.y++;

        // Mid-point is inside or on the perimeter 
        if (P <= 0)
            P = P + 2 * position.y + 1;

        // Mid-point is outside the perimeter 
        else {
			position.x--;
            P = P + 2 * position.y - 2 * position.x + 1;
        }

        // All the perimeter points have already been printed 
        if (position.x < position.y)
            break;

        // Printing the generated point and its reflection 
        // in the other octants after translation 
		DrawPoint({ position.x + center.x, position.y + center.y }, color);
		DrawPoint({ -position.x + center.x, position.y + center.y }, color);
		DrawPoint({ position.x + center.x, -position.y + center.y }, color);
		DrawPoint({ -position.x + center.x, -position.y + center.y }, color);

        // If the generated point is on the line x = y then  
        // the perimeter points have alreadposition.y been printed 
        if (position.x != position.y) {
			DrawPoint({ position.y + center.x, position.x + center.y }, color);
			DrawPoint({ -position.y + center.x, position.x + center.y }, color);
			DrawPoint({ position.y + center.x, -position.x + center.y }, color);
			DrawPoint({ -position.y + center.x, -position.x + center.y }, color);
        }
    }
}

void TextureManager::Clean() {
	for (auto& pair : texture_map_) {
		SDL_DestroyTexture(pair.second);
	}
	texture_map_.clear();
}

void TextureManager::RemoveTexture(const char* texture_key) {
	auto key = Hasher::HashCString(texture_key);
	auto it = texture_map_.find(key);
	if (it != std::end(texture_map_)) {
		SDL_DestroyTexture(it->second);
		texture_map_.erase(it);
	}
}

} // namespace engine