#pragma once

#include <cstdint> // std::size_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::shared_ptr

class SDL_Texture;

namespace ptgn {

namespace interfaces {

class TextureManager {
public:
    virtual void LoadTexture(const char* texture_key, const char* texture_path) = 0;
    virtual void UnloadTexture(const char* texture_key) = 0;
};

} // namespace interface

namespace impl {

class SDLRenderer;

class SDLTextureManager : public interfaces::TextureManager {
public:
    SDLTextureManager() = default;
    ~SDLTextureManager();
    virtual void LoadTexture(const char* texture_key, const char* texture_path) override;
    virtual void UnloadTexture(const char* texture_key) override;
private:
    friend class SDLRenderer;
    std::shared_ptr<SDL_Texture> GetTexture(const char* texture_key);
	std::unordered_map<std::size_t, std::shared_ptr<SDL_Texture>> texture_map_;
};

SDLTextureManager& GetSDLTextureManager();

} // namespace impl

namespace services {

interfaces::TextureManager& GetTextureManager();

} // namespace services

} // namespace ptgn