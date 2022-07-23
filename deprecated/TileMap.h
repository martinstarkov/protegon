#pragma once

#include <tuple>

#include "math/Vector2.h"
#include "renderer/Color.h"

struct SDL_Surface;

namespace ptgn {

class TileMap {
public:
    TileMap(const char* path);
    ~TileMap();
    template <typename T>
    void ForEach(T lambda) {
        Lock();
        V2_int size{ GetSize() };
        for (auto x{ 0 }; x < size.x; ++x) {
            for (auto y{ 0 }; y < size.y; ++y) {
                V2_int location{ x, y };
                lambda(location, GetColor(location));
            }
        }
        Unlock();
    }
    Color GetColor(const V2_int& location);
    V2_int GetSize();
private:
    std::uint32_t GetPixel(const V2_int& location);
    void Lock();
    void Unlock();
    SDL_Surface* surface_{ nullptr };
};

} // namespace ptgn