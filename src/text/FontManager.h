#pragma once

#include <cstddef> // std::size_t
#include <cstdint> // std::uint32_t, std::int32_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::shared_ptr

struct _TTF_Font;
using TTF_Font = _TTF_Font;

namespace ptgn {

namespace interfaces {

class FontManager {
public:
    /*
	* @param point_size - Point size (based on 72 DPI). This translates to pixel height.
	* @param index - Font face index, fhe first face is 0.
	*/
    virtual void LoadFont(const char* font_key, const char* font_path, std::uint32_t point_size, std::uint32_t index = 0) = 0;
    virtual void UnloadFont(const char* font_key) = 0;
    // Return 0 if font key does not exist in the font manager.
    virtual std::int32_t GetHeight(const char* font_key) const = 0;
};

} // namespace interface

namespace impl {

class SDLFontManager : public interfaces::FontManager {
public:
    SDLFontManager() = default;
    ~SDLFontManager() = default;
    virtual void LoadFont(const char* font_key, const char* font_path, std::uint32_t point_size, std::uint32_t index = 0) override;
    virtual void UnloadFont(const char* font_key) override;
    virtual std::int32_t GetHeight(const char* font_key) const override;
private:
    std::shared_ptr<TTF_Font> GetFont(const char* font_key);
	std::unordered_map<std::size_t, std::shared_ptr<TTF_Font>> font_map_;
};

SDLFontManager& GetSDLFontManager();

} // namespace impl

namespace services {

interfaces::FontManager& GetFontManager();

} // namespace services

} // namespace ptgn