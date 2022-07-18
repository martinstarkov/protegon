#include "TileMap.h"

#include <cassert>
#include <sstream>

#include <SDL.h>
#include <SDL_image.h>

#include "debugging/Debug.h"
#include "renderer/PixelFormat.h"

namespace ptgn {

TileMap::TileMap(const char* path) {
    assert(path != "" && "Cannot load tile map from empty texture path");
	assert(debug::FileExists(path) && "Cannot load surface with non-existent into tile map");
	surface_ = IMG_Load(path);
    if (surface_ == nullptr) {
		debug::PrintLine("Failed to load surface into tile map: ", IMG_GetError());
	}
}

TileMap::~TileMap() {
    SDL_FreeSurface(surface_);
}

void TileMap::Lock() {
    assert(surface_ != nullptr && "Cannot lock invalid surface");
    SDL_LockSurface(surface_);
}

void TileMap::Unlock() {
    assert(surface_ != nullptr && "Cannot unlock invalid surface");
    SDL_UnlockSurface(surface_);
}

V2_int TileMap::GetSize() {
    return { surface_->w, surface_->h };
}

Color TileMap::GetColor(const V2_int& location) {
    return { GetPixel(location), surface_->format };
}

std::uint32_t TileMap::GetPixel(const V2_int& location) {
    int bpp = surface_->format->BytesPerPixel;
    std::uint8_t* p = (std::uint8_t*)surface_->pixels + location.y * surface_->pitch + location.x * bpp;
    switch (bpp) {
        case 1:
            return *p;
            break;
        case 2:
            return *(std::uint16_t*)p;
            break;
        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                return p[0] << 16 | p[1] << 8 | p[2];
            } else {
                return p[0] | p[1] << 8 | p[2] << 16;
            }
            break;
        case 4:
            return *(std::uint32_t*)p;
            break;
        default:
            return 0; // Error case.
    }
}

} // namespace ptgn