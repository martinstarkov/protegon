#include "TextureManager.h"

#include <cassert> // assert
#include <cstdint> // assert

#include <SDL_image.h>

#include "math/Hasher.h"

#include "core/Engine.h"

namespace engine {

std::unordered_map<std::size_t, Texture> TextureManager::texture_map_;

void TextureManager::Load(const char* texture_key, const char* texture_path, std::size_t display_index) {
	assert(texture_path != "" && "Cannot load empty texture path");
	assert(texture_key != "" && "Cannot load invalid texture key");
	auto key{ Hasher::HashCString(texture_key) };
	auto it{ texture_map_.find(key) };
	if (it == std::end(texture_map_)) { 
		Surface temp_surface{ texture_path };
		Texture texture{ Engine::GetDisplay(display_index).second, temp_surface };
		temp_surface.Destroy();
		texture_map_.emplace(key, texture);
	} else {
		std::cerr << "Warning: Attempting to load texture which already exists in TextureManager" << std::endl;
	}
}

Texture TextureManager::GetTexture(const char* texture_key) {
	auto key{ Hasher::HashCString(texture_key) };
	auto it{ texture_map_.find(key) };
	assert(it != std::end(texture_map_) && "Cannot retrieve texture key which does not exist in TextureManager");
	return it->second;
}

std::uint32_t& TextureManager::GetTexturePixel(void* pixels, const int pitch, const V2_int& position) {
	// Source: http://sdl.beuc.net/sdl.wiki/Pixel_Access
	//int bpp = surface->format->BytesPerPixel;
	int bpp{ sizeof(std::uint32_t) };
	/* Here p is the address to the pixel we want to retrieve */
	auto p{ (std::uint8_t*)pixels + position.y * pitch + position.x * bpp };
	return *(std::uint32_t*)p;
}

void TextureManager::Clear() {
	for (auto& pair : texture_map_) {
		pair.second.Destroy();
	}
	texture_map_.clear();
}

//void TextureManager::Load(const char* texture_key, const char* texture_path) {
//	assert(texture_path != "" && "Cannot load empty texture path");
//	assert(texture_key != "" && "Cannot load invalid texture key");
//	auto key{ Hasher::HashCString(texture_key) };
//	auto it{ texture_map_.find(key) };
//	// Only add texture if it doesn't already exists in map.
//	// TODO: Add better checks for texture not already existing (SDL_Texture pointer comparison).
//	if (it == std::end(texture_map_)) { 
//		auto temp_surface{ IMG_Load(texture_path) };
//		if (temp_surface == nullptr) {
//			printf("IMG_Load: %s\n", IMG_GetError());
//			assert(!"Failed to load image into surface");
//		}
//		Texture texture{ Engine::GetRenderer(), temp_surface };
//		SDL_FreeSurface(temp_surface);
//		assert(texture.IsValid() && "Failed to create texture from surface");
//		texture_map_.emplace(key, texture);
//	}
//}
//
//Texture TextureManager::GetTexture(const char* texture_key) {
//	auto key{ Hasher::HashCString(texture_key) };
//	auto it{ texture_map_.find(key) };
//	assert(it != texture_map_.end() && "Could not find texture from texture manager");
//	return it->second;
//}

//void TextureManager::RenderTexture(const Renderer& renderer, const Texture& texture, const AABB* source, const AABB* destination) {
//	SDL_Rect* src{ NULL };
//	SDL_Rect* dest{ NULL };
//	SDL_Rect src_rect;
//	SDL_Rect dest_rect;
//	if (source) {
//		src_rect.x = math::Ceil(source->position.x);
//		src_rect.y = math::Ceil(source->position.y);
//		src_rect.w = math::Ceil(source->size.x);
//		src_rect.h = math::Ceil(source->size.y);
//		src = &src_rect;
//	}
//	if (destination) {
//		dest_rect.x = math::Ceil(destination->position.x);
//		dest_rect.y = math::Ceil(destination->position.y);
//		dest_rect.w = math::Ceil(destination->size.x);
//		dest_rect.h = math::Ceil(destination->size.y);
//		dest = &dest_rect;
//	}
//	SDL_RenderCopy(renderer, texture, src, dest);
//}


//Color TextureManager::GetDefaultRendererColor() {
//	return DEFAULT_COLOR;
//}

//void TextureManager::SetDrawColor(const Color& color) {
//	SDL_SetRenderDrawColor(Engine::GetDisplay().second, color.r, color.g, color.b, color.a);
//}

//void TextureManager::SetDrawColor(const Renderer& renderer, const Color& color) {
//	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
//}

//void TextureManager::DrawPoint(const V2_int& point, const Color& color) {
//	SetDrawColor(color);
//	SDL_RenderDrawPoint(Engine::GetDisplay().second, point.x, point.y);
//	SetDrawColor(DEFAULT_COLOR);
//}

//void TextureManager::DrawPoint(const Renderer& renderer, const V2_int& point, const Color& color) {
//	SetDrawColor(renderer, color);
//	SDL_RenderDrawPoint(renderer, point.x, point.y);
//	SetDrawColor(renderer, DEFAULT_COLOR);
//}

//void TextureManager::DrawLine(const V2_int& origin, const V2_int& destination, const Color& color) {
//	SetDrawColor(color);
//	SDL_RenderDrawLine(Engine::GetDisplay().second, origin.x, origin.y, destination.x, destination.y);
//	SetDrawColor(DEFAULT_COLOR);
//}

//void TextureManager::DrawLine(const Renderer& renderer, const V2_int& origin, const V2_int& destination, const Color& color) {
//	SetDrawColor(renderer, color);
//	SDL_RenderDrawLine(renderer, origin.x, origin.y, destination.x, destination.y);
//	SetDrawColor(renderer, DEFAULT_COLOR);
//}

//void TextureManager::DrawSolidRectangle(const V2_int& position, const V2_int& size, const Color& color) {
//	SetDrawColor(color);
//	SDL_Rect rect{ position.x, position.y, size.x, size.y };
//	SDL_RenderFillRect(Engine::GetDisplay().second, &rect);
//	SetDrawColor(DEFAULT_COLOR);
//}

//void TextureManager::DrawRectangle(const V2_int& position, const V2_int& size, const Color& color) {
//	SetDrawColor(color);
//	SDL_Rect rect{ position.x, position.y, size.x, size.y };
//	SDL_RenderDrawRect(Engine::GetDisplay().second, &rect);
//	SetDrawColor(DEFAULT_COLOR);
//}

//void TextureManager::DrawRectangle(const V2_int& position, const V2_int& size, const double rotation, const Color& color, V2_double* center_of_rotation) {
//	SetDrawColor(color);
//	SDL_Rect dest_rect{ position.x, position.y, size.x, size.y };
//	Texture texture{ Engine::GetRenderer(), size, PixelFormat::RGBA8888, TextureAccess::STATIC };
//	if (center_of_rotation) {
//		SDL_Point center{ static_cast<int>(center_of_rotation->x), static_cast<int>(center_of_rotation->y) };
//		SDL_RenderCopyEx(Engine::GetRenderer(), texture, NULL, &dest_rect, rotation, &center, SDL_FLIP_NONE);
//	} else {
//		SDL_RenderCopyEx(Engine::GetRenderer(), texture, NULL, &dest_rect, rotation, NULL, SDL_FLIP_NONE);
//	}
//	texture.Destroy();
//}

//void TextureManager::DrawRectangle(const char* texture_key, const V2_int& src_position, const V2_int& src_size, const V2_int& dest_position, const V2_int& dest_size, Flip flip, V2_double* center_of_rotation, double angle) {
//	SDL_Rect src_rect{ src_position.x, src_position.y, src_size.x, src_size.y };
//	SDL_Rect dest_rect{ dest_position.x, dest_position.y, dest_size.x, dest_size.y };
//	if (center_of_rotation) {
//		SDL_Point center{ static_cast<int>(center_of_rotation->x), static_cast<int>(center_of_rotation->y) };
//		SDL_RenderCopyEx(Engine::GetRenderer(), GetTexture(texture_key), &src_rect, &dest_rect, angle, &center, static_cast<SDL_RendererFlip>(flip));
//	} else {
//		SDL_RenderCopyEx(Engine::GetRenderer(), GetTexture(texture_key), &src_rect, &dest_rect, angle, NULL, static_cast<SDL_RendererFlip>(flip));
//	}
//}

//void TextureManager::DrawCircle(const V2_int& center, const double radius, const Color& color) {
//	int r{ engine::math::Round(radius) };
//	V2_int position{ r, 0 };
//    // Printing the initial point on the axes
//    // after translation 
//	DrawPoint(center + position, color);
//    // When radius is zero only a single 
//    // point will be printed 
//    if (radius > 0) {
//		DrawPoint(V2_int{ position.x + center.x, -position.y + center.y }, color);
//		DrawPoint(V2_int{ position.y + center.x, position.x + center.y }, color);
//		DrawPoint(V2_int{ -position.y + center.x, position.x + center.y }, color);
//    }
//
//    // Initialising the value of P 
//	int P{ 1 - r };
//    while (position.x > position.y) {
//		position.y++;
//
//        // Mid-point is inside or on the perimeter 
//        if (P <= 0)
//            P = P + 2 * position.y + 1;
//
//        // Mid-point is outside the perimeter 
//        else {
//			position.x--;
//            P = P + 2 * position.y - 2 * position.x + 1;
//        }
//
//        // All the perimeter points have already been printed 
//        if (position.x < position.y)
//            break;
//
//        // Printing the generated point and its reflection 
//        // in the other octants after translation 
//		DrawPoint(V2_int{ position.x + center.x, position.y + center.y }, color);
//		DrawPoint(V2_int{ -position.x + center.x, position.y + center.y }, color);
//		DrawPoint(V2_int{ position.x + center.x, -position.y + center.y }, color);
//		DrawPoint(V2_int{ -position.x + center.x, -position.y + center.y }, color);
//
//        // If the generated point is on the line x = y then  
//        // the perimeter points have alreadposition.y been printed 
//        if (position.x != position.y) {
//			DrawPoint(V2_int{ position.y + center.x, position.x + center.y }, color);
//			DrawPoint(V2_int{ -position.y + center.x, position.x + center.y }, color);
//			DrawPoint(V2_int{ position.y + center.x, -position.x + center.y }, color);
//			DrawPoint(V2_int{ -position.y + center.x, -position.x + center.y }, color);
//        }
//    }
//}

//void TextureManager::DrawSolidCircle(const V2_int& center, const double radius, const Color& color) {
//	int r{ engine::math::Round(radius) };
//	for (auto y{ -r }; y <= r; ++y) {
//		for (auto x{ -r }; x <= r; ++x) {
//			if (x * x + y * y <= r * r) {
//				DrawPoint({ center.x + x, center.y + y }, color);
//			}
//		}
//	}
//}

//void TextureManager::RemoveTexture(const char* texture_key) {
//	auto key{ Hasher::HashCString(texture_key) };
//	auto it{ texture_map_.find(key) };
//	if (it != std::end(texture_map_)) {
//		it->second.Destroy();
//		texture_map_.erase(it);
//	}
//}

} // namespace engine