#include "TextureManager.h"

#include <cassert>

#include <engine/core/Engine.h>

#include <SDL_image.h>

namespace engine {

std::map<std::string, SDL_Texture*> TextureManager::texture_map;

SDL_Texture& TextureManager::Load(std::string& key, const std::string& path) {
	assert(path != "" && "Cannot load empty path");
	assert(key != "" && "Cannot load invalid key");
	auto it = texture_map.find(key);
	if (it != std::end(texture_map)) { // don't create if texture already exists in map
		return *it->second;
	}
	SDL_Texture* texture = nullptr;
	SDL_Surface* temp_surface = IMG_Load(path.c_str());
	assert(temp_surface != nullptr && "Failed to load image into surface");
	texture = SDL_CreateTextureFromSurface(&Engine::GetRenderer(), temp_surface);
	SDL_FreeSurface(temp_surface);
	assert(texture != nullptr && "Failed to create texture from surface");
	texture_map.emplace(key, texture);
	return *texture;
}

Color TextureManager::GetDefaultRendererColor() {
	return DEFAULT_RENDERER_COLOR;
}

SDL_Texture& TextureManager::GetTexture(const std::string& key) {
	auto it = texture_map.find(key);
	assert(it != std::end(texture_map) && "Key does not exist in texture map");
	return *it->second;
}

void TextureManager::SetDrawColor(Color color) {
	SDL_SetRenderDrawColor(&Engine::GetRenderer(), color.r, color.g, color.b, color.a);
}

void TextureManager::DrawPoint(V2_int point, Color color) {
	SetDrawColor(color);
	SDL_RenderDrawPoint(&Engine::GetRenderer(), point.x, point.y);
	SetDrawColor(DEFAULT_RENDERER_COLOR);
}

void TextureManager::DrawLine(V2_int origin, V2_int destination, Color color) {
	SetDrawColor(color);
	SDL_RenderDrawLine(&Engine::GetRenderer(), origin.x, origin.y, destination.x, destination.y);
	SetDrawColor(DEFAULT_RENDERER_COLOR);
}

void TextureManager::DrawRectangle(V2_int position, V2_int size, Color color) {
	SetDrawColor(color);
	SDL_Rect rect{ position.x, position.y, size.x, size.y };
	SDL_RenderDrawRect(&Engine::GetRenderer(), &rect);
	SetDrawColor(DEFAULT_RENDERER_COLOR);
}

void TextureManager::DrawRectangle(const std::string& key, V2_int src_position, V2_int src_size, V2_int dest_position, V2_int dest_size, Flip flip, double angle, V2_int center_of_rotation) {
	SDL_Rect src_rect{ src_position.x, src_position.y, src_size.x, src_size.y };
	SDL_Rect dest_rect{ dest_position.x, dest_position.y, dest_size.x, dest_size.y };
	SDL_Point cor{ center_of_rotation.x, center_of_rotation.y };
	SDL_RenderCopyEx(&Engine::GetRenderer(), &GetTexture(key), &src_rect, &dest_rect, angle, &cor, static_cast<SDL_RendererFlip>(flip));
}

void TextureManager::DrawRectangle(const std::string& key, V2_int src_position, V2_int src_size, V2_int dest_position, V2_int dest_size, Flip flip, double angle) {
	SDL_Rect src_rect{ src_position.x, src_position.y, src_size.x, src_size.y };
	SDL_Rect dest_rect{ dest_position.x, dest_position.y, dest_size.x, dest_size.y };
	SDL_RenderCopyEx(&Engine::GetRenderer(), &GetTexture(key), &src_rect, &dest_rect, angle, NULL, static_cast<SDL_RendererFlip>(flip));
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
	for (auto& pair : texture_map) {
		SDL_DestroyTexture(pair.second);
	}
	texture_map.clear();
}


void TextureManager::RemoveTexture(const std::string& key) {
	auto it = texture_map.find(key);
	if (it != std::end(texture_map)) {
		SDL_DestroyTexture(it->second);
		texture_map.erase(it);
	}
}


} // namespace engine