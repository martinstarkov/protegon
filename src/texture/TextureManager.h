#pragma once

#include <cstddef> // std::size_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::unique_ptr

struct SDL_Texture;
struct SDL_Surface;

namespace ptgn {

namespace interfaces {

class TextureManager {
public:
    virtual void LoadTexture(const std::size_t texture_key, const char* texture_path) = 0;
    virtual void UnloadTexture(const std::size_t texture_key) = 0;
    virtual bool HasTexture(const std::size_t texture_key) const = 0;
};

} // namespace interface

namespace internal {

struct SDLTextureDeleter {
    void operator()(SDL_Texture* texture);
};

class SDLTextureManager : public interfaces::TextureManager {
public:
    SDLTextureManager();
    ~SDLTextureManager() = default;
    virtual void LoadTexture(const std::size_t texture_key, const char* texture_path) override;
    virtual void UnloadTexture(const std::size_t texture_key) override;
    virtual bool HasTexture(const std::size_t texture_key) const override;
    // TODO: Figure out how to make these private while working with Renderer.cpp and TextManager.cpp.
    SDL_Texture* GetTexture(const std::size_t texture_key) const;
    SDL_Texture* CreateTextureFromSurface(SDL_Surface* surface) const;
    void SetTexture(const std::size_t texture_key, SDL_Texture* texture);
private:
	
    std::unordered_map<std::size_t, std::unique_ptr<SDL_Texture, SDLTextureDeleter>> texture_map_;
};

SDLTextureManager& GetSDLTextureManager();

} // namespace internal

namespace services {

interfaces::TextureManager& GetTextureManager();

} // namespace services

} // namespace ptgn