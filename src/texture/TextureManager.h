#pragma once

#include <cstddef> // std::size_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::shared_ptr

class SDL_Texture;
class SDL_Surface;

namespace ptgn {

namespace interfaces {

class TextureManager {
public:
    virtual void LoadTexture(const std::size_t texture_key, const char* texture_path) = 0;
    virtual void UnloadTexture(const std::size_t texture_key) = 0;
    virtual bool HasTexture(const std::size_t texture_key) const = 0;
};

} // namespace interface

namespace impl {

class SDLTextureManager : public interfaces::TextureManager {
public:
    SDLTextureManager() = default;
    ~SDLTextureManager() = default;
    virtual void LoadTexture(const std::size_t texture_key, const char* texture_path) override;
    virtual void UnloadTexture(const std::size_t texture_key) override;
    virtual bool HasTexture(const std::size_t texture_key) const override;
    SDL_Texture* CreateTextureFromSurface(SDL_Surface* surface);
    void SetTexture(const std::size_t texture_key, SDL_Texture* texture);
    std::shared_ptr<SDL_Texture> GetTexture(const std::size_t texture_key);
	std::unordered_map<std::size_t, std::shared_ptr<SDL_Texture>> texture_map_;
};

SDLTextureManager& GetSDLTextureManager();

} // namespace impl

namespace services {

interfaces::TextureManager& GetTextureManager();

} // namespace services

} // namespace ptgn