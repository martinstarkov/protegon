#pragma once

#include <cstdint> // std::size_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::shared_ptr

namespace ptgn {

namespace interfaces {

class FontManager {
public:
    virtual void LoadFont(const char* font_key, const char* font_path) = 0;
    virtual void UnloadFont(const char* font_key) = 0;
};

} // namespace interface

namespace impl {

//class SDLRenderer;

class SDLFontManager : public interfaces::FontManager {
public:
    SDLFontManager() = default;
    ~SDLFontManager();
    virtual void LoadFont(const char* font_key, const char* font_path) override;
    virtual void UnloadFont(const char* font_key) override;
private:
    //friend class SDLRenderer;
    //std::shared_ptr<SDL_Font> GetFont(const char* font_key);
	std::unordered_map<std::size_t, void*> font_map_;
};

SDLFontManager& GetSDLFontManager();

} // namespace impl

namespace services {

interfaces::FontManager& GetFontManager();

} // namespace services

} // namespace ptgn