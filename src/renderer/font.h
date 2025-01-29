#pragma once

#include <cstdint>
#include <memory>

#include "core/manager.h"
#include "resources/fonts.h"
#include "utility/file.h"

#ifdef __EMSCRIPTEN__
struct _TTF_Font;
using TTF_Font = _TTF_Font;
#else
struct TTF_Font;
#endif

namespace ptgn {

class Font {
public:
private:
	ecs::Entity entity_;
};

enum class FontStyle : int {
	Normal		  = 0, // TTF_STYLE_NORMAL
	Bold		  = 1, // TTF_STYLE_BOLD
	Italic		  = 2, // TTF_STYLE_ITALIC
	Underline	  = 4, // TTF_STYLE_UNDERLINE
	Strikethrough = 8  // TTF_STYLE_STRIKETHROUGH
};

[[nodiscard]] inline FontStyle operator&(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

[[nodiscard]] inline FontStyle operator|(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

enum class FontRenderMode : int {
	Solid	= 0,
	Shaded	= 1,
	Blended = 2
};

namespace impl {

class Game;

class FontInstance {
public:
	FontInstance() = default;
	FontInstance(const path& font_path, std::int32_t point_size = 20, std::int32_t index = 0);

	explicit FontInstance(const FontBinary& binary, std::int32_t point_size = 20);

	[[nodiscard]] std::int32_t GetHeight() const;

	std::shared_ptr<TTF_Font> font_;
};

class FontManager : public MapManager<FontInstance> {
public:
	FontManager()								   = default;
	~FontManager() override						   = default;
	FontManager(FontManager&&) noexcept			   = default;
	FontManager& operator=(FontManager&&) noexcept = default;
	FontManager(const FontManager&)				   = delete;
	FontManager& operator=(const FontManager&)	   = delete;

	// TODO: Re-implement.
	// void SetDefault(const Font& font);
	//[[nodiscard]] Font GetDefault() const;

private:
	friend class Game;

	void Init();

	// Font default_font_;
};

} // namespace impl

} // namespace ptgn
