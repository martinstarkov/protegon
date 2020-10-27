#include "Texture.h"

#include <SDL.h>

namespace engine {

Texture::Texture(SDL_Texture* texture) : texture{ texture } {}
SDL_Texture* Texture::operator=(SDL_Texture* texture) { this->texture = texture; return this->texture; }
Texture::operator SDL_Texture* () const { return texture; }
Texture::operator bool() const { return texture != nullptr; }
SDL_Texture* Texture::operator&() const { return texture; }

} // namespace engine