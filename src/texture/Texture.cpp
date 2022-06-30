#include "Texture.h"

#include "debugging/Debug.h"

#include <SDL.h>
#include <SDL_image.h>

namespace ptgn {

namespace internal {

Texture::Texture(const Renderer& renderer) : renderer_{ &renderer } {}

Texture::Texture(const Renderer& renderer, const char* texture_path) : renderer_{ &renderer } {
	assert(texture_path != "" && "Cannot load empty texture path into the texture manager");
	assert(debug::FileExists(texture_path) && "Cannot load texture with non-existent file path into the texture manager");
	auto surface{ IMG_Load(texture_path) };
	if (surface == nullptr) {
		debug::PrintLine(IMG_GetError());
		assert(!"Failed to create texture by loading image from path onto surface");
	}
	Set(surface);
}

Texture::Texture(const Renderer& renderer, SDL_Surface* surface) : renderer_{ &renderer } {
	Set(surface);
}

void Texture::Reset(SDL_Surface* surface) {
	if (texture_ != nullptr) {
		SDL_DestroyTexture(texture_);
	}
	Set(surface);
}

void Texture::Set(SDL_Surface* surface) {
	assert(texture_ == nullptr && "Cannot set texture after it has already been set");
	assert(surface != nullptr && "Cannot create texture from nullptr surface");
	assert(renderer_ != nullptr && "Cannot create texture using nullptr renderer");
	texture_ = SDL_CreateTextureFromSurface(*renderer_, surface);
	assert(texture_ != nullptr && "Failed to create texture from surface");
	// TODO: CONSIDER: Change not to free surface after use.
	SDL_FreeSurface(surface);
}

Texture::~Texture() {
	SDL_DestroyTexture(texture_);
	texture_ = nullptr;
}


void Texture::Draw(const V2_int& texture_position,
		           const V2_int& texture_size,
		           const V2_int& source_position,
		           const V2_int& source_size) const {
	assert(texture_ != nullptr && "Cannot draw texture which is not loaded in the texture manager");
	SDL_Rect* source{ NULL };
	SDL_Rect source_rectangle;
	if (!source_size.IsZero()) {
		source_rectangle = { source_position.x, source_position.y, source_size.x, source_size.y };
		source = &source_rectangle;
	}
	assert(renderer_ != nullptr && "Cannot draw texture using nullptr renderer");
	SDL_Rect destination{ texture_position.x, texture_position.y, texture_size.x, texture_size.y };
	SDL_RenderCopy(*renderer_, texture_, source, &destination);
}

void Texture::Draw(const V2_int& texture_position,
				   const V2_int& texture_size,
				   const V2_int& source_position,
				   const V2_int& source_size,
				   const V2_int* center_of_rotation,
				   const double angle,
				   Flip flip) const {
	assert(renderer_ != nullptr && "Cannot draw texture using nullptr renderer");
	SDL_Rect* source{ NULL };
	SDL_Rect source_rectangle;
	if (!source_position.IsZero() && !source_size.IsZero()) {
	source_rectangle = { source_position.x, source_position.y, source_size.x, source_size.y };
	source = &source_rectangle;
	}
	SDL_Rect destination{ texture_position.x, texture_position.y, texture_size.x, texture_size.y };
	if (center_of_rotation != nullptr) {
	SDL_Point center{ center_of_rotation->x, center_of_rotation->y };
		SDL_RenderCopyEx(*renderer_, texture_, source, &destination, 
	 					 angle, &center, static_cast<SDL_RendererFlip>(static_cast<int>(flip)));
	} else {
		SDL_RenderCopyEx(*renderer_, texture_, source, &destination, 
	 				     angle, NULL, static_cast<SDL_RendererFlip>(static_cast<int>(flip)));
	}
}

Texture::operator SDL_Texture* () const {
	assert(texture_ != nullptr && "Cannot cast nullptr texture to SDL_Texture");
	return texture_;
}

const Renderer& Texture::GetRenderer() const {
	assert(renderer_ != nullptr && "Cannot dereference renderer which is nullptr");
	return *renderer_;
}

} // namespace internal

} // namespace ptgn