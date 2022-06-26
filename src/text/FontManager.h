#pragma once

#include <cstddef> // std::size_t
#include <cstdint> // std::uint32_t, std::int32_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::unique_ptr

struct _TTF_Font;
using TTF_Font = _TTF_Font;

namespace ptgn {

namespace interfaces {

class FontManager {
public:
    /*
	* @param point_size - Point size (based on 72 DPI). This translates to pixel height.
	* @param index - Font face index, the first face is 0.
	*/
    virtual void LoadFont(const std::size_t font_key, const char* font_path, std::uint32_t point_size, std::uint32_t index = 0) = 0;
    virtual void UnloadFont(const std::size_t font_key) = 0;
    virtual bool HasFont(const std::size_t font_key) const = 0;
    // Return 0 if font key does not exist in the font manager.
    virtual std::int32_t GetFontHeight(const std::size_t font_key) const = 0;
};

} // namespace interface

namespace internal {

struct SDLFontDeleter {
    void operator()(TTF_Font* font);
};

class SDLFontManager : public interfaces::FontManager {
public:
    SDLFontManager();
    ~SDLFontManager() = default;
    virtual void LoadFont(const std::size_t font_key, const char* font_path, std::uint32_t point_size, std::uint32_t index = 0) override;
    virtual void UnloadFont(const std::size_t font_key) override;
    virtual bool HasFont(const std::size_t font_key) const override;
    virtual std::int32_t GetFontHeight(const std::size_t font_key) const override;
    TTF_Font* GetFont(const std::size_t font_key);
	std::unordered_map<std::size_t, std::unique_ptr<TTF_Font, SDLFontDeleter>> font_map_;
};

SDLFontManager& GetSDLFontManager();

} // namespace internal

namespace services {

interfaces::FontManager& GetFontManager();

} // namespace services

} // namespace ptgn