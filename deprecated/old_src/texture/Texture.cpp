#include "Texture.h"

#include <SDL.h>
#include <SDL_image.h>

#include "managers/WindowManager.h"
#include "debugging/Debug.h"

namespace ptgn {

namespace internal {

void Texture::Set(SDL_Surface* surface) {
	assert(texture_ == nullptr && "Cannot set texture after it has already been set");
	assert(surface != nullptr && "Cannot create texture from nullptr surface");
	const auto& renderer{ managers::GetManager<managers::WindowManager>().GetTargetRenderer() };
	texture_ = SDL_CreateTextureFromSurface(renderer, surface);
	assert(texture_ != nullptr && "Failed to create texture from surface");
	// TODO: CONSIDER: Change not to free surface after use.
	SDL_FreeSurface(surface);
}

Texture::Texture(const char* texture_path) {
	assert(texture_path != "" && "Cannot load empty texture path into the texture manager");
	assert(debug::FileExists(texture_path) && "Cannot load texture with non-existent file path into the texture manager");
	auto surface{ IMG_Load(texture_path) };
	if (surface == nullptr) {
		debug::PrintLine(IMG_GetError());
		assert(!"Failed to create texture by loading image from path onto surface");
	}
	Set(surface);
}

Texture::Texture(SDL_Surface* surface) {
	Set(surface);
}

Texture::~Texture() {
	SDL_DestroyTexture(texture_);
	texture_ = nullptr;
}

void Texture::Reset(SDL_Surface* surface) {
	if (texture_ != nullptr) {
		SDL_DestroyTexture(texture_);
	}
	Set(surface);
}

Texture::operator SDL_Texture*() const {
	assert(texture_ != nullptr && "Cannot cast nullptr texture to SDL_Texture");
	return texture_;
}

} // namespace internal

} // namespace ptgn