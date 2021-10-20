#pragma once

#include <cstddef> // std::size_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::shared_ptr

class SDL_Texture;

namespace ptgn {

namespace interfaces {

class TextureManager {
public:
    virtual void LoadTexture(const char* texture_key, const char* texture_path) = 0;
    virtual void UnloadTexture(const char* texture_key) = 0;
    virtual bool HasTexture(const char* texture_key) const = 0;
};

} // namespace interface

namespace impl {

class SDLRenderer;

class SDLTextureManager : public interfaces::TextureManager {
public:
    SDLTextureManager() = default;
    ~SDLTextureManager() = default;
    virtual void LoadTexture(const char* texture_key, const char* texture_path) override;
    virtual void UnloadTexture(const char* texture_key) override;
    virtual bool HasTexture(const char* texture_key) const override;
private:
    friend class SDLRenderer;
    void ResetTexture(const char* texture_key, SDL_Texture* shared_texture);
    std::shared_ptr<SDL_Texture> GetTexture(const char* texture_key);
	std::unordered_map<std::size_t, std::shared_ptr<SDL_Texture>> texture_map_;
};

SDLTextureManager& GetSDLTextureManager();

} // namespace impl

namespace services {

interfaces::TextureManager& GetTextureManager();

} // namespace services

} // namespace ptgn