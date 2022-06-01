#pragma once

#include <cstddef> // std::size_t
#include <unordered_map> // std::unordered_map

#include "text/FontRenderMode.h"
#include "text/FontStyle.h"
#include "renderer/Colors.h"
#include "utils/TypeTraits.h"

namespace ptgn {

namespace interfaces {

class TextManager {
public:
    virtual void LoadText(const std::size_t text_key, const std::size_t font_key, const char* text_content, const Color& text_color) = 0;
    virtual void UnloadText(const std::size_t text_key) = 0;
    virtual bool HasText(const std::size_t text_key) const = 0;
    // Set text content.
	virtual void SetTextContent(const std::size_t text_key, const char* new_content) = 0;

	// Set text color.
	virtual void SetTextColor(const std::size_t text_key, const Color& new_color) = 0;

	// Set text font to a font that has been loaded into the font manager.
	virtual void SetTextFont(const std::size_t text_key, const std::size_t new_font_key) = 0;
};

} // namespace interface

namespace impl {

struct SDLText {
	SDLText() = delete;
	~SDLText() = default;
	SDLText(const std::size_t font_key, const char* content, const Color& color) :
        font_key_{ font_key },
        content_{ content },
        color_{ color } {}
    std::size_t font_key_;
	const char* content_;
	Color color_;
	int style_{ static_cast<int>(FontStyle::NORMAL) };
	Color background_shading_{ color::WHITE };
	FontRenderMode mode_{ FontRenderMode::SOLID };
};

class SDLTextManager : public interfaces::TextManager {
public:
    SDLTextManager();
    ~SDLTextManager();
    virtual void LoadText(const std::size_t text_key, const std::size_t font_key, const char* text_content, const Color& text_color) override;
    virtual void UnloadText(const std::size_t text_key) override;
	virtual bool HasText(const std::size_t text_key) const override;
	virtual void SetTextContent(const std::size_t text_key, const char* new_content) override;
	virtual void SetTextColor(const std::size_t text_key, const Color& new_color) override;
	virtual void SetTextFont(const std::size_t text_key, const std::size_t new_font_key) override;

    // Accepts any number of FontStyle enum values (UNDERLINED, BOLD, etc).
	// These are combined into one style and text is renderer in that style.
	template <typename ...Style,
		type_traits::are_type_e<FontStyle, Style...> = true>
	void SetStyles(const std::size_t text_key, Style... styles) {
        auto it{ text_map_.find(text_key) };
        if (it != std::end(text_map_)) {
            auto& text{ it->second };
		    text.style_ = (static_cast<int>(styles) | ...);
		    RefreshText(text_key, text);
        }
	}

	void SetSolidRenderMode(const std::size_t text_key);

	void SetShadedRenderMode(const std::size_t text_key, const Color& background_shading);

	void SetBlendedRenderMode(const std::size_t text_key);

    void RefreshText(const std::size_t text_key, const SDLText& text);

	std::unordered_map<std::size_t, SDLText> text_map_;
};

SDLTextManager& GetSDLTextManager();

} // namespace impl

namespace services {

interfaces::TextManager& GetTextManager();

} // namespace services

} // namespace ptgn